/**
 * @file test_engine_integration.cpp
 * @brief Integration tests for VeloZ engine event flow
 *
 * Tests the full event flow through the engine:
 * - MarketData -> StrategyRuntime -> OMS
 * - Event injection and processing
 * - Signal generation and routing
 * - Order submission and fill handling
 */

#include "test_harness.h"

#include <kj/test.h>

using namespace veloz::test;
using namespace veloz::market;
using namespace veloz::exec;
using namespace veloz::common;

namespace {

// ============================================================================
// Mock Component Unit Tests
// ============================================================================

KJ_TEST("MockMarketDataManager: inject and track events") {
  MockMarketDataManager mdm;

  // Inject events
  auto event1 = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  auto event2 = make_trade_event("ETHUSDT"_kj, 3000.0, 10.0);

  mdm.inject_event(event1);
  mdm.inject_event(event2);

  KJ_EXPECT(mdm.injected_event_count() == 2);
}

KJ_TEST("MockMarketDataManager: subscription tracking") {
  MockMarketDataManager mdm;

  SymbolId btc("BTCUSDT");
  SymbolId eth("ETHUSDT");

  mdm.subscribe(btc, MarketEventType::Trade);
  mdm.subscribe(btc, MarketEventType::BookTop);
  mdm.subscribe(eth, MarketEventType::Trade);

  KJ_EXPECT(mdm.subscription_count() == 3);
  KJ_EXPECT(mdm.is_subscribed(btc, MarketEventType::Trade));
  KJ_EXPECT(mdm.is_subscribed(btc, MarketEventType::BookTop));
  KJ_EXPECT(mdm.is_subscribed(eth, MarketEventType::Trade));
  KJ_EXPECT(!mdm.is_subscribed(eth, MarketEventType::BookTop));

  mdm.unsubscribe(btc, MarketEventType::Trade);
  KJ_EXPECT(mdm.subscription_count() == 2);
  KJ_EXPECT(!mdm.is_subscribed(btc, MarketEventType::Trade));
}

KJ_TEST("MockMarketDataManager: event callback invocation") {
  MockMarketDataManager mdm;

  size_t callback_count = 0;
  mdm.set_event_callback([&callback_count](const MarketEvent& event) {
    callback_count++;
    KJ_EXPECT(event.type == MarketEventType::Trade);
  });

  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  mdm.inject_event(event);

  KJ_EXPECT(callback_count == 1);
}

KJ_TEST("MockStrategyRuntime: event processing") {
  MockStrategyRuntime runtime;

  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  runtime.on_market_event(event);
  runtime.on_market_event(event);
  runtime.on_market_event(event);

  KJ_EXPECT(runtime.events_received() == 3);
  KJ_EXPECT(runtime.signals_generated() == 0); // Auto-generate disabled by default
}

KJ_TEST("MockStrategyRuntime: auto signal generation") {
  MockStrategyRuntime runtime;
  runtime.set_auto_generate_signals(true);

  size_t signals_received = 0;
  runtime.set_signal_callback([&signals_received](const kj::Vector<PlaceOrderRequest>& signals) {
    signals_received += signals.size();
  });

  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  runtime.on_market_event(event);

  KJ_EXPECT(runtime.events_received() == 1);
  KJ_EXPECT(runtime.signals_generated() == 1);
  KJ_EXPECT(signals_received == 1);
}

KJ_TEST("MockOMS: order capture") {
  MockOMS oms;

  PlaceOrderRequest order;
  order.client_order_id = kj::str("test_order_1");
  order.symbol = SymbolId("BTCUSDT");
  order.side = OrderSide::Buy;
  order.type = OrderType::Market;
  order.qty = 0.001;

  oms.submit_order(order);

  KJ_EXPECT(oms.order_count() == 1);
  KJ_EXPECT(oms.has_order("test_order_1"_kj));

  KJ_IF_SOME(side, oms.get_order_side("test_order_1"_kj)) {
    KJ_EXPECT(side == OrderSide::Buy);
  }
  else {
    KJ_FAIL_EXPECT("Order side not found");
  }
}

KJ_TEST("MockOMS: auto fill") {
  MockOMS oms;
  oms.set_auto_fill(true, 50000.0);

  size_t fills_received = 0;
  oms.set_fill_callback([&fills_received](const ExecutionReport& report) {
    fills_received++;
    KJ_EXPECT(report.status == OrderStatus::Filled);
    KJ_EXPECT(report.last_fill_price == 50000.0);
  });

  PlaceOrderRequest order;
  order.client_order_id = kj::str("test_order_1");
  order.symbol = SymbolId("BTCUSDT");
  order.side = OrderSide::Buy;
  order.type = OrderType::Market;
  order.qty = 0.001;

  oms.submit_order(order);

  KJ_EXPECT(oms.order_count() == 1);
  KJ_EXPECT(oms.fill_count() == 1);
  KJ_EXPECT(fills_received == 1);
}

// ============================================================================
// Integration Test Harness Tests
// ============================================================================

KJ_TEST("IntegrationTestHarness: basic event flow") {
  IntegrationTestHarness harness;

  // Inject a trade event
  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  harness.inject_and_process(event);

  // Verify event was injected and processed
  KJ_EXPECT(harness.events_injected() == 1);
  KJ_EXPECT(harness.events_processed() == 1);

  // No signals without auto-generate
  KJ_EXPECT(harness.signals_generated() == 0);
  KJ_EXPECT(harness.orders_submitted() == 0);
}

KJ_TEST("IntegrationTestHarness: full auto flow") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Inject a trade event
  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  harness.inject_and_process(event);

