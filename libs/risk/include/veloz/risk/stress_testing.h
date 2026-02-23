#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief Stress scenario type enumeration
 */
enum class StressScenarioType : std::uint8_t {
  Historical = 0,   ///< Replay of historical market event
  Hypothetical = 1, ///< User-defined shock scenario
  Sensitivity = 2,  ///< Single-factor sensitivity analysis
};

/**
 * @brief Convert StressScenarioType to string
 */
[[nodiscard]] kj::StringPtr stress_scenario_type_to_string(StressScenarioType type);

/**
 * @brief Market factor for stress testing
 */
enum class MarketFactor : std::uint8_t {
  Price = 0,        ///< Asset price
  Volatility = 1,   ///< Implied/realized volatility
  Correlation = 2,  ///< Cross-asset correlation
  Liquidity = 3,    ///< Market liquidity/spread
  InterestRate = 4, ///< Interest rate
  FundingRate = 5,  ///< Crypto funding rate
};

/**
 * @brief Convert MarketFactor to string
 */
[[nodiscard]] kj::StringPtr market_factor_to_string(MarketFactor factor);

/**
 * @brief Shock definition for a single factor
 */
struct FactorShock {
  MarketFactor factor{MarketFactor::Price};
  kj::String symbol;       ///< Asset symbol (empty for portfolio-wide)
  double shock_value{0.0}; ///< Shock magnitude
  bool is_relative{true};  ///< True: percentage change, False: absolute change

  FactorShock() = default;
  FactorShock(FactorShock&&) = default;
  FactorShock& operator=(FactorShock&&) = default;
  FactorShock(const FactorShock& other);
  FactorShock& operator=(const FactorShock& other);
};

/**
 * @brief Position for stress testing
 */
struct StressPosition {
  kj::String symbol;
  double size{0.0};          ///< Position size (signed)
  double entry_price{0.0};   ///< Entry price
  double current_price{0.0}; ///< Current market price
  double volatility{0.0};    ///< Current volatility

  StressPosition() = default;
  StressPosition(StressPosition&&) = default;
  StressPosition& operator=(StressPosition&&) = default;
  StressPosition(const StressPosition& other);
  StressPosition& operator=(const StressPosition& other);
};

/**
 * @brief Stress scenario definition
 */
struct StressScenario {
  kj::String id;
  kj::String name;
  kj::String description;
  StressScenarioType type{StressScenarioType::Hypothetical};

  // Factor shocks to apply
  kj::Vector<FactorShock> shocks;

  // Historical scenario metadata
  kj::String historical_event;    ///< e.g., "COVID-19 March 2020"
  int64_t historical_start_ns{0}; ///< Start timestamp
  int64_t historical_end_ns{0};   ///< End timestamp

  // Scenario metadata
  int64_t created_at_ns{0};
  kj::String created_by;

  StressScenario() = default;
  StressScenario(StressScenario&&) = default;
  StressScenario& operator=(StressScenario&&) = default;
  StressScenario(const StressScenario& other);
  StressScenario& operator=(const StressScenario& other);
};

/**
 * @brief Result of stress test for a single position
 */
struct PositionStressResult {
  kj::String symbol;
  double base_value{0.0};     ///< Value before stress
  double stressed_value{0.0}; ///< Value after stress
  double pnl_impact{0.0};     ///< P&L change
  double pnl_impact_pct{0.0}; ///< P&L change as percentage

  PositionStressResult() = default;
  PositionStressResult(PositionStressResult&&) = default;
  PositionStressResult& operator=(PositionStressResult&&) = default;
  PositionStressResult(const PositionStressResult& other);
  PositionStressResult& operator=(const PositionStressResult& other);
};

/**
 * @brief Result of stress test for entire portfolio
 */
struct StressTestResult {
  kj::String scenario_id;
  kj::String scenario_name;
  bool success{false};
  kj::String error_message;

  // Portfolio-level results
  double base_portfolio_value{0.0};
  double stressed_portfolio_value{0.0};
  double total_pnl_impact{0.0};
  double total_pnl_impact_pct{0.0};

