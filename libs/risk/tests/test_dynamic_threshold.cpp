#include "kj/test.h"
#include "veloz/risk/dynamic_threshold.h"
#include "veloz/risk/risk_engine.h"

namespace {

using namespace veloz::risk;

// ============================================================================
// DynamicThresholdController Tests
// ============================================================================

KJ_TEST("DynamicThresholdController: Default config returns base values") {
  DynamicThresholdController controller;

  // With default config (all zeros), should return base values
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.base_max_leverage = 3.0;
  config.base_stop_loss_pct = 0.05;
  controller.set_config(config);

  KJ_EXPECT(controller.get_max_position_size() == 100.0);
  KJ_EXPECT(controller.get_max_leverage() == 3.0);
  KJ_EXPECT(controller.get_stop_loss_pct() == 0.05);
}

KJ_TEST("DynamicThresholdController: Constructor with config") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 50.0;
  config.base_max_leverage = 2.0;
  config.base_stop_loss_pct = 0.03;

  DynamicThresholdController controller(config);

  KJ_EXPECT(controller.config().base_max_position_size == 50.0);
  KJ_EXPECT(controller.config().base_max_leverage == 2.0);
  KJ_EXPECT(controller.config().base_stop_loss_pct == 0.03);
}

KJ_TEST("DynamicThresholdController: High volatility reduces position size") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.vol_scale_factor = 0.5;

  DynamicThresholdController controller(config);

  // Normal volatility (50th percentile) - no reduction
  MarketConditionState normal_state;
  normal_state.volatility_percentile = 50.0;
  controller.update_market_condition(normal_state);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // High volatility (100th percentile) - max reduction
  MarketConditionState high_vol_state;
  high_vol_state.volatility_percentile = 100.0;
  controller.update_market_condition(high_vol_state);
  double multiplier = controller.get_position_size_multiplier();
  KJ_EXPECT(multiplier < 1.0);
  KJ_EXPECT(multiplier >= 0.5); // Should be reduced by vol_scale_factor
}

KJ_TEST("DynamicThresholdController: Drawdown reduces position size") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.drawdown_reduction_start = 0.05; // Start reducing at 5%
  config.drawdown_reduction_rate = 2.0;   // 2x reduction per % of drawdown

  DynamicThresholdController controller(config);

  // No drawdown - no reduction
  controller.update_current_drawdown(0.0);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // Below threshold - no reduction
  controller.update_current_drawdown(0.04);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // Above threshold - should reduce
  controller.update_current_drawdown(0.10); // 10% drawdown
  double multiplier = controller.get_position_size_multiplier();
  KJ_EXPECT(multiplier < 1.0);
}

KJ_TEST("DynamicThresholdController: Time to close reduces position") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.reduce_before_close = true;
  config.minutes_before_close = 30;

  DynamicThresholdController controller(config);

  // Far from close - no reduction
  controller.update_time_to_close(60);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // At threshold - no reduction yet
  controller.update_time_to_close(30);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // Inside window - should reduce
  controller.update_time_to_close(15);
  double multiplier = controller.get_position_size_multiplier();
  KJ_EXPECT(multiplier < 1.0);
  KJ_EXPECT(multiplier >= 0.5);

  // At close - maximum reduction
  controller.update_time_to_close(0);
  multiplier = controller.get_position_size_multiplier();
  KJ_EXPECT(multiplier == 0.5);
}

KJ_TEST("DynamicThresholdController: Market condition affects position") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;

  DynamicThresholdController controller(config);

  // Normal condition
  MarketConditionState normal;
  normal.condition = MarketCondition::Normal;
  controller.update_market_condition(normal);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);

  // High volatility condition
  MarketConditionState high_vol;
  high_vol.condition = MarketCondition::HighVolatility;
  controller.update_market_condition(high_vol);
  KJ_EXPECT(controller.get_position_size_multiplier() == 0.8);

  // Low liquidity condition
  MarketConditionState low_liq;
  low_liq.condition = MarketCondition::LowLiquidity;
  controller.update_market_condition(low_liq);
  KJ_EXPECT(controller.get_position_size_multiplier() == 0.7);

  // Crisis condition
  MarketConditionState crisis;
  crisis.condition = MarketCondition::Crisis;
  controller.update_market_condition(crisis);
  KJ_EXPECT(controller.get_position_size_multiplier() == 0.3);
}

