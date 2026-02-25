#include "veloz/exec/binance_adapter.h"

#include "veloz/core/json.h"
#include "veloz/exec/hmac_wrapper.h"

#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::exec {

using namespace veloz::core;

BinanceAdapter::BinanceAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key,
                               kj::StringPtr secret_key, bool testnet)
    : io_context_(io_context), api_key_(kj::heapString(api_key)),
      secret_key_(kj::heapString(secret_key)), connected_(false), testnet_(testnet),
      last_activity_time_(io_context.provider->getTimer().now()),
      request_timeout_(10 * kj::SECONDS), rate_limit_window_(1 * kj::SECONDS),
      rate_limit_per_window_(1200), max_retries_(3), retry_delay_(1 * kj::SECONDS) {

  // Set up API endpoints based on testnet flag
  if (testnet_) {
    base_rest_url_ = kj::str("testnet.binance.vision");
    base_ws_url_ = kj::str("wss://testnet.binance.vision");
  } else {
    base_rest_url_ = kj::str("api.binance.com");
    base_ws_url_ = kj::str("wss://stream.binance.com:9443");
  }

  // Build HTTP header table
  kj::HttpHeaderTable::Builder builder;
  header_table_ = builder.build();

  // Initialize TLS context for HTTPS
  kj::TlsContext::Options tls_options;
  tls_options.useSystemTrustStore = true;
  tls_context_ = kj::heap<kj::TlsContext>(kj::mv(tls_options));
}

BinanceAdapter::~BinanceAdapter() noexcept {
  disconnect();
}

kj::String BinanceAdapter::build_signature(kj::StringPtr query_string) {
  // Use HmacSha256 wrapper with KJ-only interface for HMAC-SHA256 signature
  // This is required for Binance API authentication - KJ does not provide HMAC functionality
  // The wrapper handles OpenSSL C API calls safely internally
  return HmacSha256::sign(secret_key_, query_string);
}

int64_t BinanceAdapter::get_timestamp_ms() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

kj::Promise<kj::Own<kj::HttpClient>> BinanceAdapter::get_http_client() {
  auto& network = io_context_.provider->getNetwork();
  auto& timer = io_context_.provider->getTimer();

  // Create TLS-wrapped network for HTTPS
  auto tls_network = tls_context_->wrapNetwork(network);

  // Parse address with port 443 for HTTPS
  return network.parseAddress(base_rest_url_, 443)
      .then([this, &timer, tls_network = kj::mv(tls_network)](
                kj::Own<kj::NetworkAddress> addr) mutable -> kj::Own<kj::HttpClient> {
        // Wrap address with TLS
        auto tls_addr = tls_context_->wrapAddress(kj::mv(addr), base_rest_url_);

        kj::HttpClientSettings settings;
        settings.idleTimeout = 30 * kj::SECONDS;

        return kj::newHttpClient(timer, *header_table_, *tls_addr, kj::mv(settings));
      });
}

kj::Promise<kj::String> BinanceAdapter::http_get_async(kj::StringPtr endpoint,
                                                       kj::StringPtr params) {
  return get_http_client().then(
      [this, endpoint = kj::str(endpoint),
       params = params == nullptr ? kj::String() : kj::str(params)](
          kj::Own<kj::HttpClient> client) mutable -> kj::Promise<kj::String> {
        // Build URL
        kj::String url;
        if (params.size() > 0) {
          url = kj::str("https://", base_rest_url_, endpoint, "?", params);
        } else {
          url = kj::str("https://", base_rest_url_, endpoint);
        }

        // Build headers
        kj::HttpHeaders headers(*header_table_);
        headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
        if (api_key_.size() > 0) {
          headers.addPtrPtr("X-MBX-APIKEY"_kj, api_key_.asPtr());
        }

        // Make request
        auto request = client->request(kj::HttpMethod::GET, url, headers);

        // Read response with timeout
        auto& timer = io_context_.provider->getTimer();
        return timer.timeoutAfter(request_timeout_, kj::mv(request.response))
            .then([](kj::HttpClient::Response response) -> kj::Promise<kj::String> {
              if (response.statusCode >= 200 && response.statusCode < 300) {
                return response.body->readAllText();
              } else {
                return response.body->readAllText().then(
                    [statusCode = response.statusCode](kj::String body) -> kj::String {
                      KJ_LOG(ERROR, "HTTP GET failed", statusCode, body);
                      return kj::str("");
                    });
              }
            });
      });
}

