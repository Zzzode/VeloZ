#include "veloz/risk/portfolio_risk.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace veloz::risk {

// ============================================================================
// Copy Constructors
// ============================================================================

PositionRiskContribution::PositionRiskContribution(const PositionRiskContribution& other)
    : symbol(kj::heapString(other.symbol)), position_value(other.position_value),
      weight(other.weight), standalone_var(other.standalone_var), marginal_var(other.marginal_var),
      component_var(other.component_var), pct_contribution(other.pct_contribution),
      diversification_benefit(other.diversification_benefit) {}

PositionRiskContribution&
PositionRiskContribution::operator=(const PositionRiskContribution& other) {
  if (this != &other) {
    symbol = kj::heapString(other.symbol);
    position_value = other.position_value;
    weight = other.weight;
    standalone_var = other.standalone_var;
    marginal_var = other.marginal_var;
    component_var = other.component_var;
    pct_contribution = other.pct_contribution;
    diversification_benefit = other.diversification_benefit;
  }
  return *this;
}

RiskAllocation::RiskAllocation(const RiskAllocation& other)
    : name(kj::heapString(other.name)), allocated_var(other.allocated_var),
      used_var(other.used_var), utilization_pct(other.utilization_pct),
      remaining_var(other.remaining_var), is_breached(other.is_breached) {}

RiskAllocation& RiskAllocation::operator=(const RiskAllocation& other) {
  if (this != &other) {
    name = kj::heapString(other.name);
    allocated_var = other.allocated_var;
    used_var = other.used_var;
    utilization_pct = other.utilization_pct;
    remaining_var = other.remaining_var;
    is_breached = other.is_breached;
  }
  return *this;
}

PortfolioPosition::PortfolioPosition(const PortfolioPosition& other)
    : symbol(kj::heapString(other.symbol)), size(other.size), price(other.price),
      value(other.value), volatility(other.volatility), strategy(kj::heapString(other.strategy)) {}

PortfolioPosition& PortfolioPosition::operator=(const PortfolioPosition& other) {
  if (this != &other) {
    symbol = kj::heapString(other.symbol);
    size = other.size;
    price = other.price;
    value = other.value;
    volatility = other.volatility;
    strategy = kj::heapString(other.strategy);
  }
  return *this;
}

// ============================================================================
// PortfolioRiskAggregator Implementation
// ============================================================================

void PortfolioRiskAggregator::update_position(PortfolioPosition position) {
  // Update value if not set
  if (position.value == 0.0 && position.size != 0.0 && position.price > 0.0) {
    position.value = std::abs(position.size) * position.price;
  }

  // Find and update or add
  for (auto& pos : positions_) {
    if (pos.symbol == position.symbol) {
      pos = kj::mv(position);
      return;
    }
  }
  positions_.add(kj::mv(position));
}

void PortfolioRiskAggregator::remove_position(kj::StringPtr symbol) {
  kj::Vector<PortfolioPosition> new_positions;
  for (auto& pos : positions_) {
    if (pos.symbol != symbol) {
      new_positions.add(kj::mv(pos));
    }
  }
  positions_ = kj::mv(new_positions);
}

const PortfolioPosition* PortfolioRiskAggregator::get_position(kj::StringPtr symbol) const {
  for (const auto& pos : positions_) {
    if (pos.symbol == symbol) {
      return &pos;
    }
  }
  return nullptr;
}

const kj::Vector<PortfolioPosition>& PortfolioRiskAggregator::get_positions() const {
  return positions_;
}

void PortfolioRiskAggregator::clear_positions() {
  positions_.clear();
}

void PortfolioRiskAggregator::set_correlation(kj::StringPtr symbol1, kj::StringPtr symbol2,
                                              double correlation) {
  // Store with consistent key ordering
  kj::String key;
  if (symbol1 < symbol2) {
    key = kj::str(symbol1, ":", symbol2);
  } else {
    key = kj::str(symbol2, ":", symbol1);
  }
  correlations_.upsert(kj::mv(key), correlation,
                       [](double& existing, double value) { existing = value; });
}

double PortfolioRiskAggregator::get_correlation(kj::StringPtr symbol1,
                                                kj::StringPtr symbol2) const {
  if (symbol1 == symbol2) {
    return 1.0;
  }

  kj::String key;
  if (symbol1 < symbol2) {
    key = kj::str(symbol1, ":", symbol2);
  } else {
    key = kj::str(symbol2, ":", symbol1);
  }

  KJ_IF_SOME(corr, correlations_.find(key)) {
    return corr;
  }
  return default_correlation_;
}

void PortfolioRiskAggregator::set_default_correlation(double correlation) {
  default_correlation_ = correlation;
}

