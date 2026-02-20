#pragma once

#include "veloz/backtest/types.h"
#include "veloz/strategy/strategy.h"

// KJ library includes
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

// std library includes with justifications
#include <utility> // std::pair - used in parameter ranges (min, max)

namespace veloz::backtest {

// Forward declaration
class IDataSource;

/**
 * @brief Optimization progress callback
 */
struct OptimizationProgress {
  int current_iteration{0};
  int total_iterations{0};
  double best_fitness{0.0};
  double current_fitness{0.0};
  double progress_fraction{0.0};
  kj::String status;
  kj::TreeMap<kj::String, double> current_parameters;
  kj::TreeMap<kj::String, double> best_parameters;
};

/**
 * @brief Ranked optimization result
 */
struct RankedResult {
  int rank{0};
  double fitness{0.0};
  kj::TreeMap<kj::String, double> parameters;
  BacktestResult result;
};

/**
 * @brief Optimization algorithm type
 */
enum class OptimizationAlgorithm : uint8_t {
  GridSearch = 0,
  GeneticAlgorithm = 1,
  RandomSearch = 2,
  BayesianOptimization = 3
};

// Parameter optimizer interface
class IParameterOptimizer {
public:
  virtual ~IParameterOptimizer() = default;

  virtual bool initialize(const BacktestConfig& config) = 0;
  // kj::Rc: matches strategy module's API (IStrategyFactory returns kj::Rc)
  virtual bool optimize(kj::Rc<veloz::strategy::IStrategy> strategy) = 0;
  virtual kj::Vector<BacktestResult> get_results() const = 0;
  // kj::TreeMap: KJ-native ordered map for parameter storage
  virtual const kj::TreeMap<kj::String, double>& get_best_parameters() const = 0;

  // kj::TreeMap: KJ-native ordered map for parameter ranges (min, max pairs)
  virtual void
  set_parameter_ranges(const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) = 0;
  virtual void set_optimization_target(kj::StringPtr target) = 0; // "sharpe", "return", "win_rate"
  virtual void set_max_iterations(int iterations) = 0;
  // kj::Rc: matches DataSourceFactory return type for reference-counted ownership
  virtual void set_data_source(kj::Rc<IDataSource> data_source) = 0;
};

// Grid search optimizer
class GridSearchOptimizer : public IParameterOptimizer {
public:
  GridSearchOptimizer();
  ~GridSearchOptimizer() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool optimize(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  const kj::TreeMap<kj::String, double>& get_best_parameters() const override;

  void
  set_parameter_ranges(const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

// Genetic algorithm optimizer
class GeneticAlgorithmOptimizer : public IParameterOptimizer {
public:
  GeneticAlgorithmOptimizer();
  ~GeneticAlgorithmOptimizer() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool optimize(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  const kj::TreeMap<kj::String, double>& get_best_parameters() const override;

  void
  set_parameter_ranges(const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

  // GA-specific configuration
  void set_population_size(int size);
  void set_mutation_rate(double rate);
  void set_crossover_rate(double rate);
  void set_elite_count(int count);
  void set_tournament_size(int size);
  void set_convergence_params(double threshold, int generations);

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

/**
 * @brief Random search optimizer
 *
 * Samples random parameter combinations from the search space.
 * Often competitive with grid search for high-dimensional spaces.
 */
class RandomSearchOptimizer : public IParameterOptimizer {
public:
  RandomSearchOptimizer();
  ~RandomSearchOptimizer() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool optimize(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  const kj::TreeMap<kj::String, double>& get_best_parameters() const override;

  void
  set_parameter_ranges(const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

  /**
   * @brief Set progress callback
   * @param callback Callback function for progress updates
   */
  void set_progress_callback(kj::Function<void(const OptimizationProgress&)> callback);

  /**
   * @brief Get ranked results
   * @param top_n Number of top results to return (0 = all)
   * @return Vector of ranked results
   */
  kj::Vector<RankedResult> get_ranked_results(int top_n = 0) const;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

/**
 * @brief Bayesian optimization using Gaussian Process surrogate model
 *
 * Uses a probabilistic model to guide the search, balancing exploration
 * and exploitation. More sample-efficient than grid/random search.
 */
class BayesianOptimizer : public IParameterOptimizer {
public:
  BayesianOptimizer();
  ~BayesianOptimizer() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool optimize(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  const kj::TreeMap<kj::String, double>& get_best_parameters() const override;

  void
  set_parameter_ranges(const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

  // Bayesian optimization configuration

  /**
   * @brief Set number of initial random samples
   * @param n_initial Number of random samples before using surrogate model
   */
  void set_initial_samples(int n_initial);

  /**
   * @brief Set acquisition function type
   * @param type "ei" (Expected Improvement), "ucb" (Upper Confidence Bound), "pi" (Probability of
   * Improvement)
   */
  void set_acquisition_function(kj::StringPtr type);

  /**
   * @brief Set exploration-exploitation trade-off parameter
   * @param kappa For UCB: higher = more exploration
   * @param xi For EI/PI: higher = more exploration
   */
  void set_exploration_params(double kappa, double xi);

  /**
   * @brief Set progress callback
   * @param callback Callback function for progress updates
   */
  void set_progress_callback(kj::Function<void(const OptimizationProgress&)> callback);

  /**
   * @brief Get ranked results
   * @param top_n Number of top results to return (0 = all)
   * @return Vector of ranked results
   */
  kj::Vector<RankedResult> get_ranked_results(int top_n = 0) const;

  /**
   * @brief Get surrogate model predictions for a parameter set
   * @param parameters Parameter values to predict
   * @return Pair of (mean, std) predictions
   */
  std::pair<double, double> predict(const kj::TreeMap<kj::String, double>& parameters) const;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

/**
 * @brief Optimizer factory for creating optimizers by type
 */
class OptimizerFactory {
public:
  /**
   * @brief Create an optimizer of the specified type
   * @param algorithm Optimization algorithm type
   * @return Owned pointer to optimizer
   */
  static kj::Own<IParameterOptimizer> create(OptimizationAlgorithm algorithm);
};

} // namespace veloz::backtest
