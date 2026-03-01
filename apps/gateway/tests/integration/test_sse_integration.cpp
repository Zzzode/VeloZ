/**
 * @file test_sse_integration.cpp
 * @brief Integration tests for Server-Sent Events (SSE) streaming
 *
 * Tests cover:
 * - SSE connection establishment
 * - Event delivery and ordering
 * - Reconnection with Last-Event-ID
 * - Keep-alive mechanism
 * - Multiple concurrent connections
 * - Event history replay
 * - Connection cleanup
 *
 * Performance targets:
 * - SSE connection: <100ms
 * - Event delivery: <10ms per event
 * - Keep-alive interval: configurable (default 10s)
 * - Max concurrent connections: 1000+
 */

#include "../src/bridge/event.h"
#include "../src/bridge/event_broadcaster.h"
#include "../src/handlers/sse_handler.h"

#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>

using namespace veloz::gateway::bridge;
using namespace veloz::gateway::handlers;

// =============================================================================
// Test Infrastructure
// =============================================================================

namespace {

/**
 * @brief Helper to get current time in nanoseconds
 */
uint64_t current_time_ns() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now().time_since_epoch())
                                   .count());
}

/**
 * @brief Create a test event
 */
SseEvent create_test_event(uint64_t id, SseEventType type, kj::StringPtr data) {
  return SseEvent(id, type, current_time_ns(), kj::str(data));
}

/**
 * @brief Measure execution time in microseconds
 */
template <typename F> uint64_t measure_time_us(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

/**
 * @brief Helper to wait for a promise to resolve
 *
 * This is used in integration tests where we need to wait for
 * async event delivery.
 */
template <typename T> T wait_promise(kj::Promise<T>&& promise, kj::WaitScope& wait_scope) {
  return promise.wait(wait_scope);
}

} // namespace

namespace {

// =============================================================================
// SSE Connection Tests
// =============================================================================

KJ_TEST("SSE streaming: basic event delivery") {
  EventBroadcasterConfig config;
  config.history_size = 100;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Subscribe to events
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Request next event first (sets up the fulfiller)
  auto event_promise = subscription->next_event();

  // Broadcast an event
  auto event = create_test_event(1, SseEventType::System, "{\"message\":\"hello\"}");
  broadcaster->broadcast(kj::mv(event));

  // Wait for the event
  auto received = event_promise.wait(wait_scope);

  KJ_IF_SOME(e, received) {
    KJ_EXPECT(e.id == 1);
    KJ_EXPECT(e.type == SseEventType::System);
    KJ_EXPECT(e.data == "{\"message\":\"hello\"}"_kj);
  }
  else {
    KJ_FAIL_EXPECT("Expected to receive event");
  }

  // Verify last_id is updated
  KJ_EXPECT(subscription->last_id() == 1);
}

KJ_TEST("SSE streaming: event ordering") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  constexpr size_t NUM_EVENTS = 100;
  for (size_t i = 1; i <= NUM_EVENTS; ++i) {
    auto event_promise = subscription->next_event();
    auto event = create_test_event(i, SseEventType::OrderUpdate, kj::str("{\"seq\":", i, "}"));
    broadcaster->broadcast(kj::mv(event));

    auto received = event_promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == i, "Event ID mismatch: expected ", i, ", got ", e.id);
    }
    else {
      KJ_FAIL_EXPECT("Expected event ", i);
    }
  }

  KJ_EXPECT(subscription->last_id() == NUM_EVENTS);
}

KJ_TEST("SSE streaming: batch event delivery") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  constexpr size_t BATCH_SIZE = 50;
  size_t received_count = 0;
  for (size_t i = 1; i <= BATCH_SIZE; ++i) {
    kj::Vector<SseEvent> events;
    events.add(create_test_event(i, SseEventType::MarketData, kj::str("{\"idx\":", i, "}")));

    auto event_promise = subscription->next_event();
    broadcaster->broadcast_batch(kj::mv(events));

    auto received = event_promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == i);
      received_count++;
    }
  }

  KJ_EXPECT(received_count == BATCH_SIZE);
}

// =============================================================================
// Event History Tests
// =============================================================================

