#include "veloz/risk/risk_engine.h"
#include <cmath>
#include <sstream>

namespace veloz::risk {

RiskCheckResult RiskEngine::check_pre_trade(const veloz::exec::PlaceOrderRequest& req) {
  // Check available funds
  if (!check_available_funds(req)) {
    return {false, "Insufficient funds"};
  }

  // Check max position
  if (!check_max_position(req)) {
    return {false, "Order size exceeds max position"};
  }

  // Check price deviation
  if (!check_price_deviation(req)) {
    return {false, "Price deviation exceeds max"};
  }

  return {true, ""};
}

void RiskEngine::set_account_balance(double balance_usdt) {
  account_balance_ = balance_usdt;
}

void RiskEngine::set_max_position_size(double max_size) {
  max_position_size_ = max_size;
}

void RiskEngine::set_max_leverage(double max_leverage) {
  max_leverage_ = max_leverage;
}

void RiskEngine::set_reference_price(double price) {
  reference_price_ = price;
}

void RiskEngine::set_max_price_deviation(double deviation) {
  max_price_deviation_ = deviation;
}

bool RiskEngine::check_available_funds(const veloz::exec::PlaceOrderRequest& req) const {
  if (!req.price.has_value()) {
    return true;  // Market orders checked elsewhere
  }

  double notional = req.qty * req.price.value();
  return notional <= account_balance_;
}

bool RiskEngine::check_max_position(const veloz::exec::PlaceOrderRequest& req) const {
  if (max_position_size_ <= 0.0) {
    return true;  // No limit
  }
  return req.qty <= max_position_size_;
}

bool RiskEngine::check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const {
  if (reference_price_ <= 0.0 || !req.price.has_value()) {
    return true;  // No reference price or market order
  }

  double deviation = std::abs((req.price.value() - reference_price_) / reference_price_);
  return deviation <= max_price_deviation_;
}

} // namespace veloz::risk