KJ_TEST("DynamicThresholdController: Leverage reduced in high volatility") {
  DynamicThresholdConfig config;
  config.base_max_leverage = 5.0;
  config.vol_scale_factor = 0.5;

  DynamicThresholdController controller(config);

  // Normal volatility
  MarketConditionState normal;
  normal.volatility_percentile = 50.0;
  controller.update_market_condition(normal);
  KJ_EXPECT(controller.get_max_leverage() == 5.0);

  // High volatility
  MarketConditionState high_vol;
  high_vol.volatility_percentile = 100.0;
  controller.update_market_condition(high_vol);
  double leverage = controller.get_max_leverage();
  KJ_EXPECT(leverage < 5.0);
  KJ_EXPECT(leverage >= 1.0); // Minimum leverage is 1.0
}

KJ_TEST("DynamicThresholdController: Stop loss tightened in high volatility") {
  DynamicThresholdConfig config;
  config.base_stop_loss_pct = 0.10; // 10%

  DynamicThresholdController controller(config);

  // Normal volatility
  MarketConditionState normal;
  normal.volatility_percentile = 50.0;
  controller.update_market_condition(normal);
  KJ_EXPECT(controller.get_stop_loss_pct() == 0.10);

  // High volatility - stop loss should be tighter (smaller %)
  MarketConditionState high_vol;
  high_vol.volatility_percentile = 100.0;
  controller.update_market_condition(high_vol);
  double stop_loss = controller.get_stop_loss_pct();
  KJ_EXPECT(stop_loss < 0.10);
  KJ_EXPECT(stop_loss >= 0.01); // Minimum 1%
}

KJ_TEST("DynamicThresholdController: Explain adjustments") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.vol_scale_factor = 0.5;
  config.drawdown_reduction_start = 0.05;
  config.drawdown_reduction_rate = 2.0;

  DynamicThresholdController controller(config);

  // No adjustments
  kj::String explanation = controller.explain_adjustments();
  KJ_EXPECT(explanation.size() > 0);

  // With high volatility
  MarketConditionState high_vol;
  high_vol.volatility_percentile = 80.0;
  controller.update_market_condition(high_vol);
  explanation = controller.explain_adjustments();
  KJ_EXPECT(explanation.size() > 0);

  // With drawdown
  controller.update_current_drawdown(0.10);
  explanation = controller.explain_adjustments();
  KJ_EXPECT(explanation.size() > 0);
}

KJ_TEST("DynamicThresholdController: Reset clears state") {
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.vol_scale_factor = 0.5;

  DynamicThresholdController controller(config);

  // Set some state
  MarketConditionState high_vol;
  high_vol.condition = MarketCondition::Crisis;
  high_vol.volatility_percentile = 100.0;
  controller.update_market_condition(high_vol);
  controller.update_current_drawdown(0.20);
  controller.update_time_to_close(5);

  // Verify state affects output
  KJ_EXPECT(controller.get_position_size_multiplier() < 1.0);

  // Reset
  controller.reset();

  // Should be back to normal
  KJ_EXPECT(controller.get_market_condition() == MarketCondition::Normal);
  KJ_EXPECT(controller.get_position_size_multiplier() == 1.0);
}

KJ_TEST("DynamicThresholdController: Get market state") {
  DynamicThresholdController controller;

  MarketConditionState state;
  state.condition = MarketCondition::Trending;
  state.volatility_percentile = 65.0;
  state.liquidity_score = 0.8;
  state.trend_strength = 45.0;
  state.last_update_ns = 123456789;

  controller.update_market_condition(state);

  const auto& retrieved = controller.get_market_state();
  KJ_EXPECT(retrieved.condition == MarketCondition::Trending);
  KJ_EXPECT(retrieved.volatility_percentile == 65.0);
  KJ_EXPECT(retrieved.liquidity_score == 0.8);
  KJ_EXPECT(retrieved.trend_strength == 45.0);
  KJ_EXPECT(retrieved.last_update_ns == 123456789);
}

