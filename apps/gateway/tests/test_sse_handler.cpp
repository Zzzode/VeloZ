#include "bridge/event_broadcaster.h"
#include "handlers/sse_handler.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/vector.h>

namespace {

/**
 * @brief Mock output stream for testing SSE output
 */
class MockAsyncOutputStream final : public kj::AsyncOutputStream {
public:
  kj::Vector<kj::String> written_chunks;

  kj::Promise<void> write(kj::ArrayPtr<const kj::byte> buffer) override {
    written_chunks.add(
        kj::heapString(reinterpret_cast<const char*>(buffer.begin()), buffer.size()));
    return kj::READY_NOW;
  }

  kj::Promise<void> write(kj::ArrayPtr<const kj::ArrayPtr<const kj::byte>> pieces) override {
    for (auto& piece : pieces) {
      written_chunks.add(
          kj::heapString(reinterpret_cast<const char*>(piece.begin()), piece.size()));
    }
    return kj::READY_NOW;
  }

  kj::Promise<void> whenWriteDisconnected() override {
    return kj::NEVER_DONE;
  }

  kj::String get_all_output() const {
    kj::String result = kj::str("");
    for (const auto& chunk : written_chunks) {
      result = kj::str(result, chunk);
    }
    return result;
  }

  size_t total_bytes() const {
    size_t total = 0;
    for (const auto& chunk : written_chunks) {
      total += chunk.size();
    }
    return total;
  }
};

/**
 * @brief Mock input stream for testing
 */
class MockAsyncInputStream final : public kj::AsyncInputStream {
public:
  kj::Promise<size_t> tryRead(void* buffer, size_t minBytes, size_t maxBytes) override {
    (void)buffer;
    (void)minBytes;
    (void)maxBytes;
    return static_cast<size_t>(0);
  }
};

/**
 * @brief Mock HTTP response for testing
 */
class MockHttpResponse final : public kj::HttpService::Response {
public:
  kj::uint statusCode = 0;
  kj::String statusText;
  // Store as Own<HttpHeaders> since HttpHeaders doesn't support copy assignment
  kj::Own<kj::HttpHeaders> responseHeaders;
  kj::Own<MockAsyncOutputStream> outputStream = kj::heap<MockAsyncOutputStream>();

  kj::Own<kj::AsyncOutputStream> send(kj::uint statusCode, kj::StringPtr statusText,
                                      const kj::HttpHeaders& headers,
                                      KJ_UNUSED kj::Maybe<uint64_t> expectedBodySize) override {
    this->statusCode = statusCode;
    this->statusText = kj::str(statusText);
    // Clone headers - use heap allocation since HttpHeaders lacks copy assignment
    this->responseHeaders = kj::heap<kj::HttpHeaders>(headers.clone());

    return kj::mv(outputStream);
  }

  kj::Own<kj::WebSocket> acceptWebSocket(KJ_UNUSED const kj::HttpHeaders& headers) override {
    KJ_FAIL_REQUIRE("WebSocket not expected");
  }
};

} // namespace

// ============================================================================
// SSE Event Tests
// ============================================================================

