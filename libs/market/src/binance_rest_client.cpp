#include "veloz/market/binance_rest_client.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/string.h>

namespace veloz::market {

namespace detail {

// Convert kj::StringPtr to double (KJ equivalent of std::stod)
// KJ doesn't provide a direct string-to-double conversion function.
double kjStodRest(kj::StringPtr str) {
  if (str.size() == 0) {
    return 0.0;
  }

  double result = 0.0;
  bool isNegative = false;
  size_t i = 0;

  // Skip leading whitespace
  while (i < str.size() && (str[i] == ' ' || str[i] == '\t')) {
    ++i;
  }

  // Check for negative sign
  if (i < str.size() && str[i] == '-') {
    isNegative = true;
    ++i;
  } else if (i < str.size() && str[i] == '+') {
    ++i;
  }

  // Parse integer part
  while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
    result = result * 10.0 + (str[i] - '0');
    ++i;
  }

  // Parse fractional part
  if (i < str.size() && str[i] == '.') {
    ++i;
    double divisor = 10.0;
    while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
      result += (str[i] - '0') / divisor;
      divisor *= 10.0;
      ++i;
    }
  }

  // Handle scientific notation (e.g., 1.5e-8)
  if (i < str.size() && (str[i] == 'e' || str[i] == 'E')) {
    ++i;
    bool expNegative = false;
    if (i < str.size() && str[i] == '-') {
      expNegative = true;
      ++i;
    } else if (i < str.size() && str[i] == '+') {
      ++i;
    }

    int exponent = 0;
    while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
      exponent = exponent * 10 + (str[i] - '0');
      ++i;
    }

    double expMultiplier = 1.0;
    for (int j = 0; j < exponent; ++j) {
      expMultiplier *= 10.0;
    }

    if (expNegative) {
      result /= expMultiplier;
    } else {
      result *= expMultiplier;
    }
  }

  return isNegative ? -result : result;
}

// Convert kj::StringPtr to int64_t
std::int64_t kjStoi64(kj::StringPtr str) {
  if (str.size() == 0) {
    return 0;
  }

  std::int64_t result = 0;
  bool isNegative = false;
  size_t i = 0;

  // Skip leading whitespace
  while (i < str.size() && (str[i] == ' ' || str[i] == '\t')) {
    ++i;
  }

  // Check for negative sign
  if (i < str.size() && str[i] == '-') {
    isNegative = true;
    ++i;
  } else if (i < str.size() && str[i] == '+') {
    ++i;
  }

  // Parse digits
  while (i < str.size() && str[i] >= '0' && str[i] <= '9') {
    result = result * 10 + (str[i] - '0');
    ++i;
  }

  return isNegative ? -result : result;
}

// Simple JSON value extractor (for specific fields)
// Returns the value as a string, or empty if not found
kj::Maybe<kj::String> extractJsonValue(kj::StringPtr json, kj::StringPtr key) {
  // Look for "key": or "key":
  kj::String searchKey = kj::str("\"", key, "\":");
  const char* pos = strstr(json.cStr(), searchKey.cStr());
  if (pos == nullptr) {
    return kj::none;
  }

  pos += searchKey.size();

  // Skip whitespace
  while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
    ++pos;
  }

  // Determine value type
  if (*pos == '"') {
    // String value
    ++pos;
    const char* end = strchr(pos, '"');
    if (end == nullptr) {
      return kj::none;
    }
    return kj::heapString(pos, end - pos);
  } else if (*pos == '[') {
    // Array - find matching ]
    int depth = 1;
    const char* start = pos;
    ++pos;
    while (*pos && depth > 0) {
      if (*pos == '[')
        ++depth;
      else if (*pos == ']')
        --depth;
      ++pos;
    }
    return kj::heapString(start, pos - start);
  } else if (*pos == '{') {
    // Object - find matching }
    int depth = 1;
    const char* start = pos;
    ++pos;
    while (*pos && depth > 0) {
      if (*pos == '{')
        ++depth;
      else if (*pos == '}')
        --depth;
      ++pos;
    }
    return kj::heapString(start, pos - start);
  } else {
    // Number or boolean
    const char* start = pos;
    while (*pos && *pos != ',' && *pos != '}' && *pos != ']' && *pos != ' ' && *pos != '\n' &&
           *pos != '\r') {
      ++pos;
    }
    return kj::heapString(start, pos - start);
  }
}

// Parse a JSON array of [price, qty] pairs into BookLevel vector
void parseBookLevels(kj::StringPtr arrayJson, kj::Vector<BookLevel>& levels) {
  // arrayJson is like: [["0.00001","100"],["0.00002","200"]]
  if (arrayJson.size() < 2 || arrayJson[0] != '[') {
    return;
  }

  size_t i = 1; // Skip opening [
  while (i < arrayJson.size()) {
    // Skip whitespace
    while (i < arrayJson.size() &&
           (arrayJson[i] == ' ' || arrayJson[i] == '\n' || arrayJson[i] == '\r' ||
            arrayJson[i] == '\t' || arrayJson[i] == ',')) {
      ++i;
    }

    if (i >= arrayJson.size() || arrayJson[i] == ']') {
      break;
    }

    // Expect [
    if (arrayJson[i] != '[') {
      break;
    }
    ++i;

    // Parse price (quoted string)
    while (i < arrayJson.size() && arrayJson[i] != '"') {
      ++i;
    }
    if (i >= arrayJson.size()) {
      break;
    }
    ++i; // Skip opening "

    size_t priceStart = i;
    while (i < arrayJson.size() && arrayJson[i] != '"') {
      ++i;
    }
    kj::String priceStr = kj::heapString(arrayJson.begin() + priceStart, i - priceStart);
    ++i; // Skip closing "

    // Skip comma
    while (i < arrayJson.size() && (arrayJson[i] == ',' || arrayJson[i] == ' ')) {
      ++i;
    }

    // Parse qty (quoted string)
    while (i < arrayJson.size() && arrayJson[i] != '"') {
      ++i;
    }
    if (i >= arrayJson.size()) {
      break;
    }
    ++i; // Skip opening "

    size_t qtyStart = i;
    while (i < arrayJson.size() && arrayJson[i] != '"') {
      ++i;
    }
    kj::String qtyStr = kj::heapString(arrayJson.begin() + qtyStart, i - qtyStart);
    ++i; // Skip closing "

    // Skip to closing ]
    while (i < arrayJson.size() && arrayJson[i] != ']') {
      ++i;
    }
    ++i; // Skip closing ]

    // Add level
    BookLevel level;
    level.price = kjStodRest(priceStr);
    level.qty = kjStodRest(qtyStr);
    levels.add(level);
  }
}

} // namespace detail

