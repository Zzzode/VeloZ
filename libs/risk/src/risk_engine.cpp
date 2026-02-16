#include "veloz/risk/risk_engine.h"

#include "veloz/risk/risk_metrics.h"

// std::abs for math operations (standard C++ library)
#include <cmath>

namespace veloz::risk {

RiskCheckResult RiskEngine::check_pre_trade(const veloz::exec::PlaceOrderRequest& req) {
  // Check circuit breaker
  // std::chrono for wall clock timestamps (KJ time is async I/O only)
  if (is_circuit_breaker_tripped()) {
    auto now = std::chrono::steady_clock::now();
    if (now < circuit_breaker_reset_time_) {
      return {false, kj::heapString("Circuit breaker tripped")};
    }
    circuit_breaker_tripped_ = false;
  }

  // Check order rate
  if (!check_order_rate()) {
    circuit_breaker_tripped_ = true;
    circuit_breaker_reset_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    add_risk_alert(RiskLevel::Critical, "Order rate limit exceeded"_kj, kj::StringPtr(req.symbol.value));
    return {false, kj::heapString("Order rate limit exceeded")};
  }

  // Check order size
  if (!check_order_size(req)) {
    add_risk_alert(RiskLevel::High, "Order size exceeds limit"_kj, kj::StringPtr(req.symbol.value));
    return {false, kj::heapString("Order size exceeds limit")};
  }

  // Check available funds
  if (!check_available_funds(req)) {
    add_risk_alert(RiskLevel::Critical, "Insufficient funds for order"_kj, kj::StringPtr(req.symbol.value));
    return {false, kj::heapString("Insufficient funds")};
  }

  // Check max position
  if (!check_max_position(req)) {
    add_risk_alert(RiskLevel::High, "Order size exceeds max position"_kj, kj::StringPtr(req.symbol.value));
    return {false, kj::heapString("Order size exceeds max position")};
  }

  // Check price deviation
  if (!check_price_deviation(req)) {
    add_risk_alert(RiskLevel::Medium, "Price deviation exceeds max"_kj, kj::StringPtr(req.symbol.value));
    return {false, kj::heapString("Price deviation exceeds max")};
  }

  // Record order timestamp
  order_timestamps_.add(std::chrono::steady_clock::now());

  // Assess risk level
  assess_risk_level();

  return {true, kj::heapString("")};
}

RiskCheckResult RiskEngine::check_post_trade(const veloz::oms::Position& position) {
  // Check stop loss
  if (stop_loss_enabled_ && !check_stop_loss(position)) {
    add_risk_alert(RiskLevel::Critical, "Stop loss triggered"_kj, kj::StringPtr(position.symbol().value));
    return {false, kj::heapString("Stop loss triggered")};
  }

  // Check take profit
  if (take_profit_enabled_ && !check_take_profit(position)) {
    add_risk_alert(RiskLevel::High, "Take profit triggered"_kj, kj::StringPtr(position.symbol().value));
    return {false, kj::heapString("Take profit triggered")};
  }

  // Assess risk level
  assess_risk_level();

  return {true, kj::heapString("")};
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

void RiskEngine::set_take_profit_enabled(bool enabled) {
  take_profit_enabled_ = enabled;
}

void RiskEngine::set_take_profit_percentage(double percentage) {
  take_profit_percentage_ = percentage;
}

void RiskEngine::set_risk_level_threshold(RiskLevel level, double threshold) {
  // Use KJ HashMap upsert pattern
  risk_level_thresholds_.upsert(level, threshold);
}

kj::Vector<RiskAlert> RiskEngine::get_risk_alerts() const {
  // Copy alerts to return
  kj::Vector<RiskAlert> result;
  for (const auto& alert : risk_alerts_) {
    RiskAlert copy;
    copy.level = alert.level;
    copy.message = kj::heapString(alert.message);
    copy.timestamp = alert.timestamp;
    copy.symbol = kj::heapString(alert.symbol);
    result.add(kj::mv(copy));
  }
  return result;
}

void RiskEngine::clear_risk_alerts() {
  risk_alerts_.clear();
}

void RiskEngine::add_risk_alert(RiskLevel level, kj::StringPtr message,
                                kj::StringPtr symbol) {
  RiskAlert alert;
  alert.level = level;
  alert.message = kj::heapString(message);
  // std::chrono for wall clock timestamps (KJ time is async I/O only)
  alert.timestamp = std::chrono::steady_clock::now();
  alert.symbol = kj::heapString(symbol);
  risk_alerts_.add(kj::mv(alert));
}

void RiskEngine::set_risk_metrics_calculator(RiskMetricsCalculator calculator) {
  metrics_calculator_ = kj::mv(calculator);
}

RiskMetrics RiskEngine::get_risk_metrics() const {
  return metrics_calculator_.calculate_all();
}

void RiskEngine::calculate_risk_metrics() {
  // Logic for calculating risk metrics can be added here
}

double RiskEngine::calculate_position_size(double notional, double leverage) const {
  return notional / (reference_price_ * leverage);
}

double RiskEngine::calculate_margin_requirement(double notional, double leverage) const {
  return notional / leverage;
}

double RiskEngine::calculate_available_funds() const {
  double used_margin = calculate_used_margin();
  return account_balance_ - used_margin;
}

double RiskEngine::calculate_used_margin() const {
  double total_used_margin = 0.0;

  // KJ HashMap iteration uses Entry with .key and .value
  for (const auto& entry : positions_) {
    const auto& position = entry.value;
    // std::abs for math operations (standard C++ library)
    double notional = std::abs(position.size()) * position.avg_price();
    total_used_margin += notional / max_leverage_;
  }

  return total_used_margin;
}

void RiskEngine::assess_risk_level() {
  // Assess risk level based on risk metrics and configured thresholds
  RiskMetrics metrics = get_risk_metrics();

  // Helper to get threshold value from kj::HashMap
  auto get_threshold = [this](RiskLevel level) -> double {
    KJ_IF_SOME(value, risk_level_thresholds_.find(level)) {
      return value;
    }
    return 0.0; // Default threshold
  };

  // Check VaR
  if (metrics.var_99 > get_threshold(RiskLevel::Critical)) {
    add_risk_alert(RiskLevel::Critical, "VaR 99% exceeds critical threshold"_kj);
  } else if (metrics.var_99 > get_threshold(RiskLevel::High)) {
    add_risk_alert(RiskLevel::High, "VaR 99% exceeds high threshold"_kj);
  } else if (metrics.var_99 > get_threshold(RiskLevel::Medium)) {
    add_risk_alert(RiskLevel::Medium, "VaR 99% exceeds medium threshold"_kj);
  }

  // Check maximum drawdown
  if (metrics.max_drawdown > get_threshold(RiskLevel::Critical)) {
    add_risk_alert(RiskLevel::Critical, "Max drawdown exceeds critical threshold"_kj);
  } else if (metrics.max_drawdown > get_threshold(RiskLevel::High)) {
    add_risk_alert(RiskLevel::High, "Max drawdown exceeds high threshold"_kj);
  } else if (metrics.max_drawdown > get_threshold(RiskLevel::Medium)) {
    add_risk_alert(RiskLevel::Medium, "Max drawdown exceeds medium threshold"_kj);
  }

  // Check Sharpe ratio
  if (metrics.sharpe_ratio < get_threshold(RiskLevel::Critical)) {
    add_risk_alert(RiskLevel::Critical, "Sharpe ratio below critical threshold"_kj);
  } else if (metrics.sharpe_ratio < get_threshold(RiskLevel::High)) {
    add_risk_alert(RiskLevel::High, "Sharpe ratio below high threshold"_kj);
  }
}

RiskLevel RiskEngine::get_risk_level() const {
  RiskMetrics metrics = get_risk_metrics();

  if (metrics.var_99 > 5.0 || metrics.max_drawdown > 20.0 || metrics.sharpe_ratio < 0.5) {
    return RiskLevel::Critical;
  } else if (metrics.var_99 > 3.0 || metrics.max_drawdown > 15.0 || metrics.sharpe_ratio < 1.0) {
    return RiskLevel::High;
  } else if (metrics.var_99 > 2.0 || metrics.max_drawdown > 10.0 || metrics.sharpe_ratio < 1.5) {
    return RiskLevel::Medium;
  }

  return RiskLevel::Low;
}

bool RiskEngine::check_take_profit(const veloz::oms::Position& position) const {
  if (std::abs(position.size()) <= 1e-9) {
    return true;
  }

  double current_pnl_percentage = (position.unrealized_pnl(reference_price_) /
                                   (std::abs(position.size()) * position.avg_price())) *
                                  100;

  if (position.side() == veloz::oms::PositionSide::Long) {
    return current_pnl_percentage < take_profit_percentage_;
  } else {
    return current_pnl_percentage < take_profit_percentage_;
  }
}

void RiskEngine::update_position(const veloz::oms::Position& position) {
  // Use kj::HashMap upsert instead of operator[]
  kj::String symbol_key = kj::str(position.symbol().value);
  positions_.upsert(kj::mv(symbol_key), position, [](veloz::oms::Position& existing, veloz::oms::Position&& replacement) {
    existing = kj::mv(replacement);
  });
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
  KJ_IF_SOME (price, req.price) {
    double notional = req.qty * price;
    double required_margin = notional / max_leverage_;
    return required_margin <= account_balance_;
  } else {
    return true; // Market orders checked elsewhere
  }
}

bool RiskEngine::check_max_position(const veloz::exec::PlaceOrderRequest& req) const {
  if (max_position_size_ <= 0.0) {
    return true; // No limit
  }

  // Use kj::HashMap find with KJ_IF_SOME pattern
  double current_size = 0.0;
  KJ_IF_SOME(position, positions_.find(kj::StringPtr(req.symbol.value))) {
    current_size = std::abs(position.size());
  }
  return (current_size + req.qty) <= max_position_size_;
}

bool RiskEngine::check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const {
  if (reference_price_ <= 0.0) {
    return true; // No reference price
  }

  KJ_IF_SOME (price, req.price) {
    double deviation = std::abs((price - reference_price_) / reference_price_);
    return deviation <= max_price_deviation_;
  } else {
    return true; // Market order
  }
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
                                   (std::abs(position.size()) * position.avg_price())) *
                                  100;

  if (position.side() == veloz::oms::PositionSide::Long) {
    return current_pnl_percentage > -stop_loss_percentage_;
  } else {
    return current_pnl_percentage > -stop_loss_percentage_;
  }
}

} // namespace veloz::risk