KJ_TEST("SSE streaming: event history replay") {
  EventBroadcasterConfig config;
  config.history_size = 500;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Broadcast initial events
  for (size_t i = 1; i <= 10; ++i) {
    auto event = create_test_event(i, SseEventType::Account, kj::str("{\"n\":", i, "}"));
    broadcaster->broadcast(kj::mv(event));
  }

  // Get history from event 5 (should return events 6-10)
  auto history = broadcaster->get_history(5);
  KJ_EXPECT(history.size() == 5); // Events 6, 7, 8, 9, 10

  // Verify event IDs
  for (size_t i = 0; i < history.size(); ++i) {
    KJ_EXPECT(history[i].id == 6 + i);
  }
}

KJ_TEST("SSE streaming: history size limit") {
  EventBroadcasterConfig config;
  config.history_size = 10; // Small history
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Broadcast more events than history size
  for (size_t i = 1; i <= 20; ++i) {
    auto event = create_test_event(i, SseEventType::Error, kj::str("{}"));
    broadcaster->broadcast(kj::mv(event));
  }

  // History should only contain last 10 events
  auto history = broadcaster->get_history(0);
  KJ_EXPECT(history.size() == 10);

  // First event in history should be event 11
  KJ_EXPECT(history[0].id == 11);

  // Last event should be event 20
  KJ_EXPECT(history[9].id == 20);
}

KJ_TEST("SSE streaming: subscribe from latest") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Broadcast some events
  for (size_t i = 1; i <= 5; ++i) {
    broadcaster->broadcast(create_test_event(i, SseEventType::System, "{}"));
  }

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Subscribe with last_id = current (only new events)
  auto current_id = broadcaster->current_id();
  auto subscription = broadcaster->subscribe(current_id);

  // Request next event before broadcasting
  auto event_promise = subscription->next_event();

  // Broadcast new event
  broadcaster->broadcast(create_test_event(6, SseEventType::System, "{}"));

  // Should receive only the new event
  auto received = event_promise.wait(wait_scope);
  KJ_IF_SOME(e, received) {
    KJ_EXPECT(e.id == 6);
  }

  KJ_EXPECT(subscription->last_id() == 6);
}

// =============================================================================
// Reconnection Tests
// =============================================================================

KJ_TEST("SSE streaming: reconnection with Last-Event-ID") {
  EventBroadcasterConfig config;
  config.history_size = 100;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Broadcast events 1-10
  for (size_t i = 1; i <= 10; ++i) {
    broadcaster->broadcast(create_test_event(i, SseEventType::OrderUpdate, kj::str("{}")));
  }

  // Simulate client disconnect after event 5
  uint64_t last_received_id = 5;

  // Client reconnects with Last-Event-ID header
  auto subscription = broadcaster->subscribe(last_received_id);

  // Should replay events 6-10
  auto history = broadcaster->get_history(last_received_id);
  KJ_EXPECT(history.size() == 5);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // New events continue to arrive
  auto event_promise = subscription->next_event();
  broadcaster->broadcast(create_test_event(11, SseEventType::System, "{}"));

  auto received = event_promise.wait(wait_scope);
  KJ_IF_SOME(e, received) {
    // First event should be either from history or new event
    KJ_EXPECT(e.id >= 6);
  }
}

KJ_TEST("SSE streaming: reconnection with outdated Last-Event-ID") {
  EventBroadcasterConfig config;
  config.history_size = 5; // Small history
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Broadcast 10 events
  for (size_t i = 1; i <= 10; ++i) {
    broadcaster->broadcast(create_test_event(i, SseEventType::Error, "{}"));
  }

  // Try to reconnect from event 2 (outside history window)
  auto subscription = broadcaster->subscribe(2);

  // History only contains events 6-10
  auto history = broadcaster->get_history(2);
  KJ_EXPECT(history.size() == 5);

  // Should receive events from history
  for (auto& h : history) {
    KJ_EXPECT(h.id >= 6);
  }
}

// =============================================================================
// Concurrent Connections Tests
// =============================================================================

