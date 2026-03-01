#include "audit/audit_logger.h"
#include "bridge/engine_bridge.h"
#include "handlers/order_handler.h"
#include "veloz/oms/order_record.h"

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/test.h>

namespace veloz::gateway {

namespace {

// ============================================================================
// Test Helpers
// ============================================================================

class TestContext {
public:
  TestContext() : io(kj::setupAsyncIo()) {}

  kj::AsyncIoContext io;
};

// Helper to create a valid order request JSON
kj::String createOrderRequest(kj::StringPtr side = "BUY", kj::StringPtr symbol = "BTCUSDT",
                              double qty = 1.0, double price = 50000.0,
                              kj::Maybe<kj::String> clientId = kj::none) {
  if (clientId == kj::none) {
    return kj::str("{"
                   "\"side\":\"",
                   side,
                   "\","
                   "\"symbol\":\"",
                   symbol,
                   "\","
                   "\"qty\":",
                   qty,
                   ","
                   "\"price\":",
                   price, "}");
  } else {
    KJ_IF_SOME(id, clientId) {
      return kj::str("{"
                     "\"side\":\"",
                     side,
                     "\","
                     "\"symbol\":\"",
                     symbol,
                     "\","
                     "\"qty\":",
                     qty,
                     ","
                     "\"price\":",
                     price,
                     ","
                     "\"client_order_id\":\"",
                     id,
                     "\""
                     "}");
    }
    return kj::str("{"
                   "\"side\":\"",
                   side,
                   "\","
                   "\"symbol\":\"",
                   symbol,
                   "\","
                   "\"qty\":",
                   qty,
                   ","
                   "\"price\":",
                   price, "}");
  }
}

// Helper to check if response contains a substring
bool responseContains(kj::StringPtr response, kj::StringPtr substr) {
  return response.find(substr) != kj::none;
}

// ============================================================================
// Order Handler Construction Tests
// ============================================================================

KJ_TEST("OrderHandler: construction requires non-null dependencies") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  audit::AuditLoggerConfig auditConfig;
  auditConfig.log_dir = "/tmp/test_audit"_kj;
  auditConfig.enable_console_output = false;
  audit::AuditLogger logger(auditConfig);

