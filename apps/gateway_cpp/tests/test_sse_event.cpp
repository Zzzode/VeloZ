#include "bridge/event.h"

#include <kj/test.h>

namespace veloz::gateway::bridge {

// ============================================================================
// SseEventType to_string() Tests
// ============================================================================

KJ_TEST("SseEventType: MarketData converts to string") {
  auto str = to_string(SseEventType::MarketData);
  KJ_EXPECT(str == "market-data"_kj);
}

KJ_TEST("SseEventType: OrderUpdate converts to string") {
  auto str = to_string(SseEventType::OrderUpdate);
  KJ_EXPECT(str == "order-update"_kj);
}

KJ_TEST("SseEventType: Account converts to string") {
  auto str = to_string(SseEventType::Account);
  KJ_EXPECT(str == "account"_kj);
}

KJ_TEST("SseEventType: System converts to string") {
  auto str = to_string(SseEventType::System);
  KJ_EXPECT(str == "system"_kj);
}

KJ_TEST("SseEventType: Error converts to string") {
  auto str = to_string(SseEventType::Error);
  KJ_EXPECT(str == "error"_kj);
}

KJ_TEST("SseEventType: KeepAlive converts to string") {
  auto str = to_string(SseEventType::KeepAlive);
  KJ_EXPECT(str == "keepalive"_kj);
}

KJ_TEST("SseEventType: Unknown converts to string") {
  auto str = to_string(SseEventType::Unknown);
  KJ_EXPECT(str == "unknown"_kj);
}

// ============================================================================
// SseEvent::format_sse() Tests
// ============================================================================

KJ_TEST("SseEvent: formats basic event correctly") {
  SseEvent event;
  event.id = 123;
  event.type = SseEventType::MarketData;
  event.timestamp_ns = 1234567890000000;
  event.data = kj::str(R"({"price":50000.0})");

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 123\n"));
  KJ_EXPECT(formatted.contains("event: market-data\n"));
  KJ_EXPECT(formatted.contains("data: {\"price\":50000.0}\n"));
  KJ_EXPECT(formatted.endsWith("\n\n")); // SSE ends with double newline
}

KJ_TEST("SseEvent: formats event with empty data") {
  SseEvent event;
  event.id = 1;
  event.type = SseEventType::KeepAlive;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted == "id: 1\nevent: keepalive\ndata: {}\n\n");
}

KJ_TEST("SseEvent: formats event with complex JSON data") {
  auto json_data =
      R"({"symbol":"BTCUSDT","price":50000.0,"quantity":1.5,"timestamp":1234567890})"_kj;
  SseEvent event;
  event.id = 999;
  event.type = SseEventType::OrderUpdate;
  event.timestamp_ns = 0;
  event.data = kj::str(json_data);

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 999\n"));
  KJ_EXPECT(formatted.contains("event: order-update\n"));
  KJ_EXPECT(formatted.contains("data: "));
  KJ_EXPECT(formatted.contains(json_data));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: formats event with ID 0") {
  SseEvent event;
  event.id = 0;
  event.type = SseEventType::System;
  event.timestamp_ns = 0;
  event.data = kj::str(R"({"status":"starting"})");

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 0\n"));
  KJ_EXPECT(formatted.contains("event: system\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: formats event with large ID") {
  SseEvent event;
  event.id = 18446744073709551615ULL; // Max uint64_t
  event.type = SseEventType::Error;
  event.timestamp_ns = 0;
  event.data = kj::str(R"({"message":"test"})");

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("id: 18446744073709551615\n"));
}

KJ_TEST("SseEvent: formats event with unicode characters in data") {
  auto json_data = R"({"message":"こんにちは世界"})"_kj;
  SseEvent event;
  event.id = 100;
  event.type = SseEventType::System;
  event.timestamp_ns = 0;
  event.data = kj::str(json_data);

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains(json_data));
}

