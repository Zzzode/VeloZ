#include "veloz/risk/scenario_analysis.h"

#include <cmath>
#include <utility>
#include <kj/test.h>

namespace veloz::risk {
namespace {

KJ_TEST("ScenarioProbability to string conversion") {
  KJ_EXPECT(scenario_probability_to_string(ScenarioProbability::VeryLow) == "very_low"_kj);
  KJ_EXPECT(scenario_probability_to_string(ScenarioProbability::Low) == "low"_kj);
  KJ_EXPECT(scenario_probability_to_string(ScenarioProbability::Medium) == "medium"_kj);
  KJ_EXPECT(scenario_probability_to_string(ScenarioProbability::High) == "high"_kj);
  KJ_EXPECT(scenario_probability_to_string(ScenarioProbability::VeryHigh) == "very_high"_kj);
}

KJ_TEST("get_probability_range returns correct ranges") {
  auto [min1, max1] = get_probability_range(ScenarioProbability::VeryLow);
  KJ_EXPECT(min1 == 0.0);
  KJ_EXPECT(max1 == 0.01);

  auto [min2, max2] = get_probability_range(ScenarioProbability::Low);
  KJ_EXPECT(min2 == 0.01);
  KJ_EXPECT(max2 == 0.05);

  auto [min3, max3] = get_probability_range(ScenarioProbability::Medium);
  KJ_EXPECT(min3 == 0.05);
  KJ_EXPECT(max3 == 0.20);
}

KJ_TEST("EnhancedScenarioBuilder - basic scenario creation") {
  auto scenario = EnhancedScenarioBuilder()
      .id("test_scenario")
      .name("Test Scenario")
      .description("A test scenario")
      .price_shock("BTC", -0.30)
      .probability(ScenarioProbability::Low)
      .category("Market Crash")
      .tag("crypto")
      .tag("black_swan")
      .build();

  KJ_EXPECT(scenario.base_scenario.id == "test_scenario"_kj);
  KJ_EXPECT(scenario.base_scenario.name == "Test Scenario"_kj);
  KJ_EXPECT(scenario.probability == ScenarioProbability::Low);
  KJ_EXPECT(scenario.category == "Market Crash"_kj);
  KJ_EXPECT(scenario.tags.size() == 2);
  KJ_EXPECT(scenario.base_scenario.shocks.size() == 1);
}

KJ_TEST("EnhancedScenarioBuilder - with recovery") {
  auto scenario = EnhancedScenarioBuilder()
      .id("recovery_test")
      .name("Recovery Test")
      .price_shock("", -0.20)
      .recovery(30, 0.02)  // 30 days, 2% daily recovery
      .build();

  KJ_EXPECT(scenario.expected_recovery_days == 30);
  KJ_EXPECT(std::abs(scenario.recovery_rate - 0.02) < 0.001);
}

KJ_TEST("ScenarioAnalysisEngine - add and get scenario") {
  ScenarioAnalysisEngine engine;

  auto scenario = EnhancedScenarioBuilder()
      .id("test_1")
      .name("Test 1")
      .category("Test")
      .build();

  engine.add_scenario(kj::mv(scenario));

  const auto* retrieved = engine.get_scenario("test_1");
  KJ_ASSERT(retrieved != nullptr);
  KJ_EXPECT(retrieved->base_scenario.name == "Test 1"_kj);

  KJ_EXPECT(engine.get_scenario("nonexistent") == nullptr);
}

KJ_TEST("ScenarioAnalysisEngine - get scenarios by category") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s1").category("Market Crash").build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s2").category("Market Crash").build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s3").category("Liquidity Crisis").build());

  auto crashes = engine.get_scenarios_by_category("Market Crash");
  KJ_EXPECT(crashes.size() == 2);

  auto liquidity = engine.get_scenarios_by_category("Liquidity Crisis");
  KJ_EXPECT(liquidity.size() == 1);
}

KJ_TEST("ScenarioAnalysisEngine - get scenarios by tag") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s1").tag("crypto").tag("major").build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s2").tag("crypto").build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s3").tag("forex").build());

  auto crypto = engine.get_scenarios_by_tag("crypto");
  KJ_EXPECT(crypto.size() == 2);

  auto major = engine.get_scenarios_by_tag("major");
  KJ_EXPECT(major.size() == 1);
}

KJ_TEST("ScenarioAnalysisEngine - remove scenario") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder().id("s1").build());
  engine.add_scenario(EnhancedScenarioBuilder().id("s2").build());

  KJ_EXPECT(engine.get_scenarios().size() == 2);

  bool removed = engine.remove_scenario("s1");
  KJ_EXPECT(removed);
  KJ_EXPECT(engine.get_scenarios().size() == 1);
  KJ_EXPECT(engine.get_scenario("s1") == nullptr);
}

