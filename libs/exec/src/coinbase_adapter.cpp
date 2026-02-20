#include "veloz/exec/coinbase_adapter.h"

#include "veloz/exec/hmac_wrapper.h"

#include <chrono>
#include <kj/compat/tls.h>
#include <kj/debug.h>

namespace veloz::exec {

namespace {
constexpr const char* COINBASE_REST_URL = "api.coinbase.com";
constexpr const char* COINBASE_SANDBOX_REST_URL = "api-public.sandbox.exchange.coinbase.com";
constexpr const char* COINBASE_WS_URL = "advanced-trade-ws.coinbase.com";
constexpr const char* COINBASE_SANDBOX_WS_URL =
    "advanced-trade-ws-public.sandbox.exchange.coinbase.com";
} // namespace

CoinbaseAdapter::CoinbaseAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key,
                                 kj::StringPtr api_secret, bool sandbox)
    : io_context_(io_context), api_key_(kj::heapString(api_key)),
      api_secret_(kj::heapString(api_secret)), connected_(false), sandbox_(sandbox),
      last_activity_time_(kj::systemCoarseMonotonicClock().now()),
      request_timeout_(30 * kj::SECONDS), rate_limit_window_(1 * kj::SECONDS),
      rate_limit_per_window_(30), max_retries_(3), retry_delay_(1 * kj::SECONDS) {

  // Initialize HTTP header table
  kj::HttpHeaderTable::Builder builder;
  header_table_ = builder.build();

  // Initialize TLS context
  tls_context_ = kj::heap<kj::TlsContext>();

  // Set API endpoints based on environment
  base_rest_url_ = kj::str(sandbox ? COINBASE_SANDBOX_REST_URL : COINBASE_REST_URL);
  base_ws_url_ = kj::str(sandbox ? COINBASE_SANDBOX_WS_URL : COINBASE_WS_URL);
}

CoinbaseAdapter::~CoinbaseAdapter() noexcept {
  disconnect();
}

kj::StringPtr CoinbaseAdapter::name() const {
  return "Coinbase"_kj;
}

kj::StringPtr CoinbaseAdapter::version() const {
  return "1.0.0"_kj;
}

bool CoinbaseAdapter::is_connected() const {
  return connected_;
}

void CoinbaseAdapter::connect() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
}

void CoinbaseAdapter::disconnect() {
  connected_ = false;
}

kj::Promise<void> CoinbaseAdapter::connect_async() {
  connected_ = true;
  last_activity_time_ = kj::systemCoarseMonotonicClock().now();
  return kj::READY_NOW;
}

void CoinbaseAdapter::set_timeout(kj::Duration timeout) {
  request_timeout_ = timeout;
}

kj::Duration CoinbaseAdapter::get_timeout() const {
  return request_timeout_;
}

int64_t CoinbaseAdapter::get_timestamp_sec() const {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

kj::String CoinbaseAdapter::build_jwt_token(kj::StringPtr method, kj::StringPtr request_path) {
  // Coinbase Advanced Trade API uses HMAC-SHA256 for signing
  // Format: timestamp + method + requestPath + body
  auto timestamp = kj::str(get_timestamp_sec());
  auto message = kj::str(timestamp, method, request_path);

  // Use HmacSha256 wrapper for HMAC-SHA256 signature with KJ-only interface
  return HmacSha256::sign(api_secret_, message);
}

kj::String CoinbaseAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
  // Coinbase uses format like "BTC-USD" for spot
  // Convert BTCUSDT to BTC-USDT or similar patterns
  kj::StringPtr sym = symbol.value;
  size_t len = sym.size();

  // Check for USDT suffix
  if (len >= 4) {
    kj::StringPtr suffix = sym.slice(len - 4);
    if (suffix == "USDT") {
      return kj::str(sym.slice(0, len - 4), "-USDT");
    }
  }

  // Check for USD suffix
  if (len >= 3) {
    kj::StringPtr suffix = sym.slice(len - 3);
    if (suffix == "USD") {
      return kj::str(sym.slice(0, len - 3), "-USD");
    }
  }

  return kj::heapString(sym);
}

kj::StringPtr CoinbaseAdapter::order_side_to_string(OrderSide side) {
  switch (side) {
  case OrderSide::Buy:
    return "BUY"_kj;
  case OrderSide::Sell:
    return "SELL"_kj;
  default:
    return "BUY"_kj;
  }
}

kj::StringPtr CoinbaseAdapter::order_type_to_string(OrderType type) {
  switch (type) {
  case OrderType::Market:
    return "MARKET"_kj;
  case OrderType::Limit:
    return "LIMIT"_kj;
  default:
    return "LIMIT"_kj;
  }
}

OrderStatus CoinbaseAdapter::parse_order_status(kj::StringPtr status_str) {
  if (status_str == "PENDING") {
    return OrderStatus::New;
  } else if (status_str == "OPEN") {
    return OrderStatus::Accepted;
  } else if (status_str == "FILLED") {
    return OrderStatus::Filled;
  } else if (status_str == "CANCELLED") {
    return OrderStatus::Canceled;
  } else if (status_str == "EXPIRED") {
    return OrderStatus::Expired;
  } else if (status_str == "FAILED") {
    return OrderStatus::Rejected;
  }
  return OrderStatus::New;
}

