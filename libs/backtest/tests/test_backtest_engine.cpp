#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

#include <atomic>
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
  void on_pause() override {}
  void on_resume() override {}
  void on_event(const veloz::market::MarketEvent& event) override {
    events_received_++;
  }
  void on_position_update(const veloz::oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {
    timer_events_++;
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

  void reset() override {
    events_received_ = 0;
    timer_events_ = 0;
  }

  int events_received() const {
    return events_received_;
  }
  int timer_events() const {
    return timer_events_;
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

private:
  kj::String id_;
  kj::String name_;
  veloz::strategy::StrategyType type_;
  int events_received_{0};
  int timer_events_{0};
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

// ============================================================================
// VirtualClock Tests
// ============================================================================

KJ_TEST("VirtualClock: BasicOperations") {
  VirtualClock clock;

  // Set start and end times
  std::int64_t start_ns = 1000000000LL; // 1 second in ns
  std::int64_t end_ns = 5000000000LL;   // 5 seconds in ns

  clock.set_start_time(start_ns);
  clock.set_end_time(end_ns);

  // Initial state
  KJ_EXPECT(clock.now_ns() == start_ns);
  KJ_EXPECT(clock.start_time_ns() == start_ns);
  KJ_EXPECT(clock.end_time_ns() == end_ns);
  KJ_EXPECT(clock.progress() == 0.0);

  // Advance time
  KJ_EXPECT(clock.advance_to(2000000000LL));
  KJ_EXPECT(clock.now_ns() == 2000000000LL);
  KJ_EXPECT(clock.progress() == 0.25); // 1/4 of the way

  // Cannot go backwards
  KJ_EXPECT(!clock.advance_to(1500000000LL));
  KJ_EXPECT(clock.now_ns() == 2000000000LL); // Still at 2s

  // Advance to end
  KJ_EXPECT(clock.advance_to(end_ns));
  KJ_EXPECT(clock.progress() == 1.0);

  // Reset
  clock.reset();
  KJ_EXPECT(clock.now_ns() == start_ns);
  KJ_EXPECT(clock.progress() == 0.0);
}

KJ_TEST("VirtualClock: ElapsedAndRemaining") {
  VirtualClock clock;
  clock.set_start_time(1000000000LL);
  clock.set_end_time(5000000000LL);

  KJ_EXPECT(clock.elapsed_ns() == 0);
  KJ_EXPECT(clock.remaining_ns() == 4000000000LL);

  clock.advance_to(3000000000LL);
  KJ_EXPECT(clock.elapsed_ns() == 2000000000LL);
  KJ_EXPECT(clock.remaining_ns() == 2000000000LL);
}

KJ_TEST("VirtualClock: MillisecondConversion") {
  VirtualClock clock;
  clock.set_start_time(1000000000LL); // 1 second in ns
  clock.advance_to(2500000000LL);     // 2.5 seconds in ns

  KJ_EXPECT(clock.now_ms() == 2500); // 2500 milliseconds
}

// ============================================================================
// BacktestState Tests
// ============================================================================

KJ_TEST("BacktestEngine: StateTransitions") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();

  // Initial state is Idle
  KJ_EXPECT(engine->get_state() == BacktestState::Idle);

  // After initialize, state is Initialized
  KJ_EXPECT(engine->initialize(config));
  KJ_EXPECT(engine->get_state() == BacktestState::Initialized);

  // Reset returns to Idle
  KJ_EXPECT(engine->reset());
  KJ_EXPECT(engine->get_state() == BacktestState::Idle);
}

KJ_TEST("BacktestEngine: PauseWithoutRunning") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  // Cannot pause when not running
  KJ_EXPECT(!engine->pause());
}

KJ_TEST("BacktestEngine: ResumeWithoutPause") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  // Cannot resume when not paused
  KJ_EXPECT(!engine->resume());
}

// ============================================================================
// BacktestEvent Tests
// ============================================================================