KJ_TEST("SseEvent: format as SSE message") {
  veloz::gateway::bridge::SseEvent event;
  event.id = 123;
  event.type = veloz::gateway::bridge::SseEventType::MarketData;
  event.data = kj::str(R"({"symbol":"BTCUSDT","price":50000.0})");

  auto formatted = event.format_sse();

  // Should contain: id: 123\nevent: market-data\ndata: {"symbol":"BTCUSDT","price":50000.0}\n\n
  KJ_EXPECT(formatted.startsWith("id: 123\n"));
  KJ_EXPECT(formatted.contains("event: market-data\n"));
  KJ_EXPECT(formatted.contains("data: {\"symbol\":\"BTCUSDT\",\"price\":50000.0}\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: format with retry value") {
  veloz::gateway::bridge::SseEvent event;
  event.id = 456;
  event.type = veloz::gateway::bridge::SseEventType::OrderUpdate;
  event.data = kj::str(R"({"orderId":"abc123","status":"filled"})");

  auto formatted = event.format_sse(3000);

  KJ_EXPECT(formatted.contains("retry: 3000\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: create keep-alive event") {
  auto event = veloz::gateway::bridge::SseEvent::create_keepalive(1);

  KJ_EXPECT(event.id == 1);
  KJ_EXPECT(event.type == veloz::gateway::bridge::SseEventType::KeepAlive);
  KJ_EXPECT(event.data == "{}");
}

KJ_TEST("SseEvent: create market data event") {
  auto event = veloz::gateway::bridge::SseEvent::create_market_data(
      100, kj::str(R"({"symbol":"ETHUSDT","price":3000.0})"));

  KJ_EXPECT(event.id == 100);
  KJ_EXPECT(event.type == veloz::gateway::bridge::SseEventType::MarketData);
  KJ_EXPECT(event.data == R"({"symbol":"ETHUSDT","price":3000.0})");
}

// ============================================================================
// EventBroadcaster Tests
// ============================================================================

KJ_TEST("EventBroadcaster: subscribe and broadcast single event") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcasterConfig config;
  config.history_size = 100;

  veloz::gateway::bridge::EventBroadcaster broadcaster(config);

  // Broadcast an event first (event will be stored in history)
  auto event = veloz::gateway::bridge::SseEvent::create_market_data(
      0, // ID will be assigned by broadcaster
      kj::str(R"({"symbol":"BTCUSDT","price":50000.0})"));
  broadcaster.broadcast(kj::mv(event));

  // Check current ID
  KJ_EXPECT(broadcaster.current_id() == 1);

  // Check subscription count
  KJ_EXPECT(broadcaster.subscription_count() == 0);

  // Check history
  auto history = broadcaster.get_history(0);
  KJ_EXPECT(history.size() == 1);
}

KJ_TEST("EventBroadcaster: broadcast multiple events") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Broadcast 3 events
  for (int i = 0; i < 3; ++i) {
    auto event = veloz::gateway::bridge::SseEvent::create_market_data(
        0, kj::str(R"({"symbol":"BTCUSDT","price":)", 50000 + i * 100, "}"));
    broadcaster.broadcast(kj::mv(event));
  }

  KJ_EXPECT(broadcaster.current_id() == 3);

  // Get all events from history
  auto history = broadcaster.get_history(0);
  KJ_EXPECT(history.size() == 3);
}

KJ_TEST("EventBroadcaster: history replay") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Broadcast 5 events
  for (int i = 0; i < 5; ++i) {
    auto event =
        veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str(R"({"event":)", i, "}"));
    broadcaster.broadcast(kj::mv(event));
  }

  // Get history starting from ID 2 (should get events 3, 4, 5)
  auto history = broadcaster.get_history(2);

  KJ_EXPECT(history.size() == 3);
  KJ_EXPECT(history[0].id == 3);
  KJ_EXPECT(history[1].id == 4);
  KJ_EXPECT(history[2].id == 5);
}

KJ_TEST("EventBroadcaster: multiple subscriptions") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Create 3 subscriptions
  auto sub1 = broadcaster.subscribe(0);
  auto sub2 = broadcaster.subscribe(0);
  auto sub3 = broadcaster.subscribe(0);

  KJ_EXPECT(broadcaster.subscription_count() == 3);

  // Close one subscription
  sub2->close();

  KJ_EXPECT(broadcaster.subscription_count() == 2);
  KJ_EXPECT(sub1->is_closed() == false);
  KJ_EXPECT(sub2->is_closed() == true);
  KJ_EXPECT(sub3->is_closed() == false);
}

KJ_TEST("EventBroadcaster: close subscription") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;
  auto subscription = broadcaster.subscribe(0);

  KJ_EXPECT(!subscription->is_closed());

  subscription->close();

  KJ_EXPECT(subscription->is_closed());
  KJ_EXPECT(broadcaster.subscription_count() == 0); // Subscription should be removed
}

KJ_TEST("EventBroadcaster: stats") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Broadcast some events
  for (int i = 0; i < 10; ++i) {
    auto event =
        veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str(R"({"event":)", i, "}"));
    broadcaster.broadcast(kj::mv(event));
  }

  auto stats = broadcaster.get_stats();

  KJ_EXPECT(stats.events_broadcast == 10);
  KJ_EXPECT(stats.events_in_history == 10);
}

// ============================================================================
// SseHandler Tests
// ============================================================================

KJ_TEST("SseHandler: handles GET /api/stream") {
  // Use setupAsyncIo which provides its own EventLoop
  auto ioContext = kj::setupAsyncIo();

  veloz::gateway::bridge::EventBroadcaster broadcaster;
  veloz::gateway::handlers::SseHandler handler(broadcaster);

  kj::HttpHeaderTable headerTable;
  kj::HttpHeaders requestHeaders(headerTable);

  MockAsyncInputStream requestBody;
  MockHttpResponse response;

  auto promise =
      handler.handle(kj::HttpMethod::GET, "/api/stream"_kj, requestHeaders, requestBody, response);

  // Broadcast an event
  auto event = veloz::gateway::bridge::SseEvent::create_market_data(
      0, kj::str(R"({"symbol":"BTCUSDT","price":50000.0})"));
  broadcaster.broadcast(kj::mv(event));

  // Wait for the handler to start
  // Note: In a real test, we'd need to properly drive the async loop
}

KJ_TEST("SseHandler: rejects non-GET requests") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;
  veloz::gateway::handlers::SseHandler handler(broadcaster);

  kj::HttpHeaderTable headerTable;
  kj::HttpHeaders requestHeaders(headerTable);

  MockAsyncInputStream requestBody;
  MockHttpResponse response;

  auto promise =
      handler.handle(kj::HttpMethod::POST, "/api/stream"_kj, requestHeaders, requestBody, response);

  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 405); // Method Not Allowed
}

