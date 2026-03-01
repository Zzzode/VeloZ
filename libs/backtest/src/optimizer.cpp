#include "veloz/backtest/optimizer.h"

#include "veloz/backtest/backtest_engine.h"
#include "veloz/core/logger.h"

// std library includes with justifications
#include <algorithm> // std::sort - KJ Vector lacks iterator compatibility for std::sort
#include <cmath>   // std::abs, std::sqrt, std::min, std::max - standard C++ math (no KJ equivalent)
#include <format>  // std::format - needed for width/precision specifiers (kj::str lacks this)
#include <limits>  // std::numeric_limits - numeric limits (no KJ equivalent)
#include <numeric> // std::accumulate - standard algorithm (no KJ equivalent)
#include <random>  // std::random_device, std::mt19937 - KJ has no RNG

// KJ library includes
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

// Helper function to format parameters for logging (kj::TreeMap version)
static kj::String format_parameters(const kj::TreeMap<kj::String, double>& params) {
  kj::Vector<kj::String> parts;
  parts.add(kj::str("{"));
  bool first = true;
  // kj::TreeMap iteration uses entry with .key and .value
  for (const auto& entry : params) {
    if (!first) {
      parts.add(kj::str(", "));
    }
    // Use std::format for width specifiers - kj::str() doesn't support precision formatting
    parts.add(kj::str(entry.key, "=", std::format("{:.4f}", entry.value).c_str()));
    first = false;
  }
  parts.add(kj::str("}"));
  return kj::strArray(parts, "");
}

// GridSearchOptimizer implementation
struct GridSearchOptimizer::Impl {
  BacktestConfig config;
  // kj::TreeMap for ordered parameter ranges (min, max pairs)
  kj::TreeMap<kj::String, std::pair<double, double>> parameter_ranges;
  kj::String optimization_target;
  int max_iterations;
  kj::Vector<BacktestResult> results;
  // kj::TreeMap for ordered best parameters storage
  kj::TreeMap<kj::String, double> best_parameters;
  // kj::Own used for unique ownership of internal logger instance
  kj::Own<veloz::core::Logger> logger;
  // kj::Rc for reference-counted data source
  kj::Rc<IDataSource> data_source;

  Impl()
      : optimization_target(kj::str("sharpe")), max_iterations(100),
        logger(kj::heap<veloz::core::Logger>()) {}
};

GridSearchOptimizer::GridSearchOptimizer() : impl_(kj::heap<Impl>()) {}

GridSearchOptimizer::~GridSearchOptimizer() noexcept {}

bool GridSearchOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing grid search optimizer");
  impl_->config.strategy_name = kj::str(config.strategy_name);
  impl_->config.symbol = kj::str(config.symbol);
  impl_->config.start_time = config.start_time;
  impl_->config.end_time = config.end_time;
  impl_->config.initial_balance = config.initial_balance;
  impl_->config.risk_per_trade = config.risk_per_trade;
  impl_->config.max_position_size = config.max_position_size;
  impl_->config.strategy_parameters.clear();
  for (const auto& entry : config.strategy_parameters) {
    impl_->config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
  }
  impl_->config.data_source = kj::str(config.data_source);
  impl_->config.data_type = kj::str(config.data_type);
  impl_->config.time_frame = kj::str(config.time_frame);
  impl_->results.clear();
  impl_->best_parameters.clear();
  return true;
}

bool GridSearchOptimizer::optimize(kj::Rc<veloz::strategy::IStrategy> strategy) {
  if (impl_->parameter_ranges.size() == 0) {
    impl_->logger->error("No parameter ranges defined");
    return false;
  }

  impl_->logger->info(kj::str("Starting grid search optimization with ",
                              impl_->parameter_ranges.size(), " parameters"));
  impl_->results.clear();
  impl_->best_parameters.clear();

  // Calculate total combinations (default step size of 0.01 or 1.0 for integers)
  // kj::TreeMap for parameter values storage
  kj::TreeMap<kj::String, kj::Vector<double>> parameter_values;

  // kj::TreeMap iteration uses entry with .key and .value
  for (const auto& entry : impl_->parameter_ranges) {
    auto [min_val, max_val] = entry.value;
    double step = (max_val - min_val) / 10.0; // Default 10 steps
    if (step < 0.001)
      step = 0.001;

    kj::Vector<double> values;
    for (double val = min_val; val <= max_val; val += step) {
      values.add(val);
    }
    parameter_values.insert(kj::str(entry.key), kj::mv(values));
  }

  // Calculate total combinations
  size_t total_combinations = 1;
  for (const auto& entry : parameter_values) {
    total_combinations *= entry.value.size();
  }

  impl_->logger->info(kj::str("Total parameter combinations to test: ", total_combinations));

  // Limit iterations if necessary
  if (total_combinations > static_cast<size_t>(impl_->max_iterations)) {
    impl_->logger->warn(kj::str("Limiting to ", impl_->max_iterations,
                                " iterations due to max_iterations setting"));
  }

  // Generate all parameter combinations using recursive iteration
  // Using kj::TreeMap for parameter combinations
  kj::Vector<kj::TreeMap<kj::String, double>> all_combinations;
  kj::Vector<kj::String> param_names;
  for (const auto& entry : parameter_values) {
    param_names.add(kj::str(entry.key));
  }

  auto generate_combinations = [&](auto&& self, size_t index,
                                   kj::TreeMap<kj::String, double>& current) -> void {
    if (index >= param_names.size()) {
      // Deep copy current to all_combinations
      kj::TreeMap<kj::String, double> copy;
      for (const auto& e : current) {
        copy.insert(kj::str(e.key), e.value);
      }
      all_combinations.add(kj::mv(copy));
      return;
    }

    kj::StringPtr param_name = param_names[index];
    KJ_IF_SOME(values_ref, parameter_values.find(param_name)) {
      for (size_t i = 0; i < values_ref.size(); ++i) {
        double val = values_ref[i];
        current.upsert(kj::str(param_name), val);
        self(self, index + 1, current);

        if (all_combinations.size() >= static_cast<size_t>(impl_->max_iterations)) {
          return;
        }
      }
    }
  };

  kj::TreeMap<kj::String, double> initial_params;
  generate_combinations(generate_combinations, 0, initial_params);

  impl_->logger->info(kj::str("Generated ", all_combinations.size(), " parameter combinations"));

  // Run backtest for each combination
  BacktestEngine engine;
  int completed = 0;

  for (size_t combo_idx = 0; combo_idx < all_combinations.size(); ++combo_idx) {
    const auto& parameters = all_combinations[combo_idx];
    BacktestConfig test_config;
    test_config.strategy_name = kj::str(impl_->config.strategy_name);
    test_config.symbol = kj::str(impl_->config.symbol);
    test_config.start_time = impl_->config.start_time;
    test_config.end_time = impl_->config.end_time;
    test_config.initial_balance = impl_->config.initial_balance;
    test_config.risk_per_trade = impl_->config.risk_per_trade;
    test_config.max_position_size = impl_->config.max_position_size;
    test_config.data_source = kj::str(impl_->config.data_source);
    test_config.data_type = kj::str(impl_->config.data_type);
    test_config.time_frame = kj::str(impl_->config.time_frame);
    for (const auto& entry : parameters) {
      test_config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
    }

    impl_->logger->info(
        kj::str("Running backtest with parameters: ", format_parameters(parameters)));

    if (engine.initialize(test_config)) {
      engine.set_strategy(strategy.addRef());
      if (impl_->data_source.get() != nullptr) {
        engine.set_data_source(impl_->data_source.addRef());
      }

      if (engine.run()) {
        BacktestResult result = engine.get_result();

        // Store parameters with result
        result.strategy_name = kj::str(impl_->config.strategy_name);
        impl_->results.add(kj::mv(result));

        completed++;
        // Use std::format for precision formatting - kj::str() doesn't support {:.2f}
        auto return_str = std::format("{:.2f}", impl_->results.back().total_return * 100.0);
        auto sharpe_str = std::format("{:.2f}", impl_->results.back().sharpe_ratio);
        impl_->logger->info(kj::str("Completed ", completed, "/", all_combinations.size(),
                                    " - Return: ", return_str.c_str(),
                                    "%, Sharpe: ", sharpe_str.c_str()));
      } else {
        impl_->logger->error(
            kj::str("Backtest failed for parameters: ", format_parameters(parameters)));
      }
    }

    engine.reset();
  }

  // Find best parameters based on optimization target
  if (impl_->results.size() > 0) {
    size_t best_index = 0;
    double best_value = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < impl_->results.size(); ++i) {
      double value;
      if (impl_->optimization_target == "sharpe"_kj) {
        value = impl_->results[i].sharpe_ratio;
      } else if (impl_->optimization_target == "return"_kj) {
        value = impl_->results[i].total_return;
      } else if (impl_->optimization_target == "win_rate"_kj) {
        value = impl_->results[i].win_rate;
      } else {
        value = impl_->results[i].sharpe_ratio;
      }

      if (value > best_value) {
        best_value = value;
        best_index = i;
      }
    }

    // Copy best parameters from all_combinations to impl_->best_parameters
    impl_->best_parameters.clear();
    for (const auto& entry : all_combinations[best_index]) {
      impl_->best_parameters.insert(kj::str(entry.key), entry.value);
    }

    // Use std::format for precision formatting - kj::str() doesn't support {:.4f}
    auto best_value_str = std::format("{:.4f}", best_value);
    impl_->logger->info(
        kj::str("Best parameters found: ", format_parameters(impl_->best_parameters), " with ",
                impl_->optimization_target, ": ", best_value_str.c_str()));
  }

  impl_->logger->info(kj::str("Grid search optimization completed. Tested ", impl_->results.size(),
                              " combinations."));
  return true;
}

