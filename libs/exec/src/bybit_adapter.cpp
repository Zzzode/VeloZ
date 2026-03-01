#include "veloz/exec/bybit_adapter.h"

#include "veloz/core/json.h"
#include "veloz/exec/hmac_wrapper.h"

#include <chrono>
#include <kj/compat/tls.h>
#include <kj/debug.h>

namespace veloz::exec {

using veloz::core::JsonDocument;

namespace {
constexpr const char* BYBIT_REST_URL = "api.bybit.com";
constexpr const char* BYBIT_TESTNET_REST_URL = "api-testnet.bybit.com";
constexpr const char* BYBIT_WS_URL = "stream.bybit.com";
constexpr const char* BYBIT_TESTNET_WS_URL = "stream-testnet.bybit.com";
constexpr int64_t DEFAULT_RECV_WINDOW = 5000;

// Bybit error code handling
// https://bybit-exchange.github.io/docs/v5/error
void log_bybit_error(int code, kj::StringPtr msg) {
  // Common error codes
  if (code == 0) {
    return; // Success
  } else if (code == 10001) {
    KJ_LOG(ERROR, "Bybit: Parameter error", msg);
  } else if (code == 10002) {
    KJ_LOG(ERROR, "Bybit: Invalid request", msg);
  } else if (code == 10003) {
    KJ_LOG(ERROR, "Bybit: Invalid API key", msg);
  } else if (code == 10004) {
    KJ_LOG(ERROR, "Bybit: Invalid sign", msg);
  } else if (code == 10005) {
    KJ_LOG(ERROR, "Bybit: Permission denied", msg);
  } else if (code == 10006) {
    KJ_LOG(WARNING, "Bybit: Rate limit exceeded", msg);
  } else if (code == 10007) {
    KJ_LOG(ERROR, "Bybit: IP not allowed", msg);
  } else if (code == 10010) {
    KJ_LOG(ERROR, "Bybit: Unmatched IP", msg);
  } else if (code == 10016) {
    KJ_LOG(ERROR, "Bybit: Server error", msg);
  } else if (code == 10017) {
    KJ_LOG(ERROR, "Bybit: Route not found", msg);
  } else if (code == 10018) {
    KJ_LOG(ERROR, "Bybit: Exceeded IP rate limit", msg);
  }
  // Order errors (110xxx)
  else if (code == 110001) {
    KJ_LOG(ERROR, "Bybit: Order does not exist", msg);
  } else if (code == 110003) {
    KJ_LOG(ERROR, "Bybit: Order already filled", msg);
  } else if (code == 110004) {
    KJ_LOG(ERROR, "Bybit: Insufficient wallet balance", msg);
  } else if (code == 110005) {
    KJ_LOG(ERROR, "Bybit: Position status error", msg);
  } else if (code == 110006) {
    KJ_LOG(ERROR, "Bybit: Insufficient available balance", msg);
  } else if (code == 110007) {
    KJ_LOG(ERROR, "Bybit: Order already cancelled", msg);
  } else if (code == 110008) {
    KJ_LOG(ERROR, "Bybit: Order quantity exceeds limit", msg);
  } else if (code == 110009) {
    KJ_LOG(ERROR, "Bybit: Order price out of range", msg);
  } else if (code == 110010) {
    KJ_LOG(ERROR, "Bybit: Order not modifiable", msg);
  } else if (code == 110012) {
    KJ_LOG(ERROR, "Bybit: Insufficient position quantity", msg);
  } else if (code == 110013) {
    KJ_LOG(ERROR, "Bybit: Cannot set position mode", msg);
  } else if (code == 110014) {
    KJ_LOG(ERROR, "Bybit: Invalid order quantity", msg);
  } else if (code == 110015) {
    KJ_LOG(ERROR, "Bybit: Order price too high", msg);
  } else if (code == 110016) {
    KJ_LOG(ERROR, "Bybit: Order price too low", msg);
  } else if (code == 110017) {
    KJ_LOG(ERROR, "Bybit: Invalid order type", msg);
  } else if (code == 110018) {
    KJ_LOG(ERROR, "Bybit: Invalid order side", msg);
  } else if (code == 110019) {
    KJ_LOG(ERROR, "Bybit: Reduce only order rejected", msg);
  } else if (code == 110020) {
    KJ_LOG(ERROR, "Bybit: Order would trigger immediately", msg);
  } else {
    KJ_LOG(ERROR, "Bybit: Unknown error code", code, msg);
  }
}
} // namespace

BybitAdapter::BybitAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key,
                           kj::StringPtr secret_key, Category category, bool testnet)
    : io_context_(io_context), api_key_(kj::heapString(api_key)),
      secret_key_(kj::heapString(secret_key)), connected_(false), testnet_(testnet),
      category_(category), last_activity_time_(kj::systemCoarseMonotonicClock().now()),
      request_timeout_(30 * kj::SECONDS), rate_limit_window_(1 * kj::SECONDS),
      rate_limit_per_window_(50), max_retries_(3), retry_delay_(1 * kj::SECONDS),
      recv_window_(DEFAULT_RECV_WINDOW) {

  // Initialize HTTP header table
  kj::HttpHeaderTable::Builder builder;
  header_table_ = builder.build();

  // Initialize TLS context
  tls_context_ = kj::heap<kj::TlsContext>();

  // Set API endpoints based on environment
  base_rest_url_ = kj::str(testnet ? BYBIT_TESTNET_REST_URL : BYBIT_REST_URL);
  base_ws_url_ = kj::str(testnet ? BYBIT_TESTNET_WS_URL : BYBIT_WS_URL);
}

