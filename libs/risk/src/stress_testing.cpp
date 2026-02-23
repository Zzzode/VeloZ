#include "veloz/risk/stress_testing.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace veloz::risk {

// ============================================================================
// String Conversion Functions
// ============================================================================

kj::StringPtr stress_scenario_type_to_string(StressScenarioType type) {
  switch (type) {
  case StressScenarioType::Historical:
    return "historical"_kj;
  case StressScenarioType::Hypothetical:
    return "hypothetical"_kj;
  case StressScenarioType::Sensitivity:
    return "sensitivity"_kj;
  }
  return "unknown"_kj;
}

kj::StringPtr market_factor_to_string(MarketFactor factor) {
  switch (factor) {
  case MarketFactor::Price:
    return "price"_kj;
  case MarketFactor::Volatility:
    return "volatility"_kj;
  case MarketFactor::Correlation:
    return "correlation"_kj;
  case MarketFactor::Liquidity:
    return "liquidity"_kj;
  case MarketFactor::InterestRate:
    return "interest_rate"_kj;
  case MarketFactor::FundingRate:
    return "funding_rate"_kj;
  }
  return "unknown"_kj;
}

// ============================================================================
// Copy Constructors for Structs
// ============================================================================

FactorShock::FactorShock(const FactorShock& other)
    : factor(other.factor),
      symbol(kj::heapString(other.symbol)),
      shock_value(other.shock_value),
      is_relative(other.is_relative) {}

FactorShock& FactorShock::operator=(const FactorShock& other) {
  if (this != &other) {
    factor = other.factor;
    symbol = kj::heapString(other.symbol);
    shock_value = other.shock_value;
    is_relative = other.is_relative;
  }
  return *this;
}

StressPosition::StressPosition(const StressPosition& other)
    : symbol(kj::heapString(other.symbol)),
      size(other.size),
      entry_price(other.entry_price),
      current_price(other.current_price),
      volatility(other.volatility) {}

StressPosition& StressPosition::operator=(const StressPosition& other) {
  if (this != &other) {
    symbol = kj::heapString(other.symbol);
    size = other.size;
    entry_price = other.entry_price;
    current_price = other.current_price;
    volatility = other.volatility;
  }
  return *this;
}

StressScenario::StressScenario(const StressScenario& other)
    : id(kj::heapString(other.id)),
      name(kj::heapString(other.name)),
      description(kj::heapString(other.description)),
      type(other.type),
      historical_event(kj::heapString(other.historical_event)),
      historical_start_ns(other.historical_start_ns),
      historical_end_ns(other.historical_end_ns),
      created_at_ns(other.created_at_ns),
      created_by(kj::heapString(other.created_by)) {
  for (const auto& shock : other.shocks) {
    shocks.add(FactorShock(shock));
  }
}

StressScenario& StressScenario::operator=(const StressScenario& other) {
  if (this != &other) {
    id = kj::heapString(other.id);
    name = kj::heapString(other.name);
    description = kj::heapString(other.description);
    type = other.type;
    shocks.clear();
    for (const auto& shock : other.shocks) {
      shocks.add(FactorShock(shock));
    }
    historical_event = kj::heapString(other.historical_event);
    historical_start_ns = other.historical_start_ns;
    historical_end_ns = other.historical_end_ns;
    created_at_ns = other.created_at_ns;
    created_by = kj::heapString(other.created_by);
  }
  return *this;
}

PositionStressResult::PositionStressResult(const PositionStressResult& other)
    : symbol(kj::heapString(other.symbol)),
      base_value(other.base_value),
      stressed_value(other.stressed_value),
      pnl_impact(other.pnl_impact),
      pnl_impact_pct(other.pnl_impact_pct) {}

PositionStressResult& PositionStressResult::operator=(const PositionStressResult& other) {
  if (this != &other) {
    symbol = kj::heapString(other.symbol);
    base_value = other.base_value;
    stressed_value = other.stressed_value;
    pnl_impact = other.pnl_impact;
    pnl_impact_pct = other.pnl_impact_pct;
  }
  return *this;
}

