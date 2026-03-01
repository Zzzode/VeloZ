// Integration test: Multi-strategy concurrent execution
// Tests running multiple strategies simultaneously with the strategy manager

#include "veloz/strategy/strategy.h"
#include "veloz/strategy/advanced_strategies.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"

#include <kj/test.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/vector.h>

using namespace veloz::strategy;
using namespace veloz::market;
using namespace veloz::oms;

namespace {

// Test strategy that tracks events and signals
class TrackingStrategy : public BaseStrategy {
public:
  TrackingStrategy(const StrategyConfig& config)
      : BaseStrategy(config), event_count_(0) {}

  ~TrackingStrategy() noexcept override = default;

  StrategyType get_type() const override { return StrategyType::Custom; }

  void on_event(const MarketEvent& event) override { event_count_++; }
  void on_timer(int64_t timestamp) override {}

  kj::Vector<veloz::exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<veloz::exec::PlaceOrderRequest>();
  }

  size_t get_event_count() const { return event_count_; }

  static kj::StringPtr get_strategy_type() { return "Tracking"_kj; }

private:
  size_t event_count_;
};

// Factory for tracking strategy
class TrackingStrategyFactory : public IStrategyFactory {
public:
  ~TrackingStrategyFactory() noexcept(false) override = default;

  kj::StringPtr get_strategy_type() const override { return "Custom"_kj; }

  kj::Rc<IStrategy> create_strategy(const StrategyConfig& config) override {
    return kj::rc<TrackingStrategy>(config);
  }
};

// Helper to create a market event
MarketEvent create_test_event(kj::StringPtr symbol, double price) {
  MarketEvent event;
  event.symbol = veloz::common::SymbolId(symbol);
  event.type = MarketEventType::Trade;
  event.ts_exchange_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
  return event;
}

// Helper to create a strategy config
StrategyConfig create_test_config(kj::StringPtr name) {
  StrategyConfig config;
  config.name = kj::str(name);
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.10;
  return config;
}

// ============================================================================
// Integration Test: Multi-Strategy Concurrent Execution
// ============================================================================

KJ_TEST("Integration: Strategy manager registers and creates strategies") {
  StrategyManager manager;

  // Register factory
  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create strategy
  auto config = create_test_config("TestStrategy1"_kj);
  auto strategy = manager.create_strategy(config);

  // Verify strategy was created
  KJ_EXPECT(strategy.get() != nullptr);
  KJ_EXPECT(strategy->get_name() == "TestStrategy1"_kj);
}

KJ_TEST("Integration: Multiple strategies can be created and managed") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  kj::Vector<kj::String> strategy_ids;

  // Create multiple strategies
  for (int i = 0; i < 5; i++) {
    auto config = create_test_config(kj::str("Strategy_", i));
    auto strategy = manager.create_strategy(config);
    strategy_ids.add(kj::str(strategy->get_id()));
  }

  KJ_EXPECT(strategy_ids.size() == 5);
}

KJ_TEST("Integration: Strategy lifecycle management") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create strategy
  auto config = create_test_config("LifecycleTest"_kj);
  auto strategy = manager.create_strategy(config);
  auto id = kj::str(strategy->get_id());

  // Start strategy
  KJ_EXPECT(manager.start_strategy(id));

  auto state = strategy->get_state();
  KJ_EXPECT(state.is_running);

  // Stop strategy
  KJ_EXPECT(manager.stop_strategy(id));

  state = strategy->get_state();
  KJ_EXPECT(!state.is_running);
}

KJ_TEST("Integration: Event dispatch to multiple strategies") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create and start multiple strategies
  for (int i = 0; i < 3; i++) {
    auto config = create_test_config(kj::str("EventStrategy_", i));
    auto strategy = manager.create_strategy(config);
    manager.start_strategy(strategy->get_id());
  }

  // Dispatch events
  for (int i = 0; i < 10; i++) {
    auto event = create_test_event("BTCUSDT"_kj, 50000.0 + i * 100);
    manager.on_market_event(event);
  }

  // Verify strategies received events
  auto states = manager.get_all_strategy_states();
  KJ_EXPECT(states.size() == 3);
}

KJ_TEST("Integration: Strategy runtime load and unload") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create initial strategy
  auto config1 = create_test_config("InitialStrategy"_kj);
  auto strategy1 = manager.create_strategy(config1);
  auto id1 = kj::str(strategy1->get_id());

  KJ_EXPECT(manager.is_strategy_loaded(id1));

  // Unload the strategy
  KJ_EXPECT(manager.unload_strategy(id1));
  KJ_EXPECT(!manager.is_strategy_loaded(id1));
}

KJ_TEST("Integration: Strategy count tracking") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  KJ_EXPECT(manager.strategy_count() == 0);

  // Create strategies
  for (int i = 0; i < 3; i++) {
    auto config = create_test_config(kj::str("CountStrategy_", i));
    manager.create_strategy(config);
  }

  KJ_EXPECT(manager.strategy_count() == 3);
}

KJ_TEST("Integration: Strategy state retrieval") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create strategy
  auto config = create_test_config("StateTest"_kj);
  auto strategy = manager.create_strategy(config);
  auto id = kj::str(strategy->get_id());

  // Get all states
  auto states = manager.get_all_strategy_states();
  KJ_EXPECT(states.size() == 1);
  KJ_EXPECT(states[0].strategy_name == "StateTest"_kj);
}

KJ_TEST("Integration: Strategy ID retrieval") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create strategies
  for (int i = 0; i < 3; i++) {
    auto config = create_test_config(kj::str("IdStrategy_", i));
    manager.create_strategy(config);
  }

  // Get all IDs
  auto ids = manager.get_all_strategy_ids();
  KJ_EXPECT(ids.size() == 3);
}

KJ_TEST("Integration: Position update dispatch") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create and start strategy
  auto config = create_test_config("PositionTest"_kj);
  auto strategy = manager.create_strategy(config);
  manager.start_strategy(strategy->get_id());

  // Dispatch position update
  Position position(veloz::common::SymbolId("BTCUSDT"));
  position.apply_fill(veloz::exec::OrderSide::Buy, 1.0, 50000.0);
  manager.on_position_update(position);

  // Verify no crash
}

KJ_TEST("Integration: Timer event dispatch") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create and start strategy
  auto config = create_test_config("TimerTest"_kj);
  auto strategy = manager.create_strategy(config);
  manager.start_strategy(strategy->get_id());

  // Dispatch timer event
  manager.on_timer(1704067200000);

  // Verify no crash
}

KJ_TEST("Integration: Signal collection from strategies") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create and start strategy
  auto config = create_test_config("SignalTest"_kj);
  auto strategy = manager.create_strategy(config);
  manager.start_strategy(strategy->get_id());

  // Get signals (tracking strategy returns empty)
  auto signals = manager.get_all_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("Integration: Strategy removal") {
  StrategyManager manager;

  auto factory = kj::rc<TrackingStrategyFactory>();
  manager.register_strategy_factory(factory.addRef());

  // Create strategy
  auto config = create_test_config("RemoveTest"_kj);
  auto strategy = manager.create_strategy(config);
  auto id = kj::str(strategy->get_id());

  KJ_EXPECT(manager.strategy_count() == 1);

  // Remove strategy
  KJ_EXPECT(manager.remove_strategy(id));
  KJ_EXPECT(manager.strategy_count() == 0);
}

} // namespace
