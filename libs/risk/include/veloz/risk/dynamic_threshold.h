#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief Market condition enumeration
 *
 * Represents different market states that affect risk thresholds.
 */
enum class MarketCondition : std::uint8_t {
  Normal = 0,         ///< Normal market conditions
  HighVolatility = 1, ///< High volatility environment
  LowLiquidity = 2,   ///< Low liquidity environment
  Trending = 3,       ///< Strong trending market
  MeanReverting = 4,  ///< Mean-reverting market
  Crisis = 5,         ///< Crisis/extreme conditions
};

/**
 * @brief Market condition state structure
 *
 * Contains current market condition metrics used for dynamic threshold adjustment.
 * Data is provided by MarketDataManager via callback.
 */
struct MarketConditionState {
  MarketCondition condition{MarketCondition::Normal};
  double volatility_percentile{50.0}; ///< Current vol vs historical (0-100)
  double liquidity_score{1.0};        ///< 0-1, based on spread and depth
  double trend_strength{0.0};         ///< ADX or similar (0-100)
  int64_t last_update_ns{0};          ///< Timestamp of last update
};

/**
 * @brief Dynamic threshold controller configuration
 */
struct DynamicThresholdConfig {
  // Base thresholds (used in Normal conditions)
  double base_max_position_size{100.0};
  double base_max_leverage{3.0};
  double base_stop_loss_pct{0.05};

  // Volatility adjustment
  double vol_scale_factor{0.5};   ///< How much to scale with volatility
  double vol_lookback_days{20.0}; ///< Lookback for volatility calculation

  // Drawdown adjustment
  double drawdown_reduction_start{0.05}; ///< Start reducing at this drawdown %
  double drawdown_reduction_rate{2.0};   ///< Reduction per % of drawdown

  // Time-based adjustment
  bool reduce_before_close{false}; ///< Reduce exposure before market close
  int minutes_before_close{30};    ///< Minutes before close to start reducing
};

/**
 * @brief Dynamic threshold controller
 *
 * Adjusts risk thresholds based on market conditions, drawdown, and time.
 * Provides volatility-based position sizing and adaptive risk controls.
 */
class DynamicThresholdController final {
public:
  DynamicThresholdController() = default;
  explicit DynamicThresholdController(DynamicThresholdConfig config);

  /**
   * @brief Set configuration
   * @param config Configuration settings
   */
  void set_config(DynamicThresholdConfig config);

  /**
   * @brief Get current configuration
   * @return Current configuration
   */
  [[nodiscard]] const DynamicThresholdConfig& config() const;

  // Update market state
  /**
   * @brief Update market condition state
   * @param state Current market condition state
   */
  void update_market_condition(const MarketConditionState& state);

  /**
   * @brief Update current drawdown percentage
   * @param drawdown_pct Current drawdown as percentage (0-1)
   */
  void update_current_drawdown(double drawdown_pct);

  /**
   * @brief Update time to market close
   * @param minutes Minutes until market close (-1 if not applicable)
   */
  void update_time_to_close(int minutes);

  // Get adjusted thresholds
  /**
   * @brief Get adjusted maximum position size
   * @return Adjusted max position size
   */
  [[nodiscard]] double get_max_position_size() const;

  /**
   * @brief Get adjusted maximum leverage
   * @return Adjusted max leverage
   */
  [[nodiscard]] double get_max_leverage() const;

  /**
   * @brief Get adjusted stop loss percentage
   * @return Adjusted stop loss percentage
   */
  [[nodiscard]] double get_stop_loss_pct() const;

  /**
   * @brief Get position size multiplier
   *
   * Returns a multiplier (0-1) that should be applied to position sizes
   * based on current market conditions and risk state.
   *
   * @return Position size multiplier
   */
  [[nodiscard]] double get_position_size_multiplier() const;

  /**
   * @brief Get current market condition
   * @return Current market condition
   */
  [[nodiscard]] MarketCondition get_market_condition() const;

  /**
   * @brief Get current market condition state
   * @return Current market condition state
   */
  [[nodiscard]] const MarketConditionState& get_market_state() const;

  /**
   * @brief Explain current adjustments
   *
   * Returns a human-readable explanation of why thresholds are adjusted.
   *
   * @return Explanation string
   */
  [[nodiscard]] kj::String explain_adjustments() const;

  /**
   * @brief Reset to default state
   */
  void reset();

private:
  [[nodiscard]] double calculate_volatility_adjustment() const;
  [[nodiscard]] double calculate_drawdown_adjustment() const;
  [[nodiscard]] double calculate_time_adjustment() const;
  [[nodiscard]] double calculate_condition_adjustment() const;

  DynamicThresholdConfig config_;
  MarketConditionState market_state_;
  double current_drawdown_{0.0};
  int minutes_to_close_{-1};
};

/**
 * @brief Convert MarketCondition to string
 * @param condition Market condition
 * @return String representation
 */
[[nodiscard]] kj::StringPtr market_condition_to_string(MarketCondition condition);

} // namespace veloz::risk