KJ_TEST("ScenarioAnalysisEngine - analyze impact") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("crash")
      .name("Market Crash")
      .price_shock("", -0.30)
      .probability(ScenarioProbability::Low)
      .probability_estimate(0.03)
      .build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.entry_price = 40000.0;
  pos.current_price = 50000.0;
  pos.volatility = 0.03;
  positions.add(kj::mv(pos));

  auto result = engine.analyze_impact("crash", positions, 100000.0, 20000.0);

  KJ_EXPECT(result.scenario_id == "crash"_kj);
  KJ_EXPECT(result.scenario_name == "Market Crash"_kj);
  KJ_EXPECT(result.immediate_pnl < 0.0);  // Should be negative (loss)
  KJ_EXPECT(result.position_impacts.size() == 1);
}

KJ_TEST("ScenarioAnalysisEngine - analyze all impacts") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s1").price_shock("", -0.20).probability_estimate(0.05).build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s2").price_shock("", -0.30).probability_estimate(0.02).build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  auto results = engine.analyze_all_impacts(positions, 100000.0);

  KJ_EXPECT(results.size() == 2);
}

KJ_TEST("ScenarioAnalysisEngine - compare scenarios") {
  ScenarioAnalysisEngine engine;

  kj::Vector<PortfolioImpactResult> impacts;

  PortfolioImpactResult r1;
  r1.scenario_id = kj::str("s1");
  r1.scenario_name = kj::str("Scenario 1");
  r1.immediate_pnl = -10000.0;
  r1.expected_pnl = -500.0;
  impacts.add(kj::mv(r1));

  PortfolioImpactResult r2;
  r2.scenario_id = kj::str("s2");
  r2.scenario_name = kj::str("Scenario 2");
  r2.immediate_pnl = -20000.0;
  r2.expected_pnl = -400.0;
  impacts.add(kj::mv(r2));

  PortfolioImpactResult r3;
  r3.scenario_id = kj::str("s3");
  r3.scenario_name = kj::str("Scenario 3");
  r3.immediate_pnl = -5000.0;
  r3.expected_pnl = -250.0;
  impacts.add(kj::mv(r3));

  auto comparison = engine.compare_scenarios(impacts);

  KJ_EXPECT(comparison.scenarios_count == 3);
  KJ_EXPECT(comparison.worst_scenario_id == "s2"_kj);
  KJ_EXPECT(std::abs(comparison.worst_pnl - (-20000.0)) < 1.0);
  KJ_EXPECT(std::abs(comparison.best_pnl - (-5000.0)) < 1.0);
  KJ_EXPECT(std::abs(comparison.average_pnl - (-11666.67)) < 1.0);
}

KJ_TEST("ScenarioAnalysisEngine - rank by severity") {
  ScenarioAnalysisEngine engine;

  kj::Vector<PortfolioImpactResult> impacts;

  PortfolioImpactResult r1;
  r1.scenario_id = kj::str("mild");
  r1.immediate_pnl = -5000.0;
  impacts.add(kj::mv(r1));

  PortfolioImpactResult r2;
  r2.scenario_id = kj::str("severe");
  r2.immediate_pnl = -25000.0;
  impacts.add(kj::mv(r2));

  PortfolioImpactResult r3;
  r3.scenario_id = kj::str("moderate");
  r3.immediate_pnl = -15000.0;
  impacts.add(kj::mv(r3));

  auto ranked = engine.rank_by_severity(impacts);

  KJ_EXPECT(ranked.size() == 3);
  KJ_EXPECT(ranked[0] == "severe"_kj);    // Worst first
  KJ_EXPECT(ranked[1] == "moderate"_kj);
  KJ_EXPECT(ranked[2] == "mild"_kj);      // Best last
}

KJ_TEST("ScenarioAnalysisEngine - calculate expected loss") {
  ScenarioAnalysisEngine engine;

  kj::Vector<PortfolioImpactResult> impacts;

  PortfolioImpactResult r1;
  r1.expected_pnl = -500.0;
  impacts.add(kj::mv(r1));

  PortfolioImpactResult r2;
  r2.expected_pnl = -300.0;
  impacts.add(kj::mv(r2));

  double expected_loss = engine.calculate_expected_loss(impacts);
  KJ_EXPECT(std::abs(expected_loss - (-800.0)) < 1.0);
}

KJ_TEST("ScenarioAnalysisEngine - risk budget") {
  ScenarioAnalysisEngine engine;

  RiskBudget budget;
  budget.name = kj::str("Trading Budget");
  budget.max_var = 10000.0;
  budget.max_stress_loss = 50000.0;

  engine.set_risk_budget(kj::mv(budget));

  const auto& retrieved = engine.get_risk_budget();
  KJ_EXPECT(retrieved.name == "Trading Budget"_kj);
  KJ_EXPECT(retrieved.max_stress_loss == 50000.0);
}