KJ_TEST("SseEvent: formats event with escaped characters in data") {
  auto json_data = R"({"message":"Line 1\nLine 2\tTabbed"})"_kj;
  SseEvent event;
  event.id = 50;
  event.type = SseEventType::Error;
  event.timestamp_ns = 0;
  event.data = kj::str(json_data);

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("data: "));
  KJ_EXPECT(formatted.contains(json_data));
}

// ============================================================================
// SseEvent::format_sse() with retry Tests
// ============================================================================

KJ_TEST("SseEvent: formats event with retry value") {
  SseEvent event;
  event.id = 42;
  event.type = SseEventType::MarketData;
  event.timestamp_ns = 0;
  event.data = kj::str(R"({"test":true})");

  auto formatted = event.format_sse(5000);

  KJ_EXPECT(formatted.startsWith("id: 42\n"));
  KJ_EXPECT(formatted.contains("event: market-data\n"));
  KJ_EXPECT(formatted.contains("data: {\"test\":true}\n"));
  KJ_EXPECT(formatted.contains("retry: 5000\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: formats event with retry value of 0") {
  SseEvent event;
  event.id = 1;
  event.type = SseEventType::KeepAlive;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  auto formatted = event.format_sse(0);

  KJ_EXPECT(formatted.contains("retry: 0\n"));
}

KJ_TEST("SseEvent: formats event with large retry value") {
  SseEvent event;
  event.id = 1;
  event.type = SseEventType::MarketData;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  auto formatted = event.format_sse(60000); // 1 minute

  KJ_EXPECT(formatted.contains("retry: 60000\n"));
}

KJ_TEST("SseEvent: ensures retry is on its own line") {
  SseEvent event;
  event.id = 123;
  event.type = SseEventType::OrderUpdate;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  auto formatted = event.format_sse(3000);

  // Check that retry appears correctly formatted
  KJ_EXPECT(formatted.contains("\nretry: 3000\n"));
}

// ============================================================================
// SseEvent::create_keepalive() Tests
// ============================================================================

KJ_TEST("SseEvent: create_keepalive with ID 1") {
  auto event = SseEvent::create_keepalive(1);

  KJ_EXPECT(event.id == 1);
  KJ_EXPECT(event.type == SseEventType::KeepAlive);
  KJ_EXPECT(event.timestamp_ns == 0);
  KJ_EXPECT(event.data == "{}");
}

KJ_TEST("SseEvent: create_keepalive with large ID") {
  auto event = SseEvent::create_keepalive(999999);

  KJ_EXPECT(event.id == 999999);
  KJ_EXPECT(event.type == SseEventType::KeepAlive);
  KJ_EXPECT(event.timestamp_ns == 0);
  KJ_EXPECT(event.data == "{}");
}

KJ_TEST("SseEvent: create_keepalive formats correctly") {
  auto event = SseEvent::create_keepalive(42);
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted == "id: 42\nevent: keepalive\ndata: {}\n\n");
}

KJ_TEST("SseEvent: create_keepalive with ID 0") {
  auto event = SseEvent::create_keepalive(0);

  KJ_EXPECT(event.id == 0);
  KJ_EXPECT(event.type == SseEventType::KeepAlive);
  KJ_EXPECT(event.timestamp_ns == 0);
  KJ_EXPECT(event.data == "{}");
}

// ============================================================================
// SseEvent::create_market_data() Tests
// ============================================================================

KJ_TEST("SseEvent: create_market_data with basic data") {
  auto json_data = R"({"symbol":"BTCUSDT","price":50000.0})"_kj;
  auto event = SseEvent::create_market_data(1, kj::str(json_data));

  KJ_EXPECT(event.id == 1);
  KJ_EXPECT(event.type == SseEventType::MarketData);
  KJ_EXPECT(event.data == json_data);
  KJ_EXPECT(event.timestamp_ns == 0); // Set by broadcaster
}

KJ_TEST("SseEvent: create_market_data with complex JSON") {
  auto json_data = R"({
    "symbol":"ETHUSDT",
    "price":3000.0,
    "bid":2999.5,
    "ask":3000.5,
    "volume":1000.5,
    "timestamp":1234567890
  })"_kj;
  auto event = SseEvent::create_market_data(100, kj::str(json_data));

  KJ_EXPECT(event.id == 100);
  KJ_EXPECT(event.type == SseEventType::MarketData);
  KJ_EXPECT(event.data == json_data);
}

