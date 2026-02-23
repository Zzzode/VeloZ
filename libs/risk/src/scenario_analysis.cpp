#include "veloz/risk/scenario_analysis.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace veloz::risk {

// ============================================================================
// String Conversion Functions
// ============================================================================

kj::StringPtr scenario_probability_to_string(ScenarioProbability prob) {
  switch (prob) {
  case ScenarioProbability::VeryLow:
    return "very_low"_kj;
  case ScenarioProbability::Low:
    return "low"_kj;
  case ScenarioProbability::Medium:
    return "medium"_kj;
  case ScenarioProbability::High:
    return "high"_kj;
  case ScenarioProbability::VeryHigh:
    return "very_high"_kj;
  }
  return "unknown"_kj;
}

std::pair<double, double> get_probability_range(ScenarioProbability prob) {
  switch (prob) {
  case ScenarioProbability::VeryLow:
    return {0.0, 0.01};
  case ScenarioProbability::Low:
    return {0.01, 0.05};
  case ScenarioProbability::Medium:
    return {0.05, 0.20};
  case ScenarioProbability::High:
    return {0.20, 0.50};
  case ScenarioProbability::VeryHigh:
    return {0.50, 1.0};
  }
  return {0.0, 0.0};
}

// ============================================================================
// Copy Constructors
// ============================================================================

EnhancedScenario::EnhancedScenario(const EnhancedScenario& other)
    : base_scenario(other.base_scenario),
      probability(other.probability),
      probability_estimate(other.probability_estimate),
      time_horizon_days(other.time_horizon_days),
      is_instantaneous(other.is_instantaneous),
      category(kj::heapString(other.category)),
      expected_recovery_days(other.expected_recovery_days),
      recovery_rate(other.recovery_rate) {
  for (const auto& t : other.tags) {
    tags.add(kj::heapString(t));
  }
}

EnhancedScenario& EnhancedScenario::operator=(const EnhancedScenario& other) {
  if (this != &other) {
    base_scenario = StressScenario(other.base_scenario);
    probability = other.probability;
    probability_estimate = other.probability_estimate;
    time_horizon_days = other.time_horizon_days;
    is_instantaneous = other.is_instantaneous;
    category = kj::heapString(other.category);
    tags.clear();
    for (const auto& t : other.tags) {
      tags.add(kj::heapString(t));
    }
    expected_recovery_days = other.expected_recovery_days;
    recovery_rate = other.recovery_rate;
  }
  return *this;
}

PortfolioImpactResult::PortfolioImpactResult(const PortfolioImpactResult& other)
    : scenario_id(kj::heapString(other.scenario_id)),
      scenario_name(kj::heapString(other.scenario_name)),
      immediate_pnl(other.immediate_pnl),
      expected_pnl(other.expected_pnl),
      worst_case_pnl(other.worst_case_pnl),
      base_var_95(other.base_var_95),
      stressed_var_95(other.stressed_var_95),
      var_increase_pct(other.var_increase_pct),
      margin_call_risk(other.margin_call_risk),
      liquidation_risk(other.liquidation_risk),
      margin_utilization(other.margin_utilization),
      days_to_breakeven(other.days_to_breakeven),
      recovery_probability(other.recovery_probability) {
  for (const auto& pi : other.position_impacts) {
    position_impacts.add(PositionStressResult(pi));
  }
}

PortfolioImpactResult& PortfolioImpactResult::operator=(const PortfolioImpactResult& other) {
  if (this != &other) {
    scenario_id = kj::heapString(other.scenario_id);
    scenario_name = kj::heapString(other.scenario_name);
    immediate_pnl = other.immediate_pnl;
    expected_pnl = other.expected_pnl;
    worst_case_pnl = other.worst_case_pnl;
    base_var_95 = other.base_var_95;
    stressed_var_95 = other.stressed_var_95;
    var_increase_pct = other.var_increase_pct;
    position_impacts.clear();
    for (const auto& pi : other.position_impacts) {
      position_impacts.add(PositionStressResult(pi));
    }
    margin_call_risk = other.margin_call_risk;
    liquidation_risk = other.liquidation_risk;
    margin_utilization = other.margin_utilization;
    days_to_breakeven = other.days_to_breakeven;
    recovery_probability = other.recovery_probability;
  }
  return *this;
}

