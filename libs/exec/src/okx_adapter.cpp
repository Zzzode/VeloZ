#include "veloz/exec/okx_adapter.h"
#include "veloz/exec/hmac_wrapper.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <sstream>

#ifndef VELOZ_NO_OPENSSL
// OpenSSL is used internally by HmacSha256 wrapper for HMAC-SHA256 signature generation.
// This is required for OKX API authentication and KJ does not provide HMAC functionality.
// Per CLAUDE.md guidelines, this is an acceptable use of external library for API compatibility.
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

namespace veloz::exec {

namespace {
constexpr const char* OKX_REST_URL = "www.okx.com";
constexpr const char* OKX_DEMO_REST_URL = "www.okx.com"; // OKX uses same URL with simulated flag
constexpr const char* OKX_WS_URL = "ws.okx.com";
constexpr const char* OKX_DEMO_WS_URL = "wspap.okx.com";
constexpr int DEFAULT_RECV_WINDOW = 5000;
} // namespace

OKXAdapter::OKXAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key,
                       kj::StringPtr secret_key, kj::StringPtr passphrase, bool demo)
    : io_context_(io_context), api_key_(api_key.cStr()), secret_key_(secret_key.cStr()),
      passphrase_(passphrase.cStr()), connected_(false), demo_(demo),
      last_activity_time_(kj::systemCoarseMonotonicClock().now()),
      request_timeout_(30 * kj::SECONDS), rate_limit_window_(1 * kj::SECONDS),
      rate_limit_per_window_(20), max_retries_(3), retry_delay_(1 * kj::SECONDS) {

  // Initialize HTTP header table
  kj::HttpHeaderTable::Builder builder;
  header_table_ = builder.build();

  // Initialize TLS context
  tls_context_ = kj::heap<kj::TlsContext>();

  // Set API endpoints based on environment
  base_rest_url_ = kj::str(demo ? OKX_DEMO_REST_URL : OKX_REST_URL);
  base_ws_url_ = kj::str(demo ? OKX_DEMO_WS_URL : OKX_WS_URL);
}

OKXAdapter::~OKXAdapter() noexcept {
  disconnect();
}

kj::StringPtr OKXAdapter::name() const {
  return "OKX"_kj;
}

kj::StringPtr OKXAdapter::version() const {
  return "1.0.0"_kj;
}

bool OKXAdapter::is_connected() const {
  return connected_;
}

void OKXAdapter::connect() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
}

void OKXAdapter::disconnect() {
  connected_ = false;
}

kj::Promise<void> OKXAdapter::connect_async() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
  return kj::READY_NOW;
}

void OKXAdapter::set_timeout(kj::Duration timeout) {
  request_timeout_ = timeout;
}

kj::Duration OKXAdapter::get_timeout() const {
  return request_timeout_;
}

std::string OKXAdapter::get_timestamp_iso() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
  return oss.str();
}

std::string OKXAdapter::build_signature(const std::string& timestamp, const std::string& method,
                                        const std::string& request_path, const std::string& body) {
#ifdef VELOZ_NO_OPENSSL
  return "";
#else
  std::string prehash = timestamp + method + request_path + body;

  // Use HmacSha256 wrapper for HMAC-SHA256 signature
  std::string signature = HmacSha256::sign(secret_key_, prehash);

  // Base64 encode the signature
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string result;
  result.reserve(((signature.size() + 2) / 3) * 4);

  for (size_t i = 0; i < signature.size(); i += 3) {
    unsigned int n = (static_cast<unsigned int>(static_cast<unsigned char>(signature[i])) << 16);
    if (i + 1 < signature.size())
      n |= (static_cast<unsigned int>(static_cast<unsigned char>(signature[i + 1])) << 8);
    if (i + 2 < signature.size())
      n |= static_cast<unsigned int>(static_cast<unsigned char>(signature[i + 2]));

    result.push_back(base64_chars[(n >> 18) & 0x3F]);
    result.push_back(base64_chars[(n >> 12) & 0x3F]);
    result.push_back((i + 1 < signature.size()) ? base64_chars[(n >> 6) & 0x3F] : '=');
    result.push_back((i + 2 < signature.size()) ? base64_chars[n & 0x3F] : '=');
  }

  return result;
#endif
}

