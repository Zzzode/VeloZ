#include "veloz/risk/var_models.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace veloz::risk {

kj::StringPtr var_method_to_string(VaRMethod method) {
  switch (method) {
  case VaRMethod::Historical:
    return "historical"_kj;
  case VaRMethod::Parametric:
    return "parametric"_kj;
  case VaRMethod::MonteCarlo:
    return "monte_carlo"_kj;
  }
  return "unknown"_kj;
}

// ============================================================================
// VaRCalculator Implementation
// ============================================================================

VaRCalculator::VaRCalculator(VaRConfig config) : config_(kj::mv(config)) {}

void VaRCalculator::set_config(VaRConfig config) {
  config_ = kj::mv(config);
}

const VaRConfig& VaRCalculator::config() const {
  return config_;
}

VaRResult VaRCalculator::calculate_historical(const kj::Vector<double>& returns,
                                              double portfolio_value) const {
  VaRResult result;
  result.method = VaRMethod::Historical;
  result.sample_size = static_cast<int>(returns.size());

  if (returns.size() < 10) {
    result.valid = false;
    result.error_message =
        kj::str("Insufficient data: need at least 10 observations, got ", returns.size());
    return result;
  }

  // Calculate mean and std dev
  result.mean_return = calculate_mean(returns);
  result.std_dev = calculate_std_dev(returns);

  // Sort returns for percentile calculation
  kj::Vector<double> sorted_returns;
  sorted_returns.addAll(returns);
  std::sort(sorted_returns.begin(), sorted_returns.end());

  // Calculate VaR at confidence levels (left tail)
  double var_95_pct = get_percentile(sorted_returns, 1.0 - config_.confidence_95);
  double var_99_pct = get_percentile(sorted_returns, 1.0 - config_.confidence_99);

  // Convert to positive loss values
  result.var_95 = std::abs(var_95_pct) * portfolio_value;
  result.var_99 = std::abs(var_99_pct) * portfolio_value;

  // Calculate CVaR (Expected Shortfall)
  if (config_.calculate_cvar) {
    double cvar_95_pct = calculate_cvar_from_sorted(sorted_returns, 1.0 - config_.confidence_95);
    double cvar_99_pct = calculate_cvar_from_sorted(sorted_returns, 1.0 - config_.confidence_99);
    result.cvar_95 = std::abs(cvar_95_pct) * portfolio_value;
    result.cvar_99 = std::abs(cvar_99_pct) * portfolio_value;
  }

  // Scale to holding period if needed
  if (config_.holding_period_days > 1) {
    double scale = std::sqrt(static_cast<double>(config_.holding_period_days));
    result.var_95 *= scale;
    result.var_99 *= scale;
    result.cvar_95 *= scale;
    result.cvar_99 *= scale;
  }

  result.valid = true;
  return result;
}

VaRResult VaRCalculator::calculate_parametric(double mean, double std_dev,
                                              double portfolio_value) const {
  VaRResult result;
  result.method = VaRMethod::Parametric;
  result.mean_return = mean;
  result.std_dev = std_dev;

  if (std_dev <= 0.0) {
    result.valid = false;
    result.error_message = kj::str("Invalid standard deviation: ", std_dev);
    return result;
  }

  // Get z-scores for confidence levels
  double z_95 = get_z_score(config_.confidence_95);
  double z_99 = get_z_score(config_.confidence_99);

  // Parametric VaR: VaR = -mean + z * std_dev
  // For loss (positive value): VaR = z * std_dev - mean
  double var_95_pct = z_95 * std_dev - mean;
  double var_99_pct = z_99 * std_dev - mean;

  result.var_95 = var_95_pct * portfolio_value;
  result.var_99 = var_99_pct * portfolio_value;

  // Parametric CVaR (Expected Shortfall) for normal distribution
  // ES = mean + std_dev * phi(z) / (1 - confidence)
  // where phi(z) is the standard normal PDF
  if (config_.calculate_cvar) {
    auto normal_pdf = [](double z) { return std::exp(-0.5 * z * z) / std::sqrt(2.0 * M_PI); };

    double es_95_factor = normal_pdf(z_95) / (1.0 - config_.confidence_95);
    double es_99_factor = normal_pdf(z_99) / (1.0 - config_.confidence_99);

    double cvar_95_pct = std_dev * es_95_factor - mean;
    double cvar_99_pct = std_dev * es_99_factor - mean;

    result.cvar_95 = cvar_95_pct * portfolio_value;
    result.cvar_99 = cvar_99_pct * portfolio_value;
  }

  // Scale to holding period
  if (config_.holding_period_days > 1) {
    double scale = std::sqrt(static_cast<double>(config_.holding_period_days));
    result.var_95 *= scale;
    result.var_99 *= scale;
    result.cvar_95 *= scale;
    result.cvar_99 *= scale;
  }

  result.valid = true;
  return result;
}

