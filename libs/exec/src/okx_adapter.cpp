#include "veloz/exec/okx_adapter.h"

#include "veloz/core/json.h"
#include "veloz/exec/hmac_wrapper.h"

#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <sstream>

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
    : io_context_(io_context), api_key_(kj::heapString(api_key)),
      secret_key_(kj::heapString(secret_key)), passphrase_(kj::heapString(passphrase)),
      connected_(false), demo_(demo), last_activity_time_(kj::systemCoarseMonotonicClock().now()),
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
  // Simple connection state management - does not make network calls
  // Use connect_async() for actual API connection validation
  connected_ = true;
  last_activity_time_ = io_context_.provider->getTimer().now();
}

void OKXAdapter::disconnect() {
  connected_ = false;
  KJ_LOG(INFO, "OKX API disconnected");
}

kj::Promise<void> OKXAdapter::connect_async() {
  if (connected_) {
    return kj::READY_NOW;
  }

  // Test connection by getting server time
  return http_get_async("/api/v5/public/time"_kj).then([this](kj::String response) {
    if (response.size() == 0) {
      KJ_LOG(ERROR, "Failed to connect to OKX API - empty response");
      connected_ = false;
      return;
    }

    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                 using veloz::core::JsonDocument;
                 auto doc = JsonDocument::parse(response);
                 auto root = doc.root();

                 if (root["code"].get_string("") == "0"_kj) {
                   connected_ = true;
                   last_activity_time_ = io_context_.provider->getTimer().now();
                   KJ_LOG(INFO, "OKX API connected successfully");
                 } else {
                   KJ_LOG(ERROR, "OKX API connection failed", root["msg"].get_string(""));
                   connected_ = false;
                 }
               })) {
      KJ_LOG(ERROR, "Error connecting to OKX API", exception.getDescription());
      connected_ = false;
    }
  });
}

void OKXAdapter::set_timeout(kj::Duration timeout) {
  request_timeout_ = timeout;
}

kj::Duration OKXAdapter::get_timeout() const {
  return request_timeout_;
}

kj::String OKXAdapter::get_timestamp_iso() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  // Build ISO 8601 timestamp with zero-padding
  // Format: YYYY-MM-DDTHH:MM:SS.mmmZ
  // Note: Using std::format for width specifiers as kj::str() doesn't support them
  // std::format returns std::string, use .c_str() for kj::str() compatibility
  auto tm = std::gmtime(&time_t_now);
  auto formatted = std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
                               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                               tm->tm_min, tm->tm_sec, static_cast<int>(ms.count()));
  return kj::str(formatted.c_str());
}

kj::String OKXAdapter::build_signature(kj::StringPtr timestamp, kj::StringPtr method,
                                       kj::StringPtr request_path, kj::StringPtr body) {
  // Build prehash string
  kj::String prehash = kj::str(timestamp, method, request_path, body);

  // Use HmacSha256 wrapper for HMAC-SHA256 signature
  auto signature = HmacSha256::sign(secret_key_, prehash);

  // Base64 encode the signature
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  kj::Vector<char> result_chars;
  result_chars.reserve(((signature.size() + 2) / 3) * 4);

  for (size_t i = 0; i < signature.size(); i += 3) {
    unsigned int n = (static_cast<unsigned int>(static_cast<unsigned char>(signature[i])) << 16);
    if (i + 1 < signature.size())
      n |= (static_cast<unsigned int>(static_cast<unsigned char>(signature[i + 1])) << 8);
    if (i + 2 < signature.size())
      n |= static_cast<unsigned int>(static_cast<unsigned char>(signature[i + 2]));

    result_chars.add(base64_chars[(n >> 18) & 0x3F]);
    result_chars.add(base64_chars[(n >> 12) & 0x3F]);
    result_chars.add((i + 1 < signature.size()) ? base64_chars[(n >> 6) & 0x3F] : '=');
    result_chars.add((i + 2 < signature.size()) ? base64_chars[n & 0x3F] : '=');
  }

  return kj::heapString(result_chars.begin(), result_chars.size());
}