KJ_TEST("SseEvent: create_market_data with empty JSON object") {
  auto event = SseEvent::create_market_data(42, kj::str("{}"));

  KJ_EXPECT(event.id == 42);
  KJ_EXPECT(event.type == SseEventType::MarketData);
  KJ_EXPECT(event.data == "{}");
}

KJ_TEST("SseEvent: create_market_data formats correctly") {
  auto json_data = R"({"price":100.0})"_kj;
  auto event = SseEvent::create_market_data(50, kj::str(json_data));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted == "id: 50\nevent: market-data\ndata: {\"price\":100.0}\n\n");
}

KJ_TEST("SseEvent: create_market_data takes ownership of data") {
  auto data = kj::str(R"({"test":"value"})");
  auto event = SseEvent::create_market_data(1, kj::mv(data));

  KJ_EXPECT(event.data == R"({"test":"value"})");
  // data should have been moved (implementation-specific check)
}

// ============================================================================
// SseEvent::create_order_update() Tests
// ============================================================================

KJ_TEST("SseEvent: create_order_update with basic data") {
  auto json_data = R"({"order_id":"123","status":"filled"})"_kj;
  auto event = SseEvent::create_order_update(1, kj::str(json_data));

  KJ_EXPECT(event.id == 1);
  KJ_EXPECT(event.type == SseEventType::OrderUpdate);
  KJ_EXPECT(event.data == json_data);
  KJ_EXPECT(event.timestamp_ns == 0); // Set by broadcaster
}

KJ_TEST("SseEvent: create_order_update with complex order data") {
  auto json_data = R"({
    "order_id":"456",
    "client_order_id":"client123",
    "symbol":"BTCUSDT",
    "side":"buy",
    "type":"limit",
    "quantity":1.5,
    "price":50000.0,
    "filled_quantity":1.5,
    "status":"filled",
    "avg_price":50000.0,
    "timestamp":1234567890000000
  })"_kj;
  auto event = SseEvent::create_order_update(200, kj::str(json_data));

  KJ_EXPECT(event.id == 200);
  KJ_EXPECT(event.type == SseEventType::OrderUpdate);
  KJ_EXPECT(event.data == json_data);
}

KJ_TEST("SseEvent: create_order_update for rejected order") {
  auto json_data = R"({"order_id":"789","status":"rejected","reason":"Insufficient funds"})"_kj;
  auto event = SseEvent::create_order_update(300, kj::str(json_data));

  KJ_EXPECT(event.id == 300);
  KJ_EXPECT(event.type == SseEventType::OrderUpdate);
  KJ_EXPECT(event.data == json_data);
}

KJ_TEST("SseEvent: create_order_update formats correctly") {
  auto json_data = R"({"status":"filled"})"_kj;
  auto event = SseEvent::create_order_update(75, kj::str(json_data));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted == "id: 75\nevent: order-update\ndata: {\"status\":\"filled\"}\n\n");
}

// ============================================================================
// SseEvent::create_error() Tests
// ============================================================================

KJ_TEST("SseEvent: create_error with basic message") {
  auto json_data = R"({"message":"Connection lost"})"_kj;
  auto event = SseEvent::create_error(1, kj::str(json_data));

  KJ_EXPECT(event.id == 1);
  KJ_EXPECT(event.type == SseEventType::Error);
  KJ_EXPECT(event.data == json_data);
  KJ_EXPECT(event.timestamp_ns == 0); // Set by broadcaster
}