kj::Vector<BacktestResult> GridSearchOptimizer::get_results() const {
  kj::Vector<BacktestResult> results;
  for (const auto& result : impl_->results) {
    BacktestResult copy;
    copy.strategy_name = kj::str(result.strategy_name);
    copy.symbol = kj::str(result.symbol);
    copy.start_time = result.start_time;
    copy.end_time = result.end_time;
    copy.initial_balance = result.initial_balance;
    copy.final_balance = result.final_balance;
    copy.total_return = result.total_return;
    copy.max_drawdown = result.max_drawdown;
    copy.sharpe_ratio = result.sharpe_ratio;
    copy.win_rate = result.win_rate;
    copy.profit_factor = result.profit_factor;
    copy.trade_count = result.trade_count;
    copy.win_count = result.win_count;
    copy.lose_count = result.lose_count;
    copy.avg_win = result.avg_win;
    copy.avg_lose = result.avg_lose;
    results.add(kj::mv(copy));
  }
  return results;
}

const kj::TreeMap<kj::String, double>& GridSearchOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void GridSearchOptimizer::set_parameter_ranges(
    const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges.clear();
  // Copy ranges to impl_->parameter_ranges
  for (const auto& entry : ranges) {
    impl_->parameter_ranges.insert(kj::str(entry.key), entry.value);
  }
}

void GridSearchOptimizer::set_optimization_target(kj::StringPtr target) {
  impl_->optimization_target = kj::str(target);
}

void GridSearchOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

void GridSearchOptimizer::set_data_source(kj::Rc<IDataSource> data_source) {
  impl_->data_source = kj::mv(data_source);
}

// GeneticAlgorithmOptimizer implementation

// Individual in the population
struct Individual {
  // kj::TreeMap for ordered parameter storage
  kj::TreeMap<kj::String, double> parameters;
  double fitness;
  BacktestResult result;

  Individual() : fitness(-std::numeric_limits<double>::infinity()) {}

  // Copy constructor - needed because BacktestResult and kj::TreeMap contain kj::String
  Individual(const Individual& other) : fitness(other.fitness) {
    // Deep copy parameters
    for (const auto& entry : other.parameters) {
      parameters.insert(kj::str(entry.key), entry.value);
    }
    result.strategy_name = kj::str(other.result.strategy_name);
    result.symbol = kj::str(other.result.symbol);
    result.start_time = other.result.start_time;
    result.end_time = other.result.end_time;
    result.initial_balance = other.result.initial_balance;
    result.final_balance = other.result.final_balance;
    result.total_return = other.result.total_return;
    result.max_drawdown = other.result.max_drawdown;
    result.sharpe_ratio = other.result.sharpe_ratio;
    result.win_rate = other.result.win_rate;
    result.profit_factor = other.result.profit_factor;
    result.trade_count = other.result.trade_count;
    result.win_count = other.result.win_count;
    result.lose_count = other.result.lose_count;
    result.avg_win = other.result.avg_win;
    result.avg_lose = other.result.avg_lose;
  }

  // Copy assignment operator
  Individual& operator=(const Individual& other) {
    if (this != &other) {
      // Deep copy parameters
      parameters.clear();
      for (const auto& entry : other.parameters) {
        parameters.insert(kj::str(entry.key), entry.value);
      }
      fitness = other.fitness;
      result.strategy_name = kj::str(other.result.strategy_name);
      result.symbol = kj::str(other.result.symbol);
      result.start_time = other.result.start_time;
      result.end_time = other.result.end_time;
      result.initial_balance = other.result.initial_balance;
      result.final_balance = other.result.final_balance;
      result.total_return = other.result.total_return;
      result.max_drawdown = other.result.max_drawdown;
      result.sharpe_ratio = other.result.sharpe_ratio;
      result.win_rate = other.result.win_rate;
      result.profit_factor = other.result.profit_factor;
      result.trade_count = other.result.trade_count;
      result.win_count = other.result.win_count;
      result.lose_count = other.result.lose_count;
      result.avg_win = other.result.avg_win;
      result.avg_lose = other.result.avg_lose;
    }
    return *this;
  }

  // Move constructor
  Individual(Individual&& other) noexcept = default;

  // Move assignment operator
  Individual& operator=(Individual&& other) noexcept = default;
};

struct GeneticAlgorithmOptimizer::Impl {
  BacktestConfig config;
  // kj::TreeMap for ordered parameter ranges (min, max pairs)
  kj::TreeMap<kj::String, std::pair<double, double>> parameter_ranges;
  kj::String optimization_target;
  int max_iterations;           // Number of generations
  int population_size;          // Size of population
  double mutation_rate;         // Probability of mutation per gene
  double crossover_rate;        // Probability of crossover
  int elite_count;              // Number of elite individuals to preserve
  int tournament_size;          // Tournament selection size
  double convergence_threshold; // Stop if improvement is below this
  int convergence_generations;  // Number of generations to check for convergence

  kj::Vector<BacktestResult> results;
  // kj::TreeMap for ordered best parameters storage
  kj::TreeMap<kj::String, double> best_parameters;
  // kj::Own used for unique ownership of internal logger instance
  kj::Own<veloz::core::Logger> logger;
  // kj::Rc for reference-counted data source
  kj::Rc<IDataSource> data_source;

  // Random number generation
  std::mt19937 rng;

