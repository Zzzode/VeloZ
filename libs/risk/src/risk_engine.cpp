#include "veloz/risk/risk_engine.h"
#include <cmath>
#include <sstream>

namespace veloz::risk {

RiskCheckResult RiskEngine::check_pre_trade(const veloz::exec::PlaceOrderRequest& req) {
  // Check circuit breaker
  if (is_circuit_breaker_tripped()) {
    auto now = std::chrono::steady_clock::now();
    if (now < circuit_breaker_reset_time_) {
      return {false, "Circuit breaker tripped"};
    }
    circuit_breaker_tripped_ = false;
  }

  // Check order rate
  if (!check_order_rate()) {
    circuit_breaker_tripped_ = true;
    circuit_breaker_reset_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    return {false, "Order rate limit exceeded"};
  }

  // Check order size
  if (!check_order_size(req)) {
    return {false, "Order size exceeds limit"};
  }

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

  // Record order timestamp
  order_timestamps_.push_back(std::chrono::steady_clock::now());

  return {true, ""};
}

RiskCheckResult RiskEngine::check_post_trade(const veloz::oms::Position& position) {
  // Check stop loss
  if (stop_loss_enabled_ && !check_stop_loss(position)) {
    return {false, "Stop loss triggered"};
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

void RiskEngine::set_max_order_rate(int orders_per_second) {
  max_order_rate_ = orders_per_second;
}

void RiskEngine::set_max_order_size(double max_qty_per_order) {
  max_order_size_ = max_qty_per_order;
}

void RiskEngine::set_stop_loss_enabled(bool enabled) {
  stop_loss_enabled_ = enabled;
}

void RiskEngine::set_stop_loss_percentage(double percentage) {
  stop_loss_percentage_ = percentage;
}

void RiskEngine::update_position(const veloz::oms::Position& position) {
  positions_[position.symbol().value] = position;
}

void RiskEngine::clear_positions() {
  positions_.clear();
}

bool RiskEngine::is_circuit_breaker_tripped() const {
  return circuit_breaker_tripped_;
}

void RiskEngine::reset_circuit_breaker() {
  circuit_breaker_tripped_ = false;
}

bool RiskEngine::check_available_funds(const veloz::exec::PlaceOrderRequest& req) const {
  if (!req.price.has_value()) {
    return true;  // Market orders checked elsewhere
  }

  double notional = req.qty * req.price.value();
  double required_margin = notional / max_leverage_;
  return required_margin <= account_balance_;
}

bool RiskEngine::check_max_position(const veloz::exec::PlaceOrderRequest& req) const {
  if (max_position_size_ <= 0.0) {
    return true;  // No limit
  }

  auto it = positions_.find(req.symbol.value);
  double current_size = (it != positions_.end()) ? std::abs(it->second.size()) : 0.0;
  return (current_size + req.qty) <= max_position_size_;
}

bool RiskEngine::check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const {
  if (reference_price_ <= 0.0 || !req.price.has_value()) {
    return true;  // No reference price or market order
  }

  double deviation = std::abs((req.price.value() - reference_price_) / reference_price_);
  return deviation <= max_price_deviation_;
}

bool RiskEngine::check_order_rate() const {
  auto now = std::chrono::steady_clock::now();
  auto one_second_ago = now - std::chrono::seconds(1);

  int count = 0;
  for (auto& ts : order_timestamps_) {
    if (ts > one_second_ago) {
      count++;
    }
  }

  return count < max_order_rate_;
}

bool RiskEngine::check_order_size(const veloz::exec::PlaceOrderRequest& req) const {
  return req.qty <= max_order_size_;
}

bool RiskEngine::check_stop_loss(const veloz::oms::Position& position) const {
  if (std::abs(position.size()) <= 1e-9) {
    return true;
  }

  double current_pnl_percentage = (position.unrealized_pnl(reference_price_) /
                                   (std::abs(position.size()) * position.avg_price())) * 100;

  if (position.side() == veloz::oms::PositionSide::Long) {
    return current_pnl_percentage > -stop_loss_percentage_;
  } else {
    return current_pnl_percentage > -stop_loss_percentage_;
  }
}

} // namespace veloz::risk