kj::Promise<kj::String> BinanceAdapter::http_post_async(kj::StringPtr endpoint,
                                                        kj::StringPtr params) {
  return get_http_client().then(
      [this, endpoint = kj::str(endpoint), params = kj::str(params)](
          kj::Own<kj::HttpClient> client) mutable -> kj::Promise<kj::String> {
        // Build URL
        auto url = kj::str("https://", base_rest_url_, endpoint);

        // Build headers
        kj::HttpHeaders headers(*header_table_);
        headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
        headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/x-www-form-urlencoded"_kj);
        if (api_key_.size() > 0) {
          headers.addPtrPtr("X-MBX-APIKEY"_kj, api_key_.asPtr());
        }

        // Make request with body
        auto request = client->request(kj::HttpMethod::POST, url, headers, params.size());

        // Write body
        auto writePromise = request.body->write(params.asBytes());

        // Read response with timeout
        auto& timer = io_context_.provider->getTimer();
        return writePromise.then([&timer, request = kj::mv(request),
                                  timeout = request_timeout_]() mutable -> kj::Promise<kj::String> {
          return timer.timeoutAfter(timeout, kj::mv(request.response))
              .then([](kj::HttpClient::Response response) -> kj::Promise<kj::String> {
                if (response.statusCode >= 200 && response.statusCode < 300) {
                  return response.body->readAllText();
                } else {
                  return response.body->readAllText().then(
                      [statusCode = response.statusCode](kj::String body) -> kj::String {
                        KJ_LOG(ERROR, "HTTP POST failed", statusCode, body);
                        return kj::str("");
                      });
                }
              });
        });
      });
}

kj::Promise<kj::String> BinanceAdapter::http_delete_async(kj::StringPtr endpoint,
                                                          kj::StringPtr params) {
  return get_http_client().then(
      [this, endpoint = kj::str(endpoint), params = kj::str(params)](
          kj::Own<kj::HttpClient> client) mutable -> kj::Promise<kj::String> {
        // Build URL with params
        kj::String url;
        if (params.size() > 0) {
          url = kj::str("https://", base_rest_url_, endpoint, "?", params);
        } else {
          url = kj::str("https://", base_rest_url_, endpoint);
        }

        // Build headers
        kj::HttpHeaders headers(*header_table_);
        headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
        if (api_key_.size() > 0) {
          headers.addPtrPtr("X-MBX-APIKEY"_kj, api_key_.asPtr());
        }

        // Make DELETE request
        auto request = client->request(kj::HttpMethod::DELETE, url, headers);

        // Read response with timeout
        auto& timer = io_context_.provider->getTimer();
        return timer.timeoutAfter(request_timeout_, kj::mv(request.response))
            .then([](kj::HttpClient::Response response) -> kj::Promise<kj::String> {
              if (response.statusCode >= 200 && response.statusCode < 300) {
                return response.body->readAllText();
              } else {
                return response.body->readAllText().then(
                    [statusCode = response.statusCode](kj::String body) -> kj::String {
                      KJ_LOG(ERROR, "HTTP DELETE failed", statusCode, body);
                      return kj::str("");
                    });
              }
            });
      });
}

kj::String BinanceAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
  // Convert to uppercase
  kj::Vector<char> formatted_chars;
  for (char c : symbol.value) {
    formatted_chars.add(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  formatted_chars.add('\0');
  return kj::heapString(formatted_chars.begin());
}

kj::StringPtr BinanceAdapter::order_side_to_string(OrderSide side) {
  return side == OrderSide::Buy ? "BUY"_kj : "SELL"_kj;
}

kj::StringPtr BinanceAdapter::order_type_to_string(OrderType type) {
  switch (type) {
  case OrderType::Market:
    return "MARKET"_kj;
  case OrderType::Limit:
    return "LIMIT"_kj;
  case OrderType::StopLoss:
    return "STOP_LOSS"_kj;
  case OrderType::StopLossLimit:
    return "STOP_LOSS_LIMIT"_kj;
  case OrderType::TakeProfit:
    return "TAKE_PROFIT"_kj;
  case OrderType::TakeProfitLimit:
    return "TAKE_PROFIT_LIMIT"_kj;
  default:
    return "LIMIT"_kj;
  }
}

kj::StringPtr BinanceAdapter::time_in_force_to_string(TimeInForce tif) {
  switch (tif) {
  case TimeInForce::GTC:
    return "GTC"_kj;
  case TimeInForce::IOC:
    return "IOC"_kj;
  case TimeInForce::FOK:
    return "FOK"_kj;
  case TimeInForce::GTX:
    return "GTX"_kj;
  default:
    return "GTC"_kj;
  }
}

kj::String BinanceAdapter::build_order_params(const PlaceOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto side = order_side_to_string(req.side);
  auto type = order_type_to_string(req.type);
  auto tif = time_in_force_to_string(req.tif);
  auto timestamp = get_timestamp_ms();

  kj::String params;
  KJ_IF_SOME(priceVal, req.price) {
    if (req.client_order_id.size() > 0) {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&price=", priceVal,
                       "&newClientOrderId=", req.client_order_id, "&timestamp=", timestamp);
    } else {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&price=", priceVal, "&timestamp=", timestamp);
    }
  }
  else {
    if (req.client_order_id.size() > 0) {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&newClientOrderId=", req.client_order_id,
                       "&timestamp=", timestamp);
    } else {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&timestamp=", timestamp);
    }
  }

  KJ_IF_SOME(stopPrice, req.stop_price) {
    params = kj::str(params, "&stopPrice=", stopPrice);
  }

  if (req.reduce_only) {
    params = kj::str(params, "&reduceOnly=true");
  }
  if (req.post_only) {
    params = kj::str(params, "&postOnly=true");
  }
  KJ_IF_SOME(positionSide, req.position_side) {
    params = kj::str(params, "&positionSide=", positionSide);
  }

  return params;
}