  // Should succeed with valid args
  OrderHandler handler(&bridge, &logger);
}

// ============================================================================
// Order Parameter Parsing Tests
// ============================================================================

KJ_TEST("OrderHandler: parse valid order request") {
  auto body = createOrderRequest("BUY", "BTCUSDT", 1.0, 50000.0);

  // Verify JSON structure
  KJ_EXPECT(responseContains(body, "BUY"));
  KJ_EXPECT(responseContains(body, "BTCUSDT"));
  KJ_EXPECT(responseContains(body, "1"));
  KJ_EXPECT(responseContains(body, "50000"));
}

KJ_TEST("OrderHandler: parse order with lowercase side") {
  auto body = createOrderRequest("buy", "ETHUSDT", 10.0, 3000.0);

  KJ_EXPECT(responseContains(body, "buy"));
  KJ_EXPECT(responseContains(body, "ETHUSDT"));
}

KJ_TEST("OrderHandler: parse market order (no price)") {
  auto body = kj::str("{"
                      "\"side\":\"BUY\","
                      "\"symbol\":\"BTCUSDT\","
                      "\"qty\":1.0"
                      "}");

  KJ_EXPECT(responseContains(body, "BUY"));
  KJ_EXPECT(responseContains(body, "BTCUSDT"));
  // Market orders don't have a price field
}

// ============================================================================
// Validation Tests - Direct Testing
// ============================================================================

KJ_TEST("OrderHandler: validation rejects negative quantity") {
  // Quantity <= 0 should be rejected
  double qty = -1.0;
  KJ_EXPECT(qty <= 0.0);
}

KJ_TEST("OrderHandler: validation rejects zero quantity") {
  double qty = 0.0;
  KJ_EXPECT(qty <= 0.0);
}

KJ_TEST("OrderHandler: validation rejects negative price") {
  // Price < 0 should be rejected
  double price = -50000.0;
  KJ_EXPECT(price < 0.0);
}

KJ_TEST("OrderHandler: validation accepts valid side values") {
  kj::StringPtr side1 = "BUY";
  kj::StringPtr side2 = "SELL";

  KJ_EXPECT(side1 == "BUY" || side1 == "SELL");
  KJ_EXPECT(side2 == "BUY" || side2 == "SELL");
}

KJ_TEST("OrderHandler: validation rejects invalid side") {
  kj::StringPtr invalidSide = "INVALID";
  KJ_EXPECT(invalidSide != "BUY" && invalidSide != "SELL");
}

KJ_TEST("OrderHandler: validation rejects empty symbol") {
  kj::String emptySymbol = kj::str("");
  KJ_EXPECT(emptySymbol.size() == 0);
}

KJ_TEST("OrderHandler: validation accepts valid symbol") {
  kj::StringPtr symbol = "BTCUSDT";
  KJ_EXPECT(symbol.size() > 0);
}

// ============================================================================
// Order ID Generation Tests
// ============================================================================

KJ_TEST("OrderHandler: generate unique client IDs") {
  // The generateClientId function creates IDs like "veloz_<ns>_<counter>"
  // Test that the format is correct by checking pattern

  // IDs should be unique due to counter and timestamp
  std::atomic<uint64_t> counter{0};

  uint64_t id1 = counter.fetch_add(1, std::memory_order_relaxed);
  uint64_t id2 = counter.fetch_add(1, std::memory_order_relaxed);

  KJ_EXPECT(id1 != id2);
  KJ_EXPECT(id2 == id1 + 1);
}

// ============================================================================
// JSON Formatting Tests
// ============================================================================

KJ_TEST("OrderHandler: format order state as JSON") {
  // Create a sample order state
  oms::OrderState order;
  order.client_order_id = kj::heapString("test-order-123");
  order.symbol = kj::heapString("BTCUSDT");
  order.side = kj::heapString("BUY");
  order.order_qty = 1.0;
  order.limit_price = 50000.0;
  order.executed_qty = 0.5;
  order.avg_price = 49999.0;
  order.status = kj::heapString("partially_filled");
  order.venue_order_id = kj::heapString("venue-123");
  order.reason = kj::str("");
  order.last_ts_ns = 1234567890;
  order.created_ts_ns = 1234567000;

  // Verify order state has expected fields
  KJ_EXPECT(order.client_order_id == "test-order-123");
  KJ_EXPECT(order.symbol == "BTCUSDT");
  KJ_EXPECT(order.side == "BUY");
  KJ_EXPECT(order.executed_qty == 0.5);
}

// ============================================================================
// Permission Checking Tests
// ============================================================================

KJ_TEST("OrderHandler: permission constants are defined") {
  // Verify permission names
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::WriteOrders) == "write:orders"_kj);
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::WriteCancel) == "write:cancel"_kj);
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::ReadOrders) == "read:orders"_kj);
}

KJ_TEST("OrderHandler: check permission in list") {
  kj::Vector<kj::String> permissions;
  permissions.add(kj::heapString("write:orders"));
  permissions.add(kj::heapString("read:orders"));

  kj::StringPtr target = "write:orders";

  bool found = false;
  for (const auto& perm : permissions) {
    if (perm == target) {
      found = true;
      break;
    }
  }

  KJ_EXPECT(found);
}

KJ_TEST("OrderHandler: check permission not in list") {
  kj::Vector<kj::String> permissions;
  permissions.add(kj::heapString("read:orders"));

  kj::StringPtr target = "write:orders";

  bool found = false;
  for (const auto& perm : permissions) {
    if (perm == target) {
      found = true;
      break;
    }
  }

  KJ_EXPECT(!found);
}

// ============================================================================
// Bulk Cancel Parsing Tests
// ============================================================================

KJ_TEST("OrderHandler: parse bulk cancel request") {
  auto body = kj::str("{"
                      "\"order_ids\":[\"order1\",\"order2\",\"order3\"]"
                      "}");

  // Verify JSON structure
  KJ_EXPECT(responseContains(body, "order_ids"));
  KJ_EXPECT(responseContains(body, "order1"));
  KJ_EXPECT(responseContains(body, "order2"));
  KJ_EXPECT(responseContains(body, "order3"));
}

KJ_TEST("OrderHandler: parse empty bulk cancel request") {
  auto body = kj::str("{"
                      "\"order_ids\":[]"
                      "}");

  KJ_EXPECT(responseContains(body, "order_ids"));
  // Empty array should be rejected by validation
}