  Impl()
      : optimization_target(kj::str("sharpe")), max_iterations(50), population_size(20),
        mutation_rate(0.1), crossover_rate(0.8), elite_count(2), tournament_size(3),
        convergence_threshold(0.001), convergence_generations(5),
        logger(kj::heap<veloz::core::Logger>()) {
    // Seed random number generator
    std::random_device rd;
    rng.seed(rd());
  }

  // Generate a random individual within parameter ranges
  Individual create_random_individual() {
    Individual ind;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // kj::TreeMap iteration uses entry with .key and .value
    for (const auto& entry : parameter_ranges) {
      double min_val = entry.value.first;
      double max_val = entry.value.second;
      ind.parameters.insert(kj::str(entry.key), min_val + dist(rng) * (max_val - min_val));
    }
    return ind;
  }

  // Evaluate fitness of an individual using backtest
  double evaluate_fitness(Individual& ind, BacktestEngine& engine,
                          kj::Rc<veloz::strategy::IStrategy>& strategy) {
    BacktestConfig test_config;
    test_config.strategy_name = kj::str(config.strategy_name);
    test_config.symbol = kj::str(config.symbol);
    test_config.start_time = config.start_time;
    test_config.end_time = config.end_time;
    test_config.initial_balance = config.initial_balance;
    test_config.risk_per_trade = config.risk_per_trade;
    test_config.max_position_size = config.max_position_size;
    test_config.data_source = kj::str(config.data_source);
    test_config.data_type = kj::str(config.data_type);
    test_config.time_frame = kj::str(config.time_frame);
    for (const auto& entry : ind.parameters) {
      test_config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
    }

    if (!engine.initialize(test_config)) {
      ind.fitness = -std::numeric_limits<double>::infinity();
      return ind.fitness;
    }

    engine.set_strategy(strategy.addRef());
    if (data_source.get() != nullptr) {
      engine.set_data_source(data_source.addRef());
    }

    if (!engine.run()) {
      ind.fitness = -std::numeric_limits<double>::infinity();
      engine.reset();
      return ind.fitness;
    }

    ind.result = engine.get_result();
    ind.result.strategy_name = kj::str(config.strategy_name);
    engine.reset();

    // Calculate fitness based on optimization target
    if (optimization_target == "sharpe"_kj) {
      ind.fitness = ind.result.sharpe_ratio;
    } else if (optimization_target == "return"_kj) {
      ind.fitness = ind.result.total_return;
    } else if (optimization_target == "win_rate"_kj) {
      ind.fitness = ind.result.win_rate;
    } else if (optimization_target == "profit_factor"_kj) {
      ind.fitness = ind.result.profit_factor;
    } else {
      // Default to Sharpe ratio
      ind.fitness = ind.result.sharpe_ratio;
    }

    // Penalize for excessive drawdown
    if (ind.result.max_drawdown > 0.3) {
      ind.fitness *= (1.0 - ind.result.max_drawdown);
    }

    return ind.fitness;
  }

  // Tournament selection
  Individual& tournament_select(kj::Vector<Individual>& population) {
    std::uniform_int_distribution<size_t> dist(0, population.size() - 1);

    size_t best_idx = dist(rng);
    double best_fitness = population[best_idx].fitness;

    for (int i = 1; i < tournament_size; ++i) {
      size_t idx = dist(rng);
      if (population[idx].fitness > best_fitness) {
        best_idx = idx;
        best_fitness = population[idx].fitness;
      }
    }

    return population[best_idx];
  }

  // Uniform crossover
  Individual crossover(const Individual& parent1, const Individual& parent2) {
    Individual child;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // kj::TreeMap iteration uses entry with .key and .value
    for (const auto& entry : parameter_ranges) {
      kj::StringPtr name = entry.key;
      if (dist(rng) < 0.5) {
        KJ_IF_SOME(val, parent1.parameters.find(name)) {
          child.parameters.insert(kj::str(name), val);
        }
      } else {
        KJ_IF_SOME(val, parent2.parameters.find(name)) {
          child.parameters.insert(kj::str(name), val);
        }
      }
    }

    return child;
  }

  // Blend crossover (BLX-alpha)
  Individual blend_crossover(const Individual& parent1, const Individual& parent2,
                             double alpha = 0.5) {
    Individual child;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // kj::TreeMap iteration uses entry with .key and .value
    for (const auto& entry : parameter_ranges) {
      kj::StringPtr name = entry.key;
      const auto& range = entry.value;
      double p1 = 0.0, p2 = 0.0;
      KJ_IF_SOME(val1, parent1.parameters.find(name)) {
        p1 = val1;
      }
      KJ_IF_SOME(val2, parent2.parameters.find(name)) {
        p2 = val2;
      }
      double min_p = std::min(p1, p2);
      double max_p = std::max(p1, p2);
      double d = max_p - min_p;

      double low = min_p - alpha * d;
      double high = max_p + alpha * d;

      // Clamp to parameter range
      low = std::max(low, range.first);
      high = std::min(high, range.second);

      child.parameters.insert(kj::str(name), low + dist(rng) * (high - low));
    }

    return child;
  }

  // Mutation
  void mutate(Individual& ind) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::normal_distribution<double> gaussian(0.0, 0.1);

    // kj::TreeMap iteration uses entry with .key and .value
    for (const auto& entry : parameter_ranges) {
      kj::StringPtr name = entry.key;
      const auto& range = entry.value;
      if (dist(rng) < mutation_rate) {
        double min_val = range.first;
        double max_val = range.second;
        double range_size = max_val - min_val;

        // Gaussian mutation
        double delta = gaussian(rng) * range_size;
        KJ_IF_SOME(current_val, ind.parameters.find(name)) {
          double new_val = current_val + delta;
          // Clamp to valid range
          new_val = std::max(min_val, std::min(max_val, new_val));
          ind.parameters.upsert(kj::str(name), new_val);
        }
      }
    }
  }
};

GeneticAlgorithmOptimizer::GeneticAlgorithmOptimizer() : impl_(kj::heap<Impl>()) {}

GeneticAlgorithmOptimizer::~GeneticAlgorithmOptimizer() noexcept {}

bool GeneticAlgorithmOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing genetic algorithm optimizer");
  impl_->config.strategy_name = kj::str(config.strategy_name);
  impl_->config.symbol = kj::str(config.symbol);
  impl_->config.start_time = config.start_time;
  impl_->config.end_time = config.end_time;
  impl_->config.initial_balance = config.initial_balance;
  impl_->config.risk_per_trade = config.risk_per_trade;
  impl_->config.max_position_size = config.max_position_size;
  impl_->config.strategy_parameters.clear();
  for (const auto& entry : config.strategy_parameters) {
    impl_->config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
  }
  impl_->config.data_source = kj::str(config.data_source);
  impl_->config.data_type = kj::str(config.data_type);
  impl_->config.time_frame = kj::str(config.time_frame);
  impl_->results.clear();
  impl_->best_parameters.clear();
  return true;
}

