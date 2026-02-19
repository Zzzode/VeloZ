/**
 * @file risk_engine.h
 * @brief Core interfaces and implementations for the risk management module
 *
 * This file contains the core interfaces and implementations for the risk management module
 * in the VeloZ quantitative trading framework, including risk checking, risk alerts,
 * risk metrics calculation, and risk control functions.
 *
 * The risk management system is one of the core components of the framework, responsible for
 * risk assessment before and after trades, controlling risk exposure, and providing risk
 * alerts and reporting functionality.
 */

#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"
#include "veloz/risk/risk_metrics.h"

// std::chrono for wall clock timestamps (KJ time is async I/O only)
#include <chrono>
#include <kj/common.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief Risk check result structure
 *
 * Contains the result information of risk checking, including whether trading is allowed and
 * rejection reason.
 */
struct RiskCheckResult {
  bool allowed{true}; ///< Whether trading is allowed
  kj::String reason;  ///< Reason for rejection (if not allowed)
};

/**
 * @brief Risk alert level enumeration
 *
 * Defines different levels of risk alerts, from low risk to critical risk.
 */
enum class RiskLevel {
  Low,     ///< Low risk
  Medium,  ///< Medium risk
  High,    ///< High risk
  Critical ///< Critical risk
};

// KJ hash function for RiskLevel enum to use in kj::HashMap
inline kj::uint KJ_HASHCODE(RiskLevel level) {
  return kj::hashCode(static_cast<int>(level));
}

/**
 * @brief Risk alert information structure
 *
 * Contains detailed information about risk alerts, including alert level, message, timestamp, and
 * trading symbol.
 */
struct RiskAlert {
  RiskLevel level;    ///< Risk alert level
  kj::String message; ///< Alert message
  // std::chrono for wall clock timestamps (KJ time is async I/O only)
  std::chrono::steady_clock::time_point timestamp; ///< Alert timestamp
  kj::String symbol;                               ///< Associated trading symbol
};

/**
 * @brief Risk engine class
 *
 * Responsible for managing and evaluating trading risk, including pre-trade checks, post-trade
 * checks, risk alerts, risk metrics calculation, and risk control functionality.
 */
class RiskEngine final {
public:
  /**
   * @brief Constructor
   */
  RiskEngine() = default;

  // Pre-trade checks
  /**
   * @brief Pre-trade risk check
   * @param req Place order request
   * @return Risk check result
   */
  [[nodiscard]] RiskCheckResult check_pre_trade(const veloz::exec::PlaceOrderRequest& req);

  // Post-trade checks
  /**
   * @brief Post-trade risk check
   * @param position Position information
   * @return Risk check result
   */
  [[nodiscard]] RiskCheckResult check_post_trade(const veloz::oms::Position& position);

  // Configuration
  /**
   * @brief Set account balance
   * @param balance_usdt Account balance in USDT
   */
  void set_account_balance(double balance_usdt);

  /**
   * @brief Set maximum position size
   * @param max_size Maximum position size
   */
  void set_max_position_size(double max_size);

  /**
   * @brief Set maximum leverage
   * @param max_leverage Maximum leverage
   */
  void set_max_leverage(double max_leverage);

  /**
   * @brief Set reference price
   * @param price Reference price
   */
  void set_reference_price(double price);

  /**
   * @brief Set maximum price deviation
   * @param deviation Maximum price deviation (ratio)
   */
  void set_max_price_deviation(double deviation);

  /**
   * @brief Set maximum order rate
   * @param orders_per_second Maximum orders per second
   */
  void set_max_order_rate(int orders_per_second);

  /**
   * @brief Set maximum order size
   * @param max_qty_per_order Maximum quantity per order
   */
  void set_max_order_size(double max_qty_per_order);

  /**
   * @brief Set whether stop loss is enabled
   * @param enabled Whether to enable stop loss
   */
  void set_stop_loss_enabled(bool enabled);

  /**
   * @brief Set stop loss percentage
   * @param percentage Stop loss percentage (0-1)
   */
  void set_stop_loss_percentage(double percentage);