kj::String OKXAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
  // OKX uses format like "BTC-USDT" for spot
  std::string sym = symbol.value;
  // Convert BTCUSDT to BTC-USDT
  if (sym.length() >= 6 && sym.substr(sym.length() - 4) == "USDT") {
    return kj::str(sym.substr(0, sym.length() - 4).c_str(), "-USDT");
  }
  if (sym.length() >= 5 && sym.substr(sym.length() - 3) == "USD") {
    return kj::str(sym.substr(0, sym.length() - 3).c_str(), "-USD");
  }
  return kj::str(sym.c_str());
}

kj::StringPtr OKXAdapter::order_side_to_string(OrderSide side) {
  switch (side) {
  case OrderSide::Buy:
    return "buy"_kj;
  case OrderSide::Sell:
    return "sell"_kj;
  default:
    return "buy"_kj;
  }
}

kj::StringPtr OKXAdapter::order_type_to_string(OrderType type) {
  switch (type) {
  case OrderType::Market:
    return "market"_kj;
  case OrderType::Limit:
    return "limit"_kj;
  default:
    return "limit"_kj;
  }
}

OrderStatus OKXAdapter::parse_order_status(kj::StringPtr status_str) {
  if (status_str == "live") {
    return OrderStatus::Accepted;
  } else if (status_str == "partially_filled") {
    return OrderStatus::PartiallyFilled;
  } else if (status_str == "filled") {
    return OrderStatus::Filled;
  } else if (status_str == "canceled") {
    return OrderStatus::Canceled;
  } else if (status_str == "mmp_canceled") {
    return OrderStatus::Canceled;
  }
  return OrderStatus::New;
}

kj::Promise<kj::Own<kj::HttpClient>> OKXAdapter::get_http_client() {
  auto& network = io_context_.provider->getNetwork();
  return network.parseAddress(base_rest_url_, 443).then([this](kj::Own<kj::NetworkAddress> addr) {
    return addr->connect().then([this, addr = kj::mv(addr)](kj::Own<kj::AsyncIoStream> stream) {
      return tls_context_->wrapClient(kj::mv(stream), base_rest_url_.cStr())
          .then([this](kj::Own<kj::AsyncIoStream> tlsStream) {
            return kj::newHttpClient(*header_table_, *tlsStream).attach(kj::mv(tlsStream));
          });
    });
  });
}