void PortfolioRiskAggregator::clear_correlations() {
  correlations_.clear();
}

void PortfolioRiskAggregator::set_risk_budget(kj::StringPtr name, double var_budget) {
  auto key = kj::str(name);
  risk_budgets_.upsert(kj::mv(key), var_budget,
                       [](double& existing, double value) { existing = value; });
}

double PortfolioRiskAggregator::get_risk_budget(kj::StringPtr name) const {
  KJ_IF_SOME(budget, risk_budgets_.find(name)) {
    return budget;
  }
  return 0.0;
}

void PortfolioRiskAggregator::set_total_risk_budget(double var_budget) {
  total_risk_budget_ = var_budget;
}

PortfolioRiskSummary PortfolioRiskAggregator::calculate_risk(double confidence) const {
  PortfolioRiskSummary summary;

  if (positions_.size() == 0) {
    return summary;
  }

  // Calculate total portfolio value
  for (const auto& pos : positions_) {
    summary.total_value += pos.value;
  }

  if (summary.total_value <= 0.0) {
    return summary;
  }

  // Calculate portfolio variance
  double portfolio_variance = calculate_portfolio_variance();
  double portfolio_std = std::sqrt(portfolio_variance);

  // Get z-score for confidence level
  double z_score = VaRCalculator::get_z_score(confidence);
  double z_score_99 = VaRCalculator::get_z_score(0.99);

  // Calculate portfolio VaR
  summary.total_var_95 = z_score * portfolio_std * summary.total_value;
  summary.total_var_99 = z_score_99 * portfolio_std * summary.total_value;

  // Scale to holding period
  if (holding_period_days_ > 1) {
    double scale = std::sqrt(static_cast<double>(holding_period_days_));
    summary.total_var_95 *= scale;
    summary.total_var_99 *= scale;
  }

  // Calculate CVaR (Expected Shortfall) for normal distribution
  auto normal_pdf = [](double z) { return std::exp(-0.5 * z * z) / std::sqrt(2.0 * M_PI); };
  double es_factor = normal_pdf(z_score) / (1.0 - confidence);
  summary.total_cvar_95 = portfolio_std * es_factor * summary.total_value;

  // Calculate undiversified VaR (sum of individual VaRs)
  for (const auto& pos : positions_) {
    double individual_var = z_score * pos.volatility * pos.value;
    summary.undiversified_var += individual_var;
  }

  // Diversification benefit
  summary.diversification_benefit = summary.undiversified_var - summary.total_var_95;

  // Calculate contributions
  summary.contributions = calculate_contributions(confidence);

  // Find largest contributor
  double max_contribution = 0.0;
  for (const auto& contrib : summary.contributions) {
    if (contrib.pct_contribution > max_contribution) {
      max_contribution = contrib.pct_contribution;
      summary.largest_risk_contributor = kj::str(contrib.symbol);
      summary.largest_contribution_pct = contrib.pct_contribution;
    }
  }

  // Concentration metrics
  summary.position_count = static_cast<int>(positions_.size());
  double hhi = 0.0;
  for (const auto& pos : positions_) {
    double weight = pos.value / summary.total_value;
    hhi += weight * weight;
  }
  summary.herfindahl_index = hhi;

  // Risk allocations
  summary.allocations = calculate_allocations();

  summary.calculated_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

  return summary;
}

kj::Vector<PositionRiskContribution>
PortfolioRiskAggregator::calculate_contributions(double confidence) const {

  kj::Vector<PositionRiskContribution> contributions;

  if (positions_.size() == 0) {
    return contributions;
  }

  double total_value = 0.0;
  for (const auto& pos : positions_) {
    total_value += pos.value;
  }

  if (total_value <= 0.0) {
    return contributions;
  }

  double portfolio_variance = calculate_portfolio_variance();
  double portfolio_std = std::sqrt(portfolio_variance);
  double z_score = VaRCalculator::get_z_score(confidence);
  double portfolio_var = z_score * portfolio_std * total_value;

  for (const auto& pos : positions_) {
    PositionRiskContribution contrib;
    contrib.symbol = kj::str(pos.symbol);
    contrib.position_value = pos.value;
    contrib.weight = pos.value / total_value;

    // Standalone VaR
    contrib.standalone_var = z_score * pos.volatility * pos.value;

    // Marginal VaR = d(VaR)/d(w_i)
    // For variance-covariance: marginal_var = VaR * (sum_j w_j * cov(i,j)) / portfolio_variance
    double weighted_cov_sum = 0.0;
    for (const auto& other : positions_) {
      double w_j = other.value / total_value;
      double corr = get_correlation(pos.symbol, other.symbol);
      double cov = corr * pos.volatility * other.volatility;
      weighted_cov_sum += w_j * cov;
    }

    if (portfolio_variance > 0.0) {
      contrib.marginal_var = portfolio_var * weighted_cov_sum / portfolio_variance;
    }

    // Component VaR = w_i * Marginal VaR
    contrib.component_var = contrib.weight * contrib.marginal_var;

    // Percentage contribution
    if (portfolio_var > 0.0) {
      contrib.pct_contribution = contrib.component_var / portfolio_var * 100.0;
    }

    // Diversification benefit
    contrib.diversification_benefit = contrib.standalone_var - contrib.component_var;

    contributions.add(kj::mv(contrib));
  }

  return contributions;
}

