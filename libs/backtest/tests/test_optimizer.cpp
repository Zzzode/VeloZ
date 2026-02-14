#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/strategy/strategy.h"

#include <gtest/gtest.h>

// Mock data source that generates synthetic market data for testing
class MockDataSource : public veloz::backtest::IDataSource {
public:
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
      event.symbol = veloz::common::SymbolId(std::string(symbol.cStr(), symbol.size()));
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

class TestStrategy : public veloz::strategy::IStrategy {
public:
  TestStrategy()
      : id_("test_strategy"), name_("TestStrategy"), type_(veloz::strategy::StrategyType::Custom) {}

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
  void on_event(const veloz::market::MarketEvent& event) override {}
  void on_position_update(const veloz::oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {}

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
  std::string id_;
  std::string name_;
  veloz::strategy::StrategyType type_;
};

class ParameterOptimizerTest : public ::testing::Test {
protected:
  void SetUp() override {
    strategy_ = std::make_shared<TestStrategy>();
    data_source_ = std::make_shared<MockDataSource>();

    config_.strategy_name = kj::str("TestStrategy");
    config_.symbol = kj::str("BTCUSDT");
    config_.start_time = 1609459200000; // 2021-01-01
    config_.end_time = 1609545600000;   // 2021-01-02 (shorter period for faster tests)
    config_.initial_balance = 10000.0;
    config_.risk_per_trade = 0.02;
    config_.max_position_size = 0.1;
    config_.data_source = kj::str("mock");
    config_.data_type = kj::str("kline");
    config_.time_frame = kj::str("1h");

    parameter_ranges_ = {{"lookback_period", {10, 30}},
                         {"stop_loss", {0.01, 0.05}},
                         {"take_profit", {0.02, 0.10}},
                         {"position_size", {0.05, 0.20}}};
  }

  void TearDown() override {
    strategy_.reset();
    data_source_.reset();
    parameter_ranges_.clear();
  }

  std::shared_ptr<TestStrategy> strategy_;
  std::shared_ptr<MockDataSource> data_source_;
  veloz::backtest::BacktestConfig config_;
  std::map<std::string, std::pair<double, double>> parameter_ranges_;
};

TEST_F(ParameterOptimizerTest, GridSearchInitialize) {
  veloz::backtest::GridSearchOptimizer optimizer;
  EXPECT_TRUE(optimizer.initialize(config_));
}

TEST_F(ParameterOptimizerTest, GridSearchSetParameterRanges) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(ParameterOptimizerTest, GridSearchSetOptimizationTarget) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_optimization_target("sharpe");
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(ParameterOptimizerTest, GridSearchSetMaxIterations) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_max_iterations(50);
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(ParameterOptimizerTest, GridSearchOptimize) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  EXPECT_TRUE(optimizer.optimize(strategy_));
}

TEST_F(ParameterOptimizerTest, GridSearchGetResults) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  optimizer.optimize(strategy_);
  auto results = optimizer.get_results();
  EXPECT_FALSE(results.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchGetBestParameters) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  optimizer.optimize(strategy_);
  auto best_params = optimizer.get_best_parameters();
  EXPECT_FALSE(best_params.empty());
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmInitialize) {
  veloz::backtest::GeneticAlgorithmOptimizer optimizer;
  EXPECT_TRUE(optimizer.initialize(config_));
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmOptimize) {
  veloz::backtest::GeneticAlgorithmOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  EXPECT_TRUE(optimizer.optimize(strategy_));
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmGetResults) {
  veloz::backtest::GeneticAlgorithmOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  optimizer.optimize(strategy_);
  auto results = optimizer.get_results();
  EXPECT_FALSE(results.empty());
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmGetBestParameters) {
  veloz::backtest::GeneticAlgorithmOptimizer optimizer;
  optimizer.initialize(config_);
  optimizer.set_parameter_ranges(parameter_ranges_);
  optimizer.set_data_source(data_source_);
  optimizer.optimize(strategy_);
  auto best_params = optimizer.get_best_parameters();
  EXPECT_FALSE(best_params.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchWithSingleParameter) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  // Set up single parameter range
  std::map<std::string, std::pair<double, double>> single_range;
  single_range["lookback_period"] = {5.0, 15.0};
  optimizer.set_parameter_ranges(single_range);
  optimizer.set_data_source(data_source_);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto results = optimizer.get_results();
  EXPECT_FALSE(results.empty());

  // Should have multiple combinations (10 steps by default)
  EXPECT_GT(results.size(), 1);
}

