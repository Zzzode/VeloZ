#pragma once

#include "veloz/exec/order_api.h"

#include <string>

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

  // Configuration
  void set_account_balance(double balance_usdt);
  void set_max_position_size(double max_size);
  void set_max_leverage(double max_leverage);
  void set_reference_price(double price);
  void set_max_price_deviation(double deviation);

private:
  [[nodiscard]] bool check_available_funds(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_max_position(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const;

  double account_balance_{0.0};
  double max_position_size_{0.0};
  double max_leverage_{1.0};
  double reference_price_{0.0};
  double max_price_deviation_{0.1};  // 10% default
};

} // namespace veloz::risk
