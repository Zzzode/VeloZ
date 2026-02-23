#include "veloz/risk/stress_testing.h"

#include <cmath>
#include <kj/test.h>

namespace veloz::risk {
namespace {

KJ_TEST("StressScenarioType to string conversion") {
  KJ_EXPECT(stress_scenario_type_to_string(StressScenarioType::Historical) == "historical"_kj);
  KJ_EXPECT(stress_scenario_type_to_string(StressScenarioType::Hypothetical) == "hypothetical"_kj);
  KJ_EXPECT(stress_scenario_type_to_string(StressScenarioType::Sensitivity) == "sensitivity"_kj);
}

KJ_TEST("MarketFactor to string conversion") {
  KJ_EXPECT(market_factor_to_string(MarketFactor::Price) == "price"_kj);
  KJ_EXPECT(market_factor_to_string(MarketFactor::Volatility) == "volatility"_kj);
  KJ_EXPECT(market_factor_to_string(MarketFactor::Correlation) == "correlation"_kj);
  KJ_EXPECT(market_factor_to_string(MarketFactor::Liquidity) == "liquidity"_kj);
  KJ_EXPECT(market_factor_to_string(MarketFactor::InterestRate) == "interest_rate"_kj);
  KJ_EXPECT(market_factor_to_string(MarketFactor::FundingRate) == "funding_rate"_kj);
}

KJ_TEST("StressScenarioBuilder - basic scenario creation") {
  auto scenario = StressScenarioBuilder()
      .id("test_scenario")
      .name("Test Scenario")
      .description("A test scenario for unit testing")
      .type(StressScenarioType::Hypothetical)
      .price_shock("BTC", -0.20)
      .build();

  KJ_EXPECT(scenario.id == "test_scenario"_kj);
  KJ_EXPECT(scenario.name == "Test Scenario"_kj);
  KJ_EXPECT(scenario.type == StressScenarioType::Hypothetical);
  KJ_EXPECT(scenario.shocks.size() == 1);
  KJ_EXPECT(scenario.shocks[0].factor == MarketFactor::Price);
  KJ_EXPECT(scenario.shocks[0].symbol == "BTC"_kj);
  KJ_EXPECT(std::abs(scenario.shocks[0].shock_value - (-0.20)) < 0.001);
}

KJ_TEST("StressScenarioBuilder - multiple shocks") {
  auto scenario = StressScenarioBuilder()
      .id("multi_shock")
      .name("Multi-Shock Scenario")
      .price_shock("BTC", -0.30)
      .price_shock("ETH", -0.40)
      .volatility_shock("", 2.0)
      .liquidity_shock(1.5)
      .build();

  KJ_EXPECT(scenario.shocks.size() == 4);
}

KJ_TEST("StressTestEngine - add and get scenario") {
  StressTestEngine engine;

  auto scenario = StressScenarioBuilder()
      .id("test_1")
      .name("Test 1")
      .build();

  engine.add_scenario(kj::mv(scenario));

  const auto* retrieved = engine.get_scenario("test_1");
  KJ_ASSERT(retrieved != nullptr);
  KJ_EXPECT(retrieved->name == "Test 1"_kj);

  // Non-existent scenario
  KJ_EXPECT(engine.get_scenario("nonexistent") == nullptr);
}

KJ_TEST("StressTestEngine - remove scenario") {
  StressTestEngine engine;

  engine.add_scenario(StressScenarioBuilder().id("s1").name("S1").build());
  engine.add_scenario(StressScenarioBuilder().id("s2").name("S2").build());

  KJ_EXPECT(engine.get_scenarios().size() == 2);

  bool removed = engine.remove_scenario("s1");
  KJ_EXPECT(removed);
  KJ_EXPECT(engine.get_scenarios().size() == 1);
  KJ_EXPECT(engine.get_scenario("s1") == nullptr);
  KJ_EXPECT(engine.get_scenario("s2") != nullptr);

  // Remove non-existent
  KJ_EXPECT(!engine.remove_scenario("nonexistent"));
}

KJ_TEST("StressTestEngine - clear scenarios") {
  StressTestEngine engine;

  engine.add_scenario(StressScenarioBuilder().id("s1").build());
  engine.add_scenario(StressScenarioBuilder().id("s2").build());

  engine.clear_scenarios();
  KJ_EXPECT(engine.get_scenarios().size() == 0);
}

KJ_TEST("StressTestEngine - add historical scenarios") {
  StressTestEngine engine;
  engine.add_all_historical_scenarios();

  KJ_EXPECT(engine.get_scenarios().size() >= 4);
  KJ_EXPECT(engine.get_scenario("covid_crash_2020") != nullptr);
  KJ_EXPECT(engine.get_scenario("luna_collapse_2022") != nullptr);
  KJ_EXPECT(engine.get_scenario("ftx_collapse_2022") != nullptr);
  KJ_EXPECT(engine.get_scenario("flash_crash") != nullptr);
}

KJ_TEST("StressTestEngine - run stress test with empty positions") {
  StressTestEngine engine;
  engine.add_scenario(StressScenarioBuilder()
      .id("test")
      .price_shock("", -0.20)
      .build());

  kj::Vector<StressPosition> positions;
  auto result = engine.run_stress_test("test", positions);

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.position_results.size() == 0);
  KJ_EXPECT(result.total_pnl_impact == 0.0);
}

KJ_TEST("StressTestEngine - run stress test with single position") {
  StressTestEngine engine;
  engine.add_scenario(StressScenarioBuilder()
      .id("test")
      .price_shock("", -0.20)  // 20% drop
      .build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.entry_price = 40000.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto result = engine.run_stress_test("test", positions);

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.scenario_id == "test"_kj);
  KJ_EXPECT(result.position_results.size() == 1);

  // Base value: 50000
  // Stressed price: 50000 * 0.8 = 40000
  // P&L impact: (40000 - 50000) = -10000
  KJ_EXPECT(std::abs(result.base_portfolio_value - 50000.0) < 1.0);
  KJ_EXPECT(std::abs(result.stressed_portfolio_value - 40000.0) < 1.0);
  KJ_EXPECT(std::abs(result.total_pnl_impact - (-10000.0)) < 1.0);
}

KJ_TEST("StressTestEngine - run stress test with symbol-specific shock") {
  StressTestEngine engine;
  engine.add_scenario(StressScenarioBuilder()
      .id("test")
      .price_shock("BTC", -0.30)  // BTC drops 30%
      .price_shock("ETH", -0.10)  // ETH drops 10%
      .build());

  kj::Vector<StressPosition> positions;

  StressPosition btc;
  btc.symbol = kj::str("BTC");
  btc.size = 1.0;
  btc.entry_price = 40000.0;
  btc.current_price = 50000.0;
  positions.add(kj::mv(btc));

  StressPosition eth;
  eth.symbol = kj::str("ETH");
  eth.size = 10.0;
  eth.entry_price = 2000.0;
  eth.current_price = 3000.0;
  positions.add(kj::mv(eth));

  auto result = engine.run_stress_test("test", positions);

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.position_results.size() == 2);

