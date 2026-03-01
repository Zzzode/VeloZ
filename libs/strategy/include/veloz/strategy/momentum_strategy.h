/**
 * @file momentum_strategy.h
 * @brief Momentum Strategy implementation
 *
 * This file implements a momentum strategy that generates trading signals
 * based on price momentum indicators like Rate of Change (ROC) and
 * Relative Strength Index (RSI).
 */

#pragma once

#include "veloz/strategy/strategy.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::strategy {

/**
 * @brief Momentum Strategy using ROC and RSI indicators
 *
 * This strategy implements a momentum-based trading approach using:
 * - Rate of Change (ROC): Measures price change over a period
 * - Relative Strength Index (RSI): Measures speed and magnitude of price changes
 *
 * Trading signals:
 * - BUY when momentum is positive and RSI is not overbought
 * - SELL when momentum is negative and RSI is not oversold
 * - EXIT long when RSI becomes overbought or momentum turns negative
 * - EXIT short when RSI becomes oversold or momentum turns positive
 *
 * Risk management:
 * - Position sizing based on momentum strength
 * - Stop-loss and take-profit levels
 * - Maximum position size limit
 * - RSI-based overbought/oversold filters
 *
 * Configurable parameters:
 * - roc_period: ROC calculation period (default: 14)
 * - rsi_period: RSI calculation period (default: 14)
 * - rsi_overbought: RSI overbought threshold (default: 70)
 * - rsi_oversold: RSI oversold threshold (default: 30)
 * - momentum_threshold: Minimum ROC for signal (default: 0.02 = 2%)
 * - position_size: Base position size (default: 1.0)
 * - use_rsi_filter: Use RSI as entry filter (default: true)
 * - allow_short: Allow short positions (default: false)
 */
class MomentumStrategy : public BaseStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit MomentumStrategy(const StrategyConfig& config);

  /**
   * @brief Virtual destructor
   */
  ~MomentumStrategy() noexcept override = default;

  /**
   * @brief Get strategy type
   * @return StrategyType::Momentum
   */
  StrategyType get_type() const override;

  /**
   * @brief Handle market event
   * @param event Market event object
   */
  void on_event(const market::MarketEvent& event) override;

  /**
   * @brief Handle timer event
   * @param timestamp Timestamp in milliseconds
   */
  void on_timer(int64_t timestamp) override;

  /**
   * @brief Get trading signals
   * @return List of trading signals
   */
  kj::Vector<exec::PlaceOrderRequest> get_signals() override;

  /**
   * @brief Reset strategy state
   */
  void reset() override;

  /**
   * @brief Get strategy type name (static)
   * @return Strategy type name string
   */
  static kj::StringPtr get_strategy_type();

  /**
   * @brief Check if strategy supports hot-reload of parameters
   * @return true (supports hot-reload)
   */
  bool supports_hot_reload() const override;

  /**
   * @brief Update strategy parameters at runtime
   * @param parameters New parameter values
   * @return Whether update was successful
   */
  bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) override;

  /**
   * @brief Get strategy performance metrics
   * @return Reference to metrics
   */
  kj::Maybe<const StrategyMetrics&> get_metrics() const override;

  // Public getters for testing
  double get_current_roc() const {
    return current_roc_;
  }
  double get_current_rsi() const {
    return current_rsi_;
  }
  double get_last_price() const {
    return last_price_;
  }
  bool is_in_position() const {
    return in_position_;
  }

private:
  // Price buffer for indicator calculation
  kj::Vector<double> price_buffer_;

  // Indicator state
  double current_roc_{0.0};  // Current Rate of Change
  double current_rsi_{50.0}; // Current RSI (neutral default)
  double last_price_{0.0};   // Last processed price
  bool indicators_ready_{false};

  // RSI calculation state
  double avg_gain_{0.0};
  double avg_loss_{0.0};
  bool rsi_initialized_{false};

  // Position tracking
  double entry_price_{0.0};
  double stop_loss_price_{0.0};
  double take_profit_price_{0.0};
  double position_qty_{0.0};
  bool in_position_{false};
  exec::OrderSide position_side_{exec::OrderSide::Buy};

  // Strategy parameters
  int roc_period_;
  int rsi_period_;
  double rsi_overbought_;
  double rsi_oversold_;
  double momentum_threshold_;
  double position_size_;
  bool use_rsi_filter_;
  bool allow_short_;

  // Pending signals
  kj::Vector<exec::PlaceOrderRequest> signals_;

  // Performance metrics
  StrategyMetrics metrics_;

  // Helper methods
  void add_price(double price);
  double calculate_roc() const;
  double calculate_rsi();
  void check_entry_signals(double price);
  void check_exit_signals(double price);
  void generate_entry_signal(double price, exec::OrderSide side);
  void generate_exit_signal(double price);
  double calculate_position_size(double momentum_strength) const;
  void check_stop_loss_take_profit(double current_price);
};

/**
 * @brief Strategy factory for Momentum Strategy
 */
class MomentumStrategyFactory : public StrategyFactory<MomentumStrategy> {
public:
  kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