BybitAdapter::~BybitAdapter() noexcept {
  disconnect();
}

kj::StringPtr BybitAdapter::name() const {
  return "Bybit"_kj;
}

kj::StringPtr BybitAdapter::version() const {
  return "1.0.0"_kj;
}

bool BybitAdapter::is_connected() const {
  return connected_;
}

void BybitAdapter::connect() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
}

void BybitAdapter::disconnect() {
  connected_ = false;
}

kj::Promise<void> BybitAdapter::connect_async() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
  return kj::READY_NOW;
}

void BybitAdapter::set_timeout(kj::Duration timeout) {
  request_timeout_ = timeout;
}

kj::Duration BybitAdapter::get_timeout() const {
  return request_timeout_;
}

void BybitAdapter::set_category(Category category) {
  category_ = category;
}

BybitAdapter::Category BybitAdapter::get_category() const {
  return category_;
}

int64_t BybitAdapter::get_timestamp_ms() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

kj::String BybitAdapter::build_signature(kj::StringPtr timestamp, kj::StringPtr params) {
  // Bybit V5 API signature: HMAC_SHA256(timestamp + api_key + recv_window + params)
  auto prehash = kj::str(timestamp, api_key_, kj::str(recv_window_), params);

  // Use HmacSha256 wrapper for HMAC-SHA256 signature
  return HmacSha256::sign(secret_key_, prehash);
}

kj::String BybitAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
  // Bybit uses format like "BTCUSDT" for spot and linear perpetual
  return kj::str(symbol.value);
}

kj::StringPtr BybitAdapter::order_side_to_string(OrderSide side) {
  switch (side) {
  case OrderSide::Buy:
    return "Buy"_kj;
  case OrderSide::Sell:
    return "Sell"_kj;
  default:
    return "Buy"_kj;
  }
}

kj::StringPtr BybitAdapter::order_type_to_string(OrderType type) {
  switch (type) {
  case OrderType::Market:
    return "Market"_kj;
  case OrderType::Limit:
    return "Limit"_kj;
  default:
    return "Limit"_kj;
  }
}

