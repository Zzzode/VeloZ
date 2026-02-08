#pragma once

#include <memory>
#include <map>
#include <vector>
#include <string>

#include "veloz/backtest/types.h"
#include "veloz/strategy/strategy.h"

namespace veloz::backtest {

// Parameter optimizer interface
class IParameterOptimizer {
public:
    virtual ~IParameterOptimizer() = default;

    virtual bool initialize(const BacktestConfig& config) = 0;
    virtual bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
    virtual std::vector<BacktestResult> get_results() const = 0;
    virtual std::map<std::string, double> get_best_parameters() const = 0;

    virtual void set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) = 0;
    virtual void set_optimization_target(const std::string& target) = 0; // "sharpe", "return", "win_rate"
    virtual void set_max_iterations(int iterations) = 0;
};

// Grid search optimizer
class GridSearchOptimizer : public IParameterOptimizer {
public:
    GridSearchOptimizer();
    ~GridSearchOptimizer() override;

    bool initialize(const BacktestConfig& config) override;
    bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
    std::vector<BacktestResult> get_results() const override;
    std::map<std::string, double> get_best_parameters() const override;

    void set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) override;
    void set_optimization_target(const std::string& target) override;
    void set_max_iterations(int iterations) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Genetic algorithm optimizer
class GeneticAlgorithmOptimizer : public IParameterOptimizer {
public:
    GeneticAlgorithmOptimizer();
    ~GeneticAlgorithmOptimizer() override;

    bool initialize(const BacktestConfig& config) override;
    bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
    std::vector<BacktestResult> get_results() const override;
    std::map<std::string, double> get_best_parameters() const override;

    void set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) override;
    void set_optimization_target(const std::string& target) override;
    void set_max_iterations(int iterations) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace veloz::backtest
