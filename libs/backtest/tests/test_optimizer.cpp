#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/strategy/strategy.h"

#include <kj/map.h>
#include <kj/refcount.h>
#include <kj/test.h>

using namespace veloz::backtest;
using namespace veloz::strategy;

namespace {

// Mock data source that generates synthetic market data for testing
class MockDataSource : public IDataSource {
public:
  ~MockDataSource() noexcept(false) override = default;

  bool connect() override {
    return true;
  }
  bool disconnect() override {
    return true;
  }

  kj::Vector<veloz::market::MarketEvent> get_data(kj::StringPtr symbol, std::int64_t start_time,
                                                  std::int64_t end_time, kj::StringPtr data_type,
                                                  kj::StringPtr time_frame) override {
    kj::Vector<veloz::market::MarketEvent> events;

    // Generate synthetic kline data
    int64_t interval_ms = 3600000; // 1 hour
    double price = 50000.0;

    for (int64_t ts = start_time; ts < end_time; ts += interval_ms) {
      veloz::market::MarketEvent event;
      event.type = veloz::market::MarketEventType::Kline;
      event.symbol = veloz::common::SymbolId(symbol);
      event.ts_exchange_ns = ts * 1'000'000; // Convert to nanoseconds

      veloz::market::KlineData kline;
      kline.start_time = ts;
      kline.close_time = ts + interval_ms;
      kline.open = price;
      kline.high = price * 1.01;
      kline.low = price * 0.99;
      kline.close = price * (1.0 + (((ts / interval_ms) % 2 == 0) ? 0.005 : -0.003));
      kline.volume = 100.0;

      event.data = kline;
      events.add(kj::mv(event));

      // Update price for next iteration
      price = kline.close;
    }

    return events;
  }

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override {
    return true;
  }
};

// Test strategy implementation
class TestStrategy : public IStrategy {
public:
  TestStrategy()
      : id_(kj::str("test_strategy")), name_(kj::str("TestStrategy")),
        type_(veloz::strategy::StrategyType::Custom) {}
  ~TestStrategy() noexcept(false) override = default;

  kj::StringPtr get_id() const override {
    return id_;
  }
  kj::StringPtr get_name() const override {
    return name_;
  }
  veloz::strategy::StrategyType get_type() const override {
    return type_;
  }

  bool initialize(const veloz::strategy::StrategyConfig& config,
                  veloz::core::Logger& logger) override {
    return true;
  }

  void on_start() override {}
  void on_stop() override {}
  void on_pause() override {}
  void on_resume() override {}
  void on_event(const veloz::market::MarketEvent& event) override {}
  void on_position_update(const veloz::oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {}

  bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) override {
    return false;
  }

  bool supports_hot_reload() const override {
    return false;
  }

  kj::Maybe<const StrategyMetrics&> get_metrics() const override {
    return kj::none;
  }

  void on_order_rejected(const veloz::exec::PlaceOrderRequest& req, kj::StringPtr reason) override {
  }

  veloz::strategy::StrategyState get_state() const override {
    veloz::strategy::StrategyState state;
    state.strategy_id = kj::str(id_);
    state.strategy_name = kj::str(name_);
    state.is_running = true;
    state.pnl = 0.0;
    state.max_drawdown = 0.0;
    state.trade_count = 0;
    state.win_count = 0;
    state.lose_count = 0;
    state.win_rate = 0.0;
    state.profit_factor = 0.0;
    return state;
  }

  kj::Vector<veloz::exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<veloz::exec::PlaceOrderRequest>();
  }

  void reset() override {}

private:
  kj::String id_;
  kj::String name_;
  veloz::strategy::StrategyType type_;
};

// Helper to create test configuration
BacktestConfig create_test_config() {
  BacktestConfig config;
  config.strategy_name = kj::str("TestStrategy");
  config.symbol = kj::str("BTCUSDT");
  config.start_time = 1609459200000; // 2021-01-01
  config.end_time = 1609545600000;   // 2021-01-02 (shorter period for faster tests)
  config.initial_balance = 10000.0;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.data_source = kj::str("mock");
  config.data_type = kj::str("kline");
  config.time_frame = kj::str("1h");
  return config;
}

// Helper to create default parameter ranges
kj::TreeMap<kj::String, std::pair<double, double>> create_default_parameter_ranges() {
  kj::TreeMap<kj::String, std::pair<double, double>> parameter_ranges;
  parameter_ranges.insert(kj::str("lookback_period"), {10, 30});
  parameter_ranges.insert(kj::str("stop_loss"), {0.01, 0.05});
  parameter_ranges.insert(kj::str("take_profit"), {0.02, 0.10});
  parameter_ranges.insert(kj::str("position_size"), {0.05, 0.20});
  return parameter_ranges;
}

// ============================================================================
// GridSearchOptimizer Tests
// ============================================================================

KJ_TEST("GridSearchOptimizer: Initialize") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));
}

KJ_TEST("GridSearchOptimizer: SetParameterRanges") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto parameter_ranges = create_default_parameter_ranges();
  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  // Should not throw
}

KJ_TEST("GridSearchOptimizer: SetOptimizationTarget") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));
  optimizer.set_optimization_target("sharpe");
  // Should not throw
}

KJ_TEST("GridSearchOptimizer: SetMaxIterations") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));
  optimizer.set_max_iterations(50);
  // Should not throw
}

KJ_TEST("GridSearchOptimizer: Optimize") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));
}

KJ_TEST("GridSearchOptimizer: GetResults") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  auto results = optimizer.get_results();
  KJ_EXPECT(results.size() > 0);
}

