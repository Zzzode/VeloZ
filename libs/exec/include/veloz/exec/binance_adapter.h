#pragma once

#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/reconciliation.h"

#include <kj/array.h>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace kj {
class TlsContext;
} // namespace kj

namespace veloz::exec {

// BinanceAdapter uses KJ async I/O for HTTP operations.
// HMAC signatures use kj::String interface through HmacSha256 wrapper.
// Implements ReconciliationQueryInterface for order reconciliation support.
class BinanceAdapter final : public ExchangeAdapter, public ReconciliationQueryInterface {
public:
  // Constructor requires KJ async I/O context for async HTTP operations
  BinanceAdapter(kj::AsyncIoContext& io_context, kj::StringPtr api_key, kj::StringPtr secret_key,
                 bool testnet = false);
  ~BinanceAdapter() noexcept override;

  // Place an order, returns execution report (async version)
  kj::Promise<kj::Maybe<ExecutionReport>> place_order_async(const PlaceOrderRequest& req);

  // Cancel an order, returns execution report (async version)
  kj::Promise<kj::Maybe<ExecutionReport>> cancel_order_async(const CancelOrderRequest& req);

  // Synchronous versions for ExchangeAdapter interface compatibility
  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) override;
  kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) override;

  // Connection management
  bool is_connected() const override;
  void connect() override;
  void disconnect() override;

  // Async connection management
  kj::Promise<void> connect_async();

  // Get adapter info
  [[nodiscard]] kj::StringPtr name() const override;
  [[nodiscard]] kj::StringPtr version() const override;

  // Async REST API methods for market data and account info
  kj::Promise<kj::Maybe<double>> get_current_price_async(const veloz::common::SymbolId& symbol);
  kj::Promise<kj::Maybe<kj::Array<PriceLevel>>>
  get_order_book_async(const veloz::common::SymbolId& symbol, int depth = 10);
  kj::Promise<kj::Maybe<kj::Array<TradeData>>>
  get_recent_trades_async(const veloz::common::SymbolId& symbol, int limit = 500);
  kj::Promise<kj::Maybe<double>> get_account_balance_async(kj::StringPtr asset);

  // Order query methods (Production Trading - Task #11)
  kj::Promise<kj::Maybe<ExecutionReport>> get_order_async(const veloz::common::SymbolId& symbol,
                                                          kj::StringPtr client_order_id);
  kj::Promise<kj::Maybe<kj::Array<ExecutionReport>>>
  get_open_orders_async(const veloz::common::SymbolId& symbol);
  kj::Promise<kj::Maybe<kj::Array<ExecutionReport>>> get_all_open_orders_async();

  // Synchronous order query methods
  kj::Maybe<ExecutionReport> get_order(const veloz::common::SymbolId& symbol,
                                       kj::StringPtr client_order_id);
  kj::Maybe<kj::Array<ExecutionReport>> get_open_orders(const veloz::common::SymbolId& symbol);
  kj::Maybe<kj::Array<ExecutionReport>> get_all_open_orders();

  // Synchronous versions for backward compatibility (blocks on async)
  kj::Maybe<double> get_current_price(const veloz::common::SymbolId& symbol);
  kj::Maybe<kj::Array<PriceLevel>> get_order_book(const veloz::common::SymbolId& symbol,
                                                  int depth = 10);
  kj::Maybe<kj::Array<TradeData>> get_recent_trades(const veloz::common::SymbolId& symbol,
                                                    int limit = 500);
  kj::Maybe<double> get_account_balance(kj::StringPtr asset);

  // Configuration
  void set_timeout(kj::Duration timeout);
  kj::Duration get_timeout() const;

  kj::String build_order_params(const PlaceOrderRequest& req);

  // ReconciliationQueryInterface implementation (Task #2)
  kj::Promise<kj::Vector<ExecutionReport>>
  query_open_orders_async(const veloz::common::SymbolId& symbol) override;

  kj::Promise<kj::Maybe<ExecutionReport>> query_order_async(const veloz::common::SymbolId& symbol,
                                                            kj::StringPtr client_order_id) override;

  kj::Promise<kj::Vector<ExecutionReport>> query_orders_async(const veloz::common::SymbolId& symbol,
                                                              std::int64_t start_time_ms,
                                                              std::int64_t end_time_ms) override;

  kj::Promise<kj::Maybe<ExecutionReport>>
  cancel_order_async(const veloz::common::SymbolId& symbol, kj::StringPtr client_order_id) override;

private:
  // Async HTTP methods using KJ HTTP client
  kj::Promise<kj::String> http_get_async(kj::StringPtr endpoint, kj::StringPtr params = nullptr);
  kj::Promise<kj::String> http_post_async(kj::StringPtr endpoint, kj::StringPtr params);
  kj::Promise<kj::String> http_delete_async(kj::StringPtr endpoint, kj::StringPtr params);

  // Helper to create HTTP client for a request
  kj::Promise<kj::Own<kj::HttpClient>> get_http_client();

  // Helper methods
  // Build HMAC signature using KJ string interface via HmacSha256 wrapper
  kj::String build_signature(kj::StringPtr query_string);
  kj::String format_symbol(const veloz::common::SymbolId& symbol);
  kj::StringPtr order_side_to_string(OrderSide side);
  kj::StringPtr order_type_to_string(OrderType type);
  kj::StringPtr time_in_force_to_string(TimeInForce tif);
  OrderStatus parse_order_status(kj::StringPtr status_str);

  // Get current timestamp in milliseconds
  int64_t get_timestamp_ms() const;

  // KJ async I/O context
  kj::AsyncIoContext& io_context_;

  // HTTP header table for response parsing
  kj::Own<kj::HttpHeaderTable> header_table_;

  // TLS context for HTTPS connections
  kj::Own<kj::TlsContext> tls_context_;

  // API key and secret - using kj::String for KJ compatibility
  kj::String api_key_;
  kj::String secret_key_;

  // Connection status
  bool connected_;
  bool testnet_;

  // API endpoints - using kj::String
  kj::String base_rest_url_;
  kj::String base_ws_url_;

  // Last activity time for health check
  kj::TimePoint last_activity_time_;

  // Timeout for HTTP requests
  kj::Duration request_timeout_;

  // Rate limiting configuration
  kj::Duration rate_limit_window_;
  int rate_limit_per_window_;

  // Retry configuration
  int max_retries_;
  kj::Duration retry_delay_;
};

} // namespace veloz::exec