OrderStatus BinanceAdapter::parse_order_status(kj::StringPtr status_str) {
  if (status_str == "NEW") {
    return OrderStatus::New;
  } else if (status_str == "PARTIALLY_FILLED") {
    return OrderStatus::PartiallyFilled;
  } else if (status_str == "FILLED") {
    return OrderStatus::Filled;
  } else if (status_str == "CANCELED" || status_str == "PENDING_CANCEL") {
    return OrderStatus::Canceled;
  } else if (status_str == "REJECTED") {
    return OrderStatus::Rejected;
  } else if (status_str == "EXPIRED") {
    return OrderStatus::Expired;
  }
  return OrderStatus::New;
}

namespace {
// Binance error code handling helper
// Error codes: https://binance-docs.github.io/apidocs/spot/en/#error-codes
void log_binance_error(int code, kj::StringPtr msg) {
  // General server errors (-1xxx)
  if (code == -1000) {
    KJ_LOG(ERROR, "Binance: Unknown error", msg);
  } else if (code == -1001) {
    KJ_LOG(ERROR, "Binance: Disconnected - internal error", msg);
  } else if (code == -1002) {
    KJ_LOG(ERROR, "Binance: Unauthorized - API key issue", msg);
  } else if (code == -1003) {
    KJ_LOG(WARNING, "Binance: Rate limit exceeded", msg);
  } else if (code == -1006) {
    KJ_LOG(ERROR, "Binance: Unexpected response", msg);
  } else if (code == -1007) {
    KJ_LOG(ERROR, "Binance: Timeout", msg);
  } else if (code == -1014) {
    KJ_LOG(ERROR, "Binance: Unknown order composition", msg);
  } else if (code == -1015) {
    KJ_LOG(WARNING, "Binance: Too many orders - rate limit", msg);
  } else if (code == -1016) {
    KJ_LOG(ERROR, "Binance: Service shutting down", msg);
  } else if (code == -1020) {
    KJ_LOG(ERROR, "Binance: Unsupported operation", msg);
  } else if (code == -1021) {
    KJ_LOG(ERROR, "Binance: Timestamp outside recvWindow", msg);
  } else if (code == -1022) {
    KJ_LOG(ERROR, "Binance: Invalid signature", msg);
  }
  // Request issues (-11xx)
  else if (code == -1100) {
    KJ_LOG(ERROR, "Binance: Illegal characters in parameter", msg);
  } else if (code == -1101) {
    KJ_LOG(ERROR, "Binance: Too many parameters", msg);
  } else if (code == -1102) {
    KJ_LOG(ERROR, "Binance: Mandatory parameter missing", msg);
  } else if (code == -1103) {
    KJ_LOG(ERROR, "Binance: Unknown parameter", msg);
  } else if (code == -1104) {
    KJ_LOG(ERROR, "Binance: Unread parameters", msg);
  } else if (code == -1105) {
    KJ_LOG(ERROR, "Binance: Parameter empty", msg);
  } else if (code == -1106) {
    KJ_LOG(ERROR, "Binance: Parameter not required", msg);
  } else if (code == -1111) {
    KJ_LOG(ERROR, "Binance: Bad precision", msg);
  } else if (code == -1112) {
    KJ_LOG(ERROR, "Binance: No depth", msg);
  } else if (code == -1114) {
    KJ_LOG(ERROR, "Binance: TIF not required", msg);
  } else if (code == -1115) {
    KJ_LOG(ERROR, "Binance: Invalid TIF", msg);
  } else if (code == -1116) {
    KJ_LOG(ERROR, "Binance: Invalid order type", msg);
  } else if (code == -1117) {
    KJ_LOG(ERROR, "Binance: Invalid side", msg);
  } else if (code == -1118) {
    KJ_LOG(ERROR, "Binance: Empty new client order ID", msg);
  } else if (code == -1119) {
    KJ_LOG(ERROR, "Binance: Empty original client order ID", msg);
  } else if (code == -1120) {
    KJ_LOG(ERROR, "Binance: Bad interval", msg);
  } else if (code == -1121) {
    KJ_LOG(ERROR, "Binance: Bad symbol", msg);
  } else if (code == -1125) {
    KJ_LOG(ERROR, "Binance: Invalid listen key", msg);
  } else if (code == -1127) {
    KJ_LOG(ERROR, "Binance: More than allowed lookback", msg);
  } else if (code == -1128) {
    KJ_LOG(ERROR, "Binance: Invalid combination of optional parameters", msg);
  } else if (code == -1130) {
    KJ_LOG(ERROR, "Binance: Invalid data for parameter", msg);
  }
  // Order issues (-2xxx)
  else if (code == -2010) {
    KJ_LOG(ERROR, "Binance: New order rejected", msg);
  } else if (code == -2011) {
    KJ_LOG(ERROR, "Binance: Cancel rejected", msg);
  } else if (code == -2013) {
    KJ_LOG(ERROR, "Binance: Order does not exist", msg);
  } else if (code == -2014) {
    KJ_LOG(ERROR, "Binance: API key format invalid", msg);
  } else if (code == -2015) {
    KJ_LOG(ERROR, "Binance: Invalid API key, IP, or permissions", msg);
  } else if (code == -2016) {
    KJ_LOG(ERROR, "Binance: No trading window", msg);
  } else {
    KJ_LOG(ERROR, "Binance: Unknown error code", code, msg);
  }
}
} // namespace

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceAdapter::place_order_async(const PlaceOrderRequest& req) {
  if (!is_connected()) {
    // Copy the needed fields since PlaceOrderRequest is not copyable
    auto symbol = req.symbol;
    auto side = req.side;
    auto type = req.type;
    auto tif = req.tif;
    auto qty = req.qty;
    auto price = req.price;
    auto stop_price = req.stop_price;
    auto client_order_id = kj::heapString(req.client_order_id);
    auto reduce_only = req.reduce_only;
    auto post_only = req.post_only;
    kj::Maybe<kj::String> position_side;
    KJ_IF_SOME(value, req.position_side) {
      position_side = kj::heapString(value);
    }

    return connect_async().then([this, symbol, side, type, tif, qty, price, stop_price,
                                 client_order_id = kj::mv(client_order_id), reduce_only,
                                 post_only, position_side = kj::mv(position_side)]() mutable
                                 -> kj::Promise<kj::Maybe<ExecutionReport>> {
      if (!is_connected()) {
        return kj::Maybe<ExecutionReport>(kj::none);
      }
      PlaceOrderRequest new_req;
      new_req.symbol = symbol;
      new_req.side = side;
      new_req.type = type;
      new_req.tif = tif;
      new_req.qty = qty;
      new_req.price = price;
      new_req.stop_price = stop_price;
      new_req.client_order_id = kj::mv(client_order_id);
      new_req.reduce_only = reduce_only;
      new_req.post_only = post_only;
      KJ_IF_SOME(value, position_side) {
        new_req.position_side = kj::heapString(value);
      }
      return place_order_async(new_req);
    });
  }

  kj::StringPtr endpoint = "/api/v3/order"_kj;

  auto params = build_order_params(req);

  // Add signature using KJ-only HmacSha256 interface
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  // Copy needed fields for the lambda
  auto req_symbol = req.symbol;
  auto req_client_order_id = kj::heapString(req.client_order_id);

  return http_post_async(endpoint, params)
      .then([this, req_symbol, req_client_order_id = kj::mv(req_client_order_id)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          return {};
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       log_binance_error(code, msg);
                       return;
                     }

                     ExecutionReport report;
                     report.symbol = req_symbol;
                     report.client_order_id = kj::heapString(req_client_order_id);
                     report.venue_order_id = root["orderId"].get_string(""_kj);
                     report.status = parse_order_status(root["status"].get_string("NEW"_kj));
                     report.last_fill_qty = root["executedQty"].get_double(0.0);
                     report.last_fill_price = root["price"].get_double(0.0);
                     report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
                     report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = kj::mv(report);
                   })) {
          KJ_LOG(ERROR, "Error parsing place order response", exception.getDescription());
          return {};
        }
        return result;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceAdapter::cancel_order_async(const CancelOrderRequest& req) {
  if (!is_connected()) {
    // Copy the needed fields since CancelOrderRequest is not copyable
    auto symbol = req.symbol;
    auto client_order_id = kj::heapString(req.client_order_id);

    return connect_async().then([this, symbol, client_order_id = kj::mv(client_order_id)]() mutable
                                    -> kj::Promise<kj::Maybe<ExecutionReport>> {
      if (!is_connected()) {
        return kj::Maybe<ExecutionReport>(kj::none);
      }
      CancelOrderRequest new_req;
      new_req.symbol = symbol;
      new_req.client_order_id = kj::mv(client_order_id);
      return cancel_order_async(new_req);
    });
  }

  kj::StringPtr endpoint = "/api/v3/order"_kj;

  // Build query string
  auto symbol = format_symbol(req.symbol);
  auto timestamp = get_timestamp_ms();

  auto params = kj::str("symbol=", symbol, "&origClientOrderId=", req.client_order_id,
                        "&timestamp=", timestamp);

  // Add signature using KJ-only HmacSha256 interface
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  // Copy needed fields for the lambda
  auto req_symbol = req.symbol;
  auto req_client_order_id = kj::heapString(req.client_order_id);

  return http_delete_async(endpoint, params)
      .then([this, req_symbol, req_client_order_id = kj::mv(req_client_order_id)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          return {};
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       log_binance_error(code, msg);
                       return;
                     }

                     ExecutionReport report;
                     report.symbol = req_symbol;
                     report.client_order_id = kj::heapString(req_client_order_id);
                     report.venue_order_id = root["orderId"].get_string(""_kj);
                     report.status = parse_order_status(root["status"].get_string("CANCELED"_kj));
                     report.last_fill_qty = root["executedQty"].get_double(0.0);
                     report.last_fill_price = root["price"].get_double(0.0);
                     report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
                     report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = kj::mv(report);
                   })) {
          KJ_LOG(ERROR, "Error parsing cancel order response", exception.getDescription());
          return {};
        }
        return result;
      });
}