  /**
   * @brief Set whether take profit is enabled
   * @param enabled Whether to enable take profit
   */
  void set_take_profit_enabled(bool enabled);

  /**
   * @brief Set take profit percentage
   * @param percentage Take profit percentage (0-1)
   */
  void set_take_profit_percentage(double percentage);

  /**
   * @brief Set risk level threshold
   * @param level Risk level
   * @param threshold Threshold value
   */
  void set_risk_level_threshold(RiskLevel level, double threshold);

  // Position management
  /**
   * @brief Update position information
   * @param position Position information
   */
  void update_position(const veloz::oms::Position& position);

  /**
   * @brief Clear all position information
   */
  void clear_positions();

  // Circuit breaker
  /**
   * @brief Check if circuit breaker is tripped
   * @return Whether circuit breaker is tripped
   */
  [[nodiscard]] bool is_circuit_breaker_tripped() const;

  /**
   * @brief Reset circuit breaker
   */
  void reset_circuit_breaker();

  // Risk alerts
  /**
   * @brief Get risk alerts list
   * @return Risk alerts list
   */
  kj::Vector<RiskAlert> get_risk_alerts() const;

  /**
   * @brief Clear risk alerts list
   */
  void clear_risk_alerts();

  /**
   * @brief Add risk alert
   * @param level Risk level
   * @param message Alert message
   * @param symbol Associated trading symbol (optional)
   */
  void add_risk_alert(RiskLevel level, kj::StringPtr message, kj::StringPtr symbol = ""_kj);

  // Risk metrics
  /**
   * @brief Set risk metrics calculator
   * @param calculator Risk metrics calculator (moved)
   */
  void set_risk_metrics_calculator(RiskMetricsCalculator calculator);

  /**
   * @brief Get risk metrics
   * @return Risk metrics
   */
  RiskMetrics get_risk_metrics() const;

  /**
   * @brief Calculate risk metrics
   */
  void calculate_risk_metrics();

  // Position management and fund allocation
  /**
   * @brief Calculate position size
   * @param notional Notional value
   * @param leverage Leverage multiplier
   * @return Position size
   */
  [[nodiscard]] double calculate_position_size(double notional, double leverage) const;

  /**
   * @brief Calculate margin requirement
   * @param notional Notional value
   * @param leverage Leverage multiplier
   * @return Margin requirement
   */
  [[nodiscard]] double calculate_margin_requirement(double notional, double leverage) const;

  /**
   * @brief Calculate available funds
   * @return Available funds
   */
  [[nodiscard]] double calculate_available_funds() const;

  /**
   * @brief Calculate used margin
   * @return Used margin
   */
  [[nodiscard]] double calculate_used_margin() const;

private:
  // Pre-trade check helpers
  [[nodiscard]] bool check_available_funds(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_max_position(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_order_rate() const;
  [[nodiscard]] bool check_order_size(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_stop_loss(const veloz::oms::Position& position) const;
  [[nodiscard]] bool check_take_profit(const veloz::oms::Position& position) const;

  // Risk assessment
  void assess_risk_level();
  RiskLevel get_risk_level() const;

  double account_balance_{0.0};
  double max_position_size_{0.0};
  double max_leverage_{1.0};
  double reference_price_{0.0};
  double max_price_deviation_{0.1}; // 10% default
  int max_order_rate_{100};         // orders per second
  double max_order_size_{1000.0};   // max quantity per order
  bool stop_loss_enabled_{false};
  double stop_loss_percentage_{0.05}; // 5% default
  bool take_profit_enabled_{false};
  double take_profit_percentage_{0.1}; // 10% default

  kj::HashMap<kj::String, veloz::oms::Position> positions_;
  // std::chrono for wall clock timestamps (KJ time is async I/O only)
  kj::Vector<std::chrono::steady_clock::time_point> order_timestamps_;
  bool circuit_breaker_tripped_{false};
  std::chrono::steady_clock::time_point circuit_breaker_reset_time_;

  kj::Vector<RiskAlert> risk_alerts_;
  kj::HashMap<RiskLevel, double> risk_level_thresholds_;

  RiskMetricsCalculator metrics_calculator_;
};

} // namespace veloz::risk
