#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

#include <gtest/gtest.h>
#include <kj/memory.h>
#include <kj/refcount.h>

class TestStrategy : public veloz::strategy::IStrategy {
public:
  TestStrategy()
      : id_(kj::str("test_strategy")), name_(kj::str("TestStrategy")), type_(veloz::strategy::StrategyType::Custom) {}
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
  kj::String id_;
  kj::String name_;
  veloz::strategy::StrategyType type_;
};

class BacktestEngineTest : public ::testing::Test {
public:
  ~BacktestEngineTest() noexcept override = default;

protected:
  void SetUp() override {
    engine_ = kj::heap<veloz::backtest::BacktestEngine>();
    strategy_ = kj::rc<TestStrategy>();
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
    engine_ = nullptr;
    strategy_ = nullptr;
    data_source_ = nullptr;
  }

  kj::Own<veloz::backtest::BacktestEngine> engine_;
  kj::Rc<TestStrategy> strategy_;
  kj::Rc<veloz::backtest::IDataSource> data_source_;
  veloz::backtest::BacktestConfig config_;
};

TEST_F(BacktestEngineTest, Initialize) {
  EXPECT_TRUE(engine_->initialize(config_));
}

TEST_F(BacktestEngineTest, SetStrategy) {
  engine_->initialize(config_);
  engine_->set_strategy(strategy_.addRef());
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(BacktestEngineTest, SetDataSource) {
  engine_->initialize(config_);
  engine_->set_data_source(data_source_.addRef());
  EXPECT_TRUE(true); // Should not throw
}

TEST_F(BacktestEngineTest, RunWithoutStrategy) {
  engine_->initialize(config_);
  engine_->set_data_source(data_source_.addRef());
  EXPECT_FALSE(engine_->run());
}

TEST_F(BacktestEngineTest, RunWithoutDataSource) {
  engine_->initialize(config_);
  engine_->set_strategy(strategy_.addRef());
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
