#pragma once

#include "veloz/backtest/types.h"
#include "veloz/strategy/strategy.h"

// KJ library includes
#include <kj/common.h>
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

} // namespace veloz::backtest