BinanceRestClient::BinanceRestClient(kj::AsyncIoContext& ioContext, bool testnet)
    : ioContext_(ioContext), testnet_(testnet) {

  // Initialize HTTP header table
  headerTable_ = kj::heap<kj::HttpHeaderTable>();

  // Initialize TLS context with default options (uses system trust store)
  kj::TlsContext::Options tlsOptions;
  tlsContext_ = kj::heap<kj::TlsContext>(kj::mv(tlsOptions));

  // Set base URL based on testnet flag
  if (testnet_) {
    baseUrl_ = kj::heapString("testnet.binance.vision");
  } else {
    baseUrl_ = kj::heapString("api.binance.com");
  }

  KJ_LOG(INFO, "BinanceRestClient initialized", testnet_ ? "testnet" : "mainnet", baseUrl_);
}

BinanceRestClient::~BinanceRestClient() = default;

bool BinanceRestClient::is_connected() const {
  return tlsContext_.get() != nullptr;
}

kj::Promise<kj::Own<kj::HttpClient>> BinanceRestClient::get_http_client() {
  auto& network = ioContext_.provider->getNetwork();

  // Parse address and connect
  return network.parseAddress(kj::str(baseUrl_, ":443"))
      .then(
          [this](kj::Own<kj::NetworkAddress> addr) { return addr->connect().attach(kj::mv(addr)); })
      .then([this](kj::Own<kj::AsyncIoStream> stream) {
        // Wrap with TLS
        return tlsContext_->wrapClient(kj::mv(stream), baseUrl_);
      })
      .then([this](kj::Own<kj::AsyncIoStream> tlsStream) {
        // Create HTTP client over TLS stream
        return kj::newHttpClient(*headerTable_, *tlsStream).attach(kj::mv(tlsStream));
      });
}

kj::Promise<kj::String> BinanceRestClient::http_get(kj::StringPtr endpoint, kj::StringPtr params) {
  return get_http_client().then([this, endpoint = kj::heapString(endpoint),
                                 params = kj::heapString(params)](kj::Own<kj::HttpClient> client) {
    // Build URL
    kj::String url;
    if (params.size() > 0) {
      url = kj::str("https://", baseUrl_, endpoint, "?", params);
    } else {
      url = kj::str("https://", baseUrl_, endpoint);
    }

    KJ_LOG(INFO, "HTTP GET", url);

    // Make request
    auto request = client->request(kj::HttpMethod::GET, url, kj::HttpHeaders(*headerTable_));

    return request.response
        .then([](kj::HttpClient::Response response) {
          if (response.statusCode != 200) {
            KJ_LOG(WARNING, "HTTP request failed", response.statusCode, response.statusText);
            return response.body->readAllText().then(
                [statusCode = response.statusCode](kj::String body) -> kj::String {
                  KJ_FAIL_REQUIRE("HTTP request failed", statusCode, body);
                });
          }
          return response.body->readAllText();
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<BookData> BinanceRestClient::fetch_depth_snapshot(kj::StringPtr symbol, int depth) {
  // Build params
  kj::String params = kj::str("symbol=", symbol, "&limit=", depth);

  return http_get("/api/v3/depth"_kj, params).then([](kj::String response) -> BookData {
    BookData result;
    result.is_snapshot = true;

    // Parse lastUpdateId
    KJ_IF_SOME(lastUpdateIdStr, detail::extractJsonValue(response, "lastUpdateId"_kj)) {
      result.sequence = detail::kjStoi64(lastUpdateIdStr);
    }

    // Parse bids
    KJ_IF_SOME(bidsJson, detail::extractJsonValue(response, "bids"_kj)) {
      detail::parseBookLevels(bidsJson, result.bids);
    }

    // Parse asks
    KJ_IF_SOME(asksJson, detail::extractJsonValue(response, "asks"_kj)) {
      detail::parseBookLevels(asksJson, result.asks);
    }

    KJ_LOG(INFO, "Parsed depth snapshot", result.sequence, result.bids.size(), result.asks.size());

    return result;
  });
}

kj::Promise<kj::Maybe<double>> BinanceRestClient::fetch_price(kj::StringPtr symbol) {
  kj::String params = kj::str("symbol=", symbol);

  return http_get("/api/v3/ticker/price"_kj, params)
      .then([](kj::String response) -> kj::Maybe<double> {
        KJ_IF_SOME(priceStr, detail::extractJsonValue(response, "price"_kj)) {
          return detail::kjStodRest(priceStr);
        }
        return kj::none;
      });
}

} // namespace veloz::market
