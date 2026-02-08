#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"

#include <string>
#include <unordered_map>
#include <chrono>

namespace veloz::risk {

struct RiskCheckResult {
  bool allowed{true};
  std::string reason;
};

class RiskEngine final {
public:
  RiskEngine() = default;

  // Pre-trade checks
  [[nodiscard]] RiskCheckResult check_pre_trade(const veloz::exec::PlaceOrderRequest& req);

  // Post-trade checks
  [[nodiscard]] RiskCheckResult check_post_trade(const veloz::oms::Position& position);

  // Configuration
  void set_account_balance(double balance_usdt);
  void set_max_position_size(double max_size);
  void set_max_leverage(double max_leverage);
  void set_reference_price(double price);
  void set_max_price_deviation(double deviation);
  void set_max_order_rate(int orders_per_second);
  void set_max_order_size(double max_qty_per_order);
  void set_stop_loss_enabled(bool enabled);
  void set_stop_loss_percentage(double percentage);

  // Position management
  void update_position(const veloz::oms::Position& position);
  void clear_positions();

  // Circuit breaker
  [[nodiscard]] bool is_circuit_breaker_tripped() const;
  void reset_circuit_breaker();

private:
  [[nodiscard]] bool check_available_funds(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_max_position(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_order_rate() const;
  [[nodiscard]] bool check_order_size(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_stop_loss(const veloz::oms::Position& position) const;

  double account_balance_{0.0};
  double max_position_size_{0.0};
  double max_leverage_{1.0};
  double reference_price_{0.0};
  double max_price_deviation_{0.1};  // 10% default
  int max_order_rate_{100};  // orders per second
  double max_order_size_{1000.0};  // max quantity per order
  bool stop_loss_enabled_{false};
  double stop_loss_percentage_{0.05};  // 5% default

  std::unordered_map<std::string, veloz::oms::Position> positions_;
  std::vector<std::chrono::steady_clock::time_point> order_timestamps_;
  bool circuit_breaker_tripped_{false};
  std::chrono::steady_clock::time_point circuit_breaker_reset_time_;
};

} // namespace veloz::risk
