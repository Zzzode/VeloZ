#include "veloz/exec/binance_adapter.h"

#include "veloz/core/json.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <kj/string.h>

#ifndef VELOZ_NO_OPENSSL
// OpenSSL is used for HMAC-SHA256 signature generation.
// This is required for Binance API authentication and KJ does not provide HMAC functionality.
// Per CLAUDE.md guidelines, this is an acceptable use of external library for API compatibility.
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace veloz::exec {

using namespace veloz::core;

BinanceAdapter::BinanceAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key,
                               kj::StringPtr secret_key, bool testnet)
    : io_context_(io_context), api_key_(api_key.cStr(), api_key.size()),
      secret_key_(secret_key.cStr(), secret_key.size()), connected_(false), testnet_(testnet),
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

std::string BinanceAdapter::build_signature(const std::string& query_string) {
#ifndef VELOZ_NO_OPENSSL
  // Implement HMAC-SHA256 signature using OpenSSL
  // This is required for Binance API authentication - KJ does not provide HMAC functionality
  unsigned char* digest;
  unsigned int digest_len;

  digest = HMAC(EVP_sha256(), secret_key_.c_str(), static_cast<int>(secret_key_.length()),
                reinterpret_cast<const unsigned char*>(query_string.c_str()), query_string.length(),
                nullptr, &digest_len);

  // Convert binary digest to hex string
  std::ostringstream oss;
  for (unsigned int i = 0; i < digest_len; ++i) {
    oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(digest[i]);
  }

  return oss.str();
#else
  // Placeholder for builds without OpenSSL
  // In production, this should be implemented with a proper crypto library
  (void)query_string;
  KJ_LOG(WARNING, "OpenSSL not available, using placeholder signature");
  return "placeholder_signature";
#endif
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
        headers.set(kj::HttpHeaderId::HOST, base_rest_url_);
        if (!api_key_.empty()) {
          headers.add("X-MBX-APIKEY", kj::StringPtr(api_key_.c_str()));
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
        headers.set(kj::HttpHeaderId::HOST, base_rest_url_);
        headers.set(kj::HttpHeaderId::CONTENT_TYPE, "application/x-www-form-urlencoded");
        if (!api_key_.empty()) {
          headers.add("X-MBX-APIKEY", kj::StringPtr(api_key_.c_str()));
        }

        // Make request with body
        auto request = client->request(kj::HttpMethod::POST, url, headers, params.size());

        // Write body
        auto writePromise = request.body->write(params.begin(), params.size());

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
        headers.set(kj::HttpHeaderId::HOST, base_rest_url_);
        if (!api_key_.empty()) {
          headers.add("X-MBX-APIKEY", kj::StringPtr(api_key_.c_str()));
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
  kj::String formatted = kj::str(symbol.value);
  // Convert to uppercase
  for (char& c : formatted) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return formatted;
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
    auto client_order_id = kj::heapString(req.client_order_id);
    auto reduce_only = req.reduce_only;
    auto post_only = req.post_only;

    return connect_async().then([this, symbol, side, type, tif, qty, price,
                                 client_order_id = kj::mv(client_order_id), reduce_only,
                                 post_only]() mutable -> kj::Promise<kj::Maybe<ExecutionReport>> {
      if (!is_connected()) {
        return kj::Maybe<ExecutionReport>(nullptr);
      }
      PlaceOrderRequest new_req;
      new_req.symbol = symbol;
      new_req.side = side;
      new_req.type = type;
      new_req.tif = tif;
      new_req.qty = qty;
      new_req.price = price;
      new_req.client_order_id = kj::mv(client_order_id);
      new_req.reduce_only = reduce_only;
      new_req.post_only = post_only;
      return place_order_async(new_req);
    });
  }

  kj::StringPtr endpoint = "/api/v3/order"_kj;

  // Build query string
  auto symbol = format_symbol(req.symbol);
  auto side = order_side_to_string(req.side);
  auto type = order_type_to_string(req.type);
  auto tif = time_in_force_to_string(req.tif);
  auto timestamp = get_timestamp_ms();

  kj::String params;
  KJ_IF_MAYBE (priceVal, req.price) {
    if (req.client_order_id.size() > 0) {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&price=", *priceVal,
                       "&newClientOrderId=", req.client_order_id, "&timestamp=", timestamp);
    } else {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&price=", *priceVal, "&timestamp=", timestamp);
    }
  } else {
    if (req.client_order_id.size() > 0) {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&newClientOrderId=", req.client_order_id,
                       "&timestamp=", timestamp);
    } else {
      params = kj::str("symbol=", symbol, "&side=", side, "&type=", type, "&timeInForce=", tif,
                       "&quantity=", req.qty, "&timestamp=", timestamp);
    }
  }

  if (req.reduce_only) {
    params = kj::str(params, "&reduceOnly=true");
  }
  if (req.post_only) {
    params = kj::str(params, "&postOnly=true");
  }

  // Add signature
  std::string query_string(params.cStr(), params.size());
  std::string signature = build_signature(query_string);
  params = kj::str(params, "&signature=", signature);

  // Copy needed fields for the lambda
  auto req_symbol = req.symbol;
  auto req_client_order_id = kj::heapString(req.client_order_id);

  return http_post_async(endpoint, params)
      .then([this, req_symbol, req_client_order_id = kj::mv(req_client_order_id)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          return nullptr;
        }

        try {
          auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
          auto root = doc.root();

          ExecutionReport report;
          report.symbol = req_symbol;
          report.client_order_id = kj::mv(req_client_order_id);
          report.venue_order_id = kj::heapString(root["orderId"].get_string(""));
          report.status = parse_order_status(kj::StringPtr(root["status"].get_string("NEW")));
          report.last_fill_qty = root["executedQty"].get_double(0.0);
          report.last_fill_price = root["price"].get_double(0.0);
          report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
          report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();

          last_activity_time_ = io_context_.provider->getTimer().now();
          return report;
        } catch (const std::exception& e) {
          KJ_LOG(ERROR, "Error parsing place order response", e.what());
          return nullptr;
        }
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
        return kj::Maybe<ExecutionReport>(nullptr);
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

  // Add signature
  std::string query_string(params.cStr(), params.size());
  std::string signature = build_signature(query_string);
  params = kj::str(params, "&signature=", signature);

  // Copy needed fields for the lambda
  auto req_symbol = req.symbol;
  auto req_client_order_id = kj::heapString(req.client_order_id);

  return http_delete_async(endpoint, params)
      .then([this, req_symbol, req_client_order_id = kj::mv(req_client_order_id)](
                kj::String response) mutable -> kj::Maybe<ExecutionReport> {
        if (response.size() == 0) {
          return nullptr;
        }

        try {
          auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
          auto root = doc.root();

          ExecutionReport report;
          report.symbol = req_symbol;
          report.client_order_id = kj::mv(req_client_order_id);
          report.venue_order_id = kj::heapString(root["orderId"].get_string(""));
          report.status = parse_order_status(kj::StringPtr(root["status"].get_string("CANCELED")));
          report.last_fill_qty = root["executedQty"].get_double(0.0);
          report.last_fill_price = root["price"].get_double(0.0);
          report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
          report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();

          last_activity_time_ = io_context_.provider->getTimer().now();
          return report;
        } catch (const std::exception& e) {
          KJ_LOG(ERROR, "Error parsing cancel order response", e.what());
          return nullptr;
        }
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

    try {
      auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
      auto root = doc.root();
      if (root["serverTime"].is_valid()) {
        connected_ = true;
        last_activity_time_ = io_context_.provider->getTimer().now();
        KJ_LOG(INFO, "Binance API connected successfully");
      }
    } catch (const std::exception& e) {
      KJ_LOG(ERROR, "Error connecting to Binance API", e.what());
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

const char* BinanceAdapter::name() const {
  return "Binance";
}

const char* BinanceAdapter::version() const {
  return "2.0.0-kj-async";
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
      return nullptr;
    }

    try {
      auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
      auto root = doc.root();
      auto price_val = root["price"];
      if (price_val.is_string()) {
        last_activity_time_ = io_context_.provider->getTimer().now();
        return std::stod(price_val.get_string());
      } else if (price_val.is_real()) {
        last_activity_time_ = io_context_.provider->getTimer().now();
        return price_val.get_double();
      }
    } catch (const std::exception& e) {
      KJ_LOG(ERROR, "Error getting current price", e.what());
    }

    return nullptr;
  });
}

kj::Promise<kj::Maybe<std::map<double, double>>>
BinanceAdapter::get_order_book_async(const veloz::common::SymbolId& symbol, int depth) {
  kj::StringPtr endpoint = "/api/v3/depth"_kj;
  auto params = kj::str("symbol=", format_symbol(symbol), "&limit=", depth);

  return http_get_async(endpoint, params)
      .then([this](kj::String response) -> kj::Maybe<std::map<double, double>> {
        if (response.size() == 0) {
          return nullptr;
        }

        try {
          auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
          auto root = doc.root();

          std::map<double, double> order_book;

          auto bids = root["bids"];
          if (bids.is_array()) {
            for (size_t i = 0; i < bids.size(); ++i) {
              auto bid = bids[i];
              double price = std::stod(bid[0].get_string());
              double qty = std::stod(bid[1].get_string());
              order_book[price] = qty;
            }
          }

          auto asks = root["asks"];
          if (asks.is_array()) {
            for (size_t i = 0; i < asks.size(); ++i) {
              auto ask = asks[i];
              double price = std::stod(ask[0].get_string());
              double qty = std::stod(ask[1].get_string());
              order_book[price] = qty;
            }
          }

          last_activity_time_ = io_context_.provider->getTimer().now();
          return order_book;
        } catch (const std::exception& e) {
          KJ_LOG(ERROR, "Error getting order book", e.what());
        }

        return nullptr;
      });
}

kj::Promise<kj::Maybe<std::vector<std::pair<double, double>>>>
BinanceAdapter::get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit) {
  kj::StringPtr endpoint = "/api/v3/trades"_kj;
  auto params = kj::str("symbol=", format_symbol(symbol), "&limit=", limit);

  return http_get_async(endpoint, params)
      .then([this](kj::String response) -> kj::Maybe<std::vector<std::pair<double, double>>> {
        if (response.size() == 0) {
          return nullptr;
        }

        try {
          auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
          auto root = doc.root();

          std::vector<std::pair<double, double>> trades;

          for (size_t i = 0; i < root.size(); ++i) {
            auto trade = root[i];
            double price = std::stod(trade["price"].get_string());
            double qty = std::stod(trade["qty"].get_string());
            trades.emplace_back(price, qty);
          }

          last_activity_time_ = io_context_.provider->getTimer().now();
          return trades;
        } catch (const std::exception& e) {
          KJ_LOG(ERROR, "Error getting recent trades", e.what());
        }

        return nullptr;
      });
}