TEST_F(ParameterOptimizerTest, GridSearchWithMultipleParameters) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  // Set up multiple parameter ranges
  std::map<std::string, std::pair<double, double>> multi_range;
  multi_range["lookback_period"] = {10.0, 20.0};
  multi_range["stop_loss"] = {0.01, 0.03};
  optimizer.set_parameter_ranges(multi_range);
  optimizer.set_data_source(data_source_);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto results = optimizer.get_results();
  EXPECT_FALSE(results.empty());

  // Should have 10*10 = 100 combinations
  EXPECT_LE(results.size(), 100);
}

TEST_F(ParameterOptimizerTest, GridSearchMaxIterationsLimit) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  // Set up parameter range that would produce many combinations
  std::map<std::string, std::pair<double, double>> wide_range;
  wide_range["param1"] = {1.0, 100.0};
  wide_range["param2"] = {1.0, 100.0};
  optimizer.set_parameter_ranges(wide_range);
  optimizer.set_data_source(data_source_);

  // Limit iterations
  optimizer.set_max_iterations(50);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto results = optimizer.get_results();
  // Should be limited to 50 iterations
  EXPECT_LE(results.size(), 50);
}

TEST_F(ParameterOptimizerTest, GridSearchOptimizationTargetSharpe) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  std::map<std::string, std::pair<double, double>> ranges;
  ranges["lookback_period"] = {10.0, 30.0};
  optimizer.set_parameter_ranges(ranges);
  optimizer.set_optimization_target("sharpe");
  optimizer.set_data_source(data_source_);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto best_params = optimizer.get_best_parameters();
  EXPECT_FALSE(best_params.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchOptimizationTargetReturn) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  std::map<std::string, std::pair<double, double>> ranges;
  ranges["lookback_period"] = {10.0, 30.0};
  optimizer.set_parameter_ranges(ranges);
  optimizer.set_optimization_target("return");
  optimizer.set_data_source(data_source_);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto best_params = optimizer.get_best_parameters();
  EXPECT_FALSE(best_params.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchOptimizationTargetWinRate) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  std::map<std::string, std::pair<double, double>> ranges;
  ranges["lookback_period"] = {10.0, 30.0};
  optimizer.set_parameter_ranges(ranges);
  optimizer.set_optimization_target("win_rate");
  optimizer.set_data_source(data_source_);

  EXPECT_TRUE(optimizer.optimize(strategy_));

  auto best_params = optimizer.get_best_parameters();
  EXPECT_FALSE(best_params.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchClearsPreviousResults) {
  veloz::backtest::GridSearchOptimizer optimizer;
  optimizer.initialize(config_);

  std::map<std::string, std::pair<double, double>> ranges;
  ranges["lookback_period"] = {10.0, 20.0};
  optimizer.set_parameter_ranges(ranges);
  optimizer.set_data_source(data_source_);

  // Run first optimization
  EXPECT_TRUE(optimizer.optimize(strategy_));
  auto first_results = optimizer.get_results();
  size_t first_count = first_results.size();

  // Run second optimization
  EXPECT_TRUE(optimizer.optimize(strategy_));
  auto second_results = optimizer.get_results();
  size_t second_count = second_results.size();

  // Results should be cleared between runs
  EXPECT_EQ(first_count, second_count);
}