VaRResult VaRCalculator::calculate_monte_carlo(double mean, double std_dev,
                                               double portfolio_value) const {
  VaRResult result;
  result.method = VaRMethod::MonteCarlo;
  result.mean_return = mean;
  result.std_dev = std_dev;
  result.simulation_paths = config_.monte_carlo_paths;

  if (std_dev <= 0.0) {
    result.valid = false;
    result.error_message = kj::str("Invalid standard deviation: ", std_dev);
    return result;
  }

  if (config_.monte_carlo_paths < 100) {
    result.valid = false;
    result.error_message =
        kj::str("Need at least 100 Monte Carlo paths, got ", config_.monte_carlo_paths);
    return result;
  }

  // Initialize random seed
  uint64_t seed = config_.random_seed;
  if (seed == 0) {
    seed =
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  }

  // Generate simulated returns
  kj::Vector<double> simulated_returns;
  simulated_returns.resize(config_.monte_carlo_paths);

  for (int i = 0; i < config_.monte_carlo_paths; ++i) {
    double z = generate_normal_random(seed);
    simulated_returns[i] = mean + std_dev * z;
  }

  // Sort for percentile calculation
  std::sort(simulated_returns.begin(), simulated_returns.end());

  // Calculate VaR from simulated distribution
  double var_95_pct = get_percentile(simulated_returns, 1.0 - config_.confidence_95);
  double var_99_pct = get_percentile(simulated_returns, 1.0 - config_.confidence_99);

  result.var_95 = std::abs(var_95_pct) * portfolio_value;
  result.var_99 = std::abs(var_99_pct) * portfolio_value;

  // Calculate CVaR
  if (config_.calculate_cvar) {
    double cvar_95_pct = calculate_cvar_from_sorted(simulated_returns, 1.0 - config_.confidence_95);
    double cvar_99_pct = calculate_cvar_from_sorted(simulated_returns, 1.0 - config_.confidence_99);
    result.cvar_95 = std::abs(cvar_95_pct) * portfolio_value;
    result.cvar_99 = std::abs(cvar_99_pct) * portfolio_value;
  }

  // Scale to holding period
  if (config_.holding_period_days > 1) {
    double scale = std::sqrt(static_cast<double>(config_.holding_period_days));
    result.var_95 *= scale;
    result.var_99 *= scale;
    result.cvar_95 *= scale;
    result.cvar_99 *= scale;
  }

  result.valid = true;
  return result;
}

VaRResult VaRCalculator::calculate(const kj::Vector<double>& returns,
                                   double portfolio_value) const {
  switch (config_.method) {
  case VaRMethod::Historical:
    return calculate_historical(returns, portfolio_value);

  case VaRMethod::Parametric: {
    double mean = calculate_mean(returns);
    double std_dev = calculate_std_dev(returns);
    return calculate_parametric(mean, std_dev, portfolio_value);
  }

  case VaRMethod::MonteCarlo: {
    double mean = calculate_mean(returns);
    double std_dev = calculate_std_dev(returns);
    return calculate_monte_carlo(mean, std_dev, portfolio_value);
  }
  }

  VaRResult result;
  result.valid = false;
  result.error_message = kj::str("Unknown VaR method");
  return result;
}