KJ_TEST("BacktestEvent: PriorityComparison") {
  BacktestEvent low_priority;
  low_priority.priority = BacktestEventPriority::Low;
  low_priority.timestamp_ns = 1000;
  low_priority.sequence = 0;

  BacktestEvent high_priority;
  high_priority.priority = BacktestEventPriority::High;
  high_priority.timestamp_ns = 2000; // Later timestamp
  high_priority.sequence = 1;

  // Higher priority should come first (operator> returns true)
  KJ_EXPECT(high_priority > low_priority);
  KJ_EXPECT(!(low_priority > high_priority));
}

KJ_TEST("BacktestEvent: TimestampComparison") {
  BacktestEvent earlier;
  earlier.priority = BacktestEventPriority::Normal;
  earlier.timestamp_ns = 1000;
  earlier.sequence = 0;

  BacktestEvent later;
  later.priority = BacktestEventPriority::Normal;
  later.timestamp_ns = 2000;
  later.sequence = 1;

  // Same priority, earlier timestamp should come first
  KJ_EXPECT(earlier > later);
  KJ_EXPECT(!(later > earlier));
}

KJ_TEST("BacktestEvent: SequenceComparison") {
  BacktestEvent first;
  first.priority = BacktestEventPriority::Normal;
  first.timestamp_ns = 1000;
  first.sequence = 0;

  BacktestEvent second;
  second.priority = BacktestEventPriority::Normal;
  second.timestamp_ns = 1000; // Same timestamp
  second.sequence = 1;

  // Same priority and timestamp, lower sequence should come first
  KJ_EXPECT(first > second);
  KJ_EXPECT(!(second > first));
}

// ============================================================================
// Event Queue Tests
// ============================================================================

KJ_TEST("BacktestEngine: AddEvent") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  // Add custom events
  BacktestEvent event1;
  event1.type = BacktestEventType::Custom;
  event1.priority = BacktestEventPriority::Normal;
  event1.timestamp_ns = 1000000000LL;
  event1.custom_data = kj::str("test1");

  BacktestEvent event2;
  event2.type = BacktestEventType::Custom;
  event2.priority = BacktestEventPriority::High;
  event2.timestamp_ns = 2000000000LL;
  event2.custom_data = kj::str("test2");

  engine->add_event(kj::mv(event1));
  engine->add_event(kj::mv(event2));

  KJ_EXPECT(engine->pending_events() == 2);
}

KJ_TEST("BacktestEngine: GetClock") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  const auto& clock = engine->get_clock();

  // Clock should be initialized with config times (converted to ns)
  KJ_EXPECT(clock.start_time_ns() == config.start_time * 1'000'000);
  KJ_EXPECT(clock.end_time_ns() == config.end_time * 1'000'000);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

KJ_TEST("BacktestState: ToString") {
  KJ_EXPECT(to_string(BacktestState::Idle) == "Idle"_kj);
  KJ_EXPECT(to_string(BacktestState::Initialized) == "Initialized"_kj);
  KJ_EXPECT(to_string(BacktestState::Running) == "Running"_kj);
  KJ_EXPECT(to_string(BacktestState::Paused) == "Paused"_kj);
  KJ_EXPECT(to_string(BacktestState::Completed) == "Completed"_kj);
  KJ_EXPECT(to_string(BacktestState::Stopped) == "Stopped"_kj);
  KJ_EXPECT(to_string(BacktestState::Error) == "Error"_kj);
}

KJ_TEST("BacktestEventPriority: ToString") {
  KJ_EXPECT(to_string(BacktestEventPriority::Low) == "Low"_kj);
  KJ_EXPECT(to_string(BacktestEventPriority::Normal) == "Normal"_kj);
  KJ_EXPECT(to_string(BacktestEventPriority::High) == "High"_kj);
  KJ_EXPECT(to_string(BacktestEventPriority::Critical) == "Critical"_kj);
}

KJ_TEST("BacktestEventType: ToString") {
  KJ_EXPECT(to_string(BacktestEventType::MarketData) == "MarketData"_kj);
  KJ_EXPECT(to_string(BacktestEventType::OrderFill) == "OrderFill"_kj);
  KJ_EXPECT(to_string(BacktestEventType::Timer) == "Timer"_kj);
  KJ_EXPECT(to_string(BacktestEventType::RiskCheck) == "RiskCheck"_kj);
  KJ_EXPECT(to_string(BacktestEventType::Custom) == "Custom"_kj);
}