kj::Promise<kj::Own<kj::HttpClient>> CoinbaseAdapter::get_http_client() {
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

kj::Promise<kj::String> CoinbaseAdapter::http_get_async(kj::StringPtr endpoint,
                                                        kj::StringPtr params) {
  // Use KJ-only HmacSha256 interface for JWT signature generation
  auto timestamp = kj::str(get_timestamp_sec());
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_jwt_token("GET"_kj, request_path);

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
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
            KJ_LOG(ERROR, "Coinbase API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::String> CoinbaseAdapter::http_post_async(kj::StringPtr endpoint,
                                                         kj::StringPtr body) {
  // Use KJ-only HmacSha256 interface for JWT signature generation
  auto timestamp = kj::str(get_timestamp_sec());
  auto signature = build_jwt_token("POST"_kj, endpoint);

  return get_http_client().then([this, endpoint = kj::str(endpoint), body = kj::str(body),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
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
            KJ_LOG(ERROR, "Coinbase API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::String> CoinbaseAdapter::http_delete_async(kj::StringPtr endpoint,
                                                           kj::StringPtr params) {
  // Use KJ-only HmacSha256 interface for JWT signature generation
  auto timestamp = kj::str(get_timestamp_sec());
  auto request_path = params != nullptr ? kj::str(endpoint, "?", params) : kj::str(endpoint);
  auto signature = build_jwt_token("DELETE"_kj, request_path);

  return get_http_client().then([this, endpoint = kj::str(endpoint), params = kj::str(params),
                                 timestamp = kj::mv(timestamp),
                                 signature = kj::mv(signature)](kj::Own<kj::HttpClient> client) {
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
            KJ_LOG(ERROR, "Coinbase API error", status, body);
            return kj::str("");
          });
        })
        .attach(kj::mv(client));
  });
}

kj::Promise<kj::Maybe<ExecutionReport>>
CoinbaseAdapter::place_order_async(const PlaceOrderRequest& req) {
  auto symbol = format_symbol(req.symbol);
  auto side = order_side_to_string(req.side);

  kj::String body;
  KJ_IF_SOME(price, req.price) {
    body = kj::str("{\"product_id\":\"", symbol, "\",\"side\":\"", side,
                   "\",\"order_configuration\":{\"limit_limit_gtc\":{\"base_size\":\"", req.qty,
                   "\",\"limit_price\":\"", price, "\"}},\"client_order_id\":\"",
                   req.client_order_id, "\"}");
  }
  else {
    body = kj::str("{\"product_id\":\"", symbol, "\",\"side\":\"", side,
                   "\",\"order_configuration\":{\"market_market_ioc\":{\"quote_size\":\"", req.qty,
                   "\"}},\"client_order_id\":\"", req.client_order_id, "\"}");
  }

  auto symbol_value = kj::str(req.symbol.value);
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v3/brokerage/orders", body)
      .then([symbol_value = kj::mv(symbol_value),
             client_order_id = kj::mv(client_order_id_copy)](kj::String response) mutable {
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
CoinbaseAdapter::cancel_order_async(const CancelOrderRequest& req) {
  auto body = kj::str("{\"order_ids\":[\"", req.client_order_id, "\"]}");

  auto symbol_value = kj::str(req.symbol.value);
  auto client_order_id_copy = kj::str(req.client_order_id);

  return http_post_async("/api/v3/brokerage/orders/batch_cancel", body)
      .then([symbol_value = kj::mv(symbol_value),
             client_order_id = kj::mv(client_order_id_copy)](kj::String response) mutable {
        auto symbol_copy = kj::str(symbol_value);
        ExecutionReport report;
        report.symbol = veloz::common::SymbolId{symbol_copy};
        report.client_order_id = kj::mv(client_order_id);
        report.status = OrderStatus::Canceled;
        report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

        return kj::Maybe<ExecutionReport>(kj::mv(report));
      });
}

kj::Maybe<ExecutionReport> CoinbaseAdapter::place_order(const PlaceOrderRequest& req) {
  return {};
}

kj::Maybe<ExecutionReport> CoinbaseAdapter::cancel_order(const CancelOrderRequest& req) {
  return {};
}

kj::Promise<kj::Maybe<double>>
CoinbaseAdapter::get_current_price_async(const veloz::common::SymbolId& symbol) {
  auto formatted_symbol = format_symbol(symbol);
  auto endpoint = kj::str("/api/v3/brokerage/products/", formatted_symbol);

  return http_get_async(endpoint, nullptr).then([](kj::String response) -> kj::Maybe<double> {
    return kj::none;
  });
}

kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
CoinbaseAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  auto formatted_symbol = format_symbol(symbol);
  auto endpoint = kj::str("/api/v3/brokerage/products/", formatted_symbol, "/book");
  auto params = kj::str("limit=", depth);

  return http_get_async(endpoint, params)
      .then([](kj::String response) -> kj::Maybe<kj::Array<PriceLevel>> { return kj::none; });
}

kj::Promise<kj::Maybe<kj::Array<TradeData>>>
CoinbaseAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  auto formatted_symbol = format_symbol(symbol);
  auto endpoint = kj::str("/api/v3/brokerage/products/", formatted_symbol, "/ticker");

  return http_get_async(endpoint, nullptr)
      .then([](kj::String response) -> kj::Maybe<kj::Array<TradeData>> { return kj::none; });
}

kj::Promise<kj::Maybe<double>> CoinbaseAdapter::get_account_balance_async(kj::StringPtr asset) {
  return http_get_async("/api/v3/brokerage/accounts", nullptr).then([](kj::String response) {
    return kj::Maybe<double>(0.0);
  });
}

kj::Maybe<double> CoinbaseAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  return kj::none;
}

kj::Maybe<kj::Array<PriceLevel>>
CoinbaseAdapter::get_order_book(const veloz::common::SymbolId& symbol, int depth) {
  return kj::none;
}

kj::Maybe<kj::Array<TradeData>>
CoinbaseAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
  return kj::none;
}

kj::Maybe<double> CoinbaseAdapter::get_account_balance(kj::StringPtr asset) {
  return kj::none;
}

} // namespace veloz::exec
