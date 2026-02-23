#pragma once

#include "veloz/risk/risk_metrics.h"
#include "veloz/risk/var_models.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief Risk contribution for a single position
 */
struct PositionRiskContribution {
  kj::String symbol;
  double position_value{0.0};
  double weight{0.0};                  ///< Portfolio weight (0-1)
  double standalone_var{0.0};          ///< VaR if only this position
  double marginal_var{0.0};            ///< Change in portfolio VaR per unit
  double component_var{0.0};           ///< Contribution to portfolio VaR
  double pct_contribution{0.0};        ///< Percentage of total VaR
  double diversification_benefit{0.0}; ///< Reduction from diversification

  PositionRiskContribution() = default;
  PositionRiskContribution(PositionRiskContribution&&) = default;
  PositionRiskContribution& operator=(PositionRiskContribution&&) = default;
  PositionRiskContribution(const PositionRiskContribution& other);
  PositionRiskContribution& operator=(const PositionRiskContribution& other);
};

/**
 * @brief Risk budget allocation for a position or strategy
 */
struct RiskAllocation {
  kj::String name;
  double allocated_var{0.0};   ///< Allocated VaR budget
  double used_var{0.0};        ///< Current VaR usage
  double utilization_pct{0.0}; ///< Usage as percentage
  double remaining_var{0.0};   ///< Remaining VaR budget
  bool is_breached{false};     ///< True if over budget

  RiskAllocation() = default;
  RiskAllocation(RiskAllocation&&) = default;
  RiskAllocation& operator=(RiskAllocation&&) = default;
  RiskAllocation(const RiskAllocation& other);
  RiskAllocation& operator=(const RiskAllocation& other);
};

/**
 * @brief Portfolio risk summary
 */
struct PortfolioRiskSummary {
  // Portfolio metrics
  double total_value{0.0};
  double total_var_95{0.0};
  double total_var_99{0.0};
  double total_cvar_95{0.0};
  double undiversified_var{0.0};       ///< Sum of individual VaRs
  double diversification_benefit{0.0}; ///< Reduction from diversification

  // Risk breakdown
  kj::Vector<PositionRiskContribution> contributions;

  // Concentration metrics
  double herfindahl_index{0.0};
  int position_count{0};
  kj::String largest_risk_contributor;
  double largest_contribution_pct{0.0};

  // Risk budgets
  kj::Vector<RiskAllocation> allocations;

  // Timestamp
  int64_t calculated_at_ns{0};

  PortfolioRiskSummary() = default;
  PortfolioRiskSummary(PortfolioRiskSummary&&) = default;
  PortfolioRiskSummary& operator=(PortfolioRiskSummary&&) = default;
};

/**
 * @brief Portfolio position with risk data
 */
struct PortfolioPosition {
  kj::String symbol;
  double size{0.0};
  double price{0.0};
  double value{0.0};
  double volatility{0.0}; ///< Annualized volatility
  kj::String strategy;    ///< Optional: strategy name

  PortfolioPosition() = default;
  PortfolioPosition(PortfolioPosition&&) = default;
  PortfolioPosition& operator=(PortfolioPosition&&) = default;
  PortfolioPosition(const PortfolioPosition& other);
  PortfolioPosition& operator=(const PortfolioPosition& other);
};

/**
 * @brief Portfolio risk aggregator
 *
 * Aggregates risk across multiple positions considering correlations.
 * Provides risk contribution analysis and risk budgeting.
 */
class PortfolioRiskAggregator final {
public:
  PortfolioRiskAggregator() = default;

  // === Position Management ===

  /**
   * @brief Update a position
   */
  void update_position(PortfolioPosition position);

  /**
   * @brief Remove a position
   */
  void remove_position(kj::StringPtr symbol);

  /**
   * @brief Get position
   */
  [[nodiscard]] const PortfolioPosition* get_position(kj::StringPtr symbol) const;

  /**
   * @brief Get all positions
   */
  [[nodiscard]] const kj::Vector<PortfolioPosition>& get_positions() const;

  /**
   * @brief Clear all positions
   */
  void clear_positions();

  // === Correlation Management ===

  /**
   * @brief Set correlation between two assets
   */
  void set_correlation(kj::StringPtr symbol1, kj::StringPtr symbol2, double correlation);

  /**
   * @brief Get correlation between two assets
   */
  [[nodiscard]] double get_correlation(kj::StringPtr symbol1, kj::StringPtr symbol2) const;

  /**
   * @brief Set default correlation for unknown pairs
   */
  void set_default_correlation(double correlation);

  /**
   * @brief Clear all correlations
   */
  void clear_correlations();

  // === Risk Budget Management ===

  /**
   * @brief Set risk budget for a strategy
   */
  void set_risk_budget(kj::StringPtr name, double var_budget);

