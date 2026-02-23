#pragma once

#include "veloz/risk/stress_testing.h"
#include "veloz/risk/var_models.h"

#include <cstdint>
// std::pair for return type (KJ lacks pair equivalent)
#include <kj/common.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <utility>

namespace veloz::risk {

/**
 * @brief Scenario impact type
 */
enum class ScenarioImpactType : std::uint8_t {
  PnL = 0,       ///< Profit/Loss impact
  VaR = 1,       ///< VaR change
  Margin = 2,    ///< Margin requirement change
  Liquidity = 3, ///< Liquidation risk
};

/**
 * @brief Scenario probability assessment
 */
enum class ScenarioProbability : std::uint8_t {
  VeryLow = 0,  ///< < 1% probability
  Low = 1,      ///< 1-5% probability
  Medium = 2,   ///< 5-20% probability
  High = 3,     ///< 20-50% probability
  VeryHigh = 4, ///< > 50% probability
};

/**
 * @brief Convert ScenarioProbability to string
 */
[[nodiscard]] kj::StringPtr scenario_probability_to_string(ScenarioProbability prob);

/**
 * @brief Get probability range for a ScenarioProbability level
 * @return Pair of (min_probability, max_probability)
 */
[[nodiscard]] std::pair<double, double> get_probability_range(ScenarioProbability prob);

/**
 * @brief Enhanced scenario definition with probability and time horizon
 */
struct EnhancedScenario {
  // Base scenario
  StressScenario base_scenario;

  // Probability assessment
  ScenarioProbability probability{ScenarioProbability::Low};
  double probability_estimate{0.05}; ///< Numeric probability (0-1)

  // Time horizon
  int time_horizon_days{1};    ///< Scenario time horizon
  bool is_instantaneous{true}; ///< True if shock is immediate

  // Scenario category
  kj::String category;         ///< e.g., "Market Crash", "Liquidity Crisis"
  kj::Vector<kj::String> tags; ///< Additional tags for filtering

  // Expected recovery
  int expected_recovery_days{0}; ///< Expected days to recover
  double recovery_rate{0.0};     ///< Expected daily recovery rate

  EnhancedScenario() = default;
  EnhancedScenario(EnhancedScenario&&) = default;
  EnhancedScenario& operator=(EnhancedScenario&&) = default;
  EnhancedScenario(const EnhancedScenario& other);
  EnhancedScenario& operator=(const EnhancedScenario& other);
};

/**
 * @brief Portfolio impact analysis result
 */
struct PortfolioImpactResult {
  // Scenario identification
  kj::String scenario_id;
  kj::String scenario_name;

  // P&L impact
  double immediate_pnl{0.0};  ///< Immediate P&L impact
  double expected_pnl{0.0};   ///< Probability-weighted P&L
  double worst_case_pnl{0.0}; ///< Worst case P&L (no recovery)

  // Risk metrics impact
  double base_var_95{0.0};
  double stressed_var_95{0.0};
  double var_increase_pct{0.0};

  // Position-level breakdown
  kj::Vector<PositionStressResult> position_impacts;

  // Risk indicators
  bool margin_call_risk{false};   ///< Would trigger margin call
  bool liquidation_risk{false};   ///< Would trigger liquidation
  double margin_utilization{0.0}; ///< Post-stress margin utilization

  // Recovery analysis
  int days_to_breakeven{0};         ///< Days to recover losses
  double recovery_probability{0.0}; ///< Probability of full recovery

  PortfolioImpactResult() = default;
  PortfolioImpactResult(PortfolioImpactResult&&) = default;
  PortfolioImpactResult& operator=(PortfolioImpactResult&&) = default;
  PortfolioImpactResult(const PortfolioImpactResult& other);
  PortfolioImpactResult& operator=(const PortfolioImpactResult& other);
};

/**
 * @brief Scenario comparison result
 */
struct ScenarioComparisonResult {
  // Scenarios compared
  kj::Vector<kj::String> scenario_ids;
  int scenarios_count{0};

  // Aggregate statistics
  double worst_pnl{0.0};
  double best_pnl{0.0};
  double average_pnl{0.0};
  double median_pnl{0.0};
  double expected_pnl{0.0}; ///< Probability-weighted average

  // Worst scenario details
  kj::String worst_scenario_id;
  kj::String worst_scenario_name;