bool GeneticAlgorithmOptimizer::optimize(kj::Rc<veloz::strategy::IStrategy> strategy) {
  if (impl_->parameter_ranges.size() == 0) {
    impl_->logger->error("No parameter ranges defined");
    return false;
  }

  // Use std::format for precision formatting - kj::str() doesn't support {:.2f}
  auto mutation_str = std::format("{:.2f}", impl_->mutation_rate);
  impl_->logger->info(
      kj::str("Starting genetic algorithm optimization: pop_size=", impl_->population_size,
              ", generations=", impl_->max_iterations, ", mutation_rate=", mutation_str.c_str()));
  impl_->results.clear();
  impl_->best_parameters.clear();

  BacktestEngine engine;

  // Initialize population
  kj::Vector<Individual> population;
  population.reserve(impl_->population_size);

  impl_->logger->info(
      kj::str("Initializing population with ", impl_->population_size, " individuals"));

  for (int i = 0; i < impl_->population_size; ++i) {
    Individual ind = impl_->create_random_individual();
    impl_->evaluate_fitness(ind, engine, strategy);
    population.add(kj::mv(ind));
  }

  // Track best individual
  Individual best_overall;
  best_overall.fitness = -std::numeric_limits<double>::infinity();

  // Track fitness history for convergence detection
  kj::Vector<double> best_fitness_history;

  // Evolution loop
  for (int generation = 0; generation < impl_->max_iterations; ++generation) {
    // Sort population by fitness (descending)
    std::sort(population.begin(), population.end(),
              [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });

    // Update best overall
    if (population[0].fitness > best_overall.fitness) {
      best_overall = population[0];
      best_overall.result.strategy_name = kj::str(impl_->config.strategy_name);
    }

    best_fitness_history.add(population[0].fitness);

    // Use std::format for precision formatting - kj::str() doesn't support {:.4f}
    auto best_str = std::format("{:.4f}", population[0].fitness);
    // std::accumulate needed for algorithm - no KJ equivalent
    double avg_fitness =
        std::accumulate(population.begin(), population.end(), 0.0,
                        [](double sum, const Individual& ind) { return sum + ind.fitness; }) /
        population.size();
    auto avg_str = std::format("{:.4f}", avg_fitness);
    impl_->logger->info(kj::str("Generation ", generation + 1, "/", impl_->max_iterations,
                                ": Best fitness = ", best_str.c_str(),
                                ", Avg fitness = ", avg_str.c_str()));

    // Check for convergence
    if (best_fitness_history.size() >= static_cast<size_t>(impl_->convergence_generations)) {
      size_t start_idx = best_fitness_history.size() - impl_->convergence_generations;
      double improvement = best_fitness_history.back() - best_fitness_history[start_idx];

      if (std::abs(improvement) < impl_->convergence_threshold) {
        // Use std::format for precision formatting - kj::str() doesn't support {:.6f}
        auto imp_str = std::format("{:.6f}", improvement);
        auto thresh_str = std::format("{:.6f}", impl_->convergence_threshold);
        impl_->logger->info(kj::str("Convergence detected at generation ", generation + 1,
                                    " (improvement ", imp_str.c_str(), " < threshold ",
                                    thresh_str.c_str(), ")"));
        break;
      }
    }

    // Create next generation
    kj::Vector<Individual> next_generation;
    next_generation.reserve(impl_->population_size);

    // Elitism: preserve best individuals
    for (int i = 0; i < impl_->elite_count && i < static_cast<int>(population.size()); ++i) {
      next_generation.add(population[i]);
    }

    // Fill rest of population with offspring
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    while (static_cast<int>(next_generation.size()) < impl_->population_size) {
      // Select parents using tournament selection
      Individual& parent1 = impl_->tournament_select(population);
      Individual& parent2 = impl_->tournament_select(population);

      Individual child;

      // Crossover
      if (dist(impl_->rng) < impl_->crossover_rate) {
        // Use blend crossover for continuous parameters
        child = impl_->blend_crossover(parent1, parent2, 0.5);
      } else {
        // Copy one parent
        child = (dist(impl_->rng) < 0.5) ? parent1 : parent2;
      }

      // Mutation
      impl_->mutate(child);

      // Evaluate fitness
      impl_->evaluate_fitness(child, engine, strategy);

      next_generation.add(kj::mv(child));
    }

    population = kj::mv(next_generation);
  }

  // Final sort and update best
  std::sort(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });

  if (population[0].fitness > best_overall.fitness) {
    best_overall = population[0];
    best_overall.result.strategy_name = kj::str(impl_->config.strategy_name);
  }

  // Store best parameters and result - deep copy kj::TreeMap
  impl_->best_parameters.clear();
  for (const auto& entry : best_overall.parameters) {
    impl_->best_parameters.insert(kj::str(entry.key), entry.value);
  }

  // Store all results from final population
  for (const auto& ind : population) {
    if (ind.fitness > -std::numeric_limits<double>::infinity()) {
      BacktestResult result_copy;
      result_copy.strategy_name = kj::str(ind.result.strategy_name);
      result_copy.symbol = kj::str(ind.result.symbol);
      result_copy.start_time = ind.result.start_time;
      result_copy.end_time = ind.result.end_time;
      result_copy.initial_balance = ind.result.initial_balance;
      result_copy.final_balance = ind.result.final_balance;
      result_copy.total_return = ind.result.total_return;
      result_copy.max_drawdown = ind.result.max_drawdown;
      result_copy.sharpe_ratio = ind.result.sharpe_ratio;
      result_copy.win_rate = ind.result.win_rate;
      result_copy.profit_factor = ind.result.profit_factor;
      result_copy.trade_count = ind.result.trade_count;
      result_copy.win_count = ind.result.win_count;
      result_copy.lose_count = ind.result.lose_count;
      result_copy.avg_win = ind.result.avg_win;
      result_copy.avg_lose = ind.result.avg_lose;
      impl_->results.add(kj::mv(result_copy));
    }
  }

  // Use std::format for precision formatting - kj::str() doesn't support {:.4f}
  auto fitness_str = std::format("{:.4f}", best_overall.fitness);
  impl_->logger->info(kj::str("Genetic algorithm optimization completed. Best ",
                              impl_->optimization_target, ": ", fitness_str.c_str()));
  impl_->logger->info(kj::str("Best parameters: ", format_parameters(impl_->best_parameters)));

  return true;
}

kj::Vector<BacktestResult> GeneticAlgorithmOptimizer::get_results() const {
  kj::Vector<BacktestResult> results;
  for (const auto& result : impl_->results) {
    BacktestResult copy;
    copy.strategy_name = kj::str(result.strategy_name);
    copy.symbol = kj::str(result.symbol);
    copy.start_time = result.start_time;
    copy.end_time = result.end_time;
    copy.initial_balance = result.initial_balance;
    copy.final_balance = result.final_balance;
    copy.total_return = result.total_return;
    copy.max_drawdown = result.max_drawdown;
    copy.sharpe_ratio = result.sharpe_ratio;
    copy.win_rate = result.win_rate;
    copy.profit_factor = result.profit_factor;
    copy.trade_count = result.trade_count;
    copy.win_count = result.win_count;
    copy.lose_count = result.lose_count;
    copy.avg_win = result.avg_win;
    copy.avg_lose = result.avg_lose;
    results.add(kj::mv(copy));
  }
  return results;
}

const kj::TreeMap<kj::String, double>& GeneticAlgorithmOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void GeneticAlgorithmOptimizer::set_parameter_ranges(
    const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges.clear();
  // Copy ranges to impl_->parameter_ranges
  for (const auto& entry : ranges) {
    impl_->parameter_ranges.insert(kj::str(entry.key), entry.value);
  }
}

void GeneticAlgorithmOptimizer::set_optimization_target(kj::StringPtr target) {
  impl_->optimization_target = kj::str(target);
}

void GeneticAlgorithmOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

void GeneticAlgorithmOptimizer::set_population_size(int size) {
  impl_->population_size = std::max(4, size); // Minimum 4 individuals
}