RiskBudget::RiskBudget(const RiskBudget& other)
    : name(kj::heapString(other.name)),
      max_var(other.max_var),
      max_stress_loss(other.max_stress_loss),
      current_var(other.current_var),
      current_stress_loss(other.current_stress_loss),
      utilization_pct(other.utilization_pct) {}

RiskBudget& RiskBudget::operator=(const RiskBudget& other) {
  if (this != &other) {
    name = kj::heapString(other.name);
    max_var = other.max_var;
    max_stress_loss = other.max_stress_loss;
    current_var = other.current_var;
    current_stress_loss = other.current_stress_loss;
    utilization_pct = other.utilization_pct;
  }
  return *this;
}

// ============================================================================
// ScenarioAnalysisEngine Implementation
// ============================================================================

void ScenarioAnalysisEngine::add_scenario(EnhancedScenario scenario) {
  scenarios_.add(kj::mv(scenario));
}

const EnhancedScenario* ScenarioAnalysisEngine::get_scenario(kj::StringPtr id) const {
  for (const auto& scenario : scenarios_) {
    if (scenario.base_scenario.id == id) {
      return &scenario;
    }
  }
  return nullptr;
}

const kj::Vector<EnhancedScenario>& ScenarioAnalysisEngine::get_scenarios() const {
  return scenarios_;
}

kj::Vector<const EnhancedScenario*> ScenarioAnalysisEngine::get_scenarios_by_category(
    kj::StringPtr category) const {
  kj::Vector<const EnhancedScenario*> result;
  for (const auto& scenario : scenarios_) {
    if (scenario.category == category) {
      result.add(&scenario);
    }
  }
  return result;
}

kj::Vector<const EnhancedScenario*> ScenarioAnalysisEngine::get_scenarios_by_tag(
    kj::StringPtr tag) const {
  kj::Vector<const EnhancedScenario*> result;
  for (const auto& scenario : scenarios_) {
    for (const auto& t : scenario.tags) {
      if (t == tag) {
        result.add(&scenario);
        break;
      }
    }
  }
  return result;
}

bool ScenarioAnalysisEngine::remove_scenario(kj::StringPtr id) {
  for (size_t i = 0; i < scenarios_.size(); ++i) {
    if (scenarios_[i].base_scenario.id == id) {
      kj::Vector<EnhancedScenario> new_scenarios;
      for (size_t j = 0; j < scenarios_.size(); ++j) {
        if (j != i) {
          new_scenarios.add(kj::mv(scenarios_[j]));
        }
      }
      scenarios_ = kj::mv(new_scenarios);
      return true;
    }
  }
  return false;
}

void ScenarioAnalysisEngine::clear_scenarios() {
  scenarios_.clear();
}

PortfolioImpactResult ScenarioAnalysisEngine::analyze_impact(
    kj::StringPtr scenario_id,
    const kj::Vector<StressPosition>& positions,
    double account_equity,
    double margin_requirement) const {

  PortfolioImpactResult result;

  const EnhancedScenario* scenario = get_scenario(scenario_id);
  if (scenario == nullptr) {
    result.scenario_id = kj::str(scenario_id);
    return result;
  }

  result.scenario_id = kj::str(scenario->base_scenario.id);
  result.scenario_name = kj::str(scenario->base_scenario.name);

  // Run stress test using the base scenario
  StressTestEngine stress_engine;
  auto stress_result = stress_engine.run_stress_test(scenario->base_scenario, positions);

  // Copy position impacts
  for (const auto& pr : stress_result.position_results) {
    result.position_impacts.add(PositionStressResult(pr));
  }

  // Calculate P&L impacts
  result.immediate_pnl = stress_result.total_pnl_impact;
  result.expected_pnl = result.immediate_pnl * scenario->probability_estimate;
  result.worst_case_pnl = result.immediate_pnl;  // No recovery assumption

  // Calculate VaR impact (simplified - uses volatility shock as proxy)
  result.base_var_95 = calculate_stressed_var(positions, *scenario, 0.95);
  result.stressed_var_95 = result.base_var_95;  // Would need full recalculation

  if (result.base_var_95 > 0.0) {
    result.var_increase_pct = (result.stressed_var_95 - result.base_var_95) /
                               result.base_var_95 * 100.0;
  }

  // Check margin/liquidation risk
  check_margin_risk(result, account_equity, margin_requirement);

  // Calculate recovery metrics
  if (scenario->recovery_rate > 0.0 && result.immediate_pnl < 0.0) {
    double daily_recovery = std::abs(result.immediate_pnl) * scenario->recovery_rate;
    if (daily_recovery > 0.0) {
      result.days_to_breakeven = static_cast<int>(
          std::ceil(std::abs(result.immediate_pnl) / daily_recovery));
    }
    result.recovery_probability = std::min(1.0, scenario->recovery_rate * 10.0);
  }

  return result;
}

