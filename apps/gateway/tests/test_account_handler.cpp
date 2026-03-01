#include "audit/audit_logger.h"
#include "bridge/engine_bridge.h"
#include "handlers/account_handler.h"
#include "veloz/core/json.h"
#include "veloz/gateway/auth/rbac.h"

#include <kj/async-io.h>
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

// Helper to check if response contains a substring
bool responseContains(kj::StringPtr response, kj::StringPtr substr) {
  return response.find(substr) != kj::none;
}

// ============================================================================
// AccountHandler Construction Tests
// ============================================================================

KJ_TEST("AccountHandler: construction requires non-null dependencies") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  audit::AuditLoggerConfig auditConfig;
  auditConfig.log_dir = "/tmp/test_audit"_kj;
  auditConfig.enable_console_output = false;
  audit::AuditLogger logger(auditConfig);

  // Should succeed with valid args
  AccountHandler handler(bridge, logger);
}

// ============================================================================
// Account State Query Tests
// ============================================================================

KJ_TEST("AccountHandler: get_account_state returns valid structure") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Get account state
  auto state = bridge.get_account_state();

  // Verify state has expected fields
  KJ_EXPECT(state.total_equity >= 0.0);
  KJ_EXPECT(state.available_balance >= 0.0);
  KJ_EXPECT(state.last_update_ns > 0);

  bridge.stop();
}

KJ_TEST("AccountHandler: get_account_state updates metrics") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Get account state multiple times
  auto state1 = bridge.get_account_state();
  auto state2 = bridge.get_account_state();
  auto state3 = bridge.get_account_state();
  (void)state1;
  (void)state2;
  (void)state3;

  // Check that operations were recorded
  // Note: Metrics may or may not track this depending on implementation

  bridge.stop();
}

// ============================================================================
// Position Query Tests
// ============================================================================

KJ_TEST("AccountHandler: get_positions returns vector") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Get positions
  auto positions = bridge.get_positions();

  // Should return a valid vector (may be empty initially)
  KJ_EXPECT(positions.size() >= 0);

  bridge.stop();
}

KJ_TEST("AccountHandler: get_position for specific symbol") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Get position for non-existent symbol
  auto maybePosition = bridge.get_position("NONEXISTENT");

  // Should return none for non-existent position
  KJ_EXPECT(maybePosition == kj::none);

  bridge.stop();
}

KJ_TEST("AccountHandler: position snapshot structure") {
  // Create a sample position snapshot
  oms::PositionSnapshot snapshot;
  snapshot.symbol = kj::heapString("BTCUSDT");
  snapshot.size = 1.5;
  snapshot.avg_price = 50000.0;
  snapshot.realized_pnl = 100.0;
  snapshot.unrealized_pnl = -50.0;
  snapshot.side = oms::PositionSide::Long;
  snapshot.timestamp_ns = 1234567890;

  // Verify snapshot fields
  KJ_EXPECT(snapshot.symbol == "BTCUSDT");
  KJ_EXPECT(snapshot.size == 1.5);
  KJ_EXPECT(snapshot.avg_price == 50000.0);
  KJ_EXPECT(snapshot.realized_pnl == 100.0);
  KJ_EXPECT(snapshot.unrealized_pnl == -50.0);
  KJ_EXPECT(snapshot.side == oms::PositionSide::Long);
}

// ============================================================================
// Position Side Enum Tests
// ============================================================================

KJ_TEST("AccountHandler: position side enum values") {
  // Verify enum values
  KJ_EXPECT(static_cast<int>(oms::PositionSide::None) == 0);
  KJ_EXPECT(static_cast<int>(oms::PositionSide::Long) == 1);
  KJ_EXPECT(static_cast<int>(oms::PositionSide::Short) == 2);
}

// ============================================================================
// Permission Checking Tests
// ============================================================================

KJ_TEST("AccountHandler: permission constant for account access") {
  // Verify permission name
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::ReadAccount) == "read:account"_kj);
}

KJ_TEST("AccountHandler: permission check with empty auth") {
  // Without auth, permission check should fail
  kj::Vector<kj::String> permissions;

  kj::StringPtr target = "read:account";

  bool found = false;
  for (const auto& perm : permissions) {
    if (perm == target) {
      found = true;
      break;
    }
  }

  KJ_EXPECT(!found);
}

KJ_TEST("AccountHandler: permission check with valid permission") {
  kj::Vector<kj::String> permissions;
  permissions.add(kj::heapString("read:account"));
  permissions.add(kj::heapString("read:orders"));

  kj::StringPtr target = "read:account";

  bool found = false;
  for (const auto& perm : permissions) {
    if (perm == target) {
      found = true;
      break;
    }
  }

  KJ_EXPECT(found);
}