StressTestResult::StressTestResult(const StressTestResult& other)
    : scenario_id(kj::heapString(other.scenario_id)),
      scenario_name(kj::heapString(other.scenario_name)),
      success(other.success),
      error_message(kj::heapString(other.error_message)),
      base_portfolio_value(other.base_portfolio_value),
      stressed_portfolio_value(other.stressed_portfolio_value),
      total_pnl_impact(other.total_pnl_impact),
      total_pnl_impact_pct(other.total_pnl_impact_pct),
      stressed_var_95(other.stressed_var_95),
      stressed_var_99(other.stressed_var_99),
      executed_at_ns(other.executed_at_ns),
      execution_time_us(other.execution_time_us) {
  for (const auto& pr : other.position_results) {
    position_results.add(PositionStressResult(pr));
  }
}

StressTestResult& StressTestResult::operator=(const StressTestResult& other) {
  if (this != &other) {
    scenario_id = kj::heapString(other.scenario_id);
    scenario_name = kj::heapString(other.scenario_name);
    success = other.success;
    error_message = kj::heapString(other.error_message);
    base_portfolio_value = other.base_portfolio_value;
    stressed_portfolio_value = other.stressed_portfolio_value;
    total_pnl_impact = other.total_pnl_impact;
    total_pnl_impact_pct = other.total_pnl_impact_pct;
    position_results.clear();
    for (const auto& pr : other.position_results) {
      position_results.add(PositionStressResult(pr));
    }
    stressed_var_95 = other.stressed_var_95;
    stressed_var_99 = other.stressed_var_99;
    executed_at_ns = other.executed_at_ns;
    execution_time_us = other.execution_time_us;
  }
  return *this;
}

SensitivityResult::SensitivityResult(const SensitivityResult& other)
    : factor(other.factor),
      symbol(kj::heapString(other.symbol)),
      delta(other.delta),
      gamma(other.gamma) {
  shock_levels.addAll(other.shock_levels);
  pnl_impacts.addAll(other.pnl_impacts);
}

SensitivityResult& SensitivityResult::operator=(const SensitivityResult& other) {
  if (this != &other) {
    factor = other.factor;
    symbol = kj::heapString(other.symbol);
    shock_levels.clear();
    shock_levels.addAll(other.shock_levels);
    pnl_impacts.clear();
    pnl_impacts.addAll(other.pnl_impacts);
    delta = other.delta;
    gamma = other.gamma;
  }
  return *this;
}

// ============================================================================
// StressTestEngine Implementation
// ============================================================================

void StressTestEngine::add_scenario(StressScenario scenario) {
  scenarios_.add(kj::mv(scenario));
}

const StressScenario* StressTestEngine::get_scenario(kj::StringPtr id) const {
  for (const auto& scenario : scenarios_) {
    if (scenario.id == id) {
      return &scenario;
    }
  }
  return nullptr;
}

const kj::Vector<StressScenario>& StressTestEngine::get_scenarios() const {
  return scenarios_;
}