  /**
   * @brief Get risk budget
   */
  [[nodiscard]] double get_risk_budget(kj::StringPtr name) const;

  /**
   * @brief Set total portfolio risk budget
   */
  void set_total_risk_budget(double var_budget);

  // === Risk Calculation ===

  /**
   * @brief Calculate portfolio risk summary
   * @param confidence VaR confidence level (default 0.95)
   * @return Portfolio risk summary
   */
  [[nodiscard]] PortfolioRiskSummary calculate_risk(double confidence = 0.95) const;

  /**
   * @brief Calculate risk contribution for each position
   */
  [[nodiscard]] kj::Vector<PositionRiskContribution>
  calculate_contributions(double confidence = 0.95) const;

  /**
   * @brief Calculate risk budget utilization
   */
  [[nodiscard]] kj::Vector<RiskAllocation> calculate_allocations() const;

  /**
   * @brief Check if any risk budget is breached
   */
  [[nodiscard]] bool is_any_budget_breached() const;

  // === Optimization Hints ===

  /**
   * @brief Get positions that should be reduced to meet budget
   * @param target_var Target portfolio VaR
   * @return Vector of (symbol, reduction_amount) pairs
   */
  [[nodiscard]] kj::Vector<std::pair<kj::String, double>>
  suggest_reductions(double target_var) const;

  /**
   * @brief Calculate maximum position size within risk budget
   * @param symbol Asset symbol
   * @param available_budget Available VaR budget
   * @return Maximum position value
   */
  [[nodiscard]] double calculate_max_position(kj::StringPtr symbol, double available_budget) const;

  // === Configuration ===

  /**
   * @brief Set VaR calculation method
   */
  void set_var_method(VaRMethod method);

  /**
   * @brief Set holding period for VaR
   */
  void set_holding_period(int days);

private:
  /**
   * @brief Calculate covariance matrix
   */
  [[nodiscard]] kj::Vector<CovarianceEntry> build_covariance_matrix() const;

  /**
   * @brief Calculate portfolio variance
   */
  [[nodiscard]] double calculate_portfolio_variance() const;

  kj::Vector<PortfolioPosition> positions_;
  kj::HashMap<kj::String, double> correlations_; // "sym1:sym2" -> correlation
  kj::HashMap<kj::String, double> risk_budgets_; // strategy -> var_budget
  double default_correlation_{0.5};
  double total_risk_budget_{0.0};
  VaRMethod var_method_{VaRMethod::Parametric};
  int holding_period_days_{1};
};

/**
 * @brief Real-time portfolio risk monitor
 *
 * Monitors portfolio risk in real-time and triggers alerts
 * when thresholds are breached.
 */
class PortfolioRiskMonitor final {
public:
  PortfolioRiskMonitor() = default;

  /**
   * @brief Risk alert levels
   */
  enum class AlertLevel : std::uint8_t {
    Info = 0,
    Warning = 1,
    Critical = 2,
  };

  /**
   * @brief Risk alert
   */
  struct RiskAlert {
    AlertLevel level{AlertLevel::Info};
    kj::String message;
    kj::String symbol; ///< Related symbol (if applicable)
    double current_value{0.0};
    double threshold{0.0};
    int64_t timestamp_ns{0};
  };

  /**
   * @brief Alert callback type
   */
  using AlertCallback = kj::Function<void(const RiskAlert&)>;

  // === Threshold Configuration ===

  /**
   * @brief Set VaR warning threshold (as % of budget)
   */
  void set_var_warning_threshold(double pct);

  /**
   * @brief Set VaR critical threshold (as % of budget)
   */
  void set_var_critical_threshold(double pct);

  /**
   * @brief Set concentration warning threshold
   */
  void set_concentration_warning_threshold(double pct);

  /**
   * @brief Set drawdown warning threshold
   */
  void set_drawdown_warning_threshold(double pct);

  // === Monitoring ===

  /**
   * @brief Check risk levels and generate alerts
   * @param summary Current portfolio risk summary
   * @return Vector of alerts
   */
  [[nodiscard]] kj::Vector<RiskAlert> check_risk_levels(const PortfolioRiskSummary& summary) const;

  /**
   * @brief Set alert callback
   */
  void set_alert_callback(AlertCallback callback);

  /**
   * @brief Process risk summary and trigger alerts
   */
  void process(const PortfolioRiskSummary& summary);

private:
  double var_warning_threshold_{0.8};           // 80% of budget
  double var_critical_threshold_{0.95};         // 95% of budget
  double concentration_warning_threshold_{0.5}; // 50% in one position
  double drawdown_warning_threshold_{0.1};      // 10% drawdown
  kj::Maybe<AlertCallback> alert_callback_;
};

} // namespace veloz::risk