kj::Vector<PortfolioImpactResult> ScenarioAnalysisEngine::analyze_all_impacts(
    const kj::Vector<StressPosition>& positions,
    double account_equity,
    double margin_requirement) const {

  kj::Vector<PortfolioImpactResult> results;
  for (const auto& scenario : scenarios_) {
    results.add(analyze_impact(scenario.base_scenario.id, positions,
                               account_equity, margin_requirement));
  }
  return results;
}

ScenarioComparisonResult ScenarioAnalysisEngine::compare_scenarios(
    const kj::Vector<PortfolioImpactResult>& impacts) const {

  ScenarioComparisonResult result;
  result.scenarios_count = static_cast<int>(impacts.size());

  if (impacts.size() == 0) {
    return result;
  }

  // Collect P&L values and scenario IDs
  kj::Vector<double> pnl_values;
  double sum_pnl = 0.0;
  double sum_expected_pnl = 0.0;
  double worst_pnl = 0.0;
  double best_pnl = impacts[0].immediate_pnl;

  for (const auto& impact : impacts) {
    result.scenario_ids.add(kj::str(impact.scenario_id));
    pnl_values.add(impact.immediate_pnl);
    sum_pnl += impact.immediate_pnl;
    sum_expected_pnl += impact.expected_pnl;

    if (impact.immediate_pnl < worst_pnl) {
      worst_pnl = impact.immediate_pnl;
      result.worst_scenario_id = kj::str(impact.scenario_id);
      result.worst_scenario_name = kj::str(impact.scenario_name);
    }
    if (impact.immediate_pnl > best_pnl) {
      best_pnl = impact.immediate_pnl;
    }
  }

  result.worst_pnl = worst_pnl;
  result.best_pnl = best_pnl;
  result.average_pnl = sum_pnl / impacts.size();
  result.expected_pnl = sum_expected_pnl;

  // Calculate median
  kj::Vector<double> sorted_pnl;
  sorted_pnl.addAll(pnl_values);
  std::sort(sorted_pnl.begin(), sorted_pnl.end());

  size_t mid = sorted_pnl.size() / 2;
  if (sorted_pnl.size() % 2 == 0) {
    result.median_pnl = (sorted_pnl[mid - 1] + sorted_pnl[mid]) / 2.0;
  } else {
    result.median_pnl = sorted_pnl[mid];
  }

  // Calculate standard deviation
  double sum_sq = 0.0;
  for (double pnl : pnl_values) {
    sum_sq += (pnl - result.average_pnl) * (pnl - result.average_pnl);
  }
  result.pnl_std_dev = std::sqrt(sum_sq / impacts.size());

  // Calculate percentiles
  size_t idx_5 = static_cast<size_t>(0.05 * sorted_pnl.size());
  size_t idx_95 = static_cast<size_t>(0.95 * sorted_pnl.size());
  if (idx_95 >= sorted_pnl.size()) idx_95 = sorted_pnl.size() - 1;

  result.pnl_5th_percentile = sorted_pnl[idx_5];
  result.pnl_95th_percentile = sorted_pnl[idx_95];

  // Count correlated scenarios (within 10% of each other)
  for (size_t i = 0; i < pnl_values.size(); ++i) {
    for (size_t j = i + 1; j < pnl_values.size(); ++j) {
      double diff = std::abs(pnl_values[i] - pnl_values[j]);
      double avg = (std::abs(pnl_values[i]) + std::abs(pnl_values[j])) / 2.0;
      if (avg > 0.0 && diff / avg < 0.1) {
        result.correlated_scenarios++;
      }
    }
  }

  return result;
}