  // Risk distribution
  double pnl_std_dev{0.0};
  double pnl_5th_percentile{0.0};  ///< 5th percentile P&L
  double pnl_95th_percentile{0.0}; ///< 95th percentile P&L

  // Correlation analysis
  int correlated_scenarios{0}; ///< Scenarios with similar impacts

  ScenarioComparisonResult() = default;
  ScenarioComparisonResult(ScenarioComparisonResult&&) = default;
  ScenarioComparisonResult& operator=(ScenarioComparisonResult&&) = default;
};

/**
 * @brief Risk budget allocation
 */
struct RiskBudget {
  kj::String name;
  double max_var{0.0};         ///< Maximum VaR allocation
  double max_stress_loss{0.0}; ///< Maximum stress loss
  double current_var{0.0};
  double current_stress_loss{0.0};
  double utilization_pct{0.0}; ///< Current utilization percentage

  RiskBudget() = default;
  RiskBudget(RiskBudget&&) = default;
  RiskBudget& operator=(RiskBudget&&) = default;
  RiskBudget(const RiskBudget& other);
  RiskBudget& operator=(const RiskBudget& other);
};

/**
 * @brief Scenario analysis engine
 *
 * Provides advanced scenario analysis capabilities including:
 * - Enhanced scenario definitions with probability
 * - Portfolio impact analysis
 * - Scenario comparison and ranking
 * - Risk budgeting
 */
class ScenarioAnalysisEngine final {
public:
  ScenarioAnalysisEngine() = default;

  // === Scenario Management ===

  /**
   * @brief Add an enhanced scenario
   */
  void add_scenario(EnhancedScenario scenario);

  /**
   * @brief Get scenario by ID
   */
  [[nodiscard]] const EnhancedScenario* get_scenario(kj::StringPtr id) const;

  /**
   * @brief Get all scenarios
   */
  [[nodiscard]] const kj::Vector<EnhancedScenario>& get_scenarios() const;

  /**
   * @brief Get scenarios by category
   */
  [[nodiscard]] kj::Vector<const EnhancedScenario*>
  get_scenarios_by_category(kj::StringPtr category) const;

  /**
   * @brief Get scenarios by tag
   */
  [[nodiscard]] kj::Vector<const EnhancedScenario*> get_scenarios_by_tag(kj::StringPtr tag) const;

  /**
   * @brief Remove scenario
   */
  bool remove_scenario(kj::StringPtr id);

  /**
   * @brief Clear all scenarios
   */
  void clear_scenarios();

  // === Portfolio Impact Analysis ===

  /**
   * @brief Analyze portfolio impact for a single scenario
   * @param scenario_id Scenario to analyze
   * @param positions Portfolio positions
   * @param account_equity Current account equity
   * @param margin_requirement Current margin requirement
   * @return Portfolio impact result
   */
  [[nodiscard]] PortfolioImpactResult analyze_impact(kj::StringPtr scenario_id,
                                                     const kj::Vector<StressPosition>& positions,
                                                     double account_equity,
                                                     double margin_requirement = 0.0) const;

  /**
   * @brief Analyze portfolio impact for all scenarios
   */
  [[nodiscard]] kj::Vector<PortfolioImpactResult>
  analyze_all_impacts(const kj::Vector<StressPosition>& positions, double account_equity,
                      double margin_requirement = 0.0) const;

  // === Scenario Comparison ===

  /**
   * @brief Compare multiple scenarios
   * @param impacts Vector of impact results to compare
   * @return Comparison result with statistics
   */
  [[nodiscard]] ScenarioComparisonResult
  compare_scenarios(const kj::Vector<PortfolioImpactResult>& impacts) const;

  /**
   * @brief Rank scenarios by impact severity
   * @param impacts Impact results to rank
   * @return Vector of scenario IDs sorted by severity (worst first)
   */
  [[nodiscard]] kj::Vector<kj::String>
  rank_by_severity(const kj::Vector<PortfolioImpactResult>& impacts) const;

  /**
   * @brief Calculate expected loss across all scenarios
   * @param impacts Impact results
   * @return Probability-weighted expected loss
   */
  [[nodiscard]] double
  calculate_expected_loss(const kj::Vector<PortfolioImpactResult>& impacts) const;

  // === Risk Budgeting ===

  /**
   * @brief Set risk budget
   */
  void set_risk_budget(RiskBudget budget);