KJ_TEST("GridSearchOptimizer: GetBestParameters") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  const auto& best_params = optimizer.get_best_parameters();
  KJ_EXPECT(best_params.size() > 0);
}

KJ_TEST("GridSearchOptimizer: WithSingleParameter") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  // Set up single parameter range
  kj::TreeMap<kj::String, std::pair<double, double>> single_range;
  single_range.insert(kj::str("lookback_period"), {5.0, 15.0});
  optimizer.set_parameter_ranges(kj::mv(single_range));
  optimizer.set_data_source(data_source.addRef());

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  auto results = optimizer.get_results();
  KJ_EXPECT(results.size() > 0);

  // Should have multiple combinations (10 steps by default)
  KJ_EXPECT(results.size() > 1);
}

KJ_TEST("GridSearchOptimizer: WithMultipleParameters") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  // Set up multiple parameter ranges
  kj::TreeMap<kj::String, std::pair<double, double>> multi_range;
  multi_range.insert(kj::str("lookback_period"), {10.0, 20.0});
  multi_range.insert(kj::str("stop_loss"), {0.01, 0.03});
  optimizer.set_parameter_ranges(kj::mv(multi_range));
  optimizer.set_data_source(data_source.addRef());

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  auto results = optimizer.get_results();
  KJ_EXPECT(results.size() > 0);

  // Should have 10*10 = 100 combinations
  KJ_EXPECT(results.size() <= 100);
}

KJ_TEST("GridSearchOptimizer: MaxIterationsLimit") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  // Set up parameter range that would produce many combinations
  kj::TreeMap<kj::String, std::pair<double, double>> wide_range;
  wide_range.insert(kj::str("param1"), {1.0, 100.0});
  wide_range.insert(kj::str("param2"), {1.0, 100.0});
  optimizer.set_parameter_ranges(kj::mv(wide_range));
  optimizer.set_data_source(data_source.addRef());

  // Limit iterations
  optimizer.set_max_iterations(50);

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  auto results = optimizer.get_results();
  // Should be limited to 50 iterations
  KJ_EXPECT(results.size() <= 50);
}

KJ_TEST("GridSearchOptimizer: OptimizationTargetSharpe") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  kj::TreeMap<kj::String, std::pair<double, double>> ranges;
  ranges.insert(kj::str("lookback_period"), {10.0, 30.0});
  optimizer.set_parameter_ranges(kj::mv(ranges));
  optimizer.set_optimization_target("sharpe");
  optimizer.set_data_source(data_source.addRef());

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  const auto& best_params = optimizer.get_best_parameters();
  KJ_EXPECT(best_params.size() > 0);
}

KJ_TEST("GridSearchOptimizer: OptimizationTargetReturn") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  kj::TreeMap<kj::String, std::pair<double, double>> ranges;
  ranges.insert(kj::str("lookback_period"), {10.0, 30.0});
  optimizer.set_parameter_ranges(kj::mv(ranges));
  optimizer.set_optimization_target("return");
  optimizer.set_data_source(data_source.addRef());

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  const auto& best_params = optimizer.get_best_parameters();
  KJ_EXPECT(best_params.size() > 0);
}

KJ_TEST("GridSearchOptimizer: OptimizationTargetWinRate") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  kj::TreeMap<kj::String, std::pair<double, double>> ranges;
  ranges.insert(kj::str("lookback_period"), {10.0, 30.0});
  optimizer.set_parameter_ranges(kj::mv(ranges));
  optimizer.set_optimization_target("win_rate");
  optimizer.set_data_source(data_source.addRef());

  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  const auto& best_params = optimizer.get_best_parameters();
  KJ_EXPECT(best_params.size() > 0);
}

KJ_TEST("GridSearchOptimizer: ClearsPreviousResults") {
  GridSearchOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();

  kj::TreeMap<kj::String, std::pair<double, double>> ranges;
  ranges.insert(kj::str("lookback_period"), {10.0, 20.0});
  optimizer.set_parameter_ranges(kj::mv(ranges));
  optimizer.set_data_source(data_source.addRef());

  // Run first optimization
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));
  auto first_results = optimizer.get_results();
  size_t first_count = first_results.size();

  // Run second optimization
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));
  auto second_results = optimizer.get_results();
  size_t second_count = second_results.size();

  // Results should be cleared between runs
  KJ_EXPECT(first_count == second_count);
}

// ============================================================================
// GeneticAlgorithmOptimizer Tests
// ============================================================================

KJ_TEST("GeneticAlgorithmOptimizer: Initialize") {
  GeneticAlgorithmOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));
}

KJ_TEST("GeneticAlgorithmOptimizer: Optimize") {
  GeneticAlgorithmOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));
}

KJ_TEST("GeneticAlgorithmOptimizer: GetResults") {
  GeneticAlgorithmOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  auto results = optimizer.get_results();
  KJ_EXPECT(results.size() > 0);
}

KJ_TEST("GeneticAlgorithmOptimizer: GetBestParameters") {
  GeneticAlgorithmOptimizer optimizer;
  auto config = create_test_config();
  KJ_EXPECT(optimizer.initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  auto data_source = kj::rc<MockDataSource>();
  auto parameter_ranges = create_default_parameter_ranges();

  optimizer.set_parameter_ranges(kj::mv(parameter_ranges));
  optimizer.set_data_source(data_source.addRef());
  KJ_EXPECT(optimizer.optimize(strategy.addRef()));

  const auto& best_params = optimizer.get_best_parameters();
  KJ_EXPECT(best_params.size() > 0);
}

} // namespace
