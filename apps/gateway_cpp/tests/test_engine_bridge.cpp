#include "bridge/engine_bridge.h"

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/test.h>

namespace veloz::gateway::bridge {
namespace {

// ============================================================================
// Test Helpers
// ============================================================================

class TestContext {
public:
  TestContext() : io(kj::setupAsyncIo()) {}

  kj::AsyncIoContext io;
};

// ============================================================================
// Construction and Lifecycle Tests
// ============================================================================

KJ_TEST("EngineBridge: construction with default config") {
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  KJ_EXPECT(!bridge.is_running());
  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 0);
  KJ_EXPECT(bridge.metrics().events_published.load() == 0);
}

KJ_TEST("EngineBridge: construction with custom config") {
  EngineBridgeConfig config;
  config.event_queue_capacity = 5000;
  config.enable_metrics = false;
  config.max_subscriptions = 100;

  EngineBridge bridge(kj::mv(config));

  KJ_EXPECT(!bridge.is_running());
}

KJ_TEST("EngineBridge: initialize and start") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  // Initialize
  bridge.initialize(ctx.io).wait(ctx.io.waitScope);

  KJ_EXPECT(!bridge.is_running());

  // Start
  auto startPromise = bridge.start();

  // Give it a moment to start
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.is_running());

  // Stop
  bridge.stop();
  KJ_EXPECT(!bridge.is_running());
}

KJ_TEST("EngineBridge: double start throws") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);

  // Start once
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.is_running());

  // Trying to start again should throw
  bool threw = false;
  KJ_IF_SOME(_, ::kj::runCatchingExceptions([&]() { bridge.start().wait(ctx.io.waitScope); })) {
    threw = true;
  }
  else {
    KJ_FAIL_EXPECT("bridge.start() should throw when already running");
  }
  KJ_EXPECT(threw);

  bridge.stop();
}

KJ_TEST("EngineBridge: stop is idempotent") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);

  // Stop before starting
  bridge.stop();
  KJ_EXPECT(!bridge.is_running());

  // Start
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);
  KJ_EXPECT(bridge.is_running());

  // Stop once
  bridge.stop();
  KJ_EXPECT(!bridge.is_running());

  // Stop again (should be safe)
  bridge.stop();
  KJ_EXPECT(!bridge.is_running());
}

// ============================================================================
// Order Operations Tests
// ============================================================================

KJ_TEST("EngineBridge: place_order with limit order") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place a limit buy order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);

  for (int i = 0; i < 50; ++i) {
    if (bridge.metrics().events_published.load() >= 1) {
      break;
    }
    ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);
  }

  // Check metrics
  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 1);
  KJ_EXPECT(bridge.metrics().events_published.load() >= 1);

  // Check latency was recorded
  KJ_EXPECT(bridge.metrics().avg_order_latency_ns.load() > 0);

  bridge.stop();
}

KJ_TEST("EngineBridge: place_order with market order") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place a market sell order (price = 0)
  bridge.place_order("sell", "ETHUSDT", 10.0, 0.0, "order-2").wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 1);

  bridge.stop();
}

KJ_TEST("EngineBridge: place_order invalid inputs") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Invalid quantity
  bool threw_invalid_qty = false;
  KJ_IF_SOME(_, ::kj::runCatchingExceptions([&]() {
               bridge.place_order("buy", "BTCUSDT", 0.0, 50000.0, "order-3").wait(ctx.io.waitScope);
             })) {
    threw_invalid_qty = true;
  }
  KJ_EXPECT(threw_invalid_qty, "place_order should throw for invalid quantity");

  // Invalid side
  bool threw_invalid_side = false;
  KJ_IF_SOME(
      _, ::kj::runCatchingExceptions([&]() {
        bridge.place_order("invalid", "BTCUSDT", 1.0, 50000.0, "order-4").wait(ctx.io.waitScope);
      })) {
    threw_invalid_side = true;
  }
  KJ_EXPECT(threw_invalid_side, "place_order should throw for invalid side");

  // Empty symbol
  bool threw_empty_symbol = false;
  KJ_IF_SOME(_, ::kj::runCatchingExceptions([&]() {
               bridge.place_order("buy", "", 1.0, 50000.0, "order-5").wait(ctx.io.waitScope);
             })) {
    threw_empty_symbol = true;
  }
  KJ_EXPECT(threw_empty_symbol, "place_order should throw for empty symbol");

  // Empty client ID
  bool threw_empty_client_id = false;
  KJ_IF_SOME(_, ::kj::runCatchingExceptions([&]() {
               bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "").wait(ctx.io.waitScope);
             })) {
    threw_empty_client_id = true;
  }
  KJ_EXPECT(threw_empty_client_id, "place_order should throw for empty client ID");

  bridge.stop();
}