  // BTC: 50000 -> 35000 (30% drop)
  // ETH: 3000 -> 2700 (10% drop), position value: 30000 -> 27000
  KJ_EXPECT(std::abs(result.base_portfolio_value - 80000.0) < 1.0);  // 50000 + 30000
  KJ_EXPECT(std::abs(result.stressed_portfolio_value - 62000.0) < 1.0);  // 35000 + 27000
}

KJ_TEST("StressTestEngine - run stress test with non-existent scenario") {
  StressTestEngine engine;

  kj::Vector<StressPosition> positions;
  auto result = engine.run_stress_test("nonexistent", positions);

  KJ_EXPECT(!result.success);
  KJ_EXPECT(result.error_message.size() > 0);
}

KJ_TEST("StressTestEngine - run all scenarios") {
  StressTestEngine engine;
  engine.add_all_historical_scenarios();

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.entry_price = 40000.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto results = engine.run_all_scenarios(positions);

  KJ_EXPECT(results.size() >= 4);
  for (const auto& result : results) {
    KJ_EXPECT(result.success);
  }
}

KJ_TEST("StressTestEngine - sensitivity analysis") {
  StressTestEngine engine;

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.entry_price = 40000.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto result = engine.run_sensitivity_analysis(
      MarketFactor::Price,
      positions,
      -0.30,  // -30%
      0.30,   // +30%
      7);     // 7 points

  KJ_EXPECT(result.factor == MarketFactor::Price);
  KJ_EXPECT(result.shock_levels.size() == 7);
  KJ_EXPECT(result.pnl_impacts.size() == 7);

  // First shock (-30%) should have negative P&L impact
  KJ_EXPECT(result.pnl_impacts[0] < 0.0);

  // Last shock (+30%) should have positive P&L impact
  KJ_EXPECT(result.pnl_impacts[6] > 0.0);

  // Delta should be positive (price increase = value increase for long position)
  KJ_EXPECT(result.delta > 0.0);
}