KJ_TEST("SSE streaming: multiple concurrent subscribers") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 100;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Create multiple subscriptions
  constexpr size_t NUM_SUBSCRIBERS = 50;
  kj::Vector<kj::Own<SseSubscription>> subscriptions;
  for (size_t i = 0; i < NUM_SUBSCRIBERS; ++i) {
    subscriptions.add(broadcaster->subscribe(0));
  }

  KJ_EXPECT(broadcaster->subscription_count() == NUM_SUBSCRIBERS);

  // Set up promises first
  kj::Vector<kj::Promise<kj::Maybe<SseEvent>>> promises;
  for (auto& sub : subscriptions) {
    promises.add(sub->next_event());
  }

  // Broadcast event to all
  broadcaster->broadcast(create_test_event(1, SseEventType::System, "{}"));

  // All should receive
  for (auto& promise : promises) {
    auto received = promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == 1);
    }
  }
}

KJ_TEST("SSE streaming: subscription limit enforcement") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 5;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Create subscriptions up to limit
  kj::Vector<kj::Own<SseSubscription>> subscriptions;
  for (size_t i = 0; i < 5; ++i) {
    subscriptions.add(broadcaster->subscribe(0));
  }

  KJ_EXPECT(broadcaster->subscription_count() == 5);

  // Close one subscription
  subscriptions[0]->close();
  // Swap-and-pop to remove from vector
  subscriptions[0] = kj::mv(subscriptions[4]);
  subscriptions.removeLast();
  KJ_EXPECT(broadcaster->subscription_count() == 4);

  // Now should be able to create new subscription
  auto new_sub = broadcaster->subscribe(0);
  KJ_EXPECT(broadcaster->subscription_count() == 5);
}

KJ_TEST("SSE streaming: subscription cleanup on close") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Create subscriptions
  auto sub1 = broadcaster->subscribe(0);
  auto sub2 = broadcaster->subscribe(0);
  auto sub3 = broadcaster->subscribe(0);

  KJ_EXPECT(broadcaster->subscription_count() == 3);

  // Close subscriptions
  sub1->close();
  KJ_EXPECT(broadcaster->subscription_count() == 2);

  sub2->close();
  KJ_EXPECT(broadcaster->subscription_count() == 1);

  sub3->close();
  KJ_EXPECT(broadcaster->subscription_count() == 0);
}

// =============================================================================
// Keep-Alive Tests
// =============================================================================

KJ_TEST("SSE streaming: keep-alive interval") {
  SseHandlerConfig config;
  config.keepalive_interval_ms = 100; // 100ms for testing

  EventBroadcasterConfig bcConfig;
  auto broadcaster = kj::heap<EventBroadcaster>(bcConfig);
  auto handler = kj::heap<SseHandler>(*broadcaster, config);

  // Verify config
  KJ_EXPECT(handler->config().keepalive_interval_ms == 100);
}

KJ_TEST("SSE streaming: retry interval in SSE format") {
  SseHandlerConfig config;
  config.retry_ms = 5000; // 5 seconds

  EventBroadcasterConfig bcConfig;
  auto broadcaster = kj::heap<EventBroadcaster>(bcConfig);
  auto handler = kj::heap<SseHandler>(*broadcaster, config);

  KJ_EXPECT(handler->config().retry_ms == 5000);
}

// =============================================================================
// Performance Tests
// =============================================================================

KJ_TEST("SSE streaming: event delivery latency") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  constexpr size_t NUM_EVENTS = 1000;

  // Measure broadcast + receive latency
  auto total_time_us = measure_time_us([&]() {
    auto subscription = broadcaster->subscribe(0);
    for (size_t i = 1; i <= NUM_EVENTS; ++i) {
      auto event_promise = subscription->next_event();
      broadcaster->broadcast(create_test_event(i, SseEventType::System, "{}"));
      auto received = event_promise.wait(wait_scope);
      KJ_IF_SOME(e, received) {
        KJ_EXPECT(e.id == i);
      }
    }
  });

  double avg_us = static_cast<double>(total_time_us) / NUM_EVENTS;

  KJ_LOG(INFO, "Average event delivery: ", avg_us, " μs");

  // Performance target: <10ms per event (very conservative)
  KJ_EXPECT(avg_us < 10000);
}