// ============================================================================
// Progress Callback Tests
// ============================================================================

KJ_TEST("BacktestEngine: ProgressCallback") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  std::atomic<int> callback_count{0};
  double last_progress = -1.0;

  engine->on_progress([&callback_count, &last_progress](double progress) {
    callback_count++;
    last_progress = progress;
  });

  // Callback should not be called until run() is called
  KJ_EXPECT(callback_count.load() == 0);
}

KJ_TEST("BacktestEngine: StateChangeCallback") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();

  std::atomic<int> callback_count{0};
  BacktestState last_old_state = BacktestState::Error;
  BacktestState last_new_state = BacktestState::Error;

  engine->on_state_change([&](BacktestState old_state, BacktestState new_state) {
    callback_count++;
    last_old_state = old_state;
    last_new_state = new_state;
  });

  // Initialize should trigger callback
  KJ_EXPECT(engine->initialize(config));
  KJ_EXPECT(callback_count.load() == 1);
  KJ_EXPECT(last_old_state == BacktestState::Idle);
  KJ_EXPECT(last_new_state == BacktestState::Initialized);

  // Reset should trigger callback
  KJ_EXPECT(engine->reset());
  KJ_EXPECT(callback_count.load() == 2);
  KJ_EXPECT(last_old_state == BacktestState::Initialized);
  KJ_EXPECT(last_new_state == BacktestState::Idle);
}

// ============================================================================
// Performance Tests
// ============================================================================

// Test data source that generates events in memory for performance testing
class PerformanceTestDataSource : public IDataSource {
public:
  explicit PerformanceTestDataSource(size_t event_count) : event_count_(event_count) {}

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
    events.reserve(event_count_);

    int64_t base_time = start_time > 0 ? start_time : 1609459200000000000LL; // 2021-01-01 in ns
    int64_t interval = 1000000LL;                                            // 1ms between events

    for (size_t i = 0; i < event_count_; ++i) {
      veloz::market::MarketEvent event;
      event.type = veloz::market::MarketEventType::Trade;
      event.venue = veloz::common::Venue::Binance;
      event.market = veloz::common::MarketKind::Spot;
      event.symbol = symbol;
      event.ts_exchange_ns = base_time + static_cast<int64_t>(i) * interval;
      event.ts_recv_ns = event.ts_exchange_ns;

      veloz::market::TradeData trade;
      trade.price = 50000.0 + (i % 1000) * 0.1; // Varying price
      trade.qty = 0.01;
      trade.is_buyer_maker = (i % 2 == 0);
      trade.trade_id = static_cast<int64_t>(i);
      event.data = trade;

      events.add(kj::mv(event));
    }

    return events;
  }

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override {
    return true; // No-op for performance test
  }

private:
  size_t event_count_;
};

KJ_TEST("BacktestEngine: Performance - 100K events") {
  // Test with 100K events (quick sanity check)
  constexpr size_t EVENT_COUNT = 100000;

  auto engine = kj::heap<BacktestEngine>();
  auto config = create_test_config();
  KJ_EXPECT(engine->initialize(config));

  auto strategy = kj::rc<TestStrategy>();
  engine->set_strategy(strategy.addRef());

  auto data_source = kj::rc<PerformanceTestDataSource>(EVENT_COUNT);
  engine->set_data_source(data_source.addRef());

  auto start = std::chrono::high_resolution_clock::now();
  bool success = engine->run();
  auto end = std::chrono::high_resolution_clock::now();

  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  KJ_EXPECT(success); // Run completed successfully
  KJ_EXPECT(strategy->events_received() == static_cast<int>(EVENT_COUNT));

  // 100K events should complete in under 10 seconds
  KJ_EXPECT(duration_ms < 10000);

  // Calculate events per second
  double events_per_second = static_cast<double>(EVENT_COUNT) * 1000.0 / duration_ms;
  // Log performance (visible in test output)
  KJ_LOG(INFO, "Performance: ", EVENT_COUNT, " events in ", duration_ms, "ms (",
         static_cast<int>(events_per_second), " events/sec)");
}

} // namespace