KJ_TEST("SseEvent: create_error with error code") {
  auto json_data = R"({"code":5001,"message":"Order not found"})"_kj;
  auto event = SseEvent::create_error(150, kj::str(json_data));

  KJ_EXPECT(event.id == 150);
  KJ_EXPECT(event.type == SseEventType::Error);
  KJ_EXPECT(event.data == json_data);
}

KJ_TEST("SseEvent: create_error with system error") {
  auto json_data = R"({
    "code":5000,
    "message":"Internal server error",
    "details":"Database connection failed"
  })"_kj;
  auto event = SseEvent::create_error(500, kj::str(json_data));

  KJ_EXPECT(event.id == 500);
  KJ_EXPECT(event.type == SseEventType::Error);
  KJ_EXPECT(event.data == json_data);
}

KJ_TEST("SseEvent: create_error formats correctly") {
  auto json_data = R"({"message":"test error"})"_kj;
  auto event = SseEvent::create_error(25, kj::str(json_data));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted == "id: 25\nevent: error\ndata: {\"message\":\"test error\"}\n\n");
}

// ============================================================================
// SseEvent Construction Tests
// ============================================================================

KJ_TEST("SseEvent: default constructor initializes to safe values") {
  SseEvent event;

  KJ_EXPECT(event.id == 0);
  KJ_EXPECT(event.type == SseEventType::Unknown);
  KJ_EXPECT(event.timestamp_ns == 0);
  KJ_EXPECT(event.data == ""_kj);
}

KJ_TEST("SseEvent: constructor with all parameters") {
  auto data = kj::str(R"({"test":true})");
  SseEvent event(123, SseEventType::MarketData, 9876543210000000, kj::mv(data));

  KJ_EXPECT(event.id == 123);
  KJ_EXPECT(event.type == SseEventType::MarketData);
  KJ_EXPECT(event.timestamp_ns == 9876543210000000);
  KJ_EXPECT(event.data == R"({"test":true})");
}

// ============================================================================
// SseEvent Move Semantics Tests
// ============================================================================

KJ_TEST("SseEvent: move constructor transfers ownership") {
  auto original = SseEvent::create_market_data(10, kj::str(R"({"price":100})"));
  auto moved = kj::mv(original);

  KJ_EXPECT(moved.id == 10);
  KJ_EXPECT(moved.type == SseEventType::MarketData);
  KJ_EXPECT(moved.data == R"({"price":100})");
}

KJ_TEST("SseEvent: move assignment transfers ownership") {
  auto first = SseEvent::create_market_data(10, kj::str(R"({"price":100})"));
  auto second = SseEvent::create_order_update(20, kj::str(R"({"status":"filled"})"));

  second = kj::mv(first);

  KJ_EXPECT(second.id == 10);
  KJ_EXPECT(second.type == SseEventType::MarketData);
  KJ_EXPECT(second.data == R"({"price":100})");
}

// ============================================================================
// SseEvent Data Formatting Tests
// ============================================================================

KJ_TEST("SseEvent: handles data with multiple lines (single SSE line)") {
  // Note: Current implementation assumes data is a single line
  // Multi-line data would require prefixing each line with "data: "
  auto json_data = R"({"line":"value"})"_kj;
  auto event = SseEvent(1, SseEventType::System, 0, kj::str(json_data));

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("data: "));
  KJ_EXPECT(formatted.contains(json_data));
}

KJ_TEST("SseEvent: handles very large JSON data") {
  kj::Vector<char> large_json;
  // Use pointer-based approach for string literals
  auto prefix = R"({"data":")"_kj;
  for (char c : prefix) {
    large_json.add(c);
  }
  for (int i = 0; i < 10000; ++i) {
    large_json.add('a');
  }
  auto suffix = R"("})"_kj;
  for (char c : suffix) {
    large_json.add(c);
  }

  auto event = SseEvent(1, SseEventType::MarketData, 0, kj::str(large_json));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 1\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: handles JSON with special characters") {
  auto json_data = R"({"text":"Hello \"quoted\" and 'single' and \t tabs"})"_kj;
  auto event = SseEvent(1, SseEventType::Error, 0, kj::str(json_data));

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("data: "));
}