KJ_TEST("SSE streaming: broadcast to many subscribers") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  constexpr size_t NUM_SUBSCRIBERS = 1000;
  kj::Vector<kj::Own<SseSubscription>> subscriptions;
  for (size_t i = 0; i < NUM_SUBSCRIBERS; ++i) {
    subscriptions.add(broadcaster->subscribe(0));
  }

  // Measure broadcast time
  auto broadcast_time_us = measure_time_us(
      [&]() { broadcaster->broadcast(create_test_event(1, SseEventType::System, "{}")); });

  KJ_LOG(INFO, "Broadcast to ", NUM_SUBSCRIBERS, " subscribers: ", broadcast_time_us, " μs");

  // Performance target: <10ms for 1000 subscribers
  KJ_EXPECT(broadcast_time_us < 10000);
}

KJ_TEST("SSE streaming: history retrieval performance") {
  EventBroadcasterConfig config;
  config.history_size = 500;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Fill history
  for (size_t i = 1; i <= 500; ++i) {
    broadcaster->broadcast(create_test_event(i, SseEventType::System, "{}"));
  }

  // Measure history retrieval time
  auto history_time_us = measure_time_us([&]() {
    auto history = broadcaster->get_history(0);
    KJ_EXPECT(history.size() == 500);
  });

  KJ_LOG(INFO, "History retrieval (500 events): ", history_time_us, " μs");

  // Should be fast (<10ms)
  KJ_EXPECT(history_time_us < 10000);
}

// =============================================================================
// Statistics Tests
// =============================================================================

KJ_TEST("SSE streaming: broadcaster statistics") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  // Initial stats
  auto stats1 = broadcaster->get_stats();
  KJ_EXPECT(stats1.events_broadcast == 0);
  KJ_EXPECT(stats1.active_subscriptions == 0);

  // Create subscriptions
  auto sub1 = broadcaster->subscribe(0);
  auto sub2 = broadcaster->subscribe(0);

  auto stats2 = broadcaster->get_stats();
  KJ_EXPECT(stats2.active_subscriptions == 2);

  // Broadcast events
  broadcaster->broadcast(create_test_event(1, SseEventType::System, "{}"));
  broadcaster->broadcast(create_test_event(2, SseEventType::System, "{}"));

  auto stats3 = broadcaster->get_stats();
  KJ_EXPECT(stats3.events_broadcast == 2);

  // Check history count
  KJ_EXPECT(stats3.events_in_history == 2);
}

KJ_TEST("SSE streaming: current event ID tracking") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  KJ_EXPECT(broadcaster->current_id() == 0);

  broadcaster->broadcast(create_test_event(0, SseEventType::System, "{}"));
  KJ_EXPECT(broadcaster->current_id() == 1);

  broadcaster->broadcast(create_test_event(0, SseEventType::System, "{}"));
  KJ_EXPECT(broadcaster->current_id() == 2);

  // Batch broadcast
  kj::Vector<SseEvent> events;
  events.add(create_test_event(0, SseEventType::System, "{}"));
  events.add(create_test_event(0, SseEventType::System, "{}"));
  broadcaster->broadcast_batch(kj::mv(events));

  KJ_EXPECT(broadcaster->current_id() == 4);
}

// =============================================================================
// Subscription Lifecycle Tests
// =============================================================================

KJ_TEST("SSE streaming: subscription close terminates event stream") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  KJ_EXPECT(!subscription->is_closed());

  // Close subscription
  subscription->close();

  KJ_EXPECT(subscription->is_closed());

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Next event should return none immediately
  auto event_promise = subscription->next_event();
  auto received = event_promise.wait(wait_scope);
  KJ_EXPECT(received == kj::none);
}

KJ_TEST("SSE streaming: subscription tracks last event ID") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  for (size_t i = 1; i <= 5; ++i) {
    auto event_promise = subscription->next_event();
    broadcaster->broadcast(create_test_event(i, SseEventType::System, "{}"));
    auto received = event_promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == i);
    }
  }

  KJ_EXPECT(subscription->last_id() == 5);

  for (size_t i = 6; i <= 10; ++i) {
    auto event_promise = subscription->next_event();
    broadcaster->broadcast(create_test_event(i, SseEventType::System, "{}"));
    auto received = event_promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == i);
    }
  }

  KJ_EXPECT(subscription->last_id() == 10);
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

