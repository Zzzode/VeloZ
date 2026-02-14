#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

#include <gtest/gtest.h>

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

class BacktestEngineTest : public ::testing::Test {
protected:
  void SetUp() override {
    engine_ = std::make_unique<veloz::backtest::BacktestEngine>();
    strategy_ = std::make_shared<TestStrategy>();
    data_source_ = veloz::backtest::DataSourceFactory::create_data_source("csv");

    config_.strategy_name = kj::str("TestStrategy");
    config_.symbol = kj::str("BTCUSDT");
    config_.start_time = 1609459200000; // 2021-01-01
    config_.end_time = 1640995200000;   // 2021-12-31
    config_.initial_balance = 10000.0;
    config_.risk_per_trade = 0.02;
    config_.max_position_size = 0.1;
    config_.data_source = kj::str("csv");
    config_.data_type = kj::str("kline");
    config_.time_frame = kj::str("1h");
  }

  void TearDown() override {
    engine_.reset();
    strategy_.reset();
    data_source_.reset();
  }

  std::unique_ptr<veloz::backtest::BacktestEngine> engine_;
  std::shared_ptr<TestStrategy> strategy_;
  std::shared_ptr<veloz::backtest::IDataSource> data_source_;
  veloz::backtest::BacktestConfig config_;
};

TEST_F(BacktestEngineTest, Initialize) {
  EXPECT_TRUE(engine_->initialize(config_));
}

TEST_F(BacktestEngineTest, SetStrategy) {
  engine_->initialize(config_);
  engine_->set_strategy(strategy_);
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(BacktestEngineTest, SetDataSource) {
  engine_->initialize(config_);
  engine_->set_data_source(data_source_);
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(BacktestEngineTest, RunWithoutStrategy) {
  engine_->initialize(config_);
  engine_->set_data_source(data_source_);
  EXPECT_FALSE(engine_->run());
}

TEST_F(BacktestEngineTest, RunWithoutDataSource) {
  engine_->initialize(config_);
  engine_->set_strategy(strategy_);
  EXPECT_FALSE(engine_->run());
}

TEST_F(BacktestEngineTest, Reset) {
  engine_->initialize(config_);
  EXPECT_TRUE(engine_->reset());
}

TEST_F(BacktestEngineTest, StopNotRunning) {
  engine_->initialize(config_);
  EXPECT_FALSE(engine_->stop());
}