kj::Vector<kj::String> ScenarioAnalysisEngine::rank_by_severity(
    const kj::Vector<PortfolioImpactResult>& impacts) const {

  // Create pairs of (pnl, scenario_id) for sorting
  kj::Vector<std::pair<double, kj::String>> ranked;
  for (const auto& impact : impacts) {
    ranked.add(std::make_pair(impact.immediate_pnl, kj::str(impact.scenario_id)));
  }

  // Sort by P&L ascending (worst first)
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  kj::Vector<kj::String> result;
  for (auto& pair : ranked) {
    result.add(kj::mv(pair.second));
  }

  return result;
}

double ScenarioAnalysisEngine::calculate_expected_loss(
    const kj::Vector<PortfolioImpactResult>& impacts) const {

  double expected_loss = 0.0;
  for (const auto& impact : impacts) {
    expected_loss += impact.expected_pnl;
  }
  return expected_loss;
}

void ScenarioAnalysisEngine::set_risk_budget(RiskBudget budget) {
  risk_budget_ = kj::mv(budget);
}

const RiskBudget& ScenarioAnalysisEngine::get_risk_budget() const {
  return risk_budget_;
}

bool ScenarioAnalysisEngine::is_within_budget(
    const kj::Vector<PortfolioImpactResult>& impacts) const {

  double utilization = calculate_budget_utilization(impacts);
  return utilization <= 100.0;
}

double ScenarioAnalysisEngine::calculate_budget_utilization(
    const kj::Vector<PortfolioImpactResult>& impacts) const {

  if (risk_budget_.max_stress_loss <= 0.0) {
    return 0.0;
  }

  // Find worst stress loss
  double worst_loss = 0.0;
  for (const auto& impact : impacts) {
    if (impact.immediate_pnl < worst_loss) {
      worst_loss = impact.immediate_pnl;
    }
  }

  return std::abs(worst_loss) / risk_budget_.max_stress_loss * 100.0;
}

EnhancedScenario ScenarioAnalysisEngine::generate_reverse_stress_scenario(
    const kj::Vector<StressPosition>& positions,
    double target_loss) const {

  // Calculate total portfolio value
  double total_value = 0.0;
  for (const auto& pos : positions) {
    total_value += std::abs(pos.size) * pos.current_price;
  }

  // Calculate required price shock to achieve target loss
  double required_shock = 0.0;
  if (total_value > 0.0) {
    required_shock = target_loss / total_value;
  }

  // Build reverse stress scenario
  return EnhancedScenarioBuilder()
      .id("reverse_stress")
      .name("Reverse Stress Scenario")
      .description(kj::str("Scenario to achieve ", target_loss, " loss"))
      .price_shock("", required_shock)
      .probability(ScenarioProbability::VeryLow)
      .probability_estimate(0.005)
      .category("Reverse Stress")
      .build();
}

EnhancedScenario ScenarioAnalysisEngine::generate_worst_case_scenario() const {
  EnhancedScenarioBuilder builder;
  builder.id("worst_case")
         .name("Worst Case Combined Scenario")
         .description("Combined worst factors from all scenarios")
         .probability(ScenarioProbability::VeryLow)
         .probability_estimate(0.001)
         .category("Worst Case");

  // Find worst price shock for each symbol
  kj::HashMap<kj::String, double> worst_shocks;

  for (const auto& scenario : scenarios_) {
    for (const auto& shock : scenario.base_scenario.shocks) {
      if (shock.factor == MarketFactor::Price) {
        auto key = kj::str(shock.symbol);
        KJ_IF_SOME(existing, worst_shocks.find(key)) {
          if (shock.shock_value < existing) {
            worst_shocks.upsert(kj::mv(key), shock.shock_value,
                [](double& e, double v) { e = v; });
          }
        } else {
          worst_shocks.insert(kj::mv(key), shock.shock_value);
        }
      }
    }
  }

  // Add worst shocks to scenario
  for (auto& entry : worst_shocks) {
    builder.price_shock(entry.key, entry.value);
  }

  return builder.build();
}