KJ_TEST("ScenarioAnalysisEngine - budget utilization") {
  ScenarioAnalysisEngine engine;

  RiskBudget budget;
  budget.max_stress_loss = 50000.0;
  engine.set_risk_budget(kj::mv(budget));

  kj::Vector<PortfolioImpactResult> impacts;

  PortfolioImpactResult r1;
  r1.immediate_pnl = -25000.0;  // 50% of budget
  impacts.add(kj::mv(r1));

  double utilization = engine.calculate_budget_utilization(impacts);
  KJ_EXPECT(std::abs(utilization - 50.0) < 1.0);

  KJ_EXPECT(engine.is_within_budget(impacts));

  // Add worse scenario
  PortfolioImpactResult r2;
  r2.immediate_pnl = -60000.0;  // 120% of budget
  impacts.add(kj::mv(r2));

  utilization = engine.calculate_budget_utilization(impacts);
  KJ_EXPECT(utilization > 100.0);
  KJ_EXPECT(!engine.is_within_budget(impacts));
}

KJ_TEST("ScenarioAnalysisEngine - generate reverse stress scenario") {
  ScenarioAnalysisEngine engine;

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  // Generate scenario for -10000 loss (20% of 50000)
  auto scenario = engine.generate_reverse_stress_scenario(positions, -10000.0);

  KJ_EXPECT(scenario.base_scenario.id == "reverse_stress"_kj);
  KJ_EXPECT(scenario.category == "Reverse Stress"_kj);
  KJ_EXPECT(scenario.base_scenario.shocks.size() == 1);

  // The shock should be approximately -20%
  KJ_EXPECT(std::abs(scenario.base_scenario.shocks[0].shock_value - (-0.20)) < 0.01);
}

KJ_TEST("ScenarioAnalysisEngine - generate worst case scenario") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s1").price_shock("BTC", -0.20).build());
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s2").price_shock("BTC", -0.30).build());  // Worse for BTC
  engine.add_scenario(EnhancedScenarioBuilder()
      .id("s3").price_shock("ETH", -0.40).build());

  auto worst = engine.generate_worst_case_scenario();

  KJ_EXPECT(worst.base_scenario.id == "worst_case"_kj);
  KJ_EXPECT(worst.category == "Worst Case"_kj);
  KJ_EXPECT(worst.probability == ScenarioProbability::VeryLow);
}

KJ_TEST("ScenarioAnalysisEngine - margin risk detection") {
  ScenarioAnalysisEngine engine;

  engine.add_scenario(EnhancedScenarioBuilder()
      .id("crash")
      .price_shock("", -0.50)  // 50% drop
      .build());

  kj::Vector<StressPosition> positions;
  StressPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.current_price = 50000.0;
  positions.add(kj::mv(pos));

  // Account equity: 60000, Margin: 40000
  // After 50% drop: equity becomes 60000 - 25000 = 35000
  // Margin utilization: 40000 / 35000 = 114%
  auto result = engine.analyze_impact("crash", positions, 60000.0, 40000.0);

  KJ_EXPECT(result.margin_call_risk);
  KJ_EXPECT(result.liquidation_risk);
}

KJ_TEST("EnhancedScenario - copy constructor") {
  auto original = EnhancedScenarioBuilder()
      .id("original")
      .name("Original")
      .price_shock("BTC", -0.30)
      .probability(ScenarioProbability::Medium)
      .category("Test")
      .tag("tag1")
      .tag("tag2")
      .recovery(30, 0.02)
      .build();

  EnhancedScenario copy(original);

  KJ_EXPECT(copy.base_scenario.id == "original"_kj);
  KJ_EXPECT(copy.probability == ScenarioProbability::Medium);
  KJ_EXPECT(copy.category == "Test"_kj);
  KJ_EXPECT(copy.tags.size() == 2);
  KJ_EXPECT(copy.expected_recovery_days == 30);
}

KJ_TEST("PortfolioImpactResult - copy constructor") {
  PortfolioImpactResult original;
  original.scenario_id = kj::str("test");
  original.scenario_name = kj::str("Test");
  original.immediate_pnl = -10000.0;
  original.expected_pnl = -500.0;
  original.margin_call_risk = true;

  PositionStressResult pos;
  pos.symbol = kj::str("BTC");
  pos.pnl_impact = -10000.0;
  original.position_impacts.add(kj::mv(pos));

  PortfolioImpactResult copy(original);

  KJ_EXPECT(copy.scenario_id == "test"_kj);
  KJ_EXPECT(std::abs(copy.immediate_pnl - (-10000.0)) < 1.0);
  KJ_EXPECT(copy.margin_call_risk == true);
  KJ_EXPECT(copy.position_impacts.size() == 1);
}

} // namespace
} // namespace veloz::risk