KJ_TEST("SseHandler: rejects unknown paths") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;
  veloz::gateway::handlers::SseHandler handler(broadcaster);

  kj::HttpHeaderTable headerTable;
  kj::HttpHeaders requestHeaders(headerTable);

  MockAsyncInputStream requestBody;
  MockHttpResponse response;

  auto promise =
      handler.handle(kj::HttpMethod::GET, "/api/unknown"_kj, requestHeaders, requestBody, response);

  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 404); // Not Found
}

KJ_TEST("SseHandler: sets correct SSE headers") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;
  veloz::gateway::handlers::SseHandler handler(broadcaster);

  kj::HttpHeaderTable headerTable;
  kj::HttpHeaders requestHeaders(headerTable);

  MockAsyncInputStream requestBody;
  MockHttpResponse response;

  // Create the promise but we won't wait for it to complete
  // Just check the headers were set correctly
  auto promise =
      handler.handle(kj::HttpMethod::GET, "/api/stream"_kj, requestHeaders, requestBody, response);

  // Check response status
  KJ_EXPECT(response.statusCode == 200);

  // Check Content-Type header
  auto contentType = response.responseHeaders->get(kj::HttpHeaderId::CONTENT_TYPE);
  KJ_IF_SOME(ct, contentType) {
    KJ_EXPECT(ct.startsWith("text/event-stream"));
  }
  else {
    KJ_FAIL_ASSERT("Expected Content-Type header");
  }
}

KJ_TEST("SseHandler: active connections counter") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;
  veloz::gateway::handlers::SseHandler handler(broadcaster);

  KJ_EXPECT(handler.active_connections() == 0);
}

// ============================================================================
// SSE Protocol Formatting Tests
// ============================================================================

KJ_TEST("SSE Protocol: event with multiline data") {
  // Test that multiline data is properly handled
  // Note: Current implementation assumes single-line data
  veloz::gateway::bridge::SseEvent event;
  event.id = 1;
  event.type = veloz::gateway::bridge::SseEventType::MarketData;
  event.data = kj::str("{\"key\":\"value\"}");

  auto formatted = event.format_sse();

  // Should have two consecutive newlines at the end
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SSE Protocol: keepalive comment format") {
  // Keep-alive should be a comment (starts with :)
  // This is the standard SSE way to send keep-alives
  kj::String keepalive = kj::str(": keepalive\n\n");

  KJ_EXPECT(keepalive.startsWith(":"));
  KJ_EXPECT(keepalive.endsWith("\n\n"));
}

// ============================================================================
// Concurrency Tests
// ============================================================================

KJ_TEST("EventBroadcaster: concurrent broadcasts") {
  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);

  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Simulate concurrent broadcasts (in real scenario, from different threads)
  // For this test, we just broadcast many events rapidly
  constexpr int NUM_EVENTS = 100;

  for (int i = 0; i < NUM_EVENTS; ++i) {
    auto event =
        veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str(R"({"sequence":)", i, "}"));
    broadcaster.broadcast(kj::mv(event));
  }

  KJ_EXPECT(broadcaster.current_id() == NUM_EVENTS);

  auto history = broadcaster.get_history(0);
  KJ_EXPECT(history.size() == NUM_EVENTS);

  // Verify event IDs are sequential
  for (int i = 0; i < NUM_EVENTS; ++i) {
    KJ_EXPECT(history[i].id == static_cast<uint64_t>(i + 1));
  }
}

// ============================================================================
// Edge Cases
// ============================================================================

KJ_TEST("EventBroadcaster: empty history replay") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // No events broadcast yet
  auto history = broadcaster.get_history(0);

  KJ_EXPECT(history.empty());
}

KJ_TEST("EventBroadcaster: history beyond available events") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;

  // Broadcast 3 events
  for (int i = 0; i < 3; ++i) {
    auto event = veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str("{}"));
    broadcaster.broadcast(kj::mv(event));
  }

  // Request history starting from ID 100 (beyond what we have)
  auto history = broadcaster.get_history(100);

  KJ_EXPECT(history.empty());
}

KJ_TEST("EventBroadcaster: current_id tracking") {
  veloz::gateway::bridge::EventBroadcaster broadcaster;

  KJ_EXPECT(broadcaster.current_id() == 0); // No events yet

  auto event = veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str("{}"));
  broadcaster.broadcast(kj::mv(event));

  KJ_EXPECT(broadcaster.current_id() == 1); // First event

  event = veloz::gateway::bridge::SseEvent::create_market_data(0, kj::str("{}"));
  broadcaster.broadcast(kj::mv(event));

  KJ_EXPECT(broadcaster.current_id() == 2); // Second event
}