// Synchronous wrappers for ExchangeAdapter interface
kj::Maybe<ExecutionReport> BinanceAdapter::place_order(const PlaceOrderRequest& req) {
  // Note: This blocks the event loop - prefer using place_order_async in async code
  return place_order_async(req).wait(io_context_.waitScope);
}

kj::Maybe<ExecutionReport> BinanceAdapter::cancel_order(const CancelOrderRequest& req) {
  // Note: This blocks the event loop - prefer using cancel_order_async in async code
  return cancel_order_async(req).wait(io_context_.waitScope);
}

bool BinanceAdapter::is_connected() const {
  if (!connected_) {
    return false;
  }

  // Check if last activity is within reasonable time
  auto now = io_context_.provider->getTimer().now();
  auto idle_time = now - last_activity_time_;

  return idle_time < 30 * kj::SECONDS;
}

kj::Promise<void> BinanceAdapter::connect_async() {
  if (connected_) {
    return kj::READY_NOW;
  }

  // Test connection by getting server time
  return http_get_async("/api/v3/time"_kj).then([this](kj::String response) {
    if (response.size() == 0) {
      KJ_LOG(ERROR, "Failed to connect to Binance API - empty response");
      connected_ = false;
      return;
    }

    KJ_IF_SOME(result, kj::runCatchingExceptions([&]() {
                 auto doc = JsonDocument::parse(response);
                 auto root = doc.root();
                 if (root["serverTime"].is_valid()) {
                   connected_ = true;
                   last_activity_time_ = io_context_.provider->getTimer().now();
                   KJ_LOG(INFO, "Binance API connected successfully");
                 }
                 return true;
               })) {
      // Connection successful
      (void)result;
    }
    else {
      // Exception occurred - KJ_LOG already captures exception details
      KJ_LOG(ERROR, "Error connecting to Binance API");
      connected_ = false;
    }
  });
}