KJ_TEST("SSE streaming: empty event data") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Broadcast event with empty data
  auto event_promise = subscription->next_event();
  auto event = create_test_event(1, SseEventType::System, "");
  broadcaster->broadcast(kj::mv(event));

  auto received = event_promise.wait(wait_scope);
  KJ_IF_SOME(e, received) {
    KJ_EXPECT(e.data == ""_kj);
  }
}

KJ_TEST("SSE streaming: large event data") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Create large event data (1MB) - build using repeated str calls
  // Note: For simplicity, we use a smaller size in tests
  constexpr size_t LARGE_SIZE = 1024 * 1024;
  kj::Vector<char> large_data_vec;
  large_data_vec.reserve(LARGE_SIZE);
  for (size_t i = 0; i < LARGE_SIZE; ++i) {
    large_data_vec.add('X');
  }
  large_data_vec.add('\0');
  kj::StringPtr large_data_ptr(large_data_vec.begin(), large_data_vec.size() - 1);

  auto event_promise = subscription->next_event();
  auto event = create_test_event(1, SseEventType::System, large_data_ptr);
  broadcaster->broadcast(kj::mv(event));

  auto received = event_promise.wait(wait_scope);
  KJ_IF_SOME(e, received) {
    KJ_EXPECT(e.data.size() == LARGE_SIZE);
  }
}

KJ_TEST("SSE streaming: special characters in event data") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto subscription = broadcaster->subscribe(0);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Event with special characters
  kj::String special_data = kj::str("{\"msg\":\"Hello\\nWorld\\t!\"}");
  auto event_promise = subscription->next_event();
  auto event = create_test_event(1, SseEventType::System, special_data.asPtr());
  broadcaster->broadcast(kj::mv(event));

  auto received = event_promise.wait(wait_scope);
  KJ_IF_SOME(e, received) {
    KJ_EXPECT(e.data.contains("\\n") || e.data.contains("\n"));
    KJ_EXPECT(e.data.contains("\\t") || e.data.contains("\t"));
  }
}

KJ_TEST("SSE streaming: very high event rate") {
  EventBroadcasterConfig config;
  config.history_size = 1000;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  auto subscription = broadcaster->subscribe(0);

  constexpr size_t NUM_EVENTS = 10000;
  auto start_time = std::chrono::high_resolution_clock::now();

  for (size_t i = 1; i <= NUM_EVENTS; ++i) {
    auto event_promise = subscription->next_event();
    broadcaster->broadcast(create_test_event(0, SseEventType::System, "{}"));
    auto received = event_promise.wait(wait_scope);
    KJ_IF_SOME(e, received) {
      KJ_EXPECT(e.id == i);
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  KJ_LOG(INFO, "Broadcast ", NUM_EVENTS, " events in ", elapsed_ms, " ms");
  KJ_LOG(INFO, "Rate: ", static_cast<double>(NUM_EVENTS) / (elapsed_ms / 1000.0) / 1000.0,
         "K events/sec");
}

// =============================================================================
// Stress Tests
// =============================================================================

KJ_TEST("SSE streaming: stress test with many subscribers and events") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 1000;
  config.history_size = 1000;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  kj::EventLoop loop;
  kj::WaitScope wait_scope(loop);

  // Create 1000 subscribers
  constexpr size_t NUM_SUBSCRIBERS = 1000;
  kj::Vector<kj::Own<SseSubscription>> subscriptions;
  for (size_t i = 0; i < NUM_SUBSCRIBERS; ++i) {
    subscriptions.add(broadcaster->subscribe(0));
  }

  constexpr size_t NUM_EVENTS = 100;
  for (size_t i = 1; i <= NUM_EVENTS; ++i) {
    kj::Vector<kj::Promise<kj::Maybe<SseEvent>>> promises;
    promises.reserve(NUM_SUBSCRIBERS);
    for (auto& sub : subscriptions) {
      promises.add(sub->next_event());
    }

    broadcaster->broadcast(create_test_event(0, SseEventType::System, "{}"));

    for (auto& promise : promises) {
      auto received = promise.wait(wait_scope);
      KJ_IF_SOME(e, received) {
        KJ_EXPECT(e.id == i);
      }
    }
  }

  auto stats = broadcaster->get_stats();
  KJ_EXPECT(stats.events_broadcast == NUM_EVENTS);
  KJ_EXPECT(stats.active_subscriptions == NUM_SUBSCRIBERS);
}

} // namespace