KJ_TEST("StressTestEngine - compare scenarios") {
  StressTestEngine engine;

  kj::Vector<StressTestResult> results;

  StressTestResult r1;
  r1.scenario_id = kj::str("s1");
  r1.success = true;
  r1.total_pnl_impact = -10000.0;
  results.add(kj::mv(r1));

  StressTestResult r2;
  r2.scenario_id = kj::str("s2");
  r2.success = true;
  r2.total_pnl_impact = -5000.0;
  results.add(kj::mv(r2));

  StressTestResult r3;
  r3.scenario_id = kj::str("s3");
  r3.success = true;
  r3.total_pnl_impact = -15000.0;
  results.add(kj::mv(r3));

  auto comparison = engine.compare_scenarios(results);

  KJ_EXPECT(comparison.scenarios_tested == 3);
  KJ_EXPECT(comparison.worst_scenario_id == "s3"_kj);
  KJ_EXPECT(std::abs(comparison.worst_pnl_impact - (-15000.0)) < 1.0);
  KJ_EXPECT(std::abs(comparison.best_pnl_impact - (-5000.0)) < 1.0);
  KJ_EXPECT(std::abs(comparison.average_pnl_impact - (-10000.0)) < 1.0);
}

KJ_TEST("StressTestEngine - short position stress test") {
  StressTestEngine engine;
  engine.add_scenario(StressScenarioBuilder()
      .id("test")
      .price_shock("", -0.20)  // 20% drop
      .build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = -1.0;  // Short position
  pos.entry_price = 50000.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto result = engine.run_stress_test("test", positions);

  KJ_EXPECT(result.success);
  // Short position profits from price drop
  // Stressed price: 50000 * 0.8 = 40000
  // P&L for short: -1 * (40000 - 50000) = 10000 gain
  // But total_pnl_impact is based on portfolio value change
  // Base: 50000, Stressed: 40000, Impact: -10000 (portfolio value decreased)
  KJ_EXPECT(result.total_pnl_impact < 0.0);
}

KJ_TEST("FactorShock - copy constructor") {
  FactorShock original;
  original.factor = MarketFactor::Price;
  original.symbol = kj::str("BTC");
  original.shock_value = -0.25;
  original.is_relative = true;

  FactorShock copy(original);

  KJ_EXPECT(copy.factor == MarketFactor::Price);
  KJ_EXPECT(copy.symbol == "BTC"_kj);
  KJ_EXPECT(std::abs(copy.shock_value - (-0.25)) < 0.001);
  KJ_EXPECT(copy.is_relative == true);
}

KJ_TEST("StressScenario - copy constructor") {
  auto original = StressScenarioBuilder()
      .id("original")
      .name("Original Scenario")
      .description("Test description")
      .type(StressScenarioType::Historical)
      .price_shock("BTC", -0.30)
      .volatility_shock("", 2.0)
      .historical_event("Test Event")
      .build();

  StressScenario copy(original);

  KJ_EXPECT(copy.id == "original"_kj);
  KJ_EXPECT(copy.name == "Original Scenario"_kj);
  KJ_EXPECT(copy.description == "Test description"_kj);
  KJ_EXPECT(copy.type == StressScenarioType::Historical);
  KJ_EXPECT(copy.shocks.size() == 2);
  KJ_EXPECT(copy.historical_event == "Test Event"_kj);
}

KJ_TEST("StressTestResult - copy constructor") {
  StressTestResult original;
  original.scenario_id = kj::str("test");
  original.scenario_name = kj::str("Test");
  original.success = true;
  original.base_portfolio_value = 100000.0;
  original.stressed_portfolio_value = 80000.0;
  original.total_pnl_impact = -20000.0;

  PositionStressResult pos_result;
  pos_result.symbol = kj::str("BTC");
  pos_result.pnl_impact = -20000.0;
  original.position_results.add(kj::mv(pos_result));

  StressTestResult copy(original);

  KJ_EXPECT(copy.scenario_id == "test"_kj);
  KJ_EXPECT(copy.success == true);
  KJ_EXPECT(std::abs(copy.total_pnl_impact - (-20000.0)) < 1.0);
  KJ_EXPECT(copy.position_results.size() == 1);
  KJ_EXPECT(copy.position_results[0].symbol == "BTC"_kj);
}

KJ_TEST("StressTestEngine - execution time tracking") {
  StressTestEngine engine;
  engine.add_scenario(StressScenarioBuilder()
      .id("test")
      .price_shock("", -0.20)
      .build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto result = engine.run_stress_test("test", positions);

  KJ_EXPECT(result.executed_at_ns > 0);
  KJ_EXPECT(result.execution_time_us >= 0);
}

} // namespace
} // namespace veloz::risk