  // Position-level breakdown
  kj::Vector<PositionStressResult> position_results;

  // Risk metrics under stress
  double stressed_var_95{0.0};
  double stressed_var_99{0.0};

  // Execution timestamp
  int64_t executed_at_ns{0};
  int64_t execution_time_us{0};

  StressTestResult() = default;
  StressTestResult(StressTestResult&&) = default;
  StressTestResult& operator=(StressTestResult&&) = default;
  StressTestResult(const StressTestResult& other);
  StressTestResult& operator=(const StressTestResult& other);
};

/**
 * @brief Sensitivity analysis result
 */
struct SensitivityResult {
  MarketFactor factor{MarketFactor::Price};
  kj::String symbol;
  kj::Vector<double> shock_levels; ///< Shock magnitudes tested
  kj::Vector<double> pnl_impacts;  ///< Corresponding P&L impacts

  // Key metrics
  double delta{0.0}; ///< First-order sensitivity (linear)
  double gamma{0.0}; ///< Second-order sensitivity (convexity)

  SensitivityResult() = default;
  SensitivityResult(SensitivityResult&&) = default;
  SensitivityResult& operator=(SensitivityResult&&) = default;
  SensitivityResult(const SensitivityResult& other);
  SensitivityResult& operator=(const SensitivityResult& other);
};

/**
 * @brief Stress testing engine
 *
 * Runs stress tests on portfolios using historical or hypothetical scenarios.
 * Supports sensitivity analysis and scenario comparison.
 */
class StressTestEngine final {
public:
  StressTestEngine() = default;

  // === Scenario Management ===

  /**
   * @brief Add a stress scenario
   */
  void add_scenario(StressScenario scenario);

  /**
   * @brief Get a scenario by ID
   */
  [[nodiscard]] const StressScenario* get_scenario(kj::StringPtr id) const;

  /**
   * @brief Get all scenarios
   */
  [[nodiscard]] const kj::Vector<StressScenario>& get_scenarios() const;

  /**
   * @brief Remove a scenario
   */
  bool remove_scenario(kj::StringPtr id);

  /**
   * @brief Clear all scenarios
   */
  void clear_scenarios();

  // === Built-in Historical Scenarios ===

  /**
   * @brief Add COVID-19 March 2020 crash scenario
   *
   * BTC dropped ~50% in 24 hours, volatility spiked 300%+
   */
  void add_covid_crash_scenario();

  /**
   * @brief Add LUNA/UST collapse scenario (May 2022)
   *
   * LUNA went to near zero, BTC dropped ~30%
   */
  void add_luna_collapse_scenario();

  /**
   * @brief Add FTX collapse scenario (November 2022)
   *
   * BTC dropped ~25%, exchange contagion fears
   */
  void add_ftx_collapse_scenario();

  /**
   * @brief Add flash crash scenario
   *
   * Sudden 10-20% drop and recovery within minutes
   */
  void add_flash_crash_scenario();

  /**
   * @brief Add all built-in historical scenarios
   */
  void add_all_historical_scenarios();

  // === Stress Test Execution ===

  /**
   * @brief Run stress test with a specific scenario
   * @param scenario_id Scenario ID to run
   * @param positions Portfolio positions
   * @return Stress test result
   */
  [[nodiscard]] StressTestResult run_stress_test(kj::StringPtr scenario_id,
                                                 const kj::Vector<StressPosition>& positions) const;

  /**
   * @brief Run stress test with a custom scenario (not stored)
   * @param scenario Scenario to run
   * @param positions Portfolio positions
   * @return Stress test result
   */
  [[nodiscard]] StressTestResult run_stress_test(const StressScenario& scenario,
                                                 const kj::Vector<StressPosition>& positions) const;

  /**
   * @brief Run all scenarios and return results
   * @param positions Portfolio positions
   * @return Vector of stress test results
   */
  [[nodiscard]] kj::Vector<StressTestResult>
  run_all_scenarios(const kj::Vector<StressPosition>& positions) const;

  // === Sensitivity Analysis ===