void GeneticAlgorithmOptimizer::set_mutation_rate(double rate) {
  impl_->mutation_rate = std::max(0.0, std::min(1.0, rate));
}

void GeneticAlgorithmOptimizer::set_crossover_rate(double rate) {
  impl_->crossover_rate = std::max(0.0, std::min(1.0, rate));
}

void GeneticAlgorithmOptimizer::set_elite_count(int count) {
  impl_->elite_count = std::max(0, count);
}

void GeneticAlgorithmOptimizer::set_tournament_size(int size) {
  impl_->tournament_size = std::max(2, size);
}

void GeneticAlgorithmOptimizer::set_convergence_params(double threshold, int generations) {
  impl_->convergence_threshold = std::max(0.0, threshold);
  impl_->convergence_generations = std::max(1, generations);
}

void GeneticAlgorithmOptimizer::set_data_source(kj::Rc<IDataSource> data_source) {
  impl_->data_source = kj::mv(data_source);
}

// ============================================================================
// RandomSearchOptimizer implementation
// ============================================================================

struct RandomSearchOptimizer::Impl {
  BacktestConfig config;
  kj::TreeMap<kj::String, std::pair<double, double>> parameter_ranges;
  kj::String optimization_target;
  int max_iterations;
  kj::Vector<BacktestResult> results;
  kj::Vector<kj::TreeMap<kj::String, double>> all_parameters;
  kj::TreeMap<kj::String, double> best_parameters;
  kj::Own<veloz::core::Logger> logger;
  kj::Rc<IDataSource> data_source;
  kj::Maybe<kj::Function<void(const OptimizationProgress&)>> progress_callback;

  std::mt19937 rng;

  Impl()
      : optimization_target(kj::str("sharpe")), max_iterations(100),
        logger(kj::heap<veloz::core::Logger>()) {
    std::random_device rd;
    rng.seed(rd());
  }

  kj::TreeMap<kj::String, double> generate_random_params() {
    kj::TreeMap<kj::String, double> params;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (const auto& entry : parameter_ranges) {
      double min_val = entry.value.first;
      double max_val = entry.value.second;
      params.insert(kj::str(entry.key), min_val + dist(rng) * (max_val - min_val));
    }
    return params;
  }
};

RandomSearchOptimizer::RandomSearchOptimizer() : impl_(kj::heap<Impl>()) {}

RandomSearchOptimizer::~RandomSearchOptimizer() noexcept {}

bool RandomSearchOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing random search optimizer");
  impl_->config.strategy_name = kj::str(config.strategy_name);
  impl_->config.symbol = kj::str(config.symbol);
  impl_->config.start_time = config.start_time;
  impl_->config.end_time = config.end_time;
  impl_->config.initial_balance = config.initial_balance;
  impl_->config.risk_per_trade = config.risk_per_trade;
  impl_->config.max_position_size = config.max_position_size;
  impl_->config.strategy_parameters.clear();
  for (const auto& entry : config.strategy_parameters) {
    impl_->config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
  }
  impl_->config.data_source = kj::str(config.data_source);
  impl_->config.data_type = kj::str(config.data_type);
  impl_->config.time_frame = kj::str(config.time_frame);
  impl_->results.clear();
  impl_->all_parameters.clear();
  impl_->best_parameters.clear();
  return true;
}

bool RandomSearchOptimizer::optimize(kj::Rc<veloz::strategy::IStrategy> strategy) {
  if (impl_->parameter_ranges.size() == 0) {
    impl_->logger->error("No parameter ranges defined");
    return false;
  }

  impl_->logger->info(
      kj::str("Starting random search optimization with ", impl_->max_iterations, " iterations"));
  impl_->results.clear();
  impl_->all_parameters.clear();
  impl_->best_parameters.clear();

  BacktestEngine engine;
  double best_fitness = -std::numeric_limits<double>::infinity();

  for (int iter = 0; iter < impl_->max_iterations; ++iter) {
    auto params = impl_->generate_random_params();

    BacktestConfig test_config;
    test_config.strategy_name = kj::str(impl_->config.strategy_name);
    test_config.symbol = kj::str(impl_->config.symbol);
    test_config.start_time = impl_->config.start_time;
    test_config.end_time = impl_->config.end_time;
    test_config.initial_balance = impl_->config.initial_balance;
    test_config.risk_per_trade = impl_->config.risk_per_trade;
    test_config.max_position_size = impl_->config.max_position_size;
    test_config.data_source = kj::str(impl_->config.data_source);
    test_config.data_type = kj::str(impl_->config.data_type);
    test_config.time_frame = kj::str(impl_->config.time_frame);
    for (const auto& entry : params) {
      test_config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
    }

    double fitness = -std::numeric_limits<double>::infinity();

    if (engine.initialize(test_config)) {
      engine.set_strategy(strategy.addRef());
      if (impl_->data_source.get() != nullptr) {
        engine.set_data_source(impl_->data_source.addRef());
      }

      if (engine.run()) {
        BacktestResult result = engine.get_result();
        result.strategy_name = kj::str(impl_->config.strategy_name);

        if (impl_->optimization_target == "sharpe"_kj) {
          fitness = result.sharpe_ratio;
        } else if (impl_->optimization_target == "return"_kj) {
          fitness = result.total_return;
        } else if (impl_->optimization_target == "win_rate"_kj) {
          fitness = result.win_rate;
        } else {
          fitness = result.sharpe_ratio;
        }

        impl_->results.add(kj::mv(result));

        // Deep copy params
        kj::TreeMap<kj::String, double> params_copy;
        for (const auto& e : params) {
          params_copy.insert(kj::str(e.key), e.value);
        }
        impl_->all_parameters.add(kj::mv(params_copy));

        if (fitness > best_fitness) {
          best_fitness = fitness;
          impl_->best_parameters.clear();
          for (const auto& e : params) {
            impl_->best_parameters.insert(kj::str(e.key), e.value);
          }
        }
      }
    }

    engine.reset();

    // Progress callback
    KJ_IF_SOME(callback, impl_->progress_callback) {
      OptimizationProgress progress;
      progress.current_iteration = iter + 1;
      progress.total_iterations = impl_->max_iterations;
      progress.best_fitness = best_fitness;
      progress.current_fitness = fitness;
      progress.progress_fraction =
          static_cast<double>(iter + 1) / static_cast<double>(impl_->max_iterations);
      progress.status = kj::str("Iteration ", iter + 1, "/", impl_->max_iterations);
      for (const auto& e : params) {
        progress.current_parameters.insert(kj::str(e.key), e.value);
      }
      for (const auto& e : impl_->best_parameters) {
        progress.best_parameters.insert(kj::str(e.key), e.value);
      }
      callback(progress);
    }
  }

  auto best_str = std::format("{:.4f}", best_fitness);
  impl_->logger->info(kj::str("Random search completed. Best ", impl_->optimization_target, ": ",
                              best_str.c_str()));

  return true;
}

kj::Vector<BacktestResult> RandomSearchOptimizer::get_results() const {
  kj::Vector<BacktestResult> results;
  for (const auto& result : impl_->results) {
    BacktestResult copy;
    copy.strategy_name = kj::str(result.strategy_name);
    copy.symbol = kj::str(result.symbol);
    copy.start_time = result.start_time;
    copy.end_time = result.end_time;
    copy.initial_balance = result.initial_balance;
    copy.final_balance = result.final_balance;
    copy.total_return = result.total_return;
    copy.max_drawdown = result.max_drawdown;
    copy.sharpe_ratio = result.sharpe_ratio;
    copy.win_rate = result.win_rate;
    copy.profit_factor = result.profit_factor;
    copy.trade_count = result.trade_count;
    copy.win_count = result.win_count;
    copy.lose_count = result.lose_count;
    copy.avg_win = result.avg_win;
    copy.avg_lose = result.avg_lose;
    results.add(kj::mv(copy));
  }
  return results;
}

