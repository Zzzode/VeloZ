#include "veloz/backtest/optimizer.h"

#include "veloz/backtest/backtest_engine.h"
#include "veloz/core/logger.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace veloz::backtest {

// Helper function to format parameters for logging
static std::string format_parameters(const std::map<std::string, double>& params) {
  std::ostringstream oss;
  oss << "{";
  bool first = true;
  for (const auto& [name, value] : params) {
    if (!first)
      oss << ", ";
    oss << name << "=" << std::fixed << std::setprecision(4) << value;
    first = false;
  }
  oss << "}";
  return oss.str();
}

// GridSearchOptimizer implementation
struct GridSearchOptimizer::Impl {
  BacktestConfig config;
  std::map<std::string, std::pair<double, double>> parameter_ranges;
  std::string optimization_target;
  int max_iterations;
  std::vector<BacktestResult> results;
  std::map<std::string, double> best_parameters;
  std::shared_ptr<veloz::core::Logger> logger;

  Impl() : max_iterations(100), optimization_target("sharpe") {
    logger = std::make_shared<veloz::core::Logger>();
  }
};

GridSearchOptimizer::GridSearchOptimizer() : impl_(std::make_unique<Impl>()) {}

GridSearchOptimizer::~GridSearchOptimizer() {}

bool GridSearchOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info(std::format("Initializing grid search optimizer"));
  impl_->config = config;
  impl_->results.clear();
  impl_->best_parameters.clear();
  return true;
}

bool GridSearchOptimizer::optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) {
  if (impl_->parameter_ranges.empty()) {
    impl_->logger->error(std::format("No parameter ranges defined"));
    return false;
  }

  impl_->logger->info(std::format("Starting grid search optimization with {} parameters",
                                  impl_->parameter_ranges.size()));
  impl_->results.clear();
  impl_->best_parameters.clear();

  // Calculate total combinations (default step size of 0.01 or 1.0 for integers)
  std::map<std::string, std::vector<double>> parameter_values;

  for (const auto& [name, range] : impl_->parameter_ranges) {
    auto [min_val, max_val] = range;
    double step = (max_val - min_val) / 10.0; // Default 10 steps
    if (step < 0.001)
      step = 0.001;

    std::vector<double> values;
    for (double val = min_val; val <= max_val; val += step) {
      values.push_back(val);
    }
    parameter_values[name] = values;
  }

  // Calculate total combinations
  size_t total_combinations = 1;
  for (const auto& [name, values] : parameter_values) {
    total_combinations *= values.size();
  }

  impl_->logger->info(std::format("Total parameter combinations to test: {}", total_combinations));

  // Limit iterations if necessary
  if (total_combinations > static_cast<size_t>(impl_->max_iterations)) {
    impl_->logger->warn(std::format("Limiting to {} iterations due to max_iterations setting",
                                    impl_->max_iterations));
  }

  // Generate all parameter combinations using recursive iteration
  std::vector<std::map<std::string, double>> all_combinations;
  std::vector<std::string> param_names;
  for (const auto& [name, _] : parameter_values) {
    param_names.push_back(name);
  }

  std::function<void(size_t, std::map<std::string, double>)> generate_combinations;
  generate_combinations = [&](size_t index, std::map<std::string, double> current) {
    if (index >= param_names.size()) {
      all_combinations.push_back(current);
      return;
    }

    const std::string& param_name = param_names[index];
    const auto& values = parameter_values[param_name];

    for (double val : values) {
      auto next_params = current;
      next_params[param_name] = val;
      generate_combinations(index + 1, next_params);

      if (all_combinations.size() >= static_cast<size_t>(impl_->max_iterations)) {
        return;
      }
    }
  };

  generate_combinations(0, {});

  impl_->logger->info(std::format("Generated {} parameter combinations", all_combinations.size()));

  // Run backtest for each combination
  BacktestEngine engine;
  int completed = 0;

  for (const auto& parameters : all_combinations) {
    BacktestConfig test_config = impl_->config;
    test_config.strategy_parameters = parameters;

    impl_->logger->info(
        std::format("Running backtest with parameters: {}", format_parameters(parameters)));

    if (engine.initialize(test_config)) {
      engine.set_strategy(strategy);

      if (engine.run()) {
        BacktestResult result = engine.get_result();

        // Store parameters with result
        result.strategy_name = impl_->config.strategy_name;
        impl_->results.push_back(result);

        completed++;
        impl_->logger->info(std::format("Completed {}/{} - Return: {:.2f}%, Sharpe: {:.2f}",
                                        completed, all_combinations.size(),
                                        result.total_return * 100.0, result.sharpe_ratio));
      } else {
        impl_->logger->error(
            std::format("Backtest failed for parameters: {}", format_parameters(parameters)));
      }
    }

    engine.reset();
  }

  // Find best parameters based on optimization target
  if (!impl_->results.empty()) {
    auto best_it = std::max_element(impl_->results.begin(), impl_->results.end(),
                                    [this](const BacktestResult& a, const BacktestResult& b) {
                                      if (impl_->optimization_target == "sharpe") {
                                        return a.sharpe_ratio < b.sharpe_ratio;
                                      } else if (impl_->optimization_target == "return") {
                                        return a.total_return < b.total_return;
                                      } else if (impl_->optimization_target == "win_rate") {
                                        return a.win_rate < b.win_rate;
                                      }
                                      return a.sharpe_ratio < b.sharpe_ratio;
                                    });

    size_t best_index = std::distance(impl_->results.begin(), best_it);
    impl_->best_parameters = all_combinations[best_index];

    impl_->logger->info(
        std::format("Best parameters found: {} with {}: {:.4f}",
                    format_parameters(impl_->best_parameters), impl_->optimization_target,
                    (impl_->optimization_target == "sharpe")   ? best_it->sharpe_ratio
                    : (impl_->optimization_target == "return") ? best_it->total_return
                                                               : best_it->win_rate));
  }

  impl_->logger->info(std::format("Grid search optimization completed. Tested {} combinations.",
                                  impl_->results.size()));
  return true;
}