  /**
   * @brief Run sensitivity analysis for a single factor
   * @param factor Market factor to analyze
   * @param positions Portfolio positions
   * @param shock_range Range of shocks to test (e.g., -0.5 to +0.5)
   * @param num_points Number of points in the range
   * @return Sensitivity result
   */
  [[nodiscard]] SensitivityResult
  run_sensitivity_analysis(MarketFactor factor, const kj::Vector<StressPosition>& positions,
                           double shock_min, double shock_max, int num_points = 21) const;

  /**
   * @brief Run sensitivity analysis for a specific symbol
   */
  [[nodiscard]] SensitivityResult
  run_sensitivity_analysis(MarketFactor factor, kj::StringPtr symbol,
                           const kj::Vector<StressPosition>& positions, double shock_min,
                           double shock_max, int num_points = 21) const;

  // === Scenario Comparison ===

  /**
   * @brief Compare results across multiple scenarios
   * @param results Vector of stress test results
   * @return Summary comparison (worst case, average, etc.)
   */
  struct ScenarioComparison {
    kj::String worst_scenario_id;
    double worst_pnl_impact{0.0};
    double average_pnl_impact{0.0};
    double best_pnl_impact{0.0};
    int scenarios_tested{0};
  };

  [[nodiscard]] ScenarioComparison
  compare_scenarios(const kj::Vector<StressTestResult>& results) const;

private:
  /**
   * @brief Apply shocks to positions and calculate stressed values
   */
  [[nodiscard]] StressTestResult apply_shocks(const StressScenario& scenario,
                                              const kj::Vector<StressPosition>& positions) const;

  /**
   * @brief Calculate stressed price for a position
   */
  [[nodiscard]] double calculate_stressed_price(const StressPosition& position,
                                                const kj::Vector<FactorShock>& shocks) const;

  /**
   * @brief Find shock for a specific symbol and factor
   */
  [[nodiscard]] const FactorShock* find_shock(const kj::Vector<FactorShock>& shocks,
                                              kj::StringPtr symbol, MarketFactor factor) const;

  kj::Vector<StressScenario> scenarios_;
};

/**
 * @brief Builder for creating stress scenarios
 */
class StressScenarioBuilder final {
public:
  StressScenarioBuilder() = default;

  /**
   * @brief Set scenario ID
   */
  StressScenarioBuilder& id(kj::StringPtr id);

  /**
   * @brief Set scenario name
   */
  StressScenarioBuilder& name(kj::StringPtr name);

  /**
   * @brief Set scenario description
   */
  StressScenarioBuilder& description(kj::StringPtr desc);

  /**
   * @brief Set scenario type
   */
  StressScenarioBuilder& type(StressScenarioType type);

  /**
   * @brief Add a price shock (percentage)
   * @param symbol Asset symbol (empty for all assets)
   * @param shock_pct Price change percentage (e.g., -0.30 for -30%)
   */
  StressScenarioBuilder& price_shock(kj::StringPtr symbol, double shock_pct);

  /**
   * @brief Add a volatility shock (percentage)
   * @param symbol Asset symbol (empty for all assets)
   * @param shock_pct Volatility change percentage
   */
  StressScenarioBuilder& volatility_shock(kj::StringPtr symbol, double shock_pct);

  /**
   * @brief Add a correlation shock
   * @param shock_value New correlation value or change
   * @param is_relative True if change, false if absolute
   */
  StressScenarioBuilder& correlation_shock(double shock_value, bool is_relative = true);

  /**
   * @brief Add a liquidity shock
   * @param shock_pct Spread/liquidity change percentage
   */
  StressScenarioBuilder& liquidity_shock(double shock_pct);

  /**
   * @brief Add a custom factor shock
   */
  StressScenarioBuilder& add_shock(FactorShock shock);

  /**
   * @brief Set historical event name
   */
  StressScenarioBuilder& historical_event(kj::StringPtr event);

  /**
   * @brief Build the scenario
   */
  [[nodiscard]] StressScenario build();

private:
  StressScenario scenario_;
};

} // namespace veloz::risk