const kj::TreeMap<kj::String, double>& RandomSearchOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void RandomSearchOptimizer::set_parameter_ranges(
    const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges.clear();
  for (const auto& entry : ranges) {
    impl_->parameter_ranges.insert(kj::str(entry.key), entry.value);
  }
}

void RandomSearchOptimizer::set_optimization_target(kj::StringPtr target) {
  impl_->optimization_target = kj::str(target);
}

void RandomSearchOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

void RandomSearchOptimizer::set_data_source(kj::Rc<IDataSource> data_source) {
  impl_->data_source = kj::mv(data_source);
}

void RandomSearchOptimizer::set_progress_callback(
    kj::Function<void(const OptimizationProgress&)> callback) {
  impl_->progress_callback = kj::mv(callback);
}

kj::Vector<RankedResult> RandomSearchOptimizer::get_ranked_results(int top_n) const {
  kj::Vector<RankedResult> ranked;

  // Create pairs of (fitness, index) for sorting
  std::vector<std::pair<double, size_t>> fitness_indices;
  for (size_t i = 0; i < impl_->results.size(); ++i) {
    double fitness;
    if (impl_->optimization_target == "sharpe"_kj) {
      fitness = impl_->results[i].sharpe_ratio;
    } else if (impl_->optimization_target == "return"_kj) {
      fitness = impl_->results[i].total_return;
    } else if (impl_->optimization_target == "win_rate"_kj) {
      fitness = impl_->results[i].win_rate;
    } else {
      fitness = impl_->results[i].sharpe_ratio;
    }
    fitness_indices.push_back({fitness, i});
  }

  // Sort by fitness descending
  std::sort(fitness_indices.begin(), fitness_indices.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  // Build ranked results
  size_t count = (top_n > 0 && static_cast<size_t>(top_n) < fitness_indices.size())
                     ? static_cast<size_t>(top_n)
                     : fitness_indices.size();

  for (size_t i = 0; i < count; ++i) {
    size_t idx = fitness_indices[i].second;
    RankedResult rr;
    rr.rank = static_cast<int>(i + 1);
    rr.fitness = fitness_indices[i].first;

    // Copy parameters
    if (idx < impl_->all_parameters.size()) {
      for (const auto& e : impl_->all_parameters[idx]) {
        rr.parameters.insert(kj::str(e.key), e.value);
      }
    }

    // Copy result
    const auto& result = impl_->results[idx];
    rr.result.strategy_name = kj::str(result.strategy_name);
    rr.result.symbol = kj::str(result.symbol);
    rr.result.start_time = result.start_time;
    rr.result.end_time = result.end_time;
    rr.result.initial_balance = result.initial_balance;
    rr.result.final_balance = result.final_balance;
    rr.result.total_return = result.total_return;
    rr.result.max_drawdown = result.max_drawdown;
    rr.result.sharpe_ratio = result.sharpe_ratio;
    rr.result.win_rate = result.win_rate;
    rr.result.profit_factor = result.profit_factor;
    rr.result.trade_count = result.trade_count;
    rr.result.win_count = result.win_count;
    rr.result.lose_count = result.lose_count;
    rr.result.avg_win = result.avg_win;
    rr.result.avg_lose = result.avg_lose;

    ranked.add(kj::mv(rr));
  }

  return ranked;
}

// ============================================================================
// BayesianOptimizer implementation
// ============================================================================

// Simple Gaussian Process implementation for Bayesian optimization
struct GaussianProcess {
  std::vector<std::vector<double>> X; // Observed points
  std::vector<double> y;              // Observed values
  double length_scale;
  double noise;

  GaussianProcess() : length_scale(1.0), noise(1e-6) {}

  // RBF (Squared Exponential) kernel
  double kernel(const std::vector<double>& x1, const std::vector<double>& x2) const {
    double sq_dist = 0.0;
    for (size_t i = 0; i < x1.size(); ++i) {
      double diff = x1[i] - x2[i];
      sq_dist += diff * diff;
    }
    return std::exp(-sq_dist / (2.0 * length_scale * length_scale));
  }

  // Predict mean and variance at a point
  std::pair<double, double> predict(const std::vector<double>& x) const {
    if (X.empty()) {
      return {0.0, 1.0};
    }

    size_t n = X.size();

    // Compute K(X, X) + noise * I
    std::vector<std::vector<double>> K(n, std::vector<double>(n));
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < n; ++j) {
        K[i][j] = kernel(X[i], X[j]);
        if (i == j) {
          K[i][j] += noise;
        }
      }
    }

    // Compute k(x, X)
    std::vector<double> k_star(n);
    for (size_t i = 0; i < n; ++i) {
      k_star[i] = kernel(x, X[i]);
    }

    // Simple Cholesky-free approach using matrix inversion approximation
    // For production, use proper linear algebra library

    // Approximate K^-1 * y using iterative method
    std::vector<double> alpha(n, 0.0);
    for (int iter = 0; iter < 100; ++iter) {
      for (size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < n; ++j) {
          if (i != j) {
            sum += K[i][j] * alpha[j];
          }
        }
        alpha[i] = (y[i] - sum) / K[i][i];
      }
    }

    // Mean prediction
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i) {
      mean += k_star[i] * alpha[i];
    }

    // Variance prediction (simplified)
    double k_star_star = kernel(x, x);
    double var = k_star_star;
    for (size_t i = 0; i < n; ++i) {
      var -= k_star[i] * k_star[i] / K[i][i]; // Simplified approximation
    }
    var = std::max(1e-6, var);

    return {mean, std::sqrt(var)};
  }

  void add_observation(const std::vector<double>& x, double value) {
    X.push_back(x);
    y.push_back(value);
  }
};

struct BayesianOptimizer::Impl {
  BacktestConfig config;
  kj::TreeMap<kj::String, std::pair<double, double>> parameter_ranges;
  kj::Vector<kj::String> param_names;
  kj::String optimization_target;
  int max_iterations;
  int initial_samples;
  kj::String acquisition_function;
  double kappa;
  double xi;
  kj::Vector<BacktestResult> results;
  kj::Vector<kj::TreeMap<kj::String, double>> all_parameters;
  kj::TreeMap<kj::String, double> best_parameters;
  kj::Own<veloz::core::Logger> logger;
  kj::Rc<IDataSource> data_source;
  kj::Maybe<kj::Function<void(const OptimizationProgress&)>> progress_callback;

  GaussianProcess gp;
  std::mt19937 rng;
  double best_observed;

  Impl()
      : optimization_target(kj::str("sharpe")), max_iterations(50), initial_samples(5),
        acquisition_function(kj::str("ei")), kappa(2.576), xi(0.01),
        logger(kj::heap<veloz::core::Logger>()),
        best_observed(-std::numeric_limits<double>::infinity()) {
    std::random_device rd;
    rng.seed(rd());
  }

  std::vector<double> params_to_vector(const kj::TreeMap<kj::String, double>& params) {
    std::vector<double> vec;
    for (const auto& name : param_names) {
      KJ_IF_SOME(val, params.find(name)) {
        vec.push_back(val);
      }
      else {
        vec.push_back(0.0);
      }
    }
    return vec;
  }