KJ_TEST("market_condition_to_string: All conditions") {
  KJ_EXPECT(market_condition_to_string(MarketCondition::Normal) == "Normal"_kj);
  KJ_EXPECT(market_condition_to_string(MarketCondition::HighVolatility) == "HighVolatility"_kj);
  KJ_EXPECT(market_condition_to_string(MarketCondition::LowLiquidity) == "LowLiquidity"_kj);
  KJ_EXPECT(market_condition_to_string(MarketCondition::Trending) == "Trending"_kj);
  KJ_EXPECT(market_condition_to_string(MarketCondition::MeanReverting) == "MeanReverting"_kj);
  KJ_EXPECT(market_condition_to_string(MarketCondition::Crisis) == "Crisis"_kj);
}

// ============================================================================
// RiskEngine Integration Tests
// ============================================================================

KJ_TEST("RiskEngine: Set dynamic threshold controller") {
  RiskEngine engine;

  KJ_EXPECT(!engine.has_dynamic_thresholds());

  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.base_max_leverage = 3.0;
  config.base_stop_loss_pct = 0.05;

  auto controller = kj::heap<DynamicThresholdController>(config);
  engine.set_dynamic_threshold_controller(kj::mv(controller));

  KJ_EXPECT(engine.has_dynamic_thresholds());
  KJ_EXPECT(engine.get_dynamic_threshold_controller() != nullptr);
}

KJ_TEST("RiskEngine: Effective thresholds use dynamic controller") {
  RiskEngine engine;

  // Set static thresholds
  engine.set_max_position_size(50.0);
  engine.set_max_leverage(2.0);
  engine.set_stop_loss_percentage(0.03);

  // Without dynamic controller, should use static values
  KJ_EXPECT(engine.get_effective_max_position_size() == 50.0);
  KJ_EXPECT(engine.get_effective_max_leverage() == 2.0);
  KJ_EXPECT(engine.get_effective_stop_loss_pct() == 0.03);

  // Add dynamic controller with different base values
  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.base_max_leverage = 5.0;
  config.base_stop_loss_pct = 0.10;

  auto controller = kj::heap<DynamicThresholdController>(config);
  engine.set_dynamic_threshold_controller(kj::mv(controller));

  // Should now use dynamic values
  KJ_EXPECT(engine.get_effective_max_position_size() == 100.0);
  KJ_EXPECT(engine.get_effective_max_leverage() == 5.0);
  KJ_EXPECT(engine.get_effective_stop_loss_pct() == 0.10);
}

KJ_TEST("RiskEngine: Update market condition propagates to controller") {
  RiskEngine engine;

  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;
  config.vol_scale_factor = 0.5;

  auto controller = kj::heap<DynamicThresholdController>(config);
  engine.set_dynamic_threshold_controller(kj::mv(controller));

  // Update market condition through engine
  MarketConditionState state;
  state.condition = MarketCondition::Crisis;
  state.volatility_percentile = 95.0;
  engine.update_market_condition(state);

  // Controller should reflect the update
  auto* ctrl = engine.get_dynamic_threshold_controller();
  KJ_ASSERT(ctrl != nullptr);
  KJ_EXPECT(ctrl->get_market_condition() == MarketCondition::Crisis);
  KJ_EXPECT(ctrl->get_position_size_multiplier() < 1.0);
}

KJ_TEST("RiskEngine: Const access to dynamic controller") {
  RiskEngine engine;

  DynamicThresholdConfig config;
  config.base_max_position_size = 100.0;

  auto controller = kj::heap<DynamicThresholdController>(config);
  engine.set_dynamic_threshold_controller(kj::mv(controller));

  const RiskEngine& const_engine = engine;
  const auto* ctrl = const_engine.get_dynamic_threshold_controller();
  KJ_EXPECT(ctrl != nullptr);
  KJ_EXPECT(ctrl->get_max_position_size() == 100.0);
}

} // namespace