  // Verify full flow: event -> signal -> order -> fill
  KJ_EXPECT(harness.events_injected() == 1);
  KJ_EXPECT(harness.events_processed() == 1);
  KJ_EXPECT(harness.signals_generated() == 1);
  KJ_EXPECT(harness.orders_submitted() == 1);
  KJ_EXPECT(harness.fills_received() == 1);
}

KJ_TEST("IntegrationTestHarness: multiple events") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Inject multiple events
  for (int i = 0; i < 10; i++) {
    auto event = make_trade_event("BTCUSDT"_kj, 50000.0 + i * 100, 1.0);
    harness.inject_and_process(event);
  }

  // Verify all events processed
  KJ_EXPECT(harness.events_injected() == 10);
  KJ_EXPECT(harness.events_processed() == 10);
  KJ_EXPECT(harness.signals_generated() == 10);
  KJ_EXPECT(harness.orders_submitted() == 10);
  KJ_EXPECT(harness.fills_received() == 10);
}

KJ_TEST("IntegrationTestHarness: clear state") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Inject events
  auto event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  harness.inject_and_process(event);

  KJ_EXPECT(harness.events_injected() == 1);

  // Clear state
  harness.clear();

  KJ_EXPECT(harness.events_injected() == 0);
  KJ_EXPECT(harness.events_processed() == 0);
  KJ_EXPECT(harness.signals_generated() == 0);
  KJ_EXPECT(harness.orders_submitted() == 0);
  KJ_EXPECT(harness.fills_received() == 0);
}

// ============================================================================
// Event Flow Integration Tests
// ============================================================================

KJ_TEST("Integration: MarketEvent flows from MarketData to StrategyRuntime") {
  IntegrationTestHarness harness;

  // Track events received by strategy runtime
  size_t events_at_runtime = 0;
  harness.market_data().set_event_callback([&events_at_runtime, &harness](const MarketEvent& event) {
    harness.strategy_runtime().on_market_event(event);
    events_at_runtime++;
  });

  // Inject events
  auto trade = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  auto book = make_book_top_event("BTCUSDT"_kj, 49999.0, 10.0, 50001.0, 10.0);

  harness.market_data().inject_event(trade);
  harness.market_data().inject_event(book);

  KJ_EXPECT(events_at_runtime == 2);
  KJ_EXPECT(harness.strategy_runtime().events_received() == 2);
}

KJ_TEST("Integration: Strategy signals flow to OMS") {
  IntegrationTestHarness harness;

  // Manually inject signals
  kj::Vector<PlaceOrderRequest> signals;
  PlaceOrderRequest order;
  order.client_order_id = kj::str("signal_order_1");
  order.symbol = SymbolId("BTCUSDT");
  order.side = OrderSide::Buy;
  order.type = OrderType::Limit;
  order.tif = TimeInForce::GTC;
  order.qty = 0.01;
  order.price = 50000.0;
  signals.add(kj::mv(order));

  harness.strategy_runtime().inject_signals(kj::mv(signals));

  // Verify order captured by OMS
  KJ_EXPECT(harness.oms().order_count() == 1);
  KJ_EXPECT(harness.oms().has_order("signal_order_1"_kj));
}