  kj::TreeMap<kj::String, double> vector_to_params(const std::vector<double>& vec) {
    kj::TreeMap<kj::String, double> params;
    for (size_t i = 0; i < param_names.size() && i < vec.size(); ++i) {
      params.insert(kj::str(param_names[i]), vec[i]);
    }
    return params;
  }

  std::vector<double> normalize_params(const kj::TreeMap<kj::String, double>& params) const {
    std::vector<double> normalized;
    for (const auto& name : param_names) {
      KJ_IF_SOME(val, params.find(name)) {
        KJ_IF_SOME(range, parameter_ranges.find(name)) {
          double min_val = range.first;
          double max_val = range.second;
          normalized.push_back((val - min_val) / (max_val - min_val));
        }
        else {
          normalized.push_back(val);
        }
      }
      else {
        normalized.push_back(0.5);
      }
    }
    return normalized;
  }

  kj::TreeMap<kj::String, double> generate_random_params() {
    kj::TreeMap<kj::String, double> params;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (const auto& entry : parameter_ranges) {
      double min_val = entry.value.first;
      double max_val = entry.value.second;
      params.insert(kj::str(entry.key), min_val + dist(rng) * (max_val - min_val));
    }
    return params;
  }

  // Expected Improvement acquisition function
  double expected_improvement(const std::vector<double>& x) {
    auto [mean, std] = gp.predict(x);
    if (std < 1e-6) {
      return 0.0;
    }
    double z = (mean - best_observed - xi) / std;
    // Approximate CDF and PDF of standard normal
    double cdf = 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
    double pdf = std::exp(-0.5 * z * z) / std::sqrt(2.0 * M_PI);
    return (mean - best_observed - xi) * cdf + std * pdf;
  }

  // Upper Confidence Bound acquisition function
  double upper_confidence_bound(const std::vector<double>& x) {
    auto [mean, std] = gp.predict(x);
    return mean + kappa * std;
  }

  // Probability of Improvement acquisition function
  double probability_of_improvement(const std::vector<double>& x) {
    auto [mean, std] = gp.predict(x);
    if (std < 1e-6) {
      return mean > best_observed ? 1.0 : 0.0;
    }
    double z = (mean - best_observed - xi) / std;
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
  }

  double acquisition(const std::vector<double>& x) {
    if (acquisition_function == "ucb"_kj) {
      return upper_confidence_bound(x);
    } else if (acquisition_function == "pi"_kj) {
      return probability_of_improvement(x);
    } else {
      return expected_improvement(x);
    }
  }

  // Find next point to sample by optimizing acquisition function
  kj::TreeMap<kj::String, double> suggest_next() {
    // Random search over acquisition function
    const int n_candidates = 1000;
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    double best_acq = -std::numeric_limits<double>::infinity();
    std::vector<double> best_x;

    for (int i = 0; i < n_candidates; ++i) {
      std::vector<double> x;
      for (size_t j = 0; j < param_names.size(); ++j) {
        x.push_back(dist(rng));
      }

      double acq = acquisition(x);
      if (acq > best_acq) {
        best_acq = acq;
        best_x = x;
      }
    }

    // Convert normalized values back to parameter space
    kj::TreeMap<kj::String, double> params;
    for (size_t i = 0; i < param_names.size() && i < best_x.size(); ++i) {
      KJ_IF_SOME(range, parameter_ranges.find(param_names[i])) {
        double min_val = range.first;
        double max_val = range.second;
        params.insert(kj::str(param_names[i]), min_val + best_x[i] * (max_val - min_val));
      }
    }

    return params;
  }
};

BayesianOptimizer::BayesianOptimizer() : impl_(kj::heap<Impl>()) {}

BayesianOptimizer::~BayesianOptimizer() noexcept {}

bool BayesianOptimizer::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing Bayesian optimizer");
  impl_->config.strategy_name = kj::str(config.strategy_name);
  impl_->config.symbol = kj::str(config.symbol);
  impl_->config.start_time = config.start_time;
  impl_->config.end_time = config.end_time;
  impl_->config.initial_balance = config.initial_balance;
  impl_->config.risk_per_trade = config.risk_per_trade;
  impl_->config.max_position_size = config.max_position_size;
  impl_->config.strategy_parameters.clear();
  for (const auto& entry : config.strategy_parameters) {
    impl_->config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
  }
  impl_->config.data_source = kj::str(config.data_source);
  impl_->config.data_type = kj::str(config.data_type);
  impl_->config.time_frame = kj::str(config.time_frame);
  impl_->results.clear();
  impl_->all_parameters.clear();
  impl_->best_parameters.clear();
  impl_->gp = GaussianProcess();
  impl_->best_observed = -std::numeric_limits<double>::infinity();
  return true;
}

bool BayesianOptimizer::optimize(kj::Rc<veloz::strategy::IStrategy> strategy) {
  if (impl_->parameter_ranges.size() == 0) {
    impl_->logger->error("No parameter ranges defined");
    return false;
  }

  // Build param_names list
  impl_->param_names.clear();
  for (const auto& entry : impl_->parameter_ranges) {
    impl_->param_names.add(kj::str(entry.key));
  }

  impl_->logger->info(kj::str("Starting Bayesian optimization with ", impl_->max_iterations,
                              " iterations (", impl_->initial_samples, " initial samples)"));
  impl_->results.clear();
  impl_->all_parameters.clear();
  impl_->best_parameters.clear();
  impl_->gp = GaussianProcess();
  impl_->best_observed = -std::numeric_limits<double>::infinity();

  BacktestEngine engine;

  for (int iter = 0; iter < impl_->max_iterations; ++iter) {
    kj::TreeMap<kj::String, double> params;

    // Initial random sampling phase
    if (iter < impl_->initial_samples) {
      params = impl_->generate_random_params();
    } else {
      // Use acquisition function to suggest next point
      params = impl_->suggest_next();
    }

    // Run backtest
    BacktestConfig test_config;
    test_config.strategy_name = kj::str(impl_->config.strategy_name);
    test_config.symbol = kj::str(impl_->config.symbol);
    test_config.start_time = impl_->config.start_time;
    test_config.end_time = impl_->config.end_time;
    test_config.initial_balance = impl_->config.initial_balance;
    test_config.risk_per_trade = impl_->config.risk_per_trade;
    test_config.max_position_size = impl_->config.max_position_size;
    test_config.data_source = kj::str(impl_->config.data_source);
    test_config.data_type = kj::str(impl_->config.data_type);
    test_config.time_frame = kj::str(impl_->config.time_frame);
    for (const auto& entry : params) {
      test_config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
    }

    double fitness = -std::numeric_limits<double>::infinity();

    if (engine.initialize(test_config)) {
      engine.set_strategy(strategy.addRef());
      if (impl_->data_source.get() != nullptr) {
        engine.set_data_source(impl_->data_source.addRef());
      }

      if (engine.run()) {
        BacktestResult result = engine.get_result();
        result.strategy_name = kj::str(impl_->config.strategy_name);

        if (impl_->optimization_target == "sharpe"_kj) {
          fitness = result.sharpe_ratio;
        } else if (impl_->optimization_target == "return"_kj) {
          fitness = result.total_return;
        } else if (impl_->optimization_target == "win_rate"_kj) {
          fitness = result.win_rate;
        } else {
          fitness = result.sharpe_ratio;
        }

        impl_->results.add(kj::mv(result));

        // Store parameters
        kj::TreeMap<kj::String, double> params_copy;
        for (const auto& e : params) {
          params_copy.insert(kj::str(e.key), e.value);
        }
        impl_->all_parameters.add(kj::mv(params_copy));

        // Update GP with normalized observation
        auto normalized = impl_->normalize_params(params);
        impl_->gp.add_observation(normalized, fitness);

        // Update best
        if (fitness > impl_->best_observed) {
          impl_->best_observed = fitness;
          impl_->best_parameters.clear();
          for (const auto& e : params) {
            impl_->best_parameters.insert(kj::str(e.key), e.value);
          }
        }
      }
    }

    engine.reset();

    // Progress callback
    KJ_IF_SOME(callback, impl_->progress_callback) {
      OptimizationProgress progress;
      progress.current_iteration = iter + 1;
      progress.total_iterations = impl_->max_iterations;
      progress.best_fitness = impl_->best_observed;
      progress.current_fitness = fitness;
      progress.progress_fraction =
          static_cast<double>(iter + 1) / static_cast<double>(impl_->max_iterations);
      progress.status =
          kj::str(iter < impl_->initial_samples ? "Initial sampling" : "Bayesian optimization",
                  " - Iteration ", iter + 1, "/", impl_->max_iterations);
      for (const auto& e : params) {
        progress.current_parameters.insert(kj::str(e.key), e.value);
      }
      for (const auto& e : impl_->best_parameters) {
        progress.best_parameters.insert(kj::str(e.key), e.value);
      }
      callback(progress);
    }

    auto best_str = std::format("{:.4f}", impl_->best_observed);
    auto curr_str = std::format("{:.4f}", fitness);
    impl_->logger->info(kj::str("Iteration ", iter + 1, "/", impl_->max_iterations,
                                ": current=", curr_str.c_str(), ", best=", best_str.c_str()));
  }

  auto best_str = std::format("{:.4f}", impl_->best_observed);
  impl_->logger->info(kj::str("Bayesian optimization completed. Best ", impl_->optimization_target,
                              ": ", best_str.c_str()));

  return true;
}

