#pragma once

#include <cmath>
#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief VaR calculation method enumeration
 */
enum class VaRMethod : std::uint8_t {
  Historical = 0, ///< Historical simulation
  Parametric = 1, ///< Variance-covariance (assumes normal distribution)
  MonteCarlo = 2, ///< Monte Carlo simulation
};

/**
 * @brief Convert VaRMethod to string
 */
[[nodiscard]] kj::StringPtr var_method_to_string(VaRMethod method);

/**
 * @brief VaR calculation result
 */
struct VaRResult {
  double var_95{0.0};  ///< 95% confidence VaR (potential loss)
  double var_99{0.0};  ///< 99% confidence VaR (potential loss)
  double cvar_95{0.0}; ///< 95% Conditional VaR (Expected Shortfall)
  double cvar_99{0.0}; ///< 99% Conditional VaR (Expected Shortfall)
  VaRMethod method{VaRMethod::Historical};
  int sample_size{0};       ///< Number of observations used
  int simulation_paths{0};  ///< For Monte Carlo: number of paths
  double mean_return{0.0};  ///< Mean of returns
  double std_dev{0.0};      ///< Standard deviation of returns
  bool valid{false};        ///< Whether calculation was successful
  kj::String error_message; ///< Error message if not valid
};

/**
 * @brief Configuration for VaR calculation
 */
struct VaRConfig {
  VaRMethod method{VaRMethod::Historical};
  int lookback_days{252};       ///< Historical lookback period
  int monte_carlo_paths{10000}; ///< Number of Monte Carlo simulations
  double confidence_95{0.95};   ///< 95% confidence level
  double confidence_99{0.99};   ///< 99% confidence level
  int holding_period_days{1};   ///< VaR holding period (default 1 day)
  bool calculate_cvar{true};    ///< Also calculate CVaR/Expected Shortfall
  uint64_t random_seed{0};      ///< Random seed for Monte Carlo (0 = use time)
};

/**
 * @brief Portfolio position for VaR calculation
 */
struct VaRPosition {
  kj::String symbol;
  double weight{0.0};     ///< Portfolio weight (0-1)
  double value{0.0};      ///< Position value
  double volatility{0.0}; ///< Annualized volatility
};

/**
 * @brief Covariance matrix entry
 */
struct CovarianceEntry {
  kj::String symbol1;
  kj::String symbol2;
  double covariance{0.0};
};

/**
 * @brief VaR Calculator - supports multiple calculation methods
 *
 * Calculates Value at Risk using Historical, Parametric, or Monte Carlo methods.
 * Also calculates Conditional VaR (Expected Shortfall) for tail risk assessment.
 */
class VaRCalculator final {
public:
  VaRCalculator() = default;
  explicit VaRCalculator(VaRConfig config);

  /**
   * @brief Set configuration
   */
  void set_config(VaRConfig config);

  /**
   * @brief Get current configuration
   */
  [[nodiscard]] const VaRConfig& config() const;

  // === Single Asset VaR ===

  /**
   * @brief Calculate VaR from historical returns
   * @param returns Vector of historical returns (as decimals, e.g., 0.01 = 1%)
   * @param portfolio_value Current portfolio value
   * @return VaR calculation result
   */
  [[nodiscard]] VaRResult calculate_historical(const kj::Vector<double>& returns,
                                               double portfolio_value) const;

  /**
   * @brief Calculate Parametric VaR (variance-covariance method)
   * @param mean Mean return
   * @param std_dev Standard deviation of returns
   * @param portfolio_value Current portfolio value
   * @return VaR calculation result
   */
  [[nodiscard]] VaRResult calculate_parametric(double mean, double std_dev,
                                               double portfolio_value) const;

  /**
   * @brief Calculate Monte Carlo VaR
   * @param mean Mean return
   * @param std_dev Standard deviation of returns
   * @param portfolio_value Current portfolio value
   * @return VaR calculation result
   */
  [[nodiscard]] VaRResult calculate_monte_carlo(double mean, double std_dev,
                                                double portfolio_value) const;

  /**
   * @brief Calculate VaR using configured method
   * @param returns Historical returns for Historical/auto-estimate parameters
   * @param portfolio_value Current portfolio value
   * @return VaR calculation result
   */
  [[nodiscard]] VaRResult calculate(const kj::Vector<double>& returns,
                                    double portfolio_value) const;

  // === Portfolio VaR ===

  /**
   * @brief Calculate portfolio VaR with correlations
   * @param positions Vector of portfolio positions
   * @param covariances Covariance matrix entries
   * @param portfolio_value Total portfolio value
   * @return VaR calculation result
   */
  [[nodiscard]] VaRResult calculate_portfolio_var(const kj::Vector<VaRPosition>& positions,
                                                  const kj::Vector<CovarianceEntry>& covariances,
                                                  double portfolio_value) const;

