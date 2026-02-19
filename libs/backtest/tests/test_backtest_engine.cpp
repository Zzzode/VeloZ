#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/test.h>

using namespace veloz::backtest;
using namespace veloz::strategy;

namespace {

// Test strategy implementation
class TestStrategy : public veloz::strategy::IStrategy {
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

// Helper to create test configuration
BacktestConfig create_test_config() {
  BacktestConfig config;
  config.strategy_name = kj::str("TestStrategy");
  config.symbol = kj::str("BTCUSDT");
  config.start_time = 1609459200000; // 2021-01-01
  config.end_time = 1640995200000;   // 2021-12-31
  config.initial_balance = 10000.0;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.data_source = kj::str("csv");
  config.data_type = kj::str("kline");
  config.time_frame = kj::str("1h");
  return config;
}

// ============================================================================
// BacktestEngine Tests
// ============================================================================

KJ_TEST("BacktestEngine: Initialize") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));
}

KJ_TEST("BacktestEngine: SetStrategy") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  engine->set_strategy(strategy.addRef());
  // Should not throw
}

KJ_TEST("BacktestEngine: SetDataSource") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  auto data_source = DataSourceFactory::create_data_source("csv");
  engine->set_data_source(data_source.addRef());
  // Should not throw
}

KJ_TEST("BacktestEngine: RunWithoutStrategy") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  auto data_source = DataSourceFactory::create_data_source("csv");
  engine->set_data_source(data_source.addRef());
  KJ_EXPECT(!engine->run());
}

KJ_TEST("BacktestEngine: RunWithoutDataSource") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  engine->set_strategy(strategy.addRef());
  KJ_EXPECT(!engine->run());
}

KJ_TEST("BacktestEngine: Reset") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));
  KJ_EXPECT(engine->reset());
}

KJ_TEST("BacktestEngine: StopNotRunning") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));
  KJ_EXPECT(!engine->stop());
}

} // namespace
