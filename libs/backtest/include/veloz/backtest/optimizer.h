#pragma once

#include "veloz/backtest/types.h"
#include "veloz/strategy/strategy.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <map>
#include <memory>
#include <string>

namespace veloz::backtest {

// Forward declaration
class IDataSource;

// Parameter optimizer interface
class IParameterOptimizer {
public:
  virtual ~IParameterOptimizer() = default;

  virtual bool initialize(const BacktestConfig& config) = 0;
  // std::shared_ptr used for external API compatibility with strategy interface
  virtual bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
  virtual kj::Vector<BacktestResult> get_results() const = 0;
  // std::map used for external API compatibility with parameter ranges
  virtual std::map<std::string, double> get_best_parameters() const = 0;

  // std::map used for external API compatibility with parameter ranges
  virtual void
  set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) = 0;
  virtual void set_optimization_target(kj::StringPtr target) = 0; // "sharpe", "return", "win_rate"
  virtual void set_max_iterations(int iterations) = 0;
  virtual void set_data_source(const std::shared_ptr<IDataSource>& data_source) = 0;
};

// Grid search optimizer
class GridSearchOptimizer : public IParameterOptimizer {
public:
  GridSearchOptimizer();
  ~GridSearchOptimizer() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  std::map<std::string, double> get_best_parameters() const override;

  void
  set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(const std::shared_ptr<IDataSource>& data_source) override;

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
  bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
  kj::Vector<BacktestResult> get_results() const override;
  std::map<std::string, double> get_best_parameters() const override;

  void
  set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) override;
  void set_optimization_target(kj::StringPtr target) override;
  void set_max_iterations(int iterations) override;
  void set_data_source(const std::shared_ptr<IDataSource>& data_source) override;

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

} // namespace veloz::backtest