KJ_TEST("Integration: OMS fills update position store") {
  IntegrationTestHarness harness;
  harness.oms().set_auto_fill(true, 50000.0);

  // Submit order
  PlaceOrderRequest order;
  order.client_order_id = kj::str("position_test_order");
  order.symbol = SymbolId("BTCUSDT");
  order.side = OrderSide::Buy;
  order.type = OrderType::Market;
  order.qty = 1.0;

  harness.oms().submit_order(order);

  // Verify fill was recorded
  KJ_EXPECT(harness.oms().fill_count() == 1);

  // Verify position store was updated with fill data
  // Note: OrderStore.apply_execution_report updates executed_qty and avg_price
  // but doesn't set status from report (status is set by apply_fill when order_qty is known)
  auto order_state = harness.order_store().get("position_test_order"_kj);
  KJ_IF_SOME(state, order_state) {
    KJ_EXPECT(state.executed_qty == 1.0);
    KJ_EXPECT(state.avg_price == 50000.0);
    KJ_EXPECT(state.symbol == "BTCUSDT"_kj);
  }
  else {
    KJ_FAIL_EXPECT("Order not found in position store");
  }
}

KJ_TEST("Integration: Multiple symbols processed independently") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Inject events for different symbols
  auto btc_event = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  auto eth_event = make_trade_event("ETHUSDT"_kj, 3000.0, 10.0);
  auto bnb_event = make_trade_event("BNBUSDT"_kj, 300.0, 100.0);

  harness.inject_and_process(btc_event);
  harness.inject_and_process(eth_event);
  harness.inject_and_process(bnb_event);

  // Verify all processed
  KJ_EXPECT(harness.events_processed() == 3);
  KJ_EXPECT(harness.signals_generated() == 3);
  KJ_EXPECT(harness.orders_submitted() == 3);
}

KJ_TEST("Integration: Event type filtering") {
  IntegrationTestHarness harness;
  harness.strategy_runtime().set_auto_generate_signals(true);

  // Only Trade events generate signals in MockStrategyRuntime
  auto trade = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  auto book = make_book_top_event("BTCUSDT"_kj, 49999.0, 10.0, 50001.0, 10.0);
  auto kline = make_kline_event("BTCUSDT"_kj, 50000.0, 50100.0, 49900.0, 50050.0, 1000.0);

  harness.inject_and_process(trade);
  harness.inject_and_process(book);
  harness.inject_and_process(kline);

  // All events processed
  KJ_EXPECT(harness.events_processed() == 3);

  // Only trade generates signal (mock behavior)
  KJ_EXPECT(harness.signals_generated() == 1);
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("Integration: High-throughput event processing") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  const size_t event_count = 1000;

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < event_count; i++) {
    auto event = make_trade_event("BTCUSDT"_kj, 50000.0 + i, 1.0);
    harness.inject_and_process(event);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  // Verify all events processed
  KJ_EXPECT(harness.events_processed() == event_count);
  KJ_EXPECT(harness.orders_submitted() == event_count);

  // Performance check: should process 1000 events in < 1 second
  KJ_EXPECT(duration_ms < 1000, "Event processing too slow");
}

KJ_TEST("Integration: Burst event handling") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Simulate burst of events
  const size_t burst_size = 100;
  const size_t burst_count = 10;

  for (size_t burst = 0; burst < burst_count; burst++) {
    for (size_t i = 0; i < burst_size; i++) {
      auto event = make_trade_event("BTCUSDT"_kj, 50000.0 + i, 1.0);
      harness.inject_and_process(event);
    }
  }

  KJ_EXPECT(harness.events_processed() == burst_size * burst_count);
  KJ_EXPECT(harness.orders_submitted() == burst_size * burst_count);
}

// ============================================================================
// Edge Cases
// ============================================================================

KJ_TEST("Integration: Empty signal batch") {
  IntegrationTestHarness harness;

  // Inject empty signal batch
  kj::Vector<PlaceOrderRequest> empty_signals;
  harness.strategy_runtime().inject_signals(kj::mv(empty_signals));

  // No orders should be submitted
  KJ_EXPECT(harness.orders_submitted() == 0);
}

KJ_TEST("Integration: Disable auto flow mid-processing") {
  IntegrationTestHarness harness;
  harness.enable_auto_flow();

  // Process some events with auto flow
  auto event1 = make_trade_event("BTCUSDT"_kj, 50000.0, 1.0);
  harness.inject_and_process(event1);

  KJ_EXPECT(harness.signals_generated() == 1);
  KJ_EXPECT(harness.orders_submitted() == 1);

  // Disable auto flow
  harness.disable_auto_flow();

  // Process more events
  auto event2 = make_trade_event("BTCUSDT"_kj, 50100.0, 1.0);
  harness.inject_and_process(event2);

  // Events processed but no new signals/orders
  KJ_EXPECT(harness.events_processed() == 2);
  KJ_EXPECT(harness.signals_generated() == 1); // Still 1
  KJ_EXPECT(harness.orders_submitted() == 1);  // Still 1
}

} // namespace