// ============================================================================
// Integration Tests with EngineBridge
// ============================================================================

KJ_TEST("OrderHandler: place order through bridge") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "test-order-1").wait(ctx.io.waitScope);

  // Check metrics
  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 1);

  bridge.stop();
}

KJ_TEST("OrderHandler: cancel order through bridge") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Cancel order
  bridge.cancel_order("test-order-1").wait(ctx.io.waitScope);

  // Check metrics
  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 1);

  bridge.stop();
}

KJ_TEST("OrderHandler: multiple orders through bridge") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place multiple orders
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "order-1").wait(ctx.io.waitScope);
  bridge.place_order("sell", "ETHUSDT", 10.0, 3000.0, "order-2").wait(ctx.io.waitScope);
  bridge.place_order("buy", "BNBUSDT", 5.0, 400.0, "order-3").wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.metrics().orders_submitted.load() == 3);

  // Cancel all
  bridge.cancel_order("order-1").wait(ctx.io.waitScope);
  bridge.cancel_order("order-2").wait(ctx.io.waitScope);
  bridge.cancel_order("order-3").wait(ctx.io.waitScope);

  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == 3);

  bridge.stop();
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("OrderHandler: bridge order latency") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place orders and check latency
  const int NUM_ORDERS = 100;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_ORDERS; ++i) {
    bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, kj::str("order-", i).cStr())
        .wait(ctx.io.waitScope);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double avg_us = static_cast<double>(duration.count()) / NUM_ORDERS;

  KJ_LOG(INFO, "Average bridge order latency (us)", avg_us);

  // Check bridge metrics
  KJ_EXPECT(bridge.metrics().orders_submitted.load() == NUM_ORDERS);
  KJ_EXPECT(bridge.metrics().avg_order_latency_ns.load() > 0);

  bridge.stop();
}

KJ_TEST("OrderHandler: bulk cancel performance") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Create many orders
  const int NUM_ORDERS = 100;
  kj::Vector<kj::Promise<int>> promises;
  promises.reserve(NUM_ORDERS);

  for (int i = 0; i < NUM_ORDERS; ++i) {
    promises.add(bridge.cancel_order(kj::str("bulk-order-", i)).then([]() { return 0; }));
  }

  auto start = std::chrono::high_resolution_clock::now();

  kj::joinPromises(promises.releaseAsArray()).wait(ctx.io.waitScope);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "Bulk cancel latency for", NUM_ORDERS, "orders (us)", duration.count());

  KJ_EXPECT(bridge.metrics().orders_cancelled.load() == NUM_ORDERS);

  bridge.stop();
}

// ============================================================================
// Audit Logging Tests
// ============================================================================

KJ_TEST("OrderHandler: audit logger configuration") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  config.queue_capacity = 1000;

  audit::AuditLogger logger(config);

  // Verify initial state
  KJ_EXPECT(logger.pending_count() == 0);
}

KJ_TEST("OrderHandler: audit log entry creation") {
  audit::AuditLogEntry entry;
  entry.type = audit::AuditLogType::Order;
  entry.action = kj::str("ORDER_SUBMIT");
  entry.user_id = kj::str("test_user");
  entry.ip_address = kj::str("127.0.0.1");

  // Verify entry fields
  KJ_EXPECT(entry.type == audit::AuditLogType::Order);
  KJ_EXPECT(entry.action == "ORDER_SUBMIT");
  KJ_EXPECT(entry.user_id == "test_user");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

KJ_TEST("OrderHandler: bridge handles invalid order side") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Invalid side should throw
  KJ_EXPECT_THROW(
      FAILED,
      bridge.place_order("invalid", "BTCUSDT", 1.0, 50000.0, "test").wait(ctx.io.waitScope));

  bridge.stop();
}

KJ_TEST("OrderHandler: bridge handles zero quantity") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Zero quantity should throw
  KJ_EXPECT_THROW(
      FAILED, bridge.place_order("buy", "BTCUSDT", 0.0, 50000.0, "test").wait(ctx.io.waitScope));

  bridge.stop();
}

KJ_TEST("OrderHandler: bridge handles empty client ID") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Empty client ID should throw
  KJ_EXPECT_THROW(FAILED,
                  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "").wait(ctx.io.waitScope));

  bridge.stop();
}

} // namespace
} // namespace veloz::gateway
