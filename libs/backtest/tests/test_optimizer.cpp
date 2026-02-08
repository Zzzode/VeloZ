#include <gtest/gtest.h>
#include "veloz/backtest/optimizer.h"
#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

class TestStrategy : public veloz::strategy::IStrategy {
public:
    TestStrategy() : id_("test_strategy"), name_("TestStrategy"), type_(veloz::strategy::StrategyType::Custom) {}

    std::string get_id() const override { return id_; }
    std::string get_name() const override { return name_; }
    veloz::strategy::StrategyType get_type() const override { return type_; }

    bool initialize(const veloz::strategy::StrategyConfig& config, veloz::core::Logger& logger) override {
        return true;
    }

    void on_start() override {}
    void on_stop() override {}
    void on_event(const veloz::market::MarketEvent& event) override {}
    void on_position_update(const veloz::oms::Position& position) override {}
    void on_timer(int64_t timestamp) override {}

    veloz::strategy::StrategyState get_state() const override {
        veloz::strategy::StrategyState state;
        state.strategy_id = id_;
        state.strategy_name = name_;
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

    std::vector<veloz::exec::PlaceOrderRequest> get_signals() override {
        return {};
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

        config_.strategy_name = "TestStrategy";
        config_.symbol = "BTCUSDT";
        config_.start_time = 1609459200000; // 2021-01-01
        config_.end_time = 1640995200000; // 2021-12-31
        config_.initial_balance = 10000.0;
        config_.risk_per_trade = 0.02;
        config_.max_position_size = 0.1;
        config_.data_source = "csv";
        config_.data_type = "kline";
        config_.time_frame = "1h";

        parameter_ranges_ = {
            {"lookback_period", {10, 30}},
            {"stop_loss", {0.01, 0.05}},
            {"take_profit", {0.02, 0.10}},
            {"position_size", {0.05, 0.20}}
        };
    }

    void TearDown() override {
        strategy_.reset();
        parameter_ranges_.clear();
    }

    std::shared_ptr<TestStrategy> strategy_;
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
    EXPECT_TRUE(optimizer.optimize(strategy_));
}

TEST_F(ParameterOptimizerTest, GridSearchGetResults) {
    veloz::backtest::GridSearchOptimizer optimizer;
    optimizer.initialize(config_);
    optimizer.set_parameter_ranges(parameter_ranges_);
    optimizer.optimize(strategy_);
    auto results = optimizer.get_results();
    EXPECT_FALSE(results.empty());
}

TEST_F(ParameterOptimizerTest, GridSearchGetBestParameters) {
    veloz::backtest::GridSearchOptimizer optimizer;
    optimizer.initialize(config_);
    optimizer.set_parameter_ranges(parameter_ranges_);
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
    EXPECT_TRUE(optimizer.optimize(strategy_));
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmGetResults) {
    veloz::backtest::GeneticAlgorithmOptimizer optimizer;
    optimizer.initialize(config_);
    optimizer.set_parameter_ranges(parameter_ranges_);
    optimizer.optimize(strategy_);
    auto results = optimizer.get_results();
    EXPECT_FALSE(results.empty());
}

TEST_F(ParameterOptimizerTest, GeneticAlgorithmGetBestParameters) {
    veloz::backtest::GeneticAlgorithmOptimizer optimizer;
    optimizer.initialize(config_);
    optimizer.set_parameter_ranges(parameter_ranges_);
    optimizer.optimize(strategy_);
    auto best_params = optimizer.get_best_parameters();
    EXPECT_FALSE(best_params.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