kj::String OKXAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
  // OKX uses format like "BTC-USDT" for spot
  kj::StringPtr sym = symbol.value;
  size_t len = sym.size();

  // Convert BTCUSDT to BTC-USDT
  if (len >= 4) {
    kj::StringPtr suffix = sym.slice(len - 4);
    if (suffix == "USDT") {
      return kj::str(sym.slice(0, len - 4), "-USDT");
    }
  }

  return kj::heapString(sym);
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
  // Use KJ-only HmacSha256 interface for signature generation
  auto timestamp = get_timestamp_iso();
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_signature(timestamp, "GET"_kj, request_path);

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    // OKX authentication headers
    headers.addPtrPtr("OK-ACCESS-KEY"_kj, api_key_.asPtr());
    headers.addPtrPtr("OK-ACCESS-SIGN"_kj, signature.asPtr());
    headers.addPtrPtr("OK-ACCESS-TIMESTAMP"_kj, timestamp.asPtr());
    headers.addPtrPtr("OK-ACCESS-PASSPHRASE"_kj, passphrase_.asPtr());

    // Demo mode header
    if (demo_) {
      headers.addPtrPtr("x-simulated-trading"_kj, "1"_kj);
    }

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
  // Use KJ-only HmacSha256 interface for signature generation
  auto timestamp = get_timestamp_iso();
  auto signature = build_signature(timestamp, "POST"_kj, endpoint, body);

  return get_http_client().then([this, endpoint = kj::str(endpoint), body = kj::str(body),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    // OKX authentication headers
    headers.addPtrPtr("OK-ACCESS-KEY"_kj, api_key_.asPtr());
    headers.addPtrPtr("OK-ACCESS-SIGN"_kj, signature.asPtr());
    headers.addPtrPtr("OK-ACCESS-TIMESTAMP"_kj, timestamp.asPtr());
    headers.addPtrPtr("OK-ACCESS-PASSPHRASE"_kj, passphrase_.asPtr());

    // Demo mode header
    if (demo_) {
      headers.addPtrPtr("x-simulated-trading"_kj, "1"_kj);
    }

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
  // Use KJ-only HmacSha256 interface for signature generation
  auto timestamp = get_timestamp_iso();
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_signature(timestamp, "DELETE"_kj, request_path);

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    // OKX authentication headers
    headers.addPtrPtr("OK-ACCESS-KEY"_kj, api_key_.asPtr());
    headers.addPtrPtr("OK-ACCESS-SIGN"_kj, signature.asPtr());
    headers.addPtrPtr("OK-ACCESS-TIMESTAMP"_kj, timestamp.asPtr());
    headers.addPtrPtr("OK-ACCESS-PASSPHRASE"_kj, passphrase_.asPtr());

    // Demo mode header
    if (demo_) {
      headers.addPtrPtr("x-simulated-trading"_kj, "1"_kj);
    }

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
  }
  else {
    body = kj::str("{\"instId\":\"", symbol, "\",\"tdMode\":\"cash\",\"side\":\"", side,
                   "\",\"ordType\":\"", type, "\",\"sz\":\"", req.qty, "\",\"clOrdId\":\"",
                   req.client_order_id, "\"}");
  }

  // Capture necessary fields by value since kj::String is not copyable
  auto symbol_value = kj::str(req.symbol.value);
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v5/trade/order", body)
      .then([symbol_value = kj::mv(symbol_value), client_order_id = kj::mv(client_order_id_copy)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          KJ_LOG(ERROR, "OKX place_order: empty response");
          return kj::none;
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto code = root["code"].get_string("");
                     auto msg = root["msg"].get_string("");

                     // OKX error codes:
                     // 0 = success
                     // 50001 = API key invalid
                     // 50011 = Request too frequent (rate limit)
                     // 51xxx = Trade errors (51000=param error, 51001=instrument not found, etc.)
                     if (code != "0"_kj) {
                       KJ_LOG(ERROR, "OKX place_order error", code, msg);

                       // Check for specific error codes
                       if (code == "50001"_kj) {
                         KJ_LOG(ERROR, "OKX: Invalid API key");
                       } else if (code == "50011"_kj) {
                         KJ_LOG(WARNING, "OKX: Rate limit exceeded");
                       } else if (code.startsWith("51"_kj)) {
                         KJ_LOG(ERROR, "OKX: Trade error", code, msg);
                       }
                       return;
                     }

                     auto data = root["data"];
                     if (data.is_array() && data.size() > 0) {
                       auto order_data = data[0];
                       auto sCode = order_data["sCode"].get_string("");
                       auto sMsg = order_data["sMsg"].get_string("");

                       // Check sub-code for individual order errors
                       if (sCode != "0"_kj) {
                         KJ_LOG(ERROR, "OKX place_order sub-error", sCode, sMsg);
                         return;
                       }

                       ExecutionReport report;
                       report.symbol = veloz::common::SymbolId{symbol_value.cStr()};
                       report.client_order_id = kj::mv(client_order_id);
                       report.venue_order_id = kj::str(order_data["ordId"].get_string(""));
                       report.status = OrderStatus::Accepted;
                       report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

                       result = kj::mv(report);
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX place_order response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
OKXAdapter::cancel_order_async(const CancelOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto body = kj::str("{\"instId\":\"", symbol, "\",\"clOrdId\":\"", req.client_order_id, "\"}");

  auto symbol_value = kj::str(req.symbol.value);
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v5/trade/cancel-order", body)
      .then([symbol_value = kj::mv(symbol_value), client_order_id = kj::mv(client_order_id_copy)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          KJ_LOG(ERROR, "OKX cancel_order: empty response");
          return kj::none;
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto code = root["code"].get_string("");
                     auto msg = root["msg"].get_string("");

                     // OKX error codes:
                     // 0 = success
                     // 50001 = API key invalid
                     // 50011 = Request too frequent (rate limit)
                     // 51xxx = Trade errors
                     if (code != "0"_kj) {
                       KJ_LOG(ERROR, "OKX cancel_order error", code, msg);

                       if (code == "50001"_kj) {
                         KJ_LOG(ERROR, "OKX: Invalid API key");
                       } else if (code == "50011"_kj) {
                         KJ_LOG(WARNING, "OKX: Rate limit exceeded");
                       } else if (code.startsWith("51"_kj)) {
                         KJ_LOG(ERROR, "OKX: Trade error", code, msg);
                       }
                       return;
                     }

                     auto data = root["data"];
                     if (data.is_array() && data.size() > 0) {
                       auto order_data = data[0];
                       auto sCode = order_data["sCode"].get_string("");
                       auto sMsg = order_data["sMsg"].get_string("");

                       // Check sub-code for individual order errors
                       // 51400 = order does not exist
                       // 51401 = order already canceled
                       // 51402 = order already filled
                       if (sCode != "0"_kj) {
                         KJ_LOG(ERROR, "OKX cancel_order sub-error", sCode, sMsg);

                         // Still return a report for "already canceled" or "already filled"
                         if (sCode == "51401"_kj || sCode == "51402"_kj) {
                           ExecutionReport report;
                           report.symbol = veloz::common::SymbolId{symbol_value.cStr()};
                           report.client_order_id = kj::mv(client_order_id);
                           report.status =
                               (sCode == "51401"_kj) ? OrderStatus::Canceled : OrderStatus::Filled;
                           report.ts_recv_ns =
                               std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
                           result = kj::mv(report);
                         }
                         return;
                       }

                       ExecutionReport report;
                       report.symbol = veloz::common::SymbolId{symbol_value.cStr()};
                       report.client_order_id = kj::mv(client_order_id);
                       report.venue_order_id = kj::str(order_data["ordId"].get_string(""));
                       report.status = OrderStatus::Canceled;
                       report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

                       result = kj::mv(report);
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX cancel_order response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Maybe<ExecutionReport> OKXAdapter::place_order(const PlaceOrderRequest& req) {
  // Synchronous version - blocks on async operation
  return place_order_async(req).wait(io_context_.waitScope);
}

kj::Maybe<ExecutionReport> OKXAdapter::cancel_order(const CancelOrderRequest& req) {
  // Synchronous version - blocks on async operation
  return cancel_order_async(req).wait(io_context_.waitScope);
}

kj::Promise<kj::Maybe<double>>
OKXAdapter::get_current_price_async(const veloz::common::SymbolId& symbol) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol);

  return http_get_async("/api/v5/market/ticker", params)
      .then([this](kj::String response) -> kj::Maybe<double> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<double> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // OKX response format: {"code":"0","data":[{"last":"..."}]}
                     if (root["code"].get_string("") != "0"_kj) {
                       KJ_LOG(ERROR, "OKX API error", root["msg"].get_string(""));
                       return;
                     }

                     auto data = root["data"];
                     if (data.is_array() && data.size() > 0) {
                       auto ticker = data[0];
                       auto last_str = ticker["last"].get_string();
                       result = last_str.parseAs<double>();
                       last_activity_time_ = io_context_.provider->getTimer().now();
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX ticker response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
OKXAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol, "&sz=", depth);

  return http_get_async("/api/v5/market/books", params)
      .then([this](kj::String response) -> kj::Maybe<kj::Array<PriceLevel>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<PriceLevel>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     if (root["code"].get_string("") != "0"_kj) {
                       KJ_LOG(ERROR, "OKX API error", root["msg"].get_string(""));
                       return;
                     }

                     kj::Vector<PriceLevel> levels;
                     auto data = root["data"];
                     if (data.is_array() && data.size() > 0) {
                       auto book = data[0];

                       // Parse bids
                       auto bids = book["bids"];
                       if (bids.is_array()) {
                         for (size_t i = 0; i < bids.size(); ++i) {
                           auto bid = bids[i];
                           PriceLevel level;
                           level.price = bid[0].get_string().parseAs<double>();
                           level.quantity = bid[1].get_string().parseAs<double>();
                           levels.add(kj::mv(level));
                         }
                       }

                       // Parse asks
                       auto asks = book["asks"];
                       if (asks.is_array()) {
                         for (size_t i = 0; i < asks.size(); ++i) {
                           auto ask = asks[i];
                           PriceLevel level;
                           level.price = ask[0].get_string().parseAs<double>();
                           level.quantity = ask[1].get_string().parseAs<double>();
                           levels.add(kj::mv(level));
                         }
                       }

                       last_activity_time_ = io_context_.provider->getTimer().now();
                       result = levels.releaseAsArray();
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX order book response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<TradeData>>>
OKXAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  auto formatted_symbol = format_symbol(symbol);
  auto params = kj::str("instId=", formatted_symbol, "&limit=", limit);

  return http_get_async("/api/v5/market/trades", params)
      .then([this](kj::String response) -> kj::Maybe<kj::Array<TradeData>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<TradeData>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     if (root["code"].get_string("") != "0"_kj) {
                       KJ_LOG(ERROR, "OKX API error", root["msg"].get_string(""));
                       return;
                     }

                     kj::Vector<TradeData> trades;
                     auto data = root["data"];
                     if (data.is_array()) {
                       for (size_t i = 0; i < data.size(); ++i) {
                         auto trade = data[i];
                         TradeData td;
                         td.price = trade["px"].get_string().parseAs<double>();
                         td.quantity = trade["sz"].get_string().parseAs<double>();
                         trades.add(kj::mv(td));
                       }
                       last_activity_time_ = io_context_.provider->getTimer().now();
                       result = trades.releaseAsArray();
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX trades response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<double>> OKXAdapter::get_account_balance_async(kj::StringPtr asset) {
  auto params = kj::str("ccy=", asset);

  return http_get_async("/api/v5/account/balance", params)
      .then([this, asset = kj::str(asset)](kj::String response) -> kj::Maybe<double> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<double> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     using veloz::core::JsonDocument;
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     if (root["code"].get_string("") != "0"_kj) {
                       KJ_LOG(ERROR, "OKX API error", root["msg"].get_string(""));
                       return;
                     }

                     auto data = root["data"];
                     if (data.is_array() && data.size() > 0) {
                       auto account = data[0];
                       auto details = account["details"];
                       if (details.is_array()) {
                         for (size_t i = 0; i < details.size(); ++i) {
                           auto detail = details[i];
                           if (detail["ccy"].get_string("") == asset) {
                             result = detail["availBal"].get_string().parseAs<double>();
                             last_activity_time_ = io_context_.provider->getTimer().now();
                             return;
                           }
                         }
                       }
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing OKX balance response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Maybe<double> OKXAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  return get_current_price_async(symbol).wait(io_context_.waitScope);
}

kj::Maybe<kj::Array<PriceLevel>> OKXAdapter::get_order_book(const veloz::common::SymbolId& symbol,
                                                            int depth) {
  return get_order_book_async(symbol, depth).wait(io_context_.waitScope);
}

kj::Maybe<kj::Array<TradeData>> OKXAdapter::get_recent_trades(const veloz::common::SymbolId& symbol,
                                                              int limit) {
  return get_recent_trades_async(symbol, limit).wait(io_context_.waitScope);
}

kj::Maybe<double> OKXAdapter::get_account_balance(kj::StringPtr asset) {
  return get_account_balance_async(asset).wait(io_context_.waitScope);
}

} // namespace veloz::exec