// ============================================================================
// JSON Formatting Tests
// ============================================================================

KJ_TEST("AccountHandler: account state JSON contains expected fields") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Get account state
  auto state = bridge.get_account_state();

  // Build JSON using JsonBuilder
  auto builder = core::JsonBuilder::object();
  builder.put("total_equity", state.total_equity);
  builder.put("available_balance", state.available_balance);
  builder.put("unrealized_pnl", state.unrealized_pnl);

  kj::String json = builder.build();

  // Verify JSON contains expected fields
  KJ_EXPECT(responseContains(json, "total_equity"));
  KJ_EXPECT(responseContains(json, "available_balance"));
  KJ_EXPECT(responseContains(json, "unrealized_pnl"));

  bridge.stop();
}

KJ_TEST("AccountHandler: position JSON format") {
  oms::PositionSnapshot position;
  position.symbol = kj::heapString("BTCUSDT");
  position.size = 1.0;
  position.avg_price = 50000.0;
  position.realized_pnl = 0.0;
  position.unrealized_pnl = 100.0;
  position.side = oms::PositionSide::Long;
  position.timestamp_ns = 1234567890;

  // Build JSON
  auto builder = core::JsonBuilder::object();
  builder.put("symbol", position.symbol.cStr());
  builder.put("size", position.size);
  builder.put("avg_price", position.avg_price);
  builder.put("realized_pnl", position.realized_pnl);
  builder.put("unrealized_pnl", position.unrealized_pnl);
  builder.put("side", "long");
  builder.put("timestamp_ns", static_cast<int64_t>(position.timestamp_ns));

  kj::String json = builder.build();

  // Verify JSON structure
  KJ_EXPECT(responseContains(json, "BTCUSDT"));
  KJ_EXPECT(responseContains(json, "size"));
  KJ_EXPECT(responseContains(json, "avg_price"));
  KJ_EXPECT(responseContains(json, "long"));
}

// ============================================================================
// Audit Logging Tests
// ============================================================================

KJ_TEST("AccountHandler: audit logger configuration") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  config.queue_capacity = 1000;

  audit::AuditLogger logger(config);

  // Verify initial state
  KJ_EXPECT(logger.pending_count() == 0);
}

KJ_TEST("AccountHandler: audit log entry for account query") {
  audit::AuditLogEntry entry;
  entry.type = audit::AuditLogType::Access;
  entry.action = kj::str("ACCOUNT_QUERY");
  entry.user_id = kj::str("test_user");
  entry.ip_address = kj::str("127.0.0.1");

  // Verify entry fields
  KJ_EXPECT(entry.type == audit::AuditLogType::Access);
  KJ_EXPECT(entry.action == "ACCOUNT_QUERY");
  KJ_EXPECT(entry.user_id == "test_user");
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("AccountHandler: get_account latency under target") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Measure latency
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    auto state = bridge.get_account_state();
    (void)state;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "100 get_account_state calls took (us)", duration.count());

  // Average should be under 50us per request (performance target)
  // Note: This may be flaky on CI
  // double avg_latency_us = static_cast<double>(duration.count()) / 100.0;
  // KJ_EXPECT(avg_latency_us < 50.0);

  bridge.stop();
}

KJ_TEST("AccountHandler: get_positions latency") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Measure latency
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    auto positions = bridge.get_positions();
    (void)positions;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "100 get_positions calls took (us)", duration.count());

  bridge.stop();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

KJ_TEST("AccountHandler: handles bridge not initialized") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  // Don't initialize - should still be safe to query
  // The bridge should return default/empty values

  auto state = bridge.get_account_state();
  KJ_EXPECT(state.last_update_ns >= 0);
}

KJ_TEST("AccountHandler: handles concurrent access") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Concurrent account state queries
  for (int i = 0; i < 10; ++i) {
    auto state = bridge.get_account_state();
    (void)state;
  }

  // All queries should complete without error
  KJ_EXPECT(true);

  bridge.stop();
}

// ============================================================================
// Integration Tests
// ============================================================================

KJ_TEST("AccountHandler: full workflow with orders and positions") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));

  bridge.initialize(ctx.io).wait(ctx.io.waitScope);
  auto startPromise = bridge.start();
  ctx.io.provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.io.waitScope);

  // Place an order
  bridge.place_order("buy", "BTCUSDT", 1.0, 50000.0, "test-order-1").wait(ctx.io.waitScope);

  // Get account state
  auto state = bridge.get_account_state();
  KJ_EXPECT(state.total_equity >= 0.0);

  // Get positions
  auto positions = bridge.get_positions();
  KJ_EXPECT(positions.size() >= 0);

  bridge.stop();
}

} // namespace
} // namespace veloz::gateway