kj::Vector<BacktestResult> BayesianOptimizer::get_results() const {
  kj::Vector<BacktestResult> results;
  for (const auto& result : impl_->results) {
    BacktestResult copy;
    copy.strategy_name = kj::str(result.strategy_name);
    copy.symbol = kj::str(result.symbol);
    copy.start_time = result.start_time;
    copy.end_time = result.end_time;
    copy.initial_balance = result.initial_balance;
    copy.final_balance = result.final_balance;
    copy.total_return = result.total_return;
    copy.max_drawdown = result.max_drawdown;
    copy.sharpe_ratio = result.sharpe_ratio;
    copy.win_rate = result.win_rate;
    copy.profit_factor = result.profit_factor;
    copy.trade_count = result.trade_count;
    copy.win_count = result.win_count;
    copy.lose_count = result.lose_count;
    copy.avg_win = result.avg_win;
    copy.avg_lose = result.avg_lose;
    results.add(kj::mv(copy));
  }
  return results;
}

const kj::TreeMap<kj::String, double>& BayesianOptimizer::get_best_parameters() const {
  return impl_->best_parameters;
}

void BayesianOptimizer::set_parameter_ranges(
    const kj::TreeMap<kj::String, std::pair<double, double>>& ranges) {
  impl_->parameter_ranges.clear();
  for (const auto& entry : ranges) {
    impl_->parameter_ranges.insert(kj::str(entry.key), entry.value);
  }
}

void BayesianOptimizer::set_optimization_target(kj::StringPtr target) {
  impl_->optimization_target = kj::str(target);
}

void BayesianOptimizer::set_max_iterations(int iterations) {
  impl_->max_iterations = iterations;
}

void BayesianOptimizer::set_data_source(kj::Rc<IDataSource> data_source) {
  impl_->data_source = kj::mv(data_source);
}

void BayesianOptimizer::set_initial_samples(int n_initial) {
  impl_->initial_samples = std::max(1, n_initial);
}

void BayesianOptimizer::set_acquisition_function(kj::StringPtr type) {
  impl_->acquisition_function = kj::str(type);
}

void BayesianOptimizer::set_exploration_params(double kappa, double xi) {
  impl_->kappa = kappa;
  impl_->xi = xi;
}

void BayesianOptimizer::set_progress_callback(
    kj::Function<void(const OptimizationProgress&)> callback) {
  impl_->progress_callback = kj::mv(callback);
}

kj::Vector<RankedResult> BayesianOptimizer::get_ranked_results(int top_n) const {
  kj::Vector<RankedResult> ranked;

  std::vector<std::pair<double, size_t>> fitness_indices;
  for (size_t i = 0; i < impl_->results.size(); ++i) {
    double fitness;
    if (impl_->optimization_target == "sharpe"_kj) {
      fitness = impl_->results[i].sharpe_ratio;
    } else if (impl_->optimization_target == "return"_kj) {
      fitness = impl_->results[i].total_return;
    } else if (impl_->optimization_target == "win_rate"_kj) {
      fitness = impl_->results[i].win_rate;
    } else {
      fitness = impl_->results[i].sharpe_ratio;
    }
    fitness_indices.push_back({fitness, i});
  }

  std::sort(fitness_indices.begin(), fitness_indices.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  size_t count = (top_n > 0 && static_cast<size_t>(top_n) < fitness_indices.size())
                     ? static_cast<size_t>(top_n)
                     : fitness_indices.size();

  for (size_t i = 0; i < count; ++i) {
    size_t idx = fitness_indices[i].second;
    RankedResult rr;
    rr.rank = static_cast<int>(i + 1);
    rr.fitness = fitness_indices[i].first;

    if (idx < impl_->all_parameters.size()) {
      for (const auto& e : impl_->all_parameters[idx]) {
        rr.parameters.insert(kj::str(e.key), e.value);
      }
    }

    const auto& result = impl_->results[idx];
    rr.result.strategy_name = kj::str(result.strategy_name);
    rr.result.symbol = kj::str(result.symbol);
    rr.result.start_time = result.start_time;
    rr.result.end_time = result.end_time;
    rr.result.initial_balance = result.initial_balance;
    rr.result.final_balance = result.final_balance;
    rr.result.total_return = result.total_return;
    rr.result.max_drawdown = result.max_drawdown;
    rr.result.sharpe_ratio = result.sharpe_ratio;
    rr.result.win_rate = result.win_rate;
    rr.result.profit_factor = result.profit_factor;
    rr.result.trade_count = result.trade_count;
    rr.result.win_count = result.win_count;
    rr.result.lose_count = result.lose_count;
    rr.result.avg_win = result.avg_win;
    rr.result.avg_lose = result.avg_lose;

    ranked.add(kj::mv(rr));
  }

  return ranked;
}

std::pair<double, double>
BayesianOptimizer::predict(const kj::TreeMap<kj::String, double>& parameters) const {
  auto normalized = impl_->normalize_params(parameters);
  return impl_->gp.predict(normalized);
}

// ============================================================================
// OptimizerFactory implementation
// ============================================================================

kj::Own<IParameterOptimizer> OptimizerFactory::create(OptimizationAlgorithm algorithm) {
  switch (algorithm) {
  case OptimizationAlgorithm::GridSearch:
    return kj::heap<GridSearchOptimizer>();
  case OptimizationAlgorithm::GeneticAlgorithm:
    return kj::heap<GeneticAlgorithmOptimizer>();
  case OptimizationAlgorithm::RandomSearch:
    return kj::heap<RandomSearchOptimizer>();
  case OptimizationAlgorithm::BayesianOptimization:
    return kj::heap<BayesianOptimizer>();
  default:
    return kj::heap<GridSearchOptimizer>();
  }
}

} // namespace veloz::backtest