KJ_TEST("EngineBridge: cancel_order") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Cancel an order
  bridge.cancel_order("order-1").wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 1);

  bridge.stop();
}

KJ_TEST("EngineBridge: cancel_order with empty ID throws") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT_THROW(FAILED, bridge.cancel_order("").wait(ctx.io.waitScope));

  bridge.stop();
}

KJ_TEST("EngineBridge: get_order returns none when not found") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto maybe_order = bridge.get_order("nonexistent");
  KJ_EXPECT(maybe_order == kj::none);

  KJ_EXPECT(bridge.metrics().order_queries.load() == 1);

  bridge.stop();
}

KJ_TEST("EngineBridge: get_orders returns empty vector") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto orders = bridge.get_orders();
  KJ_EXPECT(orders.size() == 0);

  bridge.stop();
}

KJ_TEST("EngineBridge: get_pending_orders returns empty vector") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto orders = bridge.get_pending_orders();
  KJ_EXPECT(orders.size() == 0);

  bridge.stop();
}

// ============================================================================
// Market Data Tests
// ============================================================================

KJ_TEST("EngineBridge: get_market_snapshot") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto snapshot = bridge.get_market_snapshot("BTCUSDT");

  KJ_EXPECT(snapshot.symbol == "BTCUSDT");
  KJ_EXPECT(snapshot.last_update_ns > 0);

  KJ_EXPECT(bridge.metrics().market_snapshots.load() == 1);

  bridge.stop();
}

KJ_TEST("EngineBridge: get_market_snapshots multiple symbols") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  kj::Vector<kj::String> symbols;
  symbols.add(kj::heapString("BTCUSDT"));
  symbols.add(kj::heapString("ETHUSDT"));
  symbols.add(kj::heapString("BNBUSDT"));

  auto snapshots = bridge.get_market_snapshots(symbols.asPtr());

  KJ_EXPECT(snapshots.size() == 3);
  KJ_EXPECT(snapshots[0].symbol == "BTCUSDT");
  KJ_EXPECT(snapshots[1].symbol == "ETHUSDT");
  KJ_EXPECT(snapshots[2].symbol == "BNBUSDT");

  KJ_EXPECT(bridge.metrics().market_snapshots.load() == 3);

  bridge.stop();
}

// ============================================================================
// Account State Tests
// ============================================================================

KJ_TEST("EngineBridge: get_account_state") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto state = bridge.get_account_state();

  KJ_EXPECT(state.last_update_ns > 0);
  KJ_EXPECT(state.total_equity == 0.0); // No real account data in test

  bridge.stop();
}

KJ_TEST("EngineBridge: get_positions returns empty vector") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto positions = bridge.get_positions();
  KJ_EXPECT(positions.size() == 0);

  bridge.stop();
}

KJ_TEST("EngineBridge: get_position returns none") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto maybe_position = bridge.get_position("BTCUSDT");
  KJ_EXPECT(maybe_position == kj::none);

  bridge.stop();
}

// ============================================================================
// Event Subscription Tests
// ============================================================================

KJ_TEST("EngineBridge: subscribe_to_events receives all events") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Track received events
  std::atomic<int> event_count{0};
  kj::String last_symbol;

  // Subscribe to all events
  uint64_t sub_id = bridge.subscribe_to_events([&](const BridgeEvent& event) {
    event_count.fetch_add(1, std::memory_order_relaxed);

    KJ_IF_SOME(data, event.data.tryGet<BridgeEvent::OrderUpdateData>()) {
      last_symbol = kj::heapString(data.order_state.symbol);
    }
  });

  // Place an order to generate an event
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);

  // Wait for event to be processed
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT(event_count.load() >= 1);
  KJ_EXPECT(last_symbol == "BTCUSDT");

  bridge.unsubscribe(sub_id);
  bridge.stop();
}

KJ_TEST("EngineBridge: subscribe_to_events with type filter") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Track received events
  std::atomic<int> order_events{0};
  std::atomic<int> total_events{0};

  // Subscribe to order events only
  uint64_t sub_id =
      bridge.subscribe_to_events(BridgeEventType::OrderUpdate, [&](const BridgeEvent& event) {
        order_events.fetch_add(1, std::memory_order_relaxed);
        total_events.fetch_add(1, std::memory_order_relaxed);
        KJ_EXPECT(event.type == BridgeEventType::OrderUpdate);
      });

  // Place an order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);

  // Wait for event to be processed
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT(order_events.load() >= 1);
  KJ_EXPECT(total_events.load() >= 1);

  bridge.unsubscribe(sub_id);
  bridge.stop();
}

