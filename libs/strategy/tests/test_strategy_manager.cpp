#include "kj/test.h"
#include "veloz/strategy/strategy.h"

namespace {

using namespace veloz::strategy;

// Test strategy implementation
class TestStrategy : public IStrategy {
public:
  explicit TestStrategy(const StrategyConfig& config)
      : config_ref_(config), id_(kj::str("strat-", rand() % 1000000)), name_(kj::str(config.name)) {
  }
  ~TestStrategy() noexcept(false) override = default;

  kj::StringPtr get_id() const override {
    return id_;
  }

  kj::StringPtr get_name() const override {
    return name_;
  }

  StrategyType get_type() const override {
    return StrategyType::Custom;
  }

  bool initialize(const StrategyConfig& config, veloz::core::Logger& logger) override {
    logger.info("Test strategy initialized");
    initialized_ = true;
    return true;
  }

  void on_start() override {
    running_ = true;
  }

  void on_stop() override {
    running_ = false;
  }

  void on_pause() override {
    running_ = false;
  }

  void on_resume() override {
    running_ = true;
  }

  void on_event(const veloz::market::MarketEvent& event) override {}
  void on_position_update(const veloz::oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {}

  StrategyState get_state() const override {
    return StrategyState{.strategy_id = kj::heapString(get_id()),
                         .strategy_name = kj::heapString(get_name()),
                         .is_running = running_,
                         .pnl = 0.0,
                         .max_drawdown = 0.0,
                         .trade_count = 0,
                         .win_count = 0,
                         .lose_count = 0,
                         .win_rate = 0.0,
                         .profit_factor = 0.0};
  }

  kj::Vector<veloz::exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<veloz::exec::PlaceOrderRequest>();
  }

  void reset() override {
    initialized_ = false;
    running_ = false;
  }

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

  static kj::StringPtr get_strategy_type() {
    return "Custom"_kj;
  }

  bool is_initialized() const {
    return initialized_;
  }
  bool is_running() const {
    return running_;
  }

private:
  const StrategyConfig& config_ref_;
  kj::String id_;
  kj::String name_;
  bool initialized_ = false;
  bool running_ = false;
};

// Test strategy factory
class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  kj::Rc<IStrategy> create_strategy(const StrategyConfig& config) override {
    return kj::rc<TestStrategy>(config);
  }

  kj::StringPtr get_strategy_type() const override {
    return TestStrategy::get_strategy_type();
  }
};

// Helper function to create test configuration
StrategyConfig createTestConfig(kj::StringPtr name) {
  StrategyConfig config;
  config.name = kj::str(name);
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));
  return config;
}

KJ_TEST("StrategyManager: Registration") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  auto strategy = manager->create_strategy(config);
  KJ_EXPECT(strategy != nullptr);
  KJ_EXPECT(strategy->get_name() == kj::StringPtr("Test Strategy"));
}

KJ_TEST("StrategyManager: Lifecycle") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  auto strategy = manager->create_strategy(config);
  KJ_EXPECT(strategy != nullptr);

  auto ids = manager->get_all_strategy_ids();
  KJ_EXPECT(ids.size() > 0);
  kj::StringPtr strategy_id = ids[0];
  KJ_EXPECT(manager->start_strategy(strategy_id));
  KJ_EXPECT(manager->stop_strategy(strategy_id));
}

KJ_TEST("StrategyManager: State query") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager->create_strategy(config);

  auto states = manager->get_all_strategy_states();
  KJ_EXPECT(states.size() == 1);
  KJ_EXPECT(states[0].strategy_name == kj::StringPtr("Test Strategy"));
  KJ_EXPECT(states[0].is_running == false);
}

KJ_TEST("StrategyManager: Event dispatch") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager->create_strategy(config);

  veloz::market::MarketEvent event;
  event.type = veloz::market::MarketEventType::Ticker;
  event.symbol = veloz::common::SymbolId{"BTCUSDT"};

  // Dispatch event - should not throw exception
  manager->on_market_event(event);
}

KJ_TEST("StrategyManager: Runtime load/unload") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Runtime Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  veloz::core::Logger logger;

  auto strategy_id = manager->load_strategy(config, logger);
  KJ_EXPECT(strategy_id.size() > 0);
  KJ_EXPECT(manager->is_strategy_loaded(strategy_id.cStr()));
  KJ_EXPECT(manager->strategy_count() == 1);

  KJ_EXPECT(manager->unload_strategy(strategy_id.cStr()));
  KJ_EXPECT(!manager->is_strategy_loaded(strategy_id.cStr()));
  KJ_EXPECT(manager->strategy_count() == 0);
}

KJ_TEST("StrategyManager: Signal callback") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Signal Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager->create_strategy(config);

  // Set up signal callback
  bool callback_called = false;
  manager->set_signal_callback(
      [&callback_called](const kj::Vector<veloz::exec::PlaceOrderRequest>& signals) {
        callback_called = true;
      });

  // Process signals (TestStrategy returns empty signals, so callback won't be called)
  manager->process_and_route_signals();
  KJ_EXPECT(callback_called == false);
}

KJ_TEST("StrategyManager: Metrics summary") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  StrategyConfig config;
  config.name = kj::str("Metrics Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager->create_strategy(config);

  auto summary = manager->get_metrics_summary();
  KJ_EXPECT(summary.size() > 0);
  // Check if summary contains expected text
  KJ_EXPECT(summary.size() >= 10); // Minimal check for non-empty summary
}

KJ_TEST("StrategyManager: Strategy count") {
  auto manager = kj::heap<StrategyManager>();
  kj::Rc<IStrategyFactory> factory = kj::rc<TestStrategyFactory>();
  manager->register_strategy_factory(kj::mv(factory));

  KJ_EXPECT(manager->strategy_count() == 0);

  StrategyConfig config1;
  config1.name = kj::str("Strategy 1");
  config1.type = StrategyType::Custom;
  config1.risk_per_trade = 0.01;
  config1.max_position_size = 1.0;
  config1.stop_loss = 0.05;
  config1.take_profit = 0.1;
  config1.symbols.add(kj::str("BTCUSDT"));

  manager->create_strategy(config1);
  KJ_EXPECT(manager->strategy_count() == 1);

  StrategyConfig config2;
  config2.name = kj::str("Strategy 2");
  config2.type = StrategyType::Custom;
  config2.risk_per_trade = 0.02;
  config2.max_position_size = 2.0;
  config2.stop_loss = 0.03;
  config2.take_profit = 0.15;
  config2.symbols.add(kj::str("ETHUSDT"));

  manager->create_strategy(config2);
  KJ_EXPECT(manager->strategy_count() == 2);
}

} // namespace