void ScenarioAnalysisEngine::set_liquidation_threshold(double threshold) {
  liquidation_threshold_ = threshold;
}

void ScenarioAnalysisEngine::set_margin_call_threshold(double threshold) {
  margin_call_threshold_ = threshold;
}

double ScenarioAnalysisEngine::calculate_stressed_var(
    const kj::Vector<StressPosition>& positions,
    const EnhancedScenario& scenario,
    double confidence) const {

  // Simplified VaR calculation based on position volatilities
  double portfolio_variance = 0.0;
  double total_value = 0.0;

  for (const auto& pos : positions) {
    double value = std::abs(pos.size) * pos.current_price;
    total_value += value;

    // Apply volatility shock if present
    double vol = pos.volatility;
    for (const auto& shock : scenario.base_scenario.shocks) {
      if (shock.factor == MarketFactor::Volatility) {
        if (shock.symbol.size() == 0 || shock.symbol == pos.symbol) {
          if (shock.is_relative) {
            vol *= (1.0 + shock.shock_value);
          } else {
            vol = shock.shock_value;
          }
        }
      }
    }

    portfolio_variance += value * value * vol * vol;
  }

  double portfolio_std = std::sqrt(portfolio_variance);
  double z_score = VaRCalculator::get_z_score(confidence);

  return z_score * portfolio_std;
}

void ScenarioAnalysisEngine::check_margin_risk(
    PortfolioImpactResult& result,
    double account_equity,
    double margin_requirement) const {

  if (margin_requirement <= 0.0 || account_equity <= 0.0) {
    return;
  }

  double post_stress_equity = account_equity + result.immediate_pnl;
  result.margin_utilization = margin_requirement / post_stress_equity;

  if (result.margin_utilization >= liquidation_threshold_) {
    result.liquidation_risk = true;
  }
  if (result.margin_utilization >= margin_call_threshold_) {
    result.margin_call_risk = true;
  }
}

// ============================================================================
// EnhancedScenarioBuilder Implementation
// ============================================================================

EnhancedScenarioBuilder& EnhancedScenarioBuilder::base_scenario(StressScenario scenario) {
  scenario_.base_scenario = kj::mv(scenario);
  has_base_scenario_ = true;
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::id(kj::StringPtr id) {
  scenario_.base_scenario.id = kj::str(id);
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::name(kj::StringPtr name) {
  scenario_.base_scenario.name = kj::str(name);
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::description(kj::StringPtr desc) {
  scenario_.base_scenario.description = kj::str(desc);
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::price_shock(kj::StringPtr symbol,
                                                               double shock_pct) {
  FactorShock shock;
  shock.factor = MarketFactor::Price;
  shock.symbol = kj::str(symbol);
  shock.shock_value = shock_pct;
  shock.is_relative = true;
  scenario_.base_scenario.shocks.add(kj::mv(shock));
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::volatility_shock(kj::StringPtr symbol,
                                                                    double shock_pct) {
  FactorShock shock;
  shock.factor = MarketFactor::Volatility;
  shock.symbol = kj::str(symbol);
  shock.shock_value = shock_pct;
  shock.is_relative = true;
  scenario_.base_scenario.shocks.add(kj::mv(shock));
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::probability(ScenarioProbability prob) {
  scenario_.probability = prob;
  auto range = get_probability_range(prob);
  scenario_.probability_estimate = (range.first + range.second) / 2.0;
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::probability_estimate(double prob) {
  scenario_.probability_estimate = prob;
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::time_horizon(int days) {
  scenario_.time_horizon_days = days;
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::instantaneous(bool instant) {
  scenario_.is_instantaneous = instant;
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::category(kj::StringPtr cat) {
  scenario_.category = kj::str(cat);
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::tag(kj::StringPtr t) {
  scenario_.tags.add(kj::str(t));
  return *this;
}

EnhancedScenarioBuilder& EnhancedScenarioBuilder::recovery(int days, double rate) {
  scenario_.expected_recovery_days = days;
  scenario_.recovery_rate = rate;
  return *this;
}

EnhancedScenario EnhancedScenarioBuilder::build() {
  scenario_.base_scenario.created_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  return kj::mv(scenario_);
}

} // namespace veloz::risk
