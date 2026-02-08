#include "veloz/risk/risk_engine.h"
#include "veloz/risk/risk_metrics.h"
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
    add_risk_alert(RiskLevel::Critical, "Order rate limit exceeded", req.symbol.value);
    return {false, "Order rate limit exceeded"};
  }

  // Check order size
  if (!check_order_size(req)) {
    add_risk_alert(RiskLevel::High, "Order size exceeds limit", req.symbol.value);
    return {false, "Order size exceeds limit"};
  }

  // Check available funds
  if (!check_available_funds(req)) {
    add_risk_alert(RiskLevel::Critical, "Insufficient funds for order", req.symbol.value);
    return {false, "Insufficient funds"};
  }

  // Check max position
  if (!check_max_position(req)) {
    add_risk_alert(RiskLevel::High, "Order size exceeds max position", req.symbol.value);
    return {false, "Order size exceeds max position"};
  }

  // Check price deviation
  if (!check_price_deviation(req)) {
    add_risk_alert(RiskLevel::Medium, "Price deviation exceeds max", req.symbol.value);
    return {false, "Price deviation exceeds max"};
  }

  // Record order timestamp
  order_timestamps_.push_back(std::chrono::steady_clock::now());

  // Assess risk level
  assess_risk_level();

  return {true, ""};
}

RiskCheckResult RiskEngine::check_post_trade(const veloz::oms::Position& position) {
  // Check stop loss
  if (stop_loss_enabled_ && !check_stop_loss(position)) {
    add_risk_alert(RiskLevel::Critical, "Stop loss triggered", position.symbol().value);
    return {false, "Stop loss triggered"};
  }

  // Check take profit
  if (take_profit_enabled_ && !check_take_profit(position)) {
    add_risk_alert(RiskLevel::High, "Take profit triggered", position.symbol().value);
    return {false, "Take profit triggered"};
  }

  // Assess risk level
  assess_risk_level();

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

void RiskEngine::set_take_profit_enabled(bool enabled) {
  take_profit_enabled_ = enabled;
}

void RiskEngine::set_take_profit_percentage(double percentage) {
  take_profit_percentage_ = percentage;
}

void RiskEngine::set_risk_level_threshold(RiskLevel level, double threshold) {
  risk_level_thresholds_[level] = threshold;
}

std::vector<RiskAlert> RiskEngine::get_risk_alerts() const {
  return risk_alerts_;
}

void RiskEngine::clear_risk_alerts() {
  risk_alerts_.clear();
}

void RiskEngine::add_risk_alert(RiskLevel level, const std::string& message, const std::string& symbol) {
  RiskAlert alert;
  alert.level = level;
  alert.message = message;
  alert.timestamp = std::chrono::steady_clock::now();
  alert.symbol = symbol;
  risk_alerts_.push_back(alert);
}

void RiskEngine::set_risk_metrics_calculator(const RiskMetricsCalculator& calculator) {
  metrics_calculator_ = calculator;
}

RiskMetrics RiskEngine::get_risk_metrics() const {
  return metrics_calculator_.calculate_all();
}

void RiskEngine::calculate_risk_metrics() {
  // 这里可以添加计算风险指标的逻辑
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

  for (const auto& [symbol, position] : positions_) {
    double notional = std::abs(position.size()) * position.avg_price();
    total_used_margin += notional / max_leverage_;
  }

  return total_used_margin;
}

void RiskEngine::assess_risk_level() {
  // 根据风险指标和配置的阈值评估风险级别
  RiskMetrics metrics = get_risk_metrics();

  // 检查VaR
  if (metrics.var_99 > risk_level_thresholds_[RiskLevel::Critical]) {
    add_risk_alert(RiskLevel::Critical, "VaR 99% exceeds critical threshold");
  } else if (metrics.var_99 > risk_level_thresholds_[RiskLevel::High]) {
    add_risk_alert(RiskLevel::High, "VaR 99% exceeds high threshold");
  } else if (metrics.var_99 > risk_level_thresholds_[RiskLevel::Medium]) {
    add_risk_alert(RiskLevel::Medium, "VaR 99% exceeds medium threshold");
  }

  // 检查最大回撤
  if (metrics.max_drawdown > risk_level_thresholds_[RiskLevel::Critical]) {
    add_risk_alert(RiskLevel::Critical, "Max drawdown exceeds critical threshold");
  } else if (metrics.max_drawdown > risk_level_thresholds_[RiskLevel::High]) {
    add_risk_alert(RiskLevel::High, "Max drawdown exceeds high threshold");
  } else if (metrics.max_drawdown > risk_level_thresholds_[RiskLevel::Medium]) {
    add_risk_alert(RiskLevel::Medium, "Max drawdown exceeds medium threshold");
  }

  // 检查夏普比率
  if (metrics.sharpe_ratio < risk_level_thresholds_[RiskLevel::Critical]) {
    add_risk_alert(RiskLevel::Critical, "Sharpe ratio below critical threshold");
  } else if (metrics.sharpe_ratio < risk_level_thresholds_[RiskLevel::High]) {
    add_risk_alert(RiskLevel::High, "Sharpe ratio below high threshold");
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
                                   (std::abs(position.size()) * position.avg_price())) * 100;

  if (position.side() == veloz::oms::PositionSide::Long) {
    return current_pnl_percentage < take_profit_percentage_;
  } else {
    return current_pnl_percentage < take_profit_percentage_;
  }
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