  /**
   * @brief Get current risk budget
   */
  [[nodiscard]] const RiskBudget& get_risk_budget() const;

  /**
   * @brief Check if portfolio is within risk budget
   * @param impacts Scenario impact results
   * @return True if within budget
   */
  [[nodiscard]] bool is_within_budget(const kj::Vector<PortfolioImpactResult>& impacts) const;

  /**
   * @brief Calculate budget utilization
   * @param impacts Scenario impact results
   * @return Budget utilization percentage
   */
  [[nodiscard]] double
  calculate_budget_utilization(const kj::Vector<PortfolioImpactResult>& impacts) const;

  // === Scenario Generation ===

  /**
   * @brief Generate reverse stress test scenario
   *
   * Finds the scenario that would cause a specific loss level.
   *
   * @param positions Portfolio positions
   * @param target_loss Target loss amount
   * @return Generated scenario that would cause approximately target_loss
   */
  [[nodiscard]] EnhancedScenario
  generate_reverse_stress_scenario(const kj::Vector<StressPosition>& positions,
                                   double target_loss) const;

  /**
   * @brief Generate worst-case scenario
   *
   * Generates a scenario combining worst factors from all scenarios.
   *
   * @return Worst-case combined scenario
   */
  [[nodiscard]] EnhancedScenario generate_worst_case_scenario() const;

  // === Configuration ===

  /**
   * @brief Set liquidation threshold
   * @param threshold Margin utilization threshold for liquidation (0-1)
   */
  void set_liquidation_threshold(double threshold);

  /**
   * @brief Set margin call threshold
   * @param threshold Margin utilization threshold for margin call (0-1)
   */
  void set_margin_call_threshold(double threshold);

private:
  /**
   * @brief Calculate stressed VaR for a scenario
   */
  [[nodiscard]] double calculate_stressed_var(const kj::Vector<StressPosition>& positions,
                                              const EnhancedScenario& scenario,
                                              double confidence = 0.95) const;

  /**
   * @brief Check margin/liquidation risk
   */
  void check_margin_risk(PortfolioImpactResult& result, double account_equity,
                         double margin_requirement) const;

  kj::Vector<EnhancedScenario> scenarios_;
  RiskBudget risk_budget_;
  double liquidation_threshold_{0.9}; // 90% margin utilization
  double margin_call_threshold_{0.8}; // 80% margin utilization
};

/**
 * @brief Builder for enhanced scenarios
 */
class EnhancedScenarioBuilder final {
public:
  EnhancedScenarioBuilder() = default;

  /**
   * @brief Set base scenario from StressScenarioBuilder
   */
  EnhancedScenarioBuilder& base_scenario(StressScenario scenario);

  /**
   * @brief Set scenario ID
   */
  EnhancedScenarioBuilder& id(kj::StringPtr id);

  /**
   * @brief Set scenario name
   */
  EnhancedScenarioBuilder& name(kj::StringPtr name);

  /**
   * @brief Set scenario description
   */
  EnhancedScenarioBuilder& description(kj::StringPtr desc);

  /**
   * @brief Add price shock
   */
  EnhancedScenarioBuilder& price_shock(kj::StringPtr symbol, double shock_pct);

  /**
   * @brief Add volatility shock
   */
  EnhancedScenarioBuilder& volatility_shock(kj::StringPtr symbol, double shock_pct);

  /**
   * @brief Set probability level
   */
  EnhancedScenarioBuilder& probability(ScenarioProbability prob);

  /**
   * @brief Set numeric probability estimate
   */
  EnhancedScenarioBuilder& probability_estimate(double prob);

  /**
   * @brief Set time horizon
   */
  EnhancedScenarioBuilder& time_horizon(int days);

  /**
   * @brief Set as instantaneous shock
   */
  EnhancedScenarioBuilder& instantaneous(bool instant = true);

  /**
   * @brief Set category
   */
  EnhancedScenarioBuilder& category(kj::StringPtr cat);

  /**
   * @brief Add tag
   */
  EnhancedScenarioBuilder& tag(kj::StringPtr t);

  /**
   * @brief Set expected recovery
   */
  EnhancedScenarioBuilder& recovery(int days, double rate);

  /**
   * @brief Build the enhanced scenario
   */
  [[nodiscard]] EnhancedScenario build();

private:
  EnhancedScenario scenario_;
  bool has_base_scenario_{false};
};

} // namespace veloz::risk