  // === Utility Functions ===

  /**
   * @brief Calculate returns from price series
   * @param prices Vector of prices (oldest first)
   * @return Vector of returns
   */
  [[nodiscard]] static kj::Vector<double> prices_to_returns(const kj::Vector<double>& prices);

  /**
   * @brief Calculate log returns from price series
   * @param prices Vector of prices (oldest first)
   * @return Vector of log returns
   */
  [[nodiscard]] static kj::Vector<double> prices_to_log_returns(const kj::Vector<double>& prices);

  /**
   * @brief Calculate mean of returns
   */
  [[nodiscard]] static double calculate_mean(const kj::Vector<double>& returns);

  /**
   * @brief Calculate standard deviation of returns
   */
  [[nodiscard]] static double calculate_std_dev(const kj::Vector<double>& returns);

  /**
   * @brief Scale VaR to different holding period
   * @param var_1day 1-day VaR
   * @param holding_days Target holding period in days
   * @return Scaled VaR (using square root of time rule)
   */
  [[nodiscard]] static double scale_var_to_holding_period(double var_1day, int holding_days);

  /**
   * @brief Get z-score for confidence level
   * @param confidence Confidence level (e.g., 0.95, 0.99)
   * @return Z-score for the given confidence level
   */
  [[nodiscard]] static double get_z_score(double confidence);

private:
  /**
   * @brief Get percentile value from sorted returns
   */
  [[nodiscard]] double get_percentile(const kj::Vector<double>& sorted_returns,
                                      double percentile) const;

  /**
   * @brief Calculate CVaR from sorted returns
   */
  [[nodiscard]] double calculate_cvar_from_sorted(const kj::Vector<double>& sorted_returns,
                                                  double percentile) const;

  /**
   * @brief Generate normal random number using Box-Muller transform
   */
  [[nodiscard]] double generate_normal_random(uint64_t& seed) const;

  VaRConfig config_;
};

/**
 * @brief Incremental VaR calculator for real-time updates
 *
 * Maintains rolling statistics for efficient VaR updates without
 * recalculating from full history.
 */
class IncrementalVaRCalculator final {
public:
  explicit IncrementalVaRCalculator(int window_size = 252);

  /**
   * @brief Add a new return observation
   * @param return_value Return value (as decimal)
   */
  void add_return(double return_value);

  /**
   * @brief Get current VaR estimate
   * @param portfolio_value Current portfolio value
   * @param confidence Confidence level (default 0.95)
   * @return VaR value
   */
  [[nodiscard]] double get_var(double portfolio_value, double confidence = 0.95) const;

  /**
   * @brief Get current CVaR estimate
   * @param portfolio_value Current portfolio value
   * @param confidence Confidence level (default 0.95)
   * @return CVaR value
   */
  [[nodiscard]] double get_cvar(double portfolio_value, double confidence = 0.95) const;

  /**
   * @brief Get current mean return
   */
  [[nodiscard]] double mean() const;

  /**
   * @brief Get current standard deviation
   */
  [[nodiscard]] double std_dev() const;

  /**
   * @brief Get number of observations
   */
  [[nodiscard]] size_t count() const;

  /**
   * @brief Check if enough data for reliable VaR
   */
  [[nodiscard]] bool is_valid() const;

  /**
   * @brief Reset calculator
   */
  void reset();

private:
  int window_size_;
  kj::Vector<double> returns_;
  double sum_{0.0};
  double sum_sq_{0.0};
};

/**
 * @brief Component VaR calculator for risk attribution
 *
 * Calculates marginal and component VaR to understand
 * risk contribution of each position.
 */
class ComponentVaRCalculator final {
public:
  ComponentVaRCalculator() = default;

  /**
   * @brief Risk contribution result for a single position
   */
  struct RiskContribution {
    kj::String symbol;
    double marginal_var{0.0};     ///< Change in VaR per unit change in position
    double component_var{0.0};    ///< Position's contribution to total VaR
    double pct_contribution{0.0}; ///< Percentage of total VaR
  };

  /**
   * @brief Calculate component VaR for portfolio
   * @param positions Portfolio positions with weights and volatilities
   * @param covariances Covariance matrix
   * @param portfolio_var Total portfolio VaR
   * @return Vector of risk contributions
   */
  [[nodiscard]] kj::Vector<RiskContribution>
  calculate(const kj::Vector<VaRPosition>& positions,
            const kj::Vector<CovarianceEntry>& covariances, double portfolio_var) const;

private:
  /**
   * @brief Find covariance between two symbols
   */
  [[nodiscard]] double find_covariance(const kj::Vector<CovarianceEntry>& covariances,
                                       kj::StringPtr symbol1, kj::StringPtr symbol2) const;
};

} // namespace veloz::risk