kj::StringPtr BybitAdapter::category_to_string(Category cat) {
  switch (cat) {
  case Category::Spot:
    return "spot"_kj;
  case Category::Linear:
    return "linear"_kj;
  case Category::Inverse:
    return "inverse"_kj;
  default:
    return "spot"_kj;
  }
}

OrderStatus BybitAdapter::parse_order_status(kj::StringPtr status_str) {
  if (status_str == "Created" || status_str == "New") {
    return OrderStatus::New;
  } else if (status_str == "PartiallyFilled") {
    return OrderStatus::PartiallyFilled;
  } else if (status_str == "Filled") {
    return OrderStatus::Filled;
  } else if (status_str == "Cancelled") {
    return OrderStatus::Canceled;
  } else if (status_str == "Rejected") {
    return OrderStatus::Rejected;
  } else if (status_str == "Deactivated") {
    return OrderStatus::Expired;
  }
  return OrderStatus::New;
}

kj::Promise<kj::Own<kj::HttpClient>> BybitAdapter::get_http_client() {
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

kj::Promise<kj::String> BybitAdapter::http_get_async(kj::StringPtr endpoint, kj::StringPtr params) {
  auto timestamp = kj::str(get_timestamp_ms());
  auto signature = build_signature(timestamp, params != nullptr ? params : kj::StringPtr());

  return get_http_client().then([this, endpoint = kj::str(endpoint),
                                 params = kj::str(params != nullptr ? params : kj::StringPtr()),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    // Bybit V5 API authentication headers
    headers.addPtrPtr("X-BAPI-API-KEY"_kj, api_key_.asPtr());
    headers.addPtrPtr("X-BAPI-SIGN"_kj, signature.asPtr());
    headers.addPtrPtr("X-BAPI-TIMESTAMP"_kj, timestamp.asPtr());
    auto recv_window_str = kj::str(recv_window_);
    headers.addPtrPtr("X-BAPI-RECV-WINDOW"_kj, recv_window_str.asPtr());

    auto url = params.size() > 0 ? kj::str("https://", base_rest_url_, endpoint, "?", params)
                                 : kj::str("https://", base_rest_url_, endpoint);

    auto request = client->request(kj::HttpMethod::GET, url, headers);
    return request.response
        .then([](kj::HttpClient::Response response) {
          return response.body->readAllText().then([status = response.statusCode](kj::String body) {
            if (status >= 200 && status < 300) {
              return kj::mv(body);
            }
            KJ_LOG(ERROR, "Bybit API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::String> BybitAdapter::http_post_async(kj::StringPtr endpoint, kj::StringPtr body) {
  auto timestamp = kj::str(get_timestamp_ms());
  auto signature = build_signature(timestamp, body);

  return get_http_client().then([this, endpoint = kj::str(endpoint), body = kj::str(body),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
    kj::HttpHeaders headers(*header_table_);
    headers.setPtr(kj::HttpHeaderId::HOST, base_rest_url_);
    headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    // Bybit V5 API authentication headers
    headers.addPtrPtr("X-BAPI-API-KEY"_kj, api_key_.asPtr());
    headers.addPtrPtr("X-BAPI-SIGN"_kj, signature.asPtr());
    headers.addPtrPtr("X-BAPI-TIMESTAMP"_kj, timestamp.asPtr());
    auto recv_window_str = kj::str(recv_window_);
    headers.addPtrPtr("X-BAPI-RECV-WINDOW"_kj, recv_window_str.asPtr());

    auto url = kj::str("https://", base_rest_url_, endpoint);
    auto request = client->request(kj::HttpMethod::POST, url, headers, body.size());

    return request.body->write(body.asBytes())
        .then([response = kj::mv(request.response)]() mutable { return kj::mv(response); })
        .then([](kj::HttpClient::Response response) {
          return response.body->readAllText().then([status = response.statusCode](kj::String body) {
            if (status >= 200 && status < 300) {
              return kj::mv(body);
            }
            KJ_LOG(ERROR, "Bybit API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BybitAdapter::place_order_async(const PlaceOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto side = order_side_to_string(req.side);
  auto type = order_type_to_string(req.type);
  auto cat = category_to_string(category_);

  kj::String body;
  KJ_IF_SOME(price, req.price) {
    body = kj::str("{\"category\":\"", cat, "\",\"symbol\":\"", symbol, "\",\"side\":\"", side,
                   "\",\"orderType\":\"", type, "\",\"qty\":\"", req.qty, "\",\"price\":\"", price,
                   "\",\"orderLinkId\":\"", req.client_order_id, "\"}");
  }
  else {
    body = kj::str("{\"category\":\"", cat, "\",\"symbol\":\"", symbol, "\",\"side\":\"", side,
                   "\",\"orderType\":\"", type, "\",\"qty\":\"", req.qty, "\",\"orderLinkId\":\"",
                   req.client_order_id, "\"}");
  }

  auto symbol_copy = req.symbol;
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/v5/order/create", body)
      .then([symbol_copy, client_order_id = kj::mv(client_order_id_copy)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          KJ_LOG(ERROR, "Bybit place_order: empty response");
          return kj::none;
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     auto retMsg = root["retMsg"].get_string(""_kj);

                     if (retCode != 0) {
                       log_bybit_error(retCode, retMsg);
                       return;
                     }

                     auto data = root["result"];
                     ExecutionReport report;
                     report.symbol = symbol_copy;
                     report.client_order_id = kj::mv(client_order_id);
                     report.venue_order_id = kj::str(data["orderId"].get_string(""_kj));
                     report.status = OrderStatus::Accepted;
                     report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

                     result = kj::mv(report);
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit place_order response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BybitAdapter::cancel_order_async(const CancelOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto cat = category_to_string(category_);
  auto body = kj::str("{\"category\":\"", cat, "\",\"symbol\":\"", symbol, "\",\"orderLinkId\":\"",
                      req.client_order_id, "\"}");

  auto symbol_copy = req.symbol;
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/v5/order/cancel", body)
      .then([symbol_copy, client_order_id = kj::mv(client_order_id_copy)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          KJ_LOG(ERROR, "Bybit cancel_order: empty response");
          return kj::none;
        }

        kj::Maybe<ExecutionReport> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     auto retMsg = root["retMsg"].get_string(""_kj);

                     if (retCode != 0) {
                       log_bybit_error(retCode, retMsg);
                       return;
                     }

                     auto data = root["result"];
                     ExecutionReport report;
                     report.symbol = symbol_copy;
                     report.client_order_id = kj::mv(client_order_id);
                     report.venue_order_id = kj::str(data["orderId"].get_string(""_kj));
                     report.status = OrderStatus::Canceled;
                     report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

                     result = kj::mv(report);
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit cancel_order response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Maybe<ExecutionReport> BybitAdapter::place_order(const PlaceOrderRequest& req) {
  return {};
}

kj::Maybe<ExecutionReport> BybitAdapter::cancel_order(const CancelOrderRequest& req) {
  return {};
}

kj::Promise<kj::Maybe<double>>
BybitAdapter::get_current_price_async(const veloz::common::SymbolId& symbol) {
  auto formatted_symbol = format_symbol(symbol);
  auto cat = category_to_string(category_);
  auto params = kj::str("category=", cat, "&symbol=", formatted_symbol);

  return http_get_async("/v5/market/tickers", params)
      .then([](kj::String response) -> kj::Maybe<double> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<double> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     if (retCode != 0) {
                       log_bybit_error(retCode, root["retMsg"].get_string(""_kj));
                       return;
                     }

                     auto list = root["result"]["list"];
                     if (list.size() > 0) {
                       auto ticker = list[0];
                       result = ticker["lastPrice"].get_double(0.0);
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit ticker response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
BybitAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  auto formatted_symbol = format_symbol(symbol);
  auto cat = category_to_string(category_);
  auto params = kj::str("category=", cat, "&symbol=", formatted_symbol, "&limit=", depth);

  return http_get_async("/v5/market/orderbook", params)
      .then([](kj::String response) -> kj::Maybe<kj::Array<PriceLevel>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<PriceLevel>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     if (retCode != 0) {
                       log_bybit_error(retCode, root["retMsg"].get_string(""_kj));
                       return;
                     }

                     auto data = root["result"];
                     auto bids = data["b"];
                     auto asks = data["a"];

                     kj::Vector<PriceLevel> levels;
                     // Add bids
                     for (size_t i = 0; i < bids.size(); ++i) {
                       auto bid = bids[i];
                       PriceLevel level;
                       level.price = bid[0].get_double(0.0);
                       level.quantity = bid[1].get_double(0.0);
                       levels.add(kj::mv(level));
                     }
                     // Add asks
                     for (size_t i = 0; i < asks.size(); ++i) {
                       auto ask = asks[i];
                       PriceLevel level;
                       level.price = ask[0].get_double(0.0);
                       level.quantity = ask[1].get_double(0.0);
                       levels.add(kj::mv(level));
                     }

                     result = levels.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit orderbook response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<kj::Array<TradeData>>>
BybitAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  auto formatted_symbol = format_symbol(symbol);
  auto cat = category_to_string(category_);
  auto params = kj::str("category=", cat, "&symbol=", formatted_symbol, "&limit=", limit);

  return http_get_async("/v5/market/recent-trade", params)
      .then([](kj::String response) -> kj::Maybe<kj::Array<TradeData>> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<kj::Array<TradeData>> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     if (retCode != 0) {
                       log_bybit_error(retCode, root["retMsg"].get_string(""_kj));
                       return;
                     }

                     auto list = root["result"]["list"];
                     kj::Vector<TradeData> trades;
                     for (size_t i = 0; i < list.size(); ++i) {
                       auto trade = list[i];
                       TradeData data;
                       data.price = trade["price"].get_double(0.0);
                       data.quantity = trade["size"].get_double(0.0);
                       trades.add(kj::mv(data));
                     }

                     result = trades.releaseAsArray();
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit trades response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Promise<kj::Maybe<double>> BybitAdapter::get_account_balance_async(kj::StringPtr asset) {
  auto params = kj::str("accountType=UNIFIED&coin=", asset);
  auto asset_copy = kj::str(asset);

  return http_get_async("/v5/account/wallet-balance", params)
      .then([asset_copy = kj::mv(asset_copy)](kj::String response) mutable -> kj::Maybe<double> {
        if (response.size() == 0) {
          return kj::none;
        }

        kj::Maybe<double> result;
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     auto doc = JsonDocument::parse(response);
                     auto root = doc.root();

                     auto retCode = root["retCode"].get_int(0);
                     if (retCode != 0) {
                       log_bybit_error(retCode, root["retMsg"].get_string(""_kj));
                       return;
                     }

                     auto list = root["result"]["list"];
                     for (size_t i = 0; i < list.size(); ++i) {
                       auto account = list[i];
                       auto coins = account["coin"];
                       for (size_t j = 0; j < coins.size(); ++j) {
                         auto coin = coins[j];
                         if (coin["coin"].get_string(""_kj) == asset_copy) {
                           result = coin["walletBalance"].get_double(0.0);
                           return;
                         }
                       }
                     }
                   })) {
          KJ_LOG(ERROR, "Error parsing Bybit balance response", exception.getDescription());
          return kj::none;
        }
        return result;
      });
}

kj::Maybe<double> BybitAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  return kj::none;
}

kj::Maybe<kj::Array<PriceLevel>> BybitAdapter::get_order_book(const veloz::common::SymbolId& symbol,
                                                              int depth) {
  return kj::none;
}

kj::Maybe<kj::Array<TradeData>>
BybitAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
  return kj::none;
}

kj::Maybe<double> BybitAdapter::get_account_balance(kj::StringPtr asset) {
  return kj::none;
}

} // namespace veloz::exec