void BinanceAdapter::connect() {
  // Synchronous version - blocks on async
  connect_async().wait(io_context_.waitScope);
}

void BinanceAdapter::disconnect() {
  connected_ = false;
  KJ_LOG(INFO, "Binance API disconnected");
}

kj::StringPtr BinanceAdapter::name() const {
  return "Binance"_kj;
}

kj::StringPtr BinanceAdapter::version() const {
  return "2.0.0-kj-async"_kj;
}

void BinanceAdapter::set_timeout(kj::Duration timeout) {
  request_timeout_ = timeout;
}

kj::Duration BinanceAdapter::get_timeout() const {
  return request_timeout_;
}

kj::Promise<kj::Maybe<double>>
BinanceAdapter::get_current_price_async(const veloz::common::SymbolId& symbol) {
  kj::StringPtr endpoint = "/api/v3/ticker/price"_kj;
  auto params = kj::str("symbol=", format_symbol(symbol));

  return http_get_async(endpoint, params).then([this](kj::String response) -> kj::Maybe<double> {
    if (response.size() == 0) {
      return {};
    }

    kj::Maybe<double> result;
    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                 auto doc = JsonDocument::parse(response);
                 auto root = doc.root();
                 auto price_val = root["price"];
                 if (price_val.is_string()) {
                   last_activity_time_ = io_context_.provider->getTimer().now();
                   auto s = price_val.get_string();
                   result = s.parseAs<double>();
                 } else if (price_val.is_real()) {
                   last_activity_time_ = io_context_.provider->getTimer().now();
                   result = price_val.get_double();
                 }
               })) {
      KJ_LOG(ERROR, "Error getting current price", exception.getDescription());
      return kj::none;
    }
    return result;
  });
}

kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
BinanceAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  kj::StringPtr endpoint = "/api/v3/depth"_kj;
  auto params = kj::str("symbol=", format_symbol(symbol), "&limit=", depth);

  return http_get_async(endpoint, params)
      .then([this](kj::String response) -> kj::Maybe<kj::Array<PriceLevel>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<PriceLevel>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     kj::Vector<PriceLevel> order_book;

                     auto bids = root["bids"];
                     if (bids.is_array()) {
                       for (size_t i = 0; i < bids.size(); ++i) {
                         auto bid = bids[i];
                         PriceLevel level;
                         auto priceStr = bid[0].get_string();
                         auto qtyStr = bid[1].get_string();
                         level.price = priceStr.parseAs<double>();
                         level.quantity = qtyStr.parseAs<double>();
                         order_book.add(kj::mv(level));
                       }
                     }

                     auto asks = root["asks"];
                     if (asks.is_array()) {
                       for (size_t i = 0; i < asks.size(); ++i) {
                         auto ask = asks[i];
                         PriceLevel level;
                         auto priceStr = ask[0].get_string();
                         auto qtyStr = ask[1].get_string();
                         level.price = priceStr.parseAs<double>();
                         level.quantity = qtyStr.parseAs<double>();
                         order_book.add(kj::mv(level));
                       }
                     }

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = order_book.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error getting order book", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<TradeData>>>
BinanceAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  kj::StringPtr endpoint = "/api/v3/trades"_kj;
  auto params = kj::str("symbol=", format_symbol(symbol), "&limit=", limit);

  return http_get_async(endpoint, params)
      .then([this](kj::String response) -> kj::Maybe<kj::Array<TradeData>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<TradeData>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     kj::Vector<TradeData> trades;

                     for (size_t i = 0; i < root.size(); ++i) {
                       auto trade = root[i];
                       TradeData data;
                       auto priceStr = trade["price"].get_string();
                       auto qtyStr = trade["qty"].get_string();
                       data.price = priceStr.parseAs<double>();
                       data.quantity = qtyStr.parseAs<double>();
                       trades.add(kj::mv(data));
                     }

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = trades.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error getting recent trades", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<double>> BinanceAdapter::get_account_balance_async(kj::StringPtr asset) {
  kj::StringPtr endpoint = "/api/v3/account"_kj;

  auto timestamp = get_timestamp_ms();
  auto params = kj::str("timestamp=", timestamp);

  // Add signature using KJ-only HmacSha256 interface
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  return http_get_async(endpoint, params)
      .then([this, asset = kj::str(asset)](kj::String response) -> kj::Maybe<double> {
        if (response.size() == 0) {
          return {};
        }

        kj::Maybe<double> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto balances = root["balances"];
                     if (balances.is_array()) {
                       for (size_t i = 0; i < balances.size(); ++i) {
                         auto balance = balances[i];
                         auto assetName = balance["asset"].get_string(""_kj);
                         if (kj::StringPtr(assetName) == asset) {
                           last_activity_time_ = io_context_.provider->getTimer().now();
                           auto freeStr = balance["free"].get_string();
                           result = freeStr.parseAs<double>();
                           return;
                         }
                       }
                     }
                   })) {
          KJ_LOG(ERROR, "Error getting account balance", exception.getDescription());
          return {};
        }
        return result;
      });
}

// Synchronous wrappers for backward compatibility
kj::Maybe<double> BinanceAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  auto result = get_current_price_async(symbol).wait(io_context_.waitScope);
  KJ_IF_SOME(price, result) {
    return price;
  }
  return kj::none;
}

kj::Maybe<kj::Array<PriceLevel>>
BinanceAdapter::get_order_book(const veloz::common::SymbolId& symbol, int depth) {
  auto result = get_order_book_async(symbol, depth).wait(io_context_.waitScope);
  KJ_IF_SOME(book, result) {
    return kj::mv(book);
  }
  return kj::none;
}