kj::Promise<kj::Maybe<double>> BinanceAdapter::get_account_balance_async(kj::StringPtr asset) {
  kj::StringPtr endpoint = "/api/v3/account"_kj;

  auto timestamp = get_timestamp_ms();
  auto params = kj::str("timestamp=", timestamp);

  // Add signature
  std::string query_string(params.cStr(), params.size());
  std::string signature = build_signature(query_string);
  params = kj::str(params, "&signature=", signature);

  return http_get_async(endpoint, params)
      .then([this, asset = kj::str(asset)](kj::String response) -> kj::Maybe<double> {
        if (response.size() == 0) {
          return nullptr;
        }

        try {
          auto doc = JsonDocument::parse(std::string(response.cStr(), response.size()));
          auto root = doc.root();

          auto balances = root["balances"];
          if (balances.is_array()) {
            for (size_t i = 0; i < balances.size(); ++i) {
              auto balance = balances[i];
              if (kj::StringPtr(balance["asset"].get_string("")) == asset) {
                last_activity_time_ = io_context_.provider->getTimer().now();
                return std::stod(balance["free"].get_string());
              }
            }
          }
        } catch (const std::exception& e) {
          KJ_LOG(ERROR, "Error getting account balance", e.what());
        }

        return nullptr;
      });
}

// Synchronous wrappers for backward compatibility
std::optional<double> BinanceAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
  auto result = get_current_price_async(symbol).wait(io_context_.waitScope);
  KJ_IF_MAYBE (price, result) {
    return *price;
  }
  return std::nullopt;
}

std::optional<std::map<double, double>>
BinanceAdapter::get_order_book(const veloz::common::SymbolId& symbol, int depth) {
  auto result = get_order_book_async(symbol, depth).wait(io_context_.waitScope);
  KJ_IF_MAYBE (book, result) {
    return *book;
  }
  return std::nullopt;
}

std::optional<std::vector<std::pair<double, double>>>
BinanceAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
  auto result = get_recent_trades_async(symbol, limit).wait(io_context_.waitScope);
  KJ_IF_MAYBE (trades, result) {
    return *trades;
  }
  return std::nullopt;
}

std::optional<double> BinanceAdapter::get_account_balance(const std::string& asset) {
  auto result = get_account_balance_async(kj::StringPtr(asset.c_str())).wait(io_context_.waitScope);
  KJ_IF_MAYBE (balance, result) {
    return *balance;
  }
  return std::nullopt;
}

} // namespace veloz::exec