bool StressTestEngine::remove_scenario(kj::StringPtr id) {
  for (size_t i = 0; i < scenarios_.size(); ++i) {
    if (scenarios_[i].id == id) {
      // Shift remaining elements
      kj::Vector<StressScenario> new_scenarios;
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

void StressTestEngine::clear_scenarios() {
  scenarios_.clear();
}

void StressTestEngine::add_covid_crash_scenario() {
  auto scenario = StressScenarioBuilder()
      .id("covid_crash_2020")
      .name("COVID-19 Crash (March 2020)")
      .description("BTC dropped ~50% in 24 hours during the COVID-19 market panic")
      .type(StressScenarioType::Historical)
      .historical_event("COVID-19 Black Thursday")
      .price_shock("", -0.50)           // 50% drop across all assets
      .volatility_shock("", 3.0)        // 300% volatility increase
      .liquidity_shock(2.0)             // Spreads doubled
      .build();

  add_scenario(kj::mv(scenario));
}

void StressTestEngine::add_luna_collapse_scenario() {
  auto scenario = StressScenarioBuilder()
      .id("luna_collapse_2022")
      .name("LUNA/UST Collapse (May 2022)")
      .description("LUNA went to near zero, causing contagion across crypto markets")
      .type(StressScenarioType::Historical)
      .historical_event("LUNA/UST Death Spiral")
      .price_shock("LUNA", -0.999)      // LUNA essentially to zero
      .price_shock("UST", -0.90)        // UST depegged
      .price_shock("BTC", -0.30)        // BTC contagion
      .price_shock("ETH", -0.35)        // ETH contagion
      .price_shock("", -0.40)           // Other alts hit harder
      .volatility_shock("", 2.5)
      .correlation_shock(0.3, false)    // Correlation spiked to near 1
      .build();

  add_scenario(kj::mv(scenario));
}

void StressTestEngine::add_ftx_collapse_scenario() {
  auto scenario = StressScenarioBuilder()
      .id("ftx_collapse_2022")
      .name("FTX Collapse (November 2022)")
      .description("FTX exchange collapse caused market-wide panic and contagion fears")
      .type(StressScenarioType::Historical)
      .historical_event("FTX Bankruptcy")
      .price_shock("FTT", -0.95)        // FTT token collapse
      .price_shock("SOL", -0.60)        // SOL heavily affected (Alameda holdings)
      .price_shock("BTC", -0.25)        // BTC dropped
      .price_shock("ETH", -0.28)        // ETH dropped
      .price_shock("", -0.35)           // General market impact
      .volatility_shock("", 2.0)
      .liquidity_shock(1.5)             // Exchange withdrawals caused liquidity issues
      .build();

  add_scenario(kj::mv(scenario));
}

void StressTestEngine::add_flash_crash_scenario() {
  auto scenario = StressScenarioBuilder()
      .id("flash_crash")
      .name("Flash Crash Scenario")
      .description("Sudden 15% drop simulating a flash crash event")
      .type(StressScenarioType::Hypothetical)
      .price_shock("", -0.15)           // 15% sudden drop
      .volatility_shock("", 4.0)        // Extreme short-term volatility
      .liquidity_shock(5.0)             // Liquidity evaporates
      .build();

  add_scenario(kj::mv(scenario));
}

void StressTestEngine::add_all_historical_scenarios() {
  add_covid_crash_scenario();
  add_luna_collapse_scenario();
  add_ftx_collapse_scenario();
  add_flash_crash_scenario();
}

StressTestResult StressTestEngine::run_stress_test(
    kj::StringPtr scenario_id,
    const kj::Vector<StressPosition>& positions) const {

  const StressScenario* scenario = get_scenario(scenario_id);
  if (scenario == nullptr) {
    StressTestResult result;
    result.success = false;
    result.error_message = kj::str("Scenario not found: ", scenario_id);
    return result;
  }

  return apply_shocks(*scenario, positions);
}

StressTestResult StressTestEngine::run_stress_test(
    const StressScenario& scenario,
    const kj::Vector<StressPosition>& positions) const {
  return apply_shocks(scenario, positions);
}

kj::Vector<StressTestResult> StressTestEngine::run_all_scenarios(
    const kj::Vector<StressPosition>& positions) const {

  kj::Vector<StressTestResult> results;
  for (const auto& scenario : scenarios_) {
    results.add(apply_shocks(scenario, positions));
  }
  return results;
}

SensitivityResult StressTestEngine::run_sensitivity_analysis(
    MarketFactor factor,
    const kj::Vector<StressPosition>& positions,
    double shock_min,
    double shock_max,
    int num_points) const {

  return run_sensitivity_analysis(factor, ""_kj, positions, shock_min, shock_max, num_points);
}

SensitivityResult StressTestEngine::run_sensitivity_analysis(
    MarketFactor factor,
    kj::StringPtr symbol,
    const kj::Vector<StressPosition>& positions,
    double shock_min,
    double shock_max,
    int num_points) const {

  SensitivityResult result;
  result.factor = factor;
  result.symbol = kj::str(symbol);

  if (num_points < 3) {
    num_points = 3;
  }

  double step = (shock_max - shock_min) / (num_points - 1);

  // Calculate base portfolio value
  double base_value = 0.0;
  for (const auto& pos : positions) {
    base_value += std::abs(pos.size) * pos.current_price;
  }

  // Run sensitivity at each shock level
  for (int i = 0; i < num_points; ++i) {
    double shock_level = shock_min + i * step;
    result.shock_levels.add(shock_level);

    // Create scenario with single shock
    StressScenario scenario;
    scenario.id = kj::str("sensitivity_", i);
    scenario.type = StressScenarioType::Sensitivity;

    FactorShock shock;
    shock.factor = factor;
    shock.symbol = kj::str(symbol);
    shock.shock_value = shock_level;
    shock.is_relative = true;
    scenario.shocks.add(kj::mv(shock));

    auto test_result = apply_shocks(scenario, positions);
    result.pnl_impacts.add(test_result.total_pnl_impact);
  }

  // Calculate delta (first derivative) using central difference
  if (result.pnl_impacts.size() >= 3) {
    int mid = num_points / 2;
    double h = step;
    if (mid > 0 && mid < static_cast<int>(result.pnl_impacts.size()) - 1) {
      result.delta = (result.pnl_impacts[mid + 1] - result.pnl_impacts[mid - 1]) / (2 * h);

      // Calculate gamma (second derivative)
      result.gamma = (result.pnl_impacts[mid + 1] - 2 * result.pnl_impacts[mid] +
                      result.pnl_impacts[mid - 1]) / (h * h);
    }
  }

  return result;
}

StressTestEngine::ScenarioComparison StressTestEngine::compare_scenarios(
    const kj::Vector<StressTestResult>& results) const {

  ScenarioComparison comparison;
  comparison.scenarios_tested = static_cast<int>(results.size());

  if (results.size() == 0) {
    return comparison;
  }

  double sum_pnl = 0.0;
  double worst_pnl = 0.0;
  double best_pnl = results[0].total_pnl_impact;

  for (const auto& result : results) {
    if (!result.success) {
      continue;
    }

    sum_pnl += result.total_pnl_impact;

    if (result.total_pnl_impact < worst_pnl) {
      worst_pnl = result.total_pnl_impact;
      comparison.worst_scenario_id = kj::str(result.scenario_id);
    }

    if (result.total_pnl_impact > best_pnl) {
      best_pnl = result.total_pnl_impact;
    }
  }

  comparison.worst_pnl_impact = worst_pnl;
  comparison.best_pnl_impact = best_pnl;
  comparison.average_pnl_impact = sum_pnl / results.size();

  return comparison;
}

StressTestResult StressTestEngine::apply_shocks(
    const StressScenario& scenario,
    const kj::Vector<StressPosition>& positions) const {

  auto start_time = std::chrono::high_resolution_clock::now();

  StressTestResult result;
  result.scenario_id = kj::str(scenario.id);
  result.scenario_name = kj::str(scenario.name);

  if (positions.size() == 0) {
    result.success = true;
    result.executed_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return result;
  }

  // Calculate base and stressed values for each position
  double total_base = 0.0;
  double total_stressed = 0.0;

  for (const auto& pos : positions) {
    PositionStressResult pos_result;
    pos_result.symbol = kj::str(pos.symbol);

    // Base value (current unrealized P&L)
    double base_pnl = pos.size * (pos.current_price - pos.entry_price);
    pos_result.base_value = base_pnl;

    // Calculate stressed price
    double stressed_price = calculate_stressed_price(pos, scenario.shocks);

    // Stressed value
    double stressed_pnl = pos.size * (stressed_price - pos.entry_price);
    pos_result.stressed_value = stressed_pnl;

    // P&L impact
    pos_result.pnl_impact = stressed_pnl - base_pnl;
    if (std::abs(base_pnl) > 0.0001) {
      pos_result.pnl_impact_pct = pos_result.pnl_impact / std::abs(base_pnl) * 100.0;
    }

    total_base += std::abs(pos.size) * pos.current_price;
    total_stressed += std::abs(pos.size) * stressed_price;

    result.position_results.add(kj::mv(pos_result));
  }

  result.base_portfolio_value = total_base;
  result.stressed_portfolio_value = total_stressed;
  result.total_pnl_impact = total_stressed - total_base;

  if (total_base > 0.0) {
    result.total_pnl_impact_pct = result.total_pnl_impact / total_base * 100.0;
  }

  result.success = true;

  auto end_time = std::chrono::high_resolution_clock::now();
  result.execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time).count();
  result.executed_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  return result;
}

double StressTestEngine::calculate_stressed_price(
    const StressPosition& position,
    const kj::Vector<FactorShock>& shocks) const {

  double price = position.current_price;

  // Look for symbol-specific price shock first
  const FactorShock* specific_shock = find_shock(shocks, position.symbol, MarketFactor::Price);
  const FactorShock* general_shock = find_shock(shocks, ""_kj, MarketFactor::Price);

  const FactorShock* price_shock = specific_shock ? specific_shock : general_shock;

  if (price_shock != nullptr) {
    if (price_shock->is_relative) {
      price *= (1.0 + price_shock->shock_value);
    } else {
      price = price_shock->shock_value;
    }
  }

  // Ensure price doesn't go negative
  if (price < 0.0) {
    price = 0.0;
  }

  return price;
}

const FactorShock* StressTestEngine::find_shock(
    const kj::Vector<FactorShock>& shocks,
    kj::StringPtr symbol,
    MarketFactor factor) const {

  for (const auto& shock : shocks) {
    if (shock.factor == factor && shock.symbol == symbol) {
      return &shock;
    }
  }
  return nullptr;
}

// ============================================================================
// StressScenarioBuilder Implementation
// ============================================================================

StressScenarioBuilder& StressScenarioBuilder::id(kj::StringPtr id) {
  scenario_.id = kj::str(id);
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::name(kj::StringPtr name) {
  scenario_.name = kj::str(name);
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::description(kj::StringPtr desc) {
  scenario_.description = kj::str(desc);
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::type(StressScenarioType type) {
  scenario_.type = type;
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::price_shock(kj::StringPtr symbol, double shock_pct) {
  FactorShock shock;
  shock.factor = MarketFactor::Price;
  shock.symbol = kj::str(symbol);
  shock.shock_value = shock_pct;
  shock.is_relative = true;
  scenario_.shocks.add(kj::mv(shock));
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::volatility_shock(kj::StringPtr symbol,
                                                                double shock_pct) {
  FactorShock shock;
  shock.factor = MarketFactor::Volatility;
  shock.symbol = kj::str(symbol);
  shock.shock_value = shock_pct;
  shock.is_relative = true;
  scenario_.shocks.add(kj::mv(shock));
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::correlation_shock(double shock_value,
                                                                 bool is_relative) {
  FactorShock shock;
  shock.factor = MarketFactor::Correlation;
  shock.shock_value = shock_value;
  shock.is_relative = is_relative;
  scenario_.shocks.add(kj::mv(shock));
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::liquidity_shock(double shock_pct) {
  FactorShock shock;
  shock.factor = MarketFactor::Liquidity;
  shock.shock_value = shock_pct;
  shock.is_relative = true;
  scenario_.shocks.add(kj::mv(shock));
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::add_shock(FactorShock shock) {
  scenario_.shocks.add(kj::mv(shock));
  return *this;
}

StressScenarioBuilder& StressScenarioBuilder::historical_event(kj::StringPtr event) {
  scenario_.historical_event = kj::str(event);
  return *this;
}

StressScenario StressScenarioBuilder::build() {
  scenario_.created_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  return kj::mv(scenario_);
}

} // namespace veloz::risk
