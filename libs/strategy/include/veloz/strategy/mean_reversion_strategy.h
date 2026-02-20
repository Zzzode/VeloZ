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
 * @brief Mean Reversion Strategy
 *
 * This strategy implements a statistical mean reversion approach:
 * - Calculates rolling mean and standard deviation of prices
 * - Identifies overbought/oversold conditions based on Z-score
 * - Generates buy signals when price is significantly below mean (oversold)
 * - Generates sell signals when price is significantly above mean (overbought)
 *
 * Trading logic:
 * - BUY when Z-score < -entry_threshold (price below mean by N std devs)
 * - SELL when Z-score > entry_threshold (price above mean by N std devs)
 * - EXIT long when Z-score > exit_threshold or hits stop-loss/take-profit
 * - EXIT short when Z-score < -exit_threshold or hits stop-loss/take-profit
 *
 * Configurable parameters:
 * - lookback_period: Number of periods for mean/std calculation (default: 20)
 * - entry_threshold: Z-score threshold for entry (default: 2.0)
 * - exit_threshold: Z-score threshold for exit (default: 0.5)
 * - position_size: Position size multiplier (default: 1.0)
 * - use_bollinger: Use Bollinger Bands visualization (default: true)
 * - enable_short: Enable short selling (default: false)
 */
class MeanReversionStrategy : public BaseStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit MeanReversionStrategy(const StrategyConfig& config);

  /**
   * @brief Virtual destructor
   */
  ~MeanReversionStrategy() noexcept override = default;

  /**
   * @brief Get strategy type
   * @return StrategyType::MeanReversion
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
  double get_current_mean() const {
    return current_mean_;
  }
  double get_current_std_dev() const {
    return current_std_dev_;
  }
  double get_current_zscore() const {
    return current_zscore_;
  }

private:
  // Price buffer for statistics calculation
  kj::Vector<double> price_buffer_;
  size_t buffer_start_{0}; // Start index for circular buffer

  // Statistical state
  double current_mean_{0.0};
  double current_std_dev_{0.0};
  double current_zscore_{0.0};
  bool stats_initialized_{false};

  // Position tracking
  double entry_price_{0.0};
  double entry_zscore_{0.0};
  double stop_loss_price_{0.0};
  double take_profit_price_{0.0};
  double position_size_{0.0};
  bool in_position_{false};
  exec::OrderSide position_side_{exec::OrderSide::Buy};

  // Strategy parameters
  int lookback_period_;
  double entry_threshold_;
  double exit_threshold_;
  double position_size_multiplier_;
  bool enable_short_;

  // Pending signals
  kj::Vector<exec::PlaceOrderRequest> signals_;

  // Performance metrics
  StrategyMetrics metrics_;

  // Helper methods
  double calculate_mean(kj::ArrayPtr<const double> prices) const;
  double calculate_std_dev(kj::ArrayPtr<const double> prices, double mean) const;
  double calculate_zscore(double price, double mean, double std_dev) const;
  kj::Array<double> get_ordered_prices() const;
  void add_price_to_buffer(double price);
  void update_statistics();
  void check_entry_signals(double price);
  void check_exit_signals(double price);
  void generate_entry_signal(double price, exec::OrderSide side);
  void generate_exit_signal(double price);
  double calculate_position_size() const;
};

/**
 * @brief Strategy factory for Mean Reversion Strategy
 */
class MeanReversionStrategyFactory : public StrategyFactory<MeanReversionStrategy> {
public:
  kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