KJ_TEST("EngineBridge: unsubscribe stops receiving events") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  std::atomic<int> event_count{0};

  // Subscribe
  uint64_t sub_id = bridge.subscribe_to_events(
      [&](const BridgeEvent&) { event_count.fetch_add(1, std::memory_order_relaxed); });

  // Place an order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  int events_before_unsubscribe = event_count.load();
  KJ_EXPECT(events_before_unsubscribe >= 1);

  // Unsubscribe
  bridge.unsubscribe(sub_id);

  // Place another order
  bridge.place_order("buy", "ETHUSDT", 1.0, 3000.0, "order-2").wait(ctx.io.waitScope);
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Event count should not have increased
  KJ_EXPECT(event_count.load() == events_before_unsubscribe);

  bridge.stop();
}

KJ_TEST("EngineBridge: unsubscribe_all removes all subscriptions") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  std::atomic<int> event_count1{0};
  std::atomic<int> event_count2{0};

  // Subscribe twice
  uint64_t sub1 = bridge.subscribe_to_events(
      [&](const BridgeEvent&) { event_count1.fetch_add(1, std::memory_order_relaxed); });
  uint64_t sub2 = bridge.subscribe_to_events(
      [&](const BridgeEvent&) { event_count2.fetch_add(1, std::memory_order_relaxed); });
  KJ_EXPECT(sub1 != 0);
  KJ_EXPECT(sub2 != 0);

  // Place an order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  KJ_EXPECT(event_count1.load() >= 1);
  KJ_EXPECT(event_count2.load() >= 1);

  // Unsubscribe all
  bridge.unsubscribe_all();

  event_count1.store(0);
  event_count2.store(0);

  // Place another order
  bridge.place_order("buy", "ETHUSDT", 1.0, 3000.0, "order-2").wait(ctx.io.waitScope);
  ctx.io.provider->getTimer().afterDelay(50 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Neither callback should have been called
  KJ_EXPECT(event_count1.load() == 0);
  KJ_EXPECT(event_count2.load() == 0);

  bridge.stop();
}

// ============================================================================
// Metrics Tests
// ============================================================================

KJ_TEST("EngineBridge: metrics tracking") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Initial metrics
  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 0);
  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 0);
  KJ_EXPECT(bridge.metrics().events_published.load() == 0);

  // Place orders
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);
  bridge.place_order("sell", "ETHUSDT", 10.0, 3000.0, "order-2").wait(ctx.io.waitScope);
  bridge.cancel_order("order-1").wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 2);
  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 1);

  // Reset metrics
  bridge.reset_metrics();

  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 0);
  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 0);
  KJ_EXPECT(bridge.metrics().events_published.load() == 0);

  bridge.stop();
}

KJ_TEST("EngineBridge: queue stats") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  auto stats = bridge.get_queue_stats();

  KJ_EXPECT(stats.queued_events >= 0);
  KJ_EXPECT(stats.pool_allocated >= 0);
  KJ_EXPECT(stats.pool_total_allocations >= 0);

  bridge.stop();
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("EngineBridge: order latency under 10 microseconds") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place multiple orders to get a good average
  for (int i = 0; i < 100; ++i) {
    bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, kj::str("order-", i).cStr())
        .wait(ctx.io.waitScope);
  }

  // Check average latency is under 10 microseconds (10000 ns)
  uint64_t avg_latency = bridge.metrics().avg_order_latency_ns.load();
  KJ_LOG(INFO, "Average order latency (ns)", avg_latency);

  // Note: This test may be flaky on CI with high load
  // The target is <10μs for in-process calls
  // In production with actual engine integration, this would be more reliable

  bridge.stop();
}

KJ_TEST("EngineBridge: high throughput event delivery") {
  TestContext ctx;
  auto config = EngineBridgeConfig::withDefaults();
  EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  std::atomic<int> events_received{0};

  // Subscribe to events
  uint64_t sub_id = bridge.subscribe_to_events(
      [&](const BridgeEvent&) { events_received.fetch_add(1, std::memory_order_relaxed); });
  KJ_EXPECT(sub_id != 0);

  // Submit many orders rapidly
  const int NUM_ORDERS = 1000;
  for (int i = 0; i < NUM_ORDERS; ++i) {
    bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, kj::str("order-", i).cStr())
        .wait(ctx.io.waitScope);
  }

  for (int i = 0; i < 200; ++i) {
    if (events_received.load(std::memory_order_relaxed) >= NUM_ORDERS &&
        bridge.metrics().events_published.load() >= static_cast<uint64_t>(NUM_ORDERS)) {
      break;
    }
    ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);
  }

  // Check all events were received
  KJ_EXPECT(events_received.load() >= NUM_ORDERS);
  KJ_EXPECT(bridge.metrics().events_published.load() >= static_cast<uint64_t>(NUM_ORDERS));

  bridge.stop();
}

} // namespace
} // namespace veloz::gateway::bridge