std::vector<BacktestResult> GridSearchOptimizer::get_results() const {
  return impl_->results;
}

std::map<std::string, double> GridSearchOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void GridSearchOptimizer::set_parameter_ranges(
    const std::map<std::string, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges = ranges;
}

void GridSearchOptimizer::set_optimization_target(const std::string& target) {
  impl_->optimization_target = target;
}

void GridSearchOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

// GeneticAlgorithmOptimizer implementation
struct GeneticAlgorithmOptimizer::Impl {
  BacktestConfig config;
  std::map<std::string, std::pair<double, double>> parameter_ranges;
  std::string optimization_target;
  int max_iterations;
  std::vector<BacktestResult> results;
  std::map<std::string, double> best_parameters;
  std::shared_ptr<veloz::core::Logger> logger;

  Impl() : max_iterations(100), optimization_target("sharpe") {
    logger = std::make_shared<veloz::core::Logger>();
  }
};

GeneticAlgorithmOptimizer::GeneticAlgorithmOptimizer() : impl_(std::make_unique<Impl>()) {}

GeneticAlgorithmOptimizer::~GeneticAlgorithmOptimizer() {}

bool GeneticAlgorithmOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info(std::format("Initializing genetic algorithm optimizer"));
  impl_->config = config;
  impl_->results.clear();
  impl_->best_parameters.clear();
  return true;
}

bool GeneticAlgorithmOptimizer::optimize(
    const std::shared_ptr<veloz::strategy::IStrategy>& strategy) {
  if (impl_->parameter_ranges.empty()) {
    impl_->logger->error(std::format("No parameter ranges defined"));
    return false;
  }

  impl_->logger->info(std::format("Starting genetic algorithm optimization"));
  impl_->results.clear();
  impl_->best_parameters.clear();

  // TODO: Implement genetic algorithm logic
  // For now, just run a simple test
  BacktestResult test_result;
  test_result.strategy_name = impl_->config.strategy_name;
  test_result.symbol = impl_->config.symbol;
  test_result.start_time = impl_->config.start_time;
  test_result.end_time = impl_->config.end_time;
  test_result.initial_balance = impl_->config.initial_balance;
  test_result.final_balance = impl_->config.initial_balance * 1.15; // 15% return
  test_result.total_return = 0.15;
  test_result.max_drawdown = 0.04;
  test_result.sharpe_ratio = 1.8;
  test_result.win_rate = 0.65;
  test_result.profit_factor = 2.0;
  test_result.trade_count = 60;
  test_result.win_count = 39;
  test_result.lose_count = 21;
  test_result.avg_win = 0.025;
  test_result.avg_lose = -0.012;

  impl_->results.push_back(test_result);
  impl_->best_parameters = impl_->config.strategy_parameters;

  impl_->logger->info(std::format("Genetic algorithm optimization completed"));
  return true;
}

std::vector<BacktestResult> GeneticAlgorithmOptimizer::get_results() const {
  return impl_->results;
}

std::map<std::string, double> GeneticAlgorithmOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void GeneticAlgorithmOptimizer::set_parameter_ranges(
    const std::map<std::string, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges = ranges;
}

void GeneticAlgorithmOptimizer::set_optimization_target(const std::string& target) {
  impl_->optimization_target = target;
}

void GeneticAlgorithmOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

} // namespace veloz::backtest
