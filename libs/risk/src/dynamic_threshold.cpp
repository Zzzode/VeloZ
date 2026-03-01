#include "veloz/risk/dynamic_threshold.h"

#include <algorithm>
#include <cmath>

namespace veloz::risk {

DynamicThresholdController::DynamicThresholdController(DynamicThresholdConfig config)
    : config_(kj::mv(config)) {}

void DynamicThresholdController::set_config(DynamicThresholdConfig config) {
  config_ = kj::mv(config);
}

const DynamicThresholdConfig& DynamicThresholdController::config() const {
  return config_;
}

void DynamicThresholdController::update_market_condition(const MarketConditionState& state) {
  market_state_ = state;
}

void DynamicThresholdController::update_current_drawdown(double drawdown_pct) {
  current_drawdown_ = std::max(0.0, std::min(1.0, drawdown_pct));
}

void DynamicThresholdController::update_time_to_close(int minutes) {
  minutes_to_close_ = minutes;
}

double DynamicThresholdController::get_max_position_size() const {
  double base = config_.base_max_position_size;
  double multiplier = get_position_size_multiplier();
  return base * multiplier;
}

double DynamicThresholdController::get_max_leverage() const {
  double base = config_.base_max_leverage;

  // Reduce leverage in high volatility
  double vol_adj = calculate_volatility_adjustment();

  // Reduce leverage during drawdown
  double dd_adj = calculate_drawdown_adjustment();

  // Reduce leverage based on market condition
  double cond_adj = calculate_condition_adjustment();

  // Apply all adjustments (multiplicative)
  double adjusted = base * vol_adj * dd_adj * cond_adj;

  // Ensure minimum leverage of 1.0
  return std::max(1.0, adjusted);
}

double DynamicThresholdController::get_stop_loss_pct() const {
  double base = config_.base_stop_loss_pct;

  // Tighten stop loss in high volatility (smaller percentage = tighter)
  // In high vol, we want tighter stops, so we reduce the percentage
  if (market_state_.volatility_percentile > 80.0) {
    // High volatility: tighten stop loss by up to 50%
    double vol_factor = (market_state_.volatility_percentile - 80.0) / 40.0; // 0 to 0.5
    base *= (1.0 - vol_factor * 0.5);
  }

  // During drawdown, tighten stops further
  if (current_drawdown_ > config_.drawdown_reduction_start) {
    double excess_dd = current_drawdown_ - config_.drawdown_reduction_start;
    double dd_factor = std::min(0.5, excess_dd * 2.0); // Max 50% tightening
    base *= (1.0 - dd_factor);
  }

  // Ensure minimum stop loss of 1%
  return std::max(0.01, base);
}

double DynamicThresholdController::get_position_size_multiplier() const {
  double vol_adj = calculate_volatility_adjustment();
  double dd_adj = calculate_drawdown_adjustment();
  double time_adj = calculate_time_adjustment();
  double cond_adj = calculate_condition_adjustment();

  // Combine all adjustments (multiplicative)
  double multiplier = vol_adj * dd_adj * time_adj * cond_adj;

  // Clamp to reasonable range
  return std::max(0.1, std::min(1.0, multiplier));
}

MarketCondition DynamicThresholdController::get_market_condition() const {
  return market_state_.condition;
}

const MarketConditionState& DynamicThresholdController::get_market_state() const {
  return market_state_;
}

kj::String DynamicThresholdController::explain_adjustments() const {
  kj::Vector<kj::String> explanations;

  double vol_adj = calculate_volatility_adjustment();
  double dd_adj = calculate_drawdown_adjustment();
  double time_adj = calculate_time_adjustment();
  double cond_adj = calculate_condition_adjustment();

  // Volatility explanation
  if (vol_adj < 1.0) {
    explanations.add(
        kj::str("Volatility at ", static_cast<int>(market_state_.volatility_percentile),
                "th percentile: position reduced to ", static_cast<int>(vol_adj * 100), "%"));
  }

  // Drawdown explanation
  if (dd_adj < 1.0) {
    explanations.add(kj::str("Drawdown at ", static_cast<int>(current_drawdown_ * 100),
                             "%: position reduced to ", static_cast<int>(dd_adj * 100), "%"));
  }

  // Time explanation
  if (time_adj < 1.0) {
    explanations.add(kj::str(minutes_to_close_, " minutes to close: position reduced to ",
                             static_cast<int>(time_adj * 100), "%"));
  }

  // Market condition explanation
  if (cond_adj < 1.0) {
    explanations.add(kj::str("Market condition (",
                             market_condition_to_string(market_state_.condition),
                             "): position reduced to ", static_cast<int>(cond_adj * 100), "%"));
  }

  if (explanations.size() == 0) {
    return kj::str("No adjustments active - operating at base thresholds");
  }

  // Join explanations
  kj::String result = kj::str("");
  for (size_t i = 0; i < explanations.size(); ++i) {
    if (i > 0) {
      result = kj::str(kj::mv(result), "; ");
    }
    result = kj::str(kj::mv(result), explanations[i]);
  }

  return result;
}

void DynamicThresholdController::reset() {
  market_state_ = MarketConditionState{};
  current_drawdown_ = 0.0;
  minutes_to_close_ = -1;
}

double DynamicThresholdController::calculate_volatility_adjustment() const {
  // Scale position inversely with volatility
  // At 50th percentile (normal), adjustment = 1.0
  // At 100th percentile (extreme), adjustment = 1.0 - vol_scale_factor

  if (market_state_.volatility_percentile <= 50.0) {
    return 1.0; // No reduction for below-average volatility
  }

  // Linear reduction from 50th to 100th percentile
  double excess_vol = (market_state_.volatility_percentile - 50.0) / 50.0; // 0 to 1
  double reduction = excess_vol * config_.vol_scale_factor;

  return std::max(0.1, 1.0 - reduction);
}

double DynamicThresholdController::calculate_drawdown_adjustment() const {
  if (current_drawdown_ <= config_.drawdown_reduction_start) {
    return 1.0; // No reduction below threshold
  }

  // Linear reduction based on excess drawdown
  double excess_dd = current_drawdown_ - config_.drawdown_reduction_start;
  double reduction = excess_dd * config_.drawdown_reduction_rate;

  return std::max(0.1, 1.0 - reduction);
}

double DynamicThresholdController::calculate_time_adjustment() const {
  if (!config_.reduce_before_close || minutes_to_close_ < 0) {
    return 1.0; // No time-based adjustment
  }

  if (minutes_to_close_ >= config_.minutes_before_close) {
    return 1.0; // Not yet in reduction window
  }

  // Linear reduction as we approach close
  double time_factor = static_cast<double>(minutes_to_close_) / config_.minutes_before_close;

  // Reduce to 50% at close
  return 0.5 + (time_factor * 0.5);
}

double DynamicThresholdController::calculate_condition_adjustment() const {
  switch (market_state_.condition) {
  case MarketCondition::Normal:
    return 1.0;

  case MarketCondition::HighVolatility:
    // Already handled by volatility adjustment, but add extra caution
    return 0.8;

  case MarketCondition::LowLiquidity:
    // Reduce position size due to execution risk
    return 0.7;

  case MarketCondition::Trending:
    // Trending markets can support larger positions
    return 1.0;

  case MarketCondition::MeanReverting:
    // Mean-reverting markets are safer for larger positions
    return 1.0;

  case MarketCondition::Crisis:
    // Extreme caution in crisis
    return 0.3;
  }

  return 1.0;
}

kj::StringPtr market_condition_to_string(MarketCondition condition) {
  switch (condition) {
  case MarketCondition::Normal:
    return "Normal"_kj;
  case MarketCondition::HighVolatility:
    return "HighVolatility"_kj;
  case MarketCondition::LowLiquidity:
    return "LowLiquidity"_kj;
  case MarketCondition::Trending:
    return "Trending"_kj;
  case MarketCondition::MeanReverting:
    return "MeanReverting"_kj;
  case MarketCondition::Crisis:
    return "Crisis"_kj;
  }
  return "Unknown"_kj;
}

} // namespace veloz::risk