kj::Maybe<kj::Array<TradeData>>
BinanceAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
  auto result = get_recent_trades_async(symbol, limit).wait(io_context_.waitScope);
  KJ_IF_SOME(trades, result) {
    return kj::mv(trades);
  }
  return kj::none;
}

kj::Maybe<double> BinanceAdapter::get_account_balance(kj::StringPtr asset) {
  auto result = get_account_balance_async(asset).wait(io_context_.waitScope);
  KJ_IF_SOME(balance, result) {
    return balance;
  }
  return kj::none;
}

// ============================================================================
// Production Trading Methods (Task #11)
// ============================================================================

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceAdapter::get_order_async(const veloz::common::SymbolId& symbol,
                                kj::StringPtr client_order_id) {
  kj::StringPtr endpoint = "/api/v3/order"_kj;

  auto formatted_symbol = format_symbol(symbol);
  auto timestamp = get_timestamp_ms();

  auto params = kj::str("symbol=", formatted_symbol, "&origClientOrderId=", client_order_id,
                        "&timestamp=", timestamp);

  // Add signature
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  // Copy needed fields for the lambda
  auto req_symbol = symbol;
  auto req_client_order_id = kj::heapString(client_order_id);

  return http_get_async(endpoint, params)
      .then([this, req_symbol, req_client_order_id = kj::mv(req_client_order_id)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       KJ_LOG(ERROR, "Binance get_order error", code, msg);
                       return;
                     }

                     ExecutionReport report;
                     report.symbol = req_symbol;
                     report.client_order_id = kj::heapString(req_client_order_id);
                     report.venue_order_id = root["orderId"].get_string(""_kj);
                     report.status = parse_order_status(root["status"].get_string("NEW"_kj));
                     report.last_fill_qty = root["executedQty"].get_double(0.0);
                     report.last_fill_price = root["price"].get_double(0.0);
                     report.ts_exchange_ns = root["updateTime"].get_int(0LL) * 1000000;
                     report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = kj::mv(report);
                   })) {
          KJ_LOG(ERROR, "Error parsing get_order response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<ExecutionReport>>>
BinanceAdapter::get_open_orders_async(const veloz::common::SymbolId& symbol) {
  kj::StringPtr endpoint = "/api/v3/openOrders"_kj;

  auto formatted_symbol = format_symbol(symbol);
  auto timestamp = get_timestamp_ms();

  auto params = kj::str("symbol=", formatted_symbol, "&timestamp=", timestamp);

  // Add signature
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  auto req_symbol = symbol;

  return http_get_async(endpoint, params)
      .then([this,
             req_symbol](kj::String response) mutable -> kj::Maybe<kj::Array<ExecutionReport>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<ExecutionReport>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       KJ_LOG(ERROR, "Binance get_open_orders error", code, msg);
                       return;
                     }

                     kj::Vector<ExecutionReport> orders;
                     for (size_t i = 0; i < root.size(); ++i) {
                       auto order = root[i];

                       ExecutionReport report;
                       report.symbol = req_symbol;
                       report.client_order_id =
                           kj::heapString(order["clientOrderId"].get_string(""_kj));
                       report.venue_order_id = order["orderId"].get_string(""_kj);
                       report.status = parse_order_status(order["status"].get_string("NEW"_kj));
                       report.last_fill_qty = order["executedQty"].get_double(0.0);
                       report.last_fill_price = order["price"].get_double(0.0);
                       report.ts_exchange_ns = order["updateTime"].get_int(0LL) * 1000000;
                       report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

                       orders.add(kj::mv(report));
                     }

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = orders.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error parsing get_open_orders response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<ExecutionReport>>> BinanceAdapter::get_all_open_orders_async() {
  kj::StringPtr endpoint = "/api/v3/openOrders"_kj;

  auto timestamp = get_timestamp_ms();
  auto params = kj::str("timestamp=", timestamp);

  // Add signature
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  return http_get_async(endpoint, params)
      .then([this](kj::String response) -> kj::Maybe<kj::Array<ExecutionReport>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<ExecutionReport>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       KJ_LOG(ERROR, "Binance get_all_open_orders error", code, msg);
                       return;
                     }

                     kj::Vector<ExecutionReport> orders;
                     for (size_t i = 0; i < root.size(); ++i) {
                       auto order = root[i];

                       ExecutionReport report;
                       // Parse symbol from response
                       auto symbol_str = order["symbol"].get_string(""_kj);
                       report.symbol = veloz::common::SymbolId{symbol_str.cStr()};
                       report.client_order_id =
                           kj::heapString(order["clientOrderId"].get_string(""_kj));
                       report.venue_order_id = order["orderId"].get_string(""_kj);
                       report.status = parse_order_status(order["status"].get_string("NEW"_kj));
                       report.last_fill_qty = order["executedQty"].get_double(0.0);
                       report.last_fill_price = order["price"].get_double(0.0);
                       report.ts_exchange_ns = order["updateTime"].get_int(0LL) * 1000000;
                       report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

                       orders.add(kj::mv(report));
                     }

                     last_activity_time_ = io_context_.provider->getTimer().now();
                     result = orders.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error parsing get_all_open_orders response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

// Synchronous order query methods
kj::Maybe<ExecutionReport> BinanceAdapter::get_order(const veloz::common::SymbolId& symbol,
                                                     kj::StringPtr client_order_id) {
  return get_order_async(symbol, client_order_id).wait(io_context_.waitScope);
}

kj::Maybe<kj::Array<ExecutionReport>>
BinanceAdapter::get_open_orders(const veloz::common::SymbolId& symbol) {
  return get_open_orders_async(symbol).wait(io_context_.waitScope);
}

kj::Maybe<kj::Array<ExecutionReport>> BinanceAdapter::get_all_open_orders() {
  return get_all_open_orders_async().wait(io_context_.waitScope);
}

// ============================================================================
// ReconciliationQueryInterface Implementation (Task #2)
// ============================================================================

kj::Promise<kj::Vector<ExecutionReport>>
BinanceAdapter::query_open_orders_async(const veloz::common::SymbolId& symbol) {
  // Reuse existing get_open_orders_async and convert Array to Vector
  return get_open_orders_async(symbol).then(
      [](kj::Maybe<kj::Array<ExecutionReport>> result) -> kj::Vector<ExecutionReport> {
        KJ_IF_SOME(orders, result) {
          kj::Vector<ExecutionReport> vec;
          for (auto& order : orders) {
            vec.add(kj::mv(order));
          }
          return vec;
        }
        // Return empty vector if no orders
        return kj::Vector<ExecutionReport>();
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceAdapter::query_order_async(const veloz::common::SymbolId& symbol,
                                  kj::StringPtr client_order_id) {
  // Reuse existing get_order_async - signature already matches
  return get_order_async(symbol, client_order_id);
}

kj::Promise<kj::Vector<ExecutionReport>>
BinanceAdapter::query_orders_async(const veloz::common::SymbolId& symbol,
                                   std::int64_t start_time_ms, std::int64_t end_time_ms) {
  // Query all orders within time range using Binance allOrders endpoint
  kj::StringPtr endpoint = "/api/v3/allOrders"_kj;

  auto formatted_symbol = format_symbol(symbol);
  auto timestamp = get_timestamp_ms();

  auto params = kj::str("symbol=", formatted_symbol, "&startTime=", start_time_ms,
                        "&endTime=", end_time_ms, "&timestamp=", timestamp);

  // Add signature
  auto signature = build_signature(params);
  params = kj::str(params, "&signature=", signature);

  auto req_symbol = symbol;

  return http_get_async(endpoint, params)
      .then([this, req_symbol](kj::String response) mutable -> kj::Vector<ExecutionReport> {
        if (response.size() == 0) {
          return kj::Vector<ExecutionReport>();
        }

        kj::Vector<ExecutionReport> orders;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     // Check for error response
                     if (root["code"].is_valid()) {
                       auto code = root["code"].get_int(0);
                       auto msg = root["msg"].get_string(""_kj);
                       KJ_LOG(ERROR, "Binance query_orders error", code, msg);
                       return;
                     }

                     for (size_t i = 0; i < root.size(); ++i) {
                       auto order = root[i];

                       ExecutionReport report;
                       report.symbol = req_symbol;
                       report.client_order_id =
                           kj::heapString(order["clientOrderId"].get_string(""_kj));
                       report.venue_order_id = order["orderId"].get_string(""_kj);
                       report.status = parse_order_status(order["status"].get_string("NEW"_kj));
                       report.last_fill_qty = order["executedQty"].get_double(0.0);
                       report.last_fill_price = order["price"].get_double(0.0);
                       report.ts_exchange_ns = order["updateTime"].get_int(0LL) * 1000000;
                       report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

                       orders.add(kj::mv(report));
                     }

                     last_activity_time_ = io_context_.provider->getTimer().now();
                   })) {
          KJ_LOG(ERROR, "Error parsing query_orders response", exception.getDescription());
        }
        return orders;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceAdapter::cancel_order_async(const veloz::common::SymbolId& symbol,
                                   kj::StringPtr client_order_id) {
  // Reuse existing cancel_order_async by creating CancelOrderRequest
  CancelOrderRequest req;
  req.symbol = symbol;
  req.client_order_id = kj::heapString(client_order_id);
  return cancel_order_async(req);
}

} // namespace veloz::exec