VaRResult VaRCalculator::calculate_portfolio_var(const kj::Vector<VaRPosition>& positions,
                                                 const kj::Vector<CovarianceEntry>& covariances,
                                                 double portfolio_value) const {

  VaRResult result;
  result.method = config_.method;

  if (positions.size() == 0) {
    result.valid = false;
    result.error_message = kj::str("No positions provided");
    return result;
  }

  // Calculate portfolio variance using variance-covariance approach
  // sigma_p^2 = sum_i sum_j w_i * w_j * cov(i,j)
  double portfolio_variance = 0.0;

  for (size_t i = 0; i < positions.size(); ++i) {
    for (size_t j = 0; j < positions.size(); ++j) {
      double w_i = positions[i].weight;
      double w_j = positions[j].weight;

      double cov = 0.0;
      if (i == j) {
        // Variance = volatility^2
        cov = positions[i].volatility * positions[i].volatility;
      } else {
        // Find covariance from matrix
        for (const auto& entry : covariances) {
          bool match1 =
              (entry.symbol1 == positions[i].symbol && entry.symbol2 == positions[j].symbol);
          bool match2 =
              (entry.symbol1 == positions[j].symbol && entry.symbol2 == positions[i].symbol);
          if (match1 || match2) {
            cov = entry.covariance;
            break;
          }
        }
      }

      portfolio_variance += w_i * w_j * cov;
    }
  }

  double portfolio_std_dev = std::sqrt(portfolio_variance);
  result.std_dev = portfolio_std_dev;

  // Calculate weighted mean return (assume 0 for simplicity)
  result.mean_return = 0.0;

  // Get z-scores
  double z_95 = get_z_score(config_.confidence_95);
  double z_99 = get_z_score(config_.confidence_99);

  // Calculate VaR
  result.var_95 = z_95 * portfolio_std_dev * portfolio_value;
  result.var_99 = z_99 * portfolio_std_dev * portfolio_value;

  // Calculate CVaR for normal distribution
  if (config_.calculate_cvar) {
    auto normal_pdf = [](double z) { return std::exp(-0.5 * z * z) / std::sqrt(2.0 * M_PI); };

    double es_95_factor = normal_pdf(z_95) / (1.0 - config_.confidence_95);
    double es_99_factor = normal_pdf(z_99) / (1.0 - config_.confidence_99);

    result.cvar_95 = portfolio_std_dev * es_95_factor * portfolio_value;
    result.cvar_99 = portfolio_std_dev * es_99_factor * portfolio_value;
  }

  // Scale to holding period
  if (config_.holding_period_days > 1) {
    double scale = std::sqrt(static_cast<double>(config_.holding_period_days));
    result.var_95 *= scale;
    result.var_99 *= scale;
    result.cvar_95 *= scale;
    result.cvar_99 *= scale;
  }

  result.valid = true;
  return result;
}

kj::Vector<double> VaRCalculator::prices_to_returns(const kj::Vector<double>& prices) {
  kj::Vector<double> returns;
  if (prices.size() < 2) {
    return returns;
  }

  returns.resize(prices.size() - 1);
  for (size_t i = 1; i < prices.size(); ++i) {
    if (prices[i - 1] > 0.0) {
      returns[i - 1] = (prices[i] - prices[i - 1]) / prices[i - 1];
    } else {
      returns[i - 1] = 0.0;
    }
  }
  return returns;
}

kj::Vector<double> VaRCalculator::prices_to_log_returns(const kj::Vector<double>& prices) {
  kj::Vector<double> returns;
  if (prices.size() < 2) {
    return returns;
  }

  returns.resize(prices.size() - 1);
  for (size_t i = 1; i < prices.size(); ++i) {
    if (prices[i - 1] > 0.0 && prices[i] > 0.0) {
      returns[i - 1] = std::log(prices[i] / prices[i - 1]);
    } else {
      returns[i - 1] = 0.0;
    }
  }
  return returns;
}

double VaRCalculator::calculate_mean(const kj::Vector<double>& returns) {
  if (returns.size() == 0) {
    return 0.0;
  }
  double sum = std::accumulate(returns.begin(), returns.end(), 0.0);
  return sum / static_cast<double>(returns.size());
}

double VaRCalculator::calculate_std_dev(const kj::Vector<double>& returns) {
  if (returns.size() < 2) {
    return 0.0;
  }

  double mean = calculate_mean(returns);
  double sum_sq = 0.0;
  for (double r : returns) {
    sum_sq += (r - mean) * (r - mean);
  }
  return std::sqrt(sum_sq / static_cast<double>(returns.size() - 1));
}

double VaRCalculator::scale_var_to_holding_period(double var_1day, int holding_days) {
  if (holding_days <= 0) {
    return var_1day;
  }
  return var_1day * std::sqrt(static_cast<double>(holding_days));
}

double VaRCalculator::get_percentile(const kj::Vector<double>& sorted_returns,
                                     double percentile) const {
  if (sorted_returns.size() == 0) {
    return 0.0;
  }

  double index = percentile * static_cast<double>(sorted_returns.size() - 1);
  size_t lower = static_cast<size_t>(std::floor(index));
  size_t upper = static_cast<size_t>(std::ceil(index));

  if (lower == upper || upper >= sorted_returns.size()) {
    return sorted_returns[lower];
  }

  // Linear interpolation
  double fraction = index - static_cast<double>(lower);
  return sorted_returns[lower] + fraction * (sorted_returns[upper] - sorted_returns[lower]);
}