kj::Vector<RiskAllocation> PortfolioRiskAggregator::calculate_allocations() const {
  kj::Vector<RiskAllocation> allocations;

  // Group positions by strategy
  kj::HashMap<kj::String, double> strategy_vars;

  double z_score = VaRCalculator::get_z_score(0.95);

  for (const auto& pos : positions_) {
    double pos_var = z_score * pos.volatility * pos.value;
    kj::String strategy = pos.strategy.size() > 0 ? kj::str(pos.strategy) : kj::str("default");

    KJ_IF_SOME(existing, strategy_vars.find(strategy)) {
      strategy_vars.upsert(kj::mv(strategy), existing + pos_var,
                           [](double& e, double v) { e = v; });
    }
    else {
      strategy_vars.insert(kj::mv(strategy), pos_var);
    }
  }

  // Create allocations
  for (auto& entry : strategy_vars) {
    RiskAllocation alloc;
    alloc.name = kj::str(entry.key);
    alloc.used_var = entry.value;

    KJ_IF_SOME(budget, risk_budgets_.find(entry.key)) {
      alloc.allocated_var = budget;
      alloc.remaining_var = budget - entry.value;
      if (budget > 0.0) {
        alloc.utilization_pct = entry.value / budget * 100.0;
      }
      alloc.is_breached = entry.value > budget;
    }

    allocations.add(kj::mv(alloc));
  }

  return allocations;
}

bool PortfolioRiskAggregator::is_any_budget_breached() const {
  auto allocations = calculate_allocations();
  for (const auto& alloc : allocations) {
    if (alloc.is_breached) {
      return true;
    }
  }

  // Check total budget
  if (total_risk_budget_ > 0.0) {
    auto summary = calculate_risk();
    if (summary.total_var_95 > total_risk_budget_) {
      return true;
    }
  }

  return false;
}

kj::Vector<std::pair<kj::String, double>>
PortfolioRiskAggregator::suggest_reductions(double target_var) const {

  kj::Vector<std::pair<kj::String, double>> suggestions;

  auto summary = calculate_risk();
  if (summary.total_var_95 <= target_var) {
    return suggestions; // Already within target
  }

  double excess_var = summary.total_var_95 - target_var;

  // Sort contributions by percentage (highest first)
  kj::Vector<PositionRiskContribution> sorted_contribs;
  for (const auto& c : summary.contributions) {
    sorted_contribs.add(PositionRiskContribution(c));
  }
  std::sort(sorted_contribs.begin(), sorted_contribs.end(),
            [](const auto& a, const auto& b) { return a.pct_contribution > b.pct_contribution; });

  // Suggest reductions starting from largest contributors
  double remaining_excess = excess_var;
  for (const auto& contrib : sorted_contribs) {
    if (remaining_excess <= 0.0) {
      break;
    }

    // Reduce this position proportionally
    double reduction_var = std::min(contrib.component_var * 0.5, remaining_excess);
    double reduction_pct = reduction_var / contrib.component_var;
    double reduction_value = contrib.position_value * reduction_pct;

    suggestions.add(std::make_pair(kj::str(contrib.symbol), reduction_value));
    remaining_excess -= reduction_var;
  }

  return suggestions;
}

double PortfolioRiskAggregator::calculate_max_position(kj::StringPtr symbol,
                                                       double available_budget) const {
  if (available_budget <= 0.0) {
    return 0.0;
  }

  // Find volatility for this symbol
  double volatility = 0.0;
  for (const auto& pos : positions_) {
    if (pos.symbol == symbol) {
      volatility = pos.volatility;
      break;
    }
  }

  if (volatility <= 0.0) {
    volatility = 0.02; // Default 2% daily volatility
  }

  double z_score = VaRCalculator::get_z_score(0.95);

  // VaR = z * vol * value
  // value = VaR / (z * vol)
  return available_budget / (z_score * volatility);
}

void PortfolioRiskAggregator::set_var_method(VaRMethod method) {
  var_method_ = method;
}

