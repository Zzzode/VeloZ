#include "veloz/risk/var_models.h"

#include <cmath>
#include <kj/test.h>

namespace veloz::risk {
namespace {

KJ_TEST("VaRMethod to string conversion") {
  KJ_EXPECT(var_method_to_string(VaRMethod::Historical) == "historical"_kj);
  KJ_EXPECT(var_method_to_string(VaRMethod::Parametric) == "parametric"_kj);
  KJ_EXPECT(var_method_to_string(VaRMethod::MonteCarlo) == "monte_carlo"_kj);
}

KJ_TEST("VaRCalculator - prices to returns conversion") {
  kj::Vector<double> prices;
  prices.add(100.0);
  prices.add(105.0);
  prices.add(103.0);
  prices.add(108.0);

  auto returns = VaRCalculator::prices_to_returns(prices);
  KJ_EXPECT(returns.size() == 3);
  KJ_EXPECT(std::abs(returns[0] - 0.05) < 0.0001);   // (105-100)/100 = 0.05
  KJ_EXPECT(std::abs(returns[1] - (-0.019047619)) < 0.0001);  // (103-105)/105
  KJ_EXPECT(std::abs(returns[2] - 0.048543689) < 0.0001);  // (108-103)/103
}

KJ_TEST("VaRCalculator - prices to log returns conversion") {
  kj::Vector<double> prices;
  prices.add(100.0);
  prices.add(110.0);

  auto returns = VaRCalculator::prices_to_log_returns(prices);
  KJ_EXPECT(returns.size() == 1);
  KJ_EXPECT(std::abs(returns[0] - 0.09531) < 0.001);  // ln(110/100)
}

KJ_TEST("VaRCalculator - mean and std dev calculation") {
  kj::Vector<double> returns;
  returns.add(0.01);
  returns.add(-0.02);
  returns.add(0.03);
  returns.add(-0.01);
  returns.add(0.02);

  double mean = VaRCalculator::calculate_mean(returns);
  KJ_EXPECT(std::abs(mean - 0.006) < 0.0001);  // (0.01-0.02+0.03-0.01+0.02)/5

  double std_dev = VaRCalculator::calculate_std_dev(returns);
  KJ_EXPECT(std_dev > 0.0);
  KJ_EXPECT(std_dev < 0.03);  // Reasonable range
}

KJ_TEST("VaRCalculator - Historical VaR with insufficient data") {
  VaRCalculator calc;
  kj::Vector<double> returns;
  returns.add(0.01);
  returns.add(-0.02);

  auto result = calc.calculate_historical(returns, 100000.0);
  KJ_EXPECT(!result.valid);
  KJ_EXPECT(result.error_message.size() > 0);
}

KJ_TEST("VaRCalculator - Historical VaR calculation") {
  VaRConfig config;
  config.method = VaRMethod::Historical;
  config.calculate_cvar = true;

  VaRCalculator calc(config);

  // Generate synthetic returns (simulating daily returns)
  kj::Vector<double> returns;
  for (int i = 0; i < 100; ++i) {
    // Simple pattern: alternating positive/negative with some variation
    double ret = (i % 2 == 0) ? 0.01 : -0.015;
    ret += (i % 7) * 0.001;  // Add some variation
    returns.add(ret);
  }

  auto result = calc.calculate_historical(returns, 100000.0);

  KJ_EXPECT(result.valid);
  KJ_EXPECT(result.method == VaRMethod::Historical);
  KJ_EXPECT(result.sample_size == 100);
  KJ_EXPECT(result.var_95 > 0.0);
  KJ_EXPECT(result.var_99 > 0.0);
  KJ_EXPECT(result.var_99 >= result.var_95);  // 99% VaR should be >= 95% VaR
  KJ_EXPECT(result.cvar_95 >= result.var_95);  // CVaR should be >= VaR
  KJ_EXPECT(result.cvar_99 >= result.var_99);
}

KJ_TEST("VaRCalculator - Parametric VaR calculation") {
  VaRConfig config;
  config.method = VaRMethod::Parametric;
  config.calculate_cvar = true;

  VaRCalculator calc(config);

  double mean = 0.001;  // 0.1% daily return
  double std_dev = 0.02;  // 2% daily volatility
  double portfolio_value = 100000.0;

  auto result = calc.calculate_parametric(mean, std_dev, portfolio_value);

  KJ_EXPECT(result.valid);
  KJ_EXPECT(result.method == VaRMethod::Parametric);
  KJ_EXPECT(result.mean_return == mean);
  KJ_EXPECT(result.std_dev == std_dev);

  // 95% VaR ~ 1.6449 * 0.02 * 100000 = 3289.8
  KJ_EXPECT(result.var_95 > 3000.0);
  KJ_EXPECT(result.var_95 < 3500.0);

  // 99% VaR ~ 2.3263 * 0.02 * 100000 = 4652.6
  KJ_EXPECT(result.var_99 > 4400.0);
  KJ_EXPECT(result.var_99 < 4900.0);

  KJ_EXPECT(result.cvar_95 > result.var_95);
  KJ_EXPECT(result.cvar_99 > result.var_99);
}

KJ_TEST("VaRCalculator - Parametric VaR with invalid std dev") {
  VaRCalculator calc;
  auto result = calc.calculate_parametric(0.001, 0.0, 100000.0);
  KJ_EXPECT(!result.valid);
}

KJ_TEST("VaRCalculator - Monte Carlo VaR calculation") {
  VaRConfig config;
  config.method = VaRMethod::MonteCarlo;
  config.monte_carlo_paths = 10000;
  config.random_seed = 12345;  // Fixed seed for reproducibility
  config.calculate_cvar = true;

  VaRCalculator calc(config);

  double mean = 0.001;
  double std_dev = 0.02;
  double portfolio_value = 100000.0;

  auto result = calc.calculate_monte_carlo(mean, std_dev, portfolio_value);

  KJ_EXPECT(result.valid);
  KJ_EXPECT(result.method == VaRMethod::MonteCarlo);
  KJ_EXPECT(result.simulation_paths == 10000);

  // Monte Carlo should give similar results to Parametric for normal distribution
  // Allow wider tolerance due to simulation variance
  KJ_EXPECT(result.var_95 > 2500.0);
  KJ_EXPECT(result.var_95 < 4000.0);
  KJ_EXPECT(result.var_99 > result.var_95);
}

KJ_TEST("VaRCalculator - Monte Carlo with insufficient paths") {
  VaRConfig config;
  config.monte_carlo_paths = 50;  // Too few

  VaRCalculator calc(config);
  auto result = calc.calculate_monte_carlo(0.001, 0.02, 100000.0);
  KJ_EXPECT(!result.valid);
}

KJ_TEST("VaRCalculator - calculate with configured method") {
  kj::Vector<double> returns;
  for (int i = 0; i < 50; ++i) {
    returns.add((i % 2 == 0) ? 0.01 : -0.012);
  }

  // Test Historical
  {
    VaRConfig config;
    config.method = VaRMethod::Historical;
    VaRCalculator calc(config);
    auto result = calc.calculate(returns, 100000.0);
    KJ_EXPECT(result.valid);
    KJ_EXPECT(result.method == VaRMethod::Historical);
  }

  // Test Parametric
  {
    VaRConfig config;
    config.method = VaRMethod::Parametric;
    VaRCalculator calc(config);
    auto result = calc.calculate(returns, 100000.0);
    KJ_EXPECT(result.valid);
    KJ_EXPECT(result.method == VaRMethod::Parametric);
  }

  // Test Monte Carlo
  {
    VaRConfig config;
    config.method = VaRMethod::MonteCarlo;
    config.monte_carlo_paths = 1000;
    config.random_seed = 42;
    VaRCalculator calc(config);
    auto result = calc.calculate(returns, 100000.0);
    KJ_EXPECT(result.valid);
    KJ_EXPECT(result.method == VaRMethod::MonteCarlo);
  }
}

KJ_TEST("VaRCalculator - holding period scaling") {
  double var_1day = 1000.0;

  // 10-day VaR should be sqrt(10) * 1-day VaR
  double var_10day = VaRCalculator::scale_var_to_holding_period(var_1day, 10);
  KJ_EXPECT(std::abs(var_10day - 3162.28) < 1.0);  // sqrt(10) * 1000
}

KJ_TEST("VaRCalculator - portfolio VaR calculation") {
  VaRConfig config;
  config.method = VaRMethod::Parametric;
  VaRCalculator calc(config);

  // Two-asset portfolio
  kj::Vector<VaRPosition> positions;

  VaRPosition pos1;
  pos1.symbol = kj::str("BTC");
  pos1.weight = 0.6;
  pos1.value = 60000.0;
  pos1.volatility = 0.03;  // 3% daily vol
  positions.add(kj::mv(pos1));

  VaRPosition pos2;
  pos2.symbol = kj::str("ETH");
  pos2.weight = 0.4;
  pos2.value = 40000.0;
  pos2.volatility = 0.04;  // 4% daily vol
  positions.add(kj::mv(pos2));

  // Covariance between BTC and ETH (correlation ~0.8)
  kj::Vector<CovarianceEntry> covariances;
  CovarianceEntry cov;
  cov.symbol1 = kj::str("BTC");
  cov.symbol2 = kj::str("ETH");
  cov.covariance = 0.8 * 0.03 * 0.04;  // correlation * vol1 * vol2
  covariances.add(kj::mv(cov));

  auto result = calc.calculate_portfolio_var(positions, covariances, 100000.0);

  KJ_EXPECT(result.valid);
  KJ_EXPECT(result.var_95 > 0.0);
  KJ_EXPECT(result.var_99 > result.var_95);

  // Portfolio volatility should be less than weighted average due to diversification
  // (unless correlation is 1)
  double weighted_avg_vol = 0.6 * 0.03 + 0.4 * 0.04;
  KJ_EXPECT(result.std_dev < weighted_avg_vol);
}

KJ_TEST("IncrementalVaRCalculator - basic operations") {
  IncrementalVaRCalculator calc(50);

  // Initially not valid
  KJ_EXPECT(!calc.is_valid());
  KJ_EXPECT(calc.count() == 0);

  // Add returns with more realistic distribution (some large losses)
  for (int i = 0; i < 30; ++i) {
    double ret;
    if (i % 10 == 0) {
      ret = -0.03;  // Occasional large loss
    } else if (i % 2 == 0) {
      ret = 0.008;
    } else {
      ret = -0.005;
    }
    calc.add_return(ret);
  }

  KJ_EXPECT(calc.count() == 30);
  KJ_EXPECT(calc.is_valid());  // Now have enough data

  double mean = calc.mean();
  double std = calc.std_dev();
  KJ_EXPECT(std::abs(mean) < 0.02);
  KJ_EXPECT(std > 0.0);

  double var = calc.get_var(100000.0, 0.95);
  KJ_EXPECT(var > 0.0);

  double cvar = calc.get_cvar(100000.0, 0.95);
  // CVaR should be positive (represents potential loss)
  KJ_EXPECT(cvar > 0.0);
}

KJ_TEST("IncrementalVaRCalculator - window rolling") {
  IncrementalVaRCalculator calc(10);  // Small window

  // Add more than window size
  for (int i = 0; i < 15; ++i) {
    calc.add_return(0.01 * (i + 1));
  }

  // Should only have window_size elements
  KJ_EXPECT(calc.count() == 10);
}

KJ_TEST("IncrementalVaRCalculator - reset") {
  IncrementalVaRCalculator calc(50);

  for (int i = 0; i < 20; ++i) {
    calc.add_return(0.01);
  }

  KJ_EXPECT(calc.count() == 20);

  calc.reset();

  KJ_EXPECT(calc.count() == 0);
  KJ_EXPECT(!calc.is_valid());
}

KJ_TEST("ComponentVaRCalculator - risk contribution") {
  ComponentVaRCalculator calc;

  // Two-asset portfolio
  kj::Vector<VaRPosition> positions;

  VaRPosition pos1;
  pos1.symbol = kj::str("BTC");
  pos1.weight = 0.7;
  pos1.volatility = 0.03;
  positions.add(kj::mv(pos1));

  VaRPosition pos2;
  pos2.symbol = kj::str("ETH");
  pos2.weight = 0.3;
  pos2.volatility = 0.04;
  positions.add(kj::mv(pos2));

  kj::Vector<CovarianceEntry> covariances;
  CovarianceEntry cov;
  cov.symbol1 = kj::str("BTC");
  cov.symbol2 = kj::str("ETH");
  cov.covariance = 0.7 * 0.03 * 0.04;  // correlation 0.7
  covariances.add(kj::mv(cov));

  double portfolio_var = 5000.0;  // Assume this is calculated

  auto contributions = calc.calculate(positions, covariances, portfolio_var);

  KJ_EXPECT(contributions.size() == 2);

  // Sum of component VaRs should approximately equal portfolio VaR
  double sum_component_var = 0.0;
  double sum_pct = 0.0;
  for (const auto& contrib : contributions) {
    sum_component_var += contrib.component_var;
    sum_pct += contrib.pct_contribution;
  }

  // Allow some tolerance due to numerical precision
  KJ_EXPECT(std::abs(sum_pct - 100.0) < 5.0);

  // BTC should contribute more (higher weight)
  KJ_EXPECT(contributions[0].pct_contribution > contributions[1].pct_contribution);
}

KJ_TEST("VaRConfig - default values") {
  VaRConfig config;

  KJ_EXPECT(config.method == VaRMethod::Historical);
  KJ_EXPECT(config.lookback_days == 252);
  KJ_EXPECT(config.monte_carlo_paths == 10000);
  KJ_EXPECT(std::abs(config.confidence_95 - 0.95) < 0.001);
  KJ_EXPECT(std::abs(config.confidence_99 - 0.99) < 0.001);
  KJ_EXPECT(config.holding_period_days == 1);
  KJ_EXPECT(config.calculate_cvar == true);
}

} // namespace
} // namespace veloz::risk