KJ_TEST("SseEvent: handles JSON with Unicode emoji") {
  auto json_data = R"({"message":"Hello 👋 World 🌍"})"_kj;
  auto event = SseEvent(1, SseEventType::System, 0, kj::str(json_data));

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("data: "));
}

// ============================================================================
// SseEvent SSE Protocol Compliance Tests
// ============================================================================

KJ_TEST("SseEvent: SSE format has id line") {
  auto event = SseEvent::create_keepalive(42);
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 42\n"));
}

KJ_TEST("SseEvent: SSE format has event type line") {
  auto event = SseEvent::create_market_data(1, kj::str("{}"));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("\nevent: market-data\n"));
}

KJ_TEST("SseEvent: SSE format has data line") {
  auto event = SseEvent::create_error(1, kj::str(R"({"msg":"err"})"));
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.contains("\ndata: "));
}

KJ_TEST("SseEvent: SSE format ends with double newline") {
  auto event = SseEvent::create_keepalive(1);
  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.endsWith("\n\n"));
}

KJ_TEST("SseEvent: SSE format with retry includes retry field") {
  auto event = SseEvent::create_market_data(1, kj::str("{}"));
  auto formatted = event.format_sse(1000);

  KJ_EXPECT(formatted.contains("\nretry: 1000\n"));
  KJ_EXPECT(formatted.endsWith("\n\n"));
}

// ============================================================================
// SseEvent Type Consistency Tests
// ============================================================================

KJ_TEST("SseEvent: event type in formatted string matches enum value") {
  SseEvent event;
  event.id = 1;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  // Test all event types
  event.type = SseEventType::MarketData;
  auto fmt1 = event.format_sse();
  KJ_EXPECT(fmt1.contains("event: market-data\n"));

  event.type = SseEventType::OrderUpdate;
  auto fmt2 = event.format_sse();
  KJ_EXPECT(fmt2.contains("event: order-update\n"));

  event.type = SseEventType::Account;
  auto fmt3 = event.format_sse();
  KJ_EXPECT(fmt3.contains("event: account\n"));

  event.type = SseEventType::System;
  auto fmt4 = event.format_sse();
  KJ_EXPECT(fmt4.contains("event: system\n"));

  event.type = SseEventType::Error;
  auto fmt5 = event.format_sse();
  KJ_EXPECT(fmt5.contains("event: error\n"));

  event.type = SseEventType::KeepAlive;
  auto fmt6 = event.format_sse();
  KJ_EXPECT(fmt6.contains("event: keepalive\n"));

  event.type = SseEventType::Unknown;
  auto fmt7 = event.format_sse();
  KJ_EXPECT(fmt7.contains("event: unknown\n"));
}

// ============================================================================
// SseEvent ID Tests
// ============================================================================

KJ_TEST("SseEvent: sequential IDs produce formatted strings in order") {
  auto event1 = SseEvent::create_market_data(1, kj::str("{}"));
  auto event2 = SseEvent::create_market_data(2, kj::str("{}"));
  auto event3 = SseEvent::create_market_data(3, kj::str("{}"));

  auto fmt1 = event1.format_sse();
  auto fmt2 = event2.format_sse();
  auto fmt3 = event3.format_sse();

  KJ_EXPECT(fmt1.startsWith("id: 1\n"));
  KJ_EXPECT(fmt2.startsWith("id: 2\n"));
  KJ_EXPECT(fmt3.startsWith("id: 3\n"));
}

KJ_TEST("SseEvent: large ID formats correctly") {
  SseEvent event;
  event.id = 9999999999999;
  event.type = SseEventType::MarketData;
  event.timestamp_ns = 0;
  event.data = kj::str("{}");

  auto formatted = event.format_sse();

  KJ_EXPECT(formatted.startsWith("id: 9999999999999\n"));
}

} // namespace veloz::gateway::bridge
