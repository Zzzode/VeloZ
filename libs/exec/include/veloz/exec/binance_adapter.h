#pragma once

#include "veloz/exec/exchange_adapter.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace veloz::exec {

class BinanceAdapter final : public ExchangeAdapter {
public:
  BinanceAdapter(const std::string& api_key, const std::string& secret_key, bool testnet = false);
  ~BinanceAdapter() override;

  // Place an order, returns execution report
  std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override;

  // Cancel an order, returns execution report
  std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override;

  // Connection management
  bool is_connected() const override;
  void connect() override;
  void disconnect() override;

  // Get adapter info
  [[nodiscard]] const char* name() const override;
  [[nodiscard]] const char* version() const override;

  // REST API methods for market data and account info
  std::optional<double> get_current_price(const veloz::common::SymbolId& symbol);
  std::optional<std::map<double, double>> get_order_book(const veloz::common::SymbolId& symbol,
                                                         int depth = 10);
  std::optional<std::vector<std::pair<double, double>>>
  get_recent_trades(const veloz::common::SymbolId& symbol, int limit = 500);
  std::optional<double> get_account_balance(const std::string& asset);

private:
  // Helper methods
  std::string build_signature(const std::string& query_string);
  std::string http_get(const std::string& endpoint, const std::string& params = "");
  std::string http_post(const std::string& endpoint, const std::string& params = "");
  std::string http_delete(const std::string& endpoint, const std::string& params = "");
  std::string format_symbol(const veloz::common::SymbolId& symbol);
  std::string order_side_to_string(OrderSide side);
  std::string order_type_to_string(OrderType type);
  std::string time_in_force_to_string(TimeInForce tif);
  OrderStatus parse_order_status(const std::string& status_str);

  // API key and secret
  std::string api_key_;
  std::string secret_key_;

  // Connection status
  bool connected_;
  bool testnet_;

  // API endpoints
  std::string base_rest_url_;
  std::string base_ws_url_;

  // Last activity time for health check
  std::chrono::steady_clock::time_point last_activity_time_;

  // Rate limiting
  std::map<std::string, std::vector<std::chrono::steady_clock::time_point>> request_timestamps_;
  std::chrono::milliseconds rate_limit_window_;
  int rate_limit_per_window_;

  // Retry configuration
  int max_retries_;
  std::chrono::milliseconds retry_delay_;
};

} // namespace veloz::exec