void PortfolioRiskAggregator::set_holding_period(int days) {
  holding_period_days_ = days;
}

kj::Vector<CovarianceEntry> PortfolioRiskAggregator::build_covariance_matrix() const {
  kj::Vector<CovarianceEntry> covariances;

  for (size_t i = 0; i < positions_.size(); ++i) {
    for (size_t j = i + 1; j < positions_.size(); ++j) {
      CovarianceEntry entry;
      entry.symbol1 = kj::str(positions_[i].symbol);
      entry.symbol2 = kj::str(positions_[j].symbol);

      double corr = get_correlation(positions_[i].symbol, positions_[j].symbol);
      entry.covariance = corr * positions_[i].volatility * positions_[j].volatility;

      covariances.add(kj::mv(entry));
    }
  }

  return covariances;
}

double PortfolioRiskAggregator::calculate_portfolio_variance() const {
  if (positions_.size() == 0) {
    return 0.0;
  }

  double total_value = 0.0;
  for (const auto& pos : positions_) {
    total_value += pos.value;
  }

  if (total_value <= 0.0) {
    return 0.0;
  }

  // Portfolio variance = sum_i sum_j w_i * w_j * cov(i,j)
  double variance = 0.0;
  for (const auto& pos_i : positions_) {
    for (const auto& pos_j : positions_) {
      double w_i = pos_i.value / total_value;
      double w_j = pos_j.value / total_value;
      double corr = get_correlation(pos_i.symbol, pos_j.symbol);
      double cov = corr * pos_i.volatility * pos_j.volatility;
      variance += w_i * w_j * cov;
    }
  }

  return variance;
}

// ============================================================================
// PortfolioRiskMonitor Implementation
// ============================================================================

void PortfolioRiskMonitor::set_var_warning_threshold(double pct) {
  var_warning_threshold_ = pct;
}

void PortfolioRiskMonitor::set_var_critical_threshold(double pct) {
  var_critical_threshold_ = pct;
}

void PortfolioRiskMonitor::set_concentration_warning_threshold(double pct) {
  concentration_warning_threshold_ = pct;
}

void PortfolioRiskMonitor::set_drawdown_warning_threshold(double pct) {
  drawdown_warning_threshold_ = pct;
}

kj::Vector<PortfolioRiskMonitor::RiskAlert>
PortfolioRiskMonitor::check_risk_levels(const PortfolioRiskSummary& summary) const {

  kj::Vector<RiskAlert> alerts;
  int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

  // Check budget utilization for each allocation
  for (const auto& alloc : summary.allocations) {
    if (alloc.allocated_var > 0.0) {
      double utilization = alloc.utilization_pct / 100.0;

      if (utilization >= var_critical_threshold_) {
        RiskAlert alert;
        alert.level = AlertLevel::Critical;
        alert.message = kj::str("VaR budget critical for ", alloc.name, ": ", alloc.utilization_pct,
                                "% utilized");
        alert.current_value = alloc.used_var;
        alert.threshold = alloc.allocated_var * var_critical_threshold_;
        alert.timestamp_ns = now;
        alerts.add(kj::mv(alert));
      } else if (utilization >= var_warning_threshold_) {
        RiskAlert alert;
        alert.level = AlertLevel::Warning;
        alert.message = kj::str("VaR budget warning for ", alloc.name, ": ", alloc.utilization_pct,
                                "% utilized");
        alert.current_value = alloc.used_var;
        alert.threshold = alloc.allocated_var * var_warning_threshold_;
        alert.timestamp_ns = now;
        alerts.add(kj::mv(alert));
      }
    }
  }

  // Check concentration
  if (summary.largest_contribution_pct >= concentration_warning_threshold_ * 100.0) {
    RiskAlert alert;
    alert.level = AlertLevel::Warning;
    alert.message = kj::str("High concentration in ", summary.largest_risk_contributor, ": ",
                            summary.largest_contribution_pct, "% of risk");
    alert.symbol = kj::str(summary.largest_risk_contributor);
    alert.current_value = summary.largest_contribution_pct;
    alert.threshold = concentration_warning_threshold_ * 100.0;
    alert.timestamp_ns = now;
    alerts.add(kj::mv(alert));
  }

  return alerts;
}

void PortfolioRiskMonitor::set_alert_callback(AlertCallback callback) {
  alert_callback_ = kj::mv(callback);
}

void PortfolioRiskMonitor::process(const PortfolioRiskSummary& summary) {
  auto alerts = check_risk_levels(summary);

  KJ_IF_SOME(callback, alert_callback_) {
    for (const auto& alert : alerts) {
      callback(alert);
    }
  }
}

} // namespace veloz::risk