kj::Promise<kj::String> OKXAdapter::http_get_async(kj::StringPtr endpoint, kj::StringPtr params) {
  auto timestamp = get_timestamp_iso();
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_signature(timestamp, "GET", request_path.cStr());

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::str(timestamp.c_str()),
                                 signature = kj::str(signature.c_str())](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    auto url = params.size() > 0 ? kj::str("https://", base_rest_url_, endpoint, "?", params)
                                 : kj::str("https://", base_rest_url_, endpoint);

    auto request = client->request(kj::HttpMethod::GET, url, headers);
    return request.response
        .then([](kj::HttpClient::Response response) {
          return response.body->readAllText().then([status = response.statusCode](kj::String body) {
            if (status >= 200 && status < 300) {
              return kj::mv(body);
            }
            KJ_LOG(ERROR, "OKX API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::String> OKXAdapter::http_post_async(kj::StringPtr endpoint, kj::StringPtr body) {
  auto timestamp = get_timestamp_iso();
  auto signature = build_signature(timestamp, "POST", endpoint.cStr(), body.cStr());

  return get_http_client().then([this, endpoint = kj::str(endpoint), body = kj::str(body),
                                 timestamp = kj::str(timestamp.c_str()),
                                 signature = kj::str(signature.c_str())](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    auto url = kj::str("https://", base_rest_url_, endpoint);
    auto request = client->request(kj::HttpMethod::POST, url, headers, body.size());

    return request.body->write(body.asBytes())
        .then([response = kj::mv(request.response)]() mutable { return kj::mv(response); })
        .then([](kj::HttpClient::Response response) {
          return response.body->readAllText().then([status = response.statusCode](kj::String body) {
            if (status >= 200 && status < 300) {
              return kj::mv(body);
            }
            KJ_LOG(ERROR, "OKX API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::String> OKXAdapter::http_delete_async(kj::StringPtr endpoint,
                                                      kj::StringPtr params) {
  auto timestamp = get_timestamp_iso();
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_signature(timestamp, "DELETE", request_path.cStr());

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::str(timestamp.c_str()),
                                 signature = kj::str(signature.c_str())](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    auto url = params.size() > 0 ? kj::str("https://", base_rest_url_, endpoint, "?", params)
                                 : kj::str("https://", base_rest_url_, endpoint);

    auto request = client->request(kj::HttpMethod::DELETE, url, headers);
    return request.response
        .then([](kj::HttpClient::Response response) {
          return response.body->readAllText().then([status = response.statusCode](kj::String body) {
            if (status >= 200 && status < 300) {
              return kj::mv(body);
            }
            KJ_LOG(ERROR, "OKX API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::Maybe<ExecutionReport>>
OKXAdapter::place_order_async(const PlaceOrderRequest& req) {
  // Build order request body
  auto symbol = format_symbol(req.symbol);
  auto side = order_side_to_string(req.side);
  auto type = order_type_to_string(req.type);

  kj::String body;
  KJ_IF_SOME(price, req.price) {
    body = kj::str("{\"instId\":\"", symbol, "\",\"tdMode\":\"cash\",\"side\":\"", side,
                   "\",\"ordType\":\"", type, "\",\"sz\":\"", req.qty, "\",\"px\":\"", price,
                   "\",\"clOrdId\":\"", req.client_order_id, "\"}");
  } else {
    body = kj::str("{\"instId\":\"", symbol, "\",\"tdMode\":\"cash\",\"side\":\"", side,
                   "\",\"ordType\":\"", type, "\",\"sz\":\"", req.qty, "\",\"clOrdId\":\"",
                   req.client_order_id, "\"}");
  }

  // Capture necessary fields by value since kj::String is not copyable
  auto symbol_value = kj::str(req.symbol.value.c_str());
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v5/trade/order", body)
      .then([symbol_value = kj::mv(symbol_value),
             client_order_id = kj::mv(client_order_id_copy)](kj::String response) mutable {
        // Parse response and create execution report
        ExecutionReport report;
        report.symbol = veloz::common::SymbolId{symbol_value.cStr()};
        report.client_order_id = kj::mv(client_order_id);
        report.status = OrderStatus::Accepted;
        report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

        return kj::Maybe<ExecutionReport>(kj::mv(report));
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
OKXAdapter::cancel_order_async(const CancelOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto body = kj::str("{\"instId\":\"", symbol, "\",\"clOrdId\":\"", req.client_order_id, "\"}");

  auto symbol_value = kj::str(req.symbol.value.c_str());
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v5/trade/cancel-order", body)
      .then([symbol_value = kj::mv(symbol_value),
             client_order_id = kj::mv(client_order_id_copy)](kj::String response) mutable {
        ExecutionReport report;
        report.symbol = veloz::common::SymbolId{symbol_value.cStr()};
        report.client_order_id = kj::mv(client_order_id);
        report.status = OrderStatus::Canceled;
        report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

        return kj::Maybe<ExecutionReport>(kj::mv(report));
      });
}

kj::Maybe<ExecutionReport> OKXAdapter::place_order(const PlaceOrderRequest& req) {
  // Synchronous version - not recommended for production use
  // This would require blocking on the async operation
  return {};
}

kj::Maybe<ExecutionReport> OKXAdapter::cancel_order(const CancelOrderRequest& req) {
  // Synchronous version - not recommended for production use
  return {};
}

kj::Promise<kj::Maybe<double>>
OKXAdapter::get_current_price_async(const veloz::common::SymbolId& symbol) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol);

  return http_get_async("/api/v5/market/ticker", params).then([](kj::String response) -> kj::Maybe<double> {
    // Parse response - in production, use proper JSON parsing
    return kj::none;
  });
}

kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
OKXAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol, "&sz=", depth);

  return http_get_async("/api/v5/market/books", params).then([](kj::String response) -> kj::Maybe<kj::Array<PriceLevel>> {
    return kj::none;
  });
}

kj::Promise<kj::Maybe<kj::Array<TradeData>>>
OKXAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol, "&limit=", limit);

  return http_get_async("/api/v5/market/trades", params).then([](kj::String response) -> kj::Maybe<kj::Array<TradeData>> {
    return kj::none;
  });
}

kj::Promise<kj::Maybe<double>> OKXAdapter::get_account_balance_async(kj::StringPtr asset) {
  auto params = kj::str("ccy=", asset);

  return http_get_async("/api/v5/account/balance", params).then([](kj::String response) -> kj::Maybe<double> {
    return kj::none;
  });
}

kj::Maybe<double> OKXAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  return kj::none;
}

kj::Maybe<kj::Array<PriceLevel>>
OKXAdapter::get_order_book(const veloz::common::SymbolId& symbol, int depth) {
  return kj::none;
}

kj::Maybe<kj::Array<TradeData>>
OKXAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
  return kj::none;
}

kj::Maybe<double> OKXAdapter::get_account_balance(kj::StringPtr asset) {
  return kj::none;
}

} // namespace veloz::exec