double VaRCalculator::calculate_cvar_from_sorted(const kj::Vector<double>& sorted_returns,
                                                 double percentile) const {
  if (sorted_returns.size() == 0) {
    return 0.0;
  }

  // CVaR is the average of returns below the VaR threshold
  size_t cutoff_index =
      static_cast<size_t>(percentile * static_cast<double>(sorted_returns.size()));
  if (cutoff_index == 0) {
    cutoff_index = 1;
  }

  double sum = 0.0;
  for (size_t i = 0; i < cutoff_index; ++i) {
    sum += sorted_returns[i];
  }

  return sum / static_cast<double>(cutoff_index);
}

double VaRCalculator::get_z_score(double confidence) {
  // Approximate inverse normal CDF using Abramowitz and Stegun approximation
  // For common confidence levels, use exact values
  if (std::abs(confidence - 0.95) < 0.001) {
    return 1.6449; // 95% one-tailed
  }
  if (std::abs(confidence - 0.99) < 0.001) {
    return 2.3263; // 99% one-tailed
  }
  if (std::abs(confidence - 0.975) < 0.001) {
    return 1.96; // 97.5% one-tailed (95% two-tailed)
  }
  if (std::abs(confidence - 0.995) < 0.001) {
    return 2.5758; // 99.5% one-tailed (99% two-tailed)
  }

  // Rational approximation for other values
  double p = 1.0 - confidence;
  if (p <= 0.0 || p >= 1.0) {
    return 0.0;
  }

  // Approximation constants
  const double a1 = -3.969683028665376e+01;
  const double a2 = 2.209460984245205e+02;
  const double a3 = -2.759285104469687e+02;
  const double a4 = 1.383577518672690e+02;
  const double a5 = -3.066479806614716e+01;
  const double a6 = 2.506628277459239e+00;

  const double b1 = -5.447609879822406e+01;
  const double b2 = 1.615858368580409e+02;
  const double b3 = -1.556989798598866e+02;
  const double b4 = 6.680131188771972e+01;
  const double b5 = -1.328068155288572e+01;

  const double c1 = -7.784894002430293e-03;
  const double c2 = -3.223964580411365e-01;
  const double c3 = -2.400758277161838e+00;
  const double c4 = -2.549732539343734e+00;
  const double c5 = 4.374664141464968e+00;
  const double c6 = 2.938163982698783e+00;

  const double d1 = 7.784695709041462e-03;
  const double d2 = 3.224671290700398e-01;
  const double d3 = 2.445134137142996e+00;
  const double d4 = 3.754408661907416e+00;

  const double p_low = 0.02425;
  const double p_high = 1.0 - p_low;

  double q, r;

  if (p < p_low) {
    q = std::sqrt(-2.0 * std::log(p));
    return (((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) /
           ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.0);
  } else if (p <= p_high) {
    q = p - 0.5;
    r = q * q;
    return (((((a1 * r + a2) * r + a3) * r + a4) * r + a5) * r + a6) * q /
           (((((b1 * r + b2) * r + b3) * r + b4) * r + b5) * r + 1.0);
  } else {
    q = std::sqrt(-2.0 * std::log(1.0 - p));
    return -(((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) /
           ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.0);
  }
}

double VaRCalculator::generate_normal_random(uint64_t& seed) const {
  // xorshift64* PRNG
  seed ^= seed >> 12;
  seed ^= seed << 25;
  seed ^= seed >> 27;
  uint64_t rand1 = seed * 0x2545F4914F6CDD1DULL;

  seed ^= seed >> 12;
  seed ^= seed << 25;
  seed ^= seed >> 27;
  uint64_t rand2 = seed * 0x2545F4914F6CDD1DULL;

  // Box-Muller transform
  double u1 = static_cast<double>(rand1) / static_cast<double>(UINT64_MAX);
  double u2 = static_cast<double>(rand2) / static_cast<double>(UINT64_MAX);

  // Avoid log(0)
  if (u1 < 1e-10) {
    u1 = 1e-10;
  }

  return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

// ============================================================================
// IncrementalVaRCalculator Implementation
// ============================================================================

IncrementalVaRCalculator::IncrementalVaRCalculator(int window_size) : window_size_(window_size) {}

void IncrementalVaRCalculator::add_return(double return_value) {
  // If at capacity, remove oldest
  if (returns_.size() >= static_cast<size_t>(window_size_)) {
    double oldest = returns_[0];
    sum_ -= oldest;
    sum_sq_ -= oldest * oldest;

    // Shift left (remove first element)
    kj::Vector<double> new_returns;
    for (size_t i = 1; i < returns_.size(); ++i) {
      new_returns.add(returns_[i]);
    }
    returns_ = kj::mv(new_returns);
  }

  returns_.add(return_value);
  sum_ += return_value;
  sum_sq_ += return_value * return_value;
}

double IncrementalVaRCalculator::get_var(double portfolio_value, double confidence) const {
  if (!is_valid()) {
    return 0.0;
  }

  double std = std_dev();
  double z = VaRCalculator::get_z_score(confidence);
  return z * std * portfolio_value;
}

double IncrementalVaRCalculator::get_cvar(double portfolio_value, double confidence) const {
  if (!is_valid()) {
    return 0.0;
  }

  // Sort returns
  kj::Vector<double> sorted;
  sorted.addAll(returns_);
  std::sort(sorted.begin(), sorted.end());

  // Calculate CVaR
  size_t cutoff = static_cast<size_t>((1.0 - confidence) * static_cast<double>(sorted.size()));
  if (cutoff == 0) {
    cutoff = 1;
  }

  double sum = 0.0;
  for (size_t i = 0; i < cutoff; ++i) {
    sum += sorted[i];
  }

  double cvar_pct = sum / static_cast<double>(cutoff);
  return std::abs(cvar_pct) * portfolio_value;
}

double IncrementalVaRCalculator::mean() const {
  if (returns_.size() == 0) {
    return 0.0;
  }
  return sum_ / static_cast<double>(returns_.size());
}

double IncrementalVaRCalculator::std_dev() const {
  if (returns_.size() < 2) {
    return 0.0;
  }

  double n = static_cast<double>(returns_.size());
  double variance = (sum_sq_ - (sum_ * sum_) / n) / (n - 1.0);
  if (variance < 0.0) {
    variance = 0.0; // Numerical stability
  }
  return std::sqrt(variance);
}

size_t IncrementalVaRCalculator::count() const {
  return returns_.size();
}

bool IncrementalVaRCalculator::is_valid() const {
  return returns_.size() >= 10;
}

void IncrementalVaRCalculator::reset() {
  returns_.clear();
  sum_ = 0.0;
  sum_sq_ = 0.0;
}

// ============================================================================
// ComponentVaRCalculator Implementation
// ============================================================================

kj::Vector<ComponentVaRCalculator::RiskContribution>
ComponentVaRCalculator::calculate(const kj::Vector<VaRPosition>& positions,
                                  const kj::Vector<CovarianceEntry>& covariances,
                                  double portfolio_var) const {

  kj::Vector<RiskContribution> contributions;

  if (positions.size() == 0 || portfolio_var <= 0.0) {
    return contributions;
  }

  // Calculate portfolio variance
  double portfolio_variance = 0.0;
  for (size_t i = 0; i < positions.size(); ++i) {
    for (size_t j = 0; j < positions.size(); ++j) {
      double w_i = positions[i].weight;
      double w_j = positions[j].weight;
      double cov = find_covariance(covariances, positions[i].symbol, positions[j].symbol);
      if (i == j) {
        cov = positions[i].volatility * positions[i].volatility;
      }
      portfolio_variance += w_i * w_j * cov;
    }
  }

  double portfolio_std = std::sqrt(portfolio_variance);
  if (portfolio_std <= 0.0) {
    return contributions;
  }

  // Calculate marginal and component VaR for each position
  for (size_t i = 0; i < positions.size(); ++i) {
    RiskContribution contrib;
    contrib.symbol = kj::str(positions[i].symbol);

    // Marginal VaR = d(VaR)/d(w_i) = VaR * (sum_j w_j * cov(i,j)) / portfolio_variance
    double weighted_cov_sum = 0.0;
    for (size_t j = 0; j < positions.size(); ++j) {
      double cov = find_covariance(covariances, positions[i].symbol, positions[j].symbol);
      if (i == j) {
        cov = positions[i].volatility * positions[i].volatility;
      }
      weighted_cov_sum += positions[j].weight * cov;
    }

    contrib.marginal_var = portfolio_var * weighted_cov_sum / portfolio_variance;

    // Component VaR = w_i * Marginal VaR
    contrib.component_var = positions[i].weight * contrib.marginal_var;

    // Percentage contribution
    contrib.pct_contribution = contrib.component_var / portfolio_var * 100.0;

    contributions.add(kj::mv(contrib));
  }

  return contributions;
}

double ComponentVaRCalculator::find_covariance(const kj::Vector<CovarianceEntry>& covariances,
                                               kj::StringPtr symbol1, kj::StringPtr symbol2) const {
  for (const auto& entry : covariances) {
    bool match1 = (entry.symbol1 == symbol1 && entry.symbol2 == symbol2);
    bool match2 = (entry.symbol1 == symbol2 && entry.symbol2 == symbol1);
    if (match1 || match2) {
      return entry.covariance;
    }
  }
  return 0.0;
}

} // namespace veloz::risk
