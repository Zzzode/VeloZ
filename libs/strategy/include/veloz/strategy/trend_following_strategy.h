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
 * @brief Trend Following Strategy using Moving Average Crossover
 *
 * This strategy implements a classic trend following approach using two moving averages:
 * - Fast MA (short period, e.g., 10 periods)
 * - Slow MA (long period, e.g., 20 periods)
 *
 * Trading signals:
 * - BUY when fast MA crosses above slow MA (golden cross)
 * - SELL when fast MA crosses below slow MA (death cross)
 *
 * Risk management:
 * - Position sizing based on risk_per_trade parameter
 * - Stop-loss and take-profit levels
 * - Maximum position size limit
 *
 * Configurable parameters:
 * - fast_period: Fast MA period (default: 10)
 * - slow_period: Slow MA period (default: 20)
 * - ma_type: MA type - "sma" or "ema" (default: "ema")
 * - position_size: Position size multiplier (default: 1.0)
 * - use_atr_stop: Use ATR-based stop-loss (default: false)
 * - atr_period: ATR period for stop calculation (default: 14)
 * - atr_multiplier: ATR multiplier for stop distance (default: 2.0)
 */
class TrendFollowingStrategy : public BaseStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit TrendFollowingStrategy(const StrategyConfig& config);

  /**
   * @brief Virtual destructor
   */
  ~TrendFollowingStrategy() noexcept override = default;

  /**
   * @brief Get strategy type
   * @return StrategyType::TrendFollowing
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

private:
  // Price buffer for MA calculation
  kj::Vector<double> price_buffer_;
  size_t buffer_start_{0}; // Start index for circular buffer

  // High/Low buffers for ATR calculation
  kj::Vector<double> high_buffer_;
  kj::Vector<double> low_buffer_;

  // MA state
  double prev_fast_ma_{0.0};
  double prev_slow_ma_{0.0};
  bool ma_initialized_{false};

  // ATR state
  double current_atr_{0.0};
  bool atr_initialized_{false};

  // Position tracking (internal, not using Position class)
  double entry_price_{0.0};
  double stop_loss_price_{0.0};
  double take_profit_price_{0.0};
  double position_size_{0.0};
  double position_avg_price_{0.0};
  bool in_position_{false};
  exec::OrderSide position_side_{exec::OrderSide::Buy};

  // Strategy parameters
  int fast_period_;
  int slow_period_;
  bool use_ema_;
  double position_size_multiplier_;
  bool use_atr_stop_;
  int atr_period_;
  double atr_multiplier_;

  // Pending signals
  kj::Vector<exec::PlaceOrderRequest> signals_;

  // Performance metrics
  StrategyMetrics metrics_;

  // Helper methods
  double calculate_sma(kj::ArrayPtr<const double> prices, int period) const;
  double calculate_ema(kj::ArrayPtr<const double> prices, int period) const;
  double calculate_atr(kj::ArrayPtr<const double> highs, kj::ArrayPtr<const double> lows,
                       kj::ArrayPtr<const double> closes, int period) const;
  kj::Array<double> get_ordered_prices() const;
  void add_price_to_buffer(double price);
  void add_high_low_to_buffer(double high, double low);
  void check_stop_loss_take_profit(double current_price);
  void generate_entry_signal(double price, exec::OrderSide side);
  void generate_exit_signal(double price);
  double calculate_position_size() const;
  double get_position_size() const;
};

/**
 * @brief Strategy factory for Trend Following Strategy
 */
class TrendFollowingStrategyFactory : public StrategyFactory<TrendFollowingStrategy> {
public:
  kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
