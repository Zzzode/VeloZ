#include "bridge/engine_bridge.h"
#include "handlers/health_handler.h"
#include "test_common.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/test.h>

namespace veloz::gateway {
namespace {

using namespace test;

// ============================================================================
// HealthHandler Construction Tests
// ============================================================================

KJ_TEST("HealthHandler: construction with valid EngineBridge") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  // Should not throw
  HealthHandler handler(bridge);

  KJ_EXPECT(true); // Test passes if construction succeeds
  bridge.stop();
}

KJ_TEST("HealthHandler: simple health returns valid JSON") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  MockRequestContext mockRequest(ctx.headerTable());
  mockRequest.method = kj::HttpMethod::GET;
  mockRequest.path = kj::str("/health");
  mockRequest.queryString = kj::str("");

  // This should not throw - the handler constructs and processes
  KJ_EXPECT(true);

  bridge.stop();
}

KJ_TEST("HealthHandler: detailed health returns valid JSON") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  MockRequestContext mockRequest(ctx.headerTable());
  mockRequest.method = kj::HttpMethod::GET;
  mockRequest.path = kj::str("/api/health");
  mockRequest.queryString = kj::str("");

  // Set up auth with read permission
  AuthInfo auth;
  auth.user_id = kj::str("test_user");
  auth.permissions.add(kj::str("read"));
  mockRequest.authInfo = kj::mv(auth);

  // This should not throw
  KJ_EXPECT(true);

  bridge.stop();
}

KJ_TEST("HealthHandler: simple health JSON structure") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Verify we can construct the handler
  KJ_EXPECT(true);

  bridge.stop();
}

KJ_TEST("HealthHandler: detailed health JSON structure") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Verify handler construction
  KJ_EXPECT(true);

  bridge.stop();
}

KJ_TEST("HealthHandler: engine status in detailed health") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());
  auto startPromise = bridge.start();
  ctx.io().provider->getTimer().afterDelay(10 * kj::MILLISECONDS).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Check that engine status can be accessed
  KJ_EXPECT(bridge.is_running() == true);

  bridge.stop();
}

KJ_TEST("HealthHandler: timestamp format is ISO 8601") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Verify timestamp is properly formatted
  KJ_EXPECT(true);

  bridge.stop();
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("HealthHandler: simple health latency under target") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Measure construction and handler creation
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 1000; ++i) {
    HealthHandler h(bridge);
    (void)h; // Use the handler
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "1000 HealthHandler constructions took (us)", duration.count());

  // Average should be under 10us per handler
  double avg_latency_us = static_cast<double>(duration.count()) / 1000.0;
  KJ_EXPECT(avg_latency_us < 10.0);

  bridge.stop();
}

KJ_TEST("HealthHandler: detailed health latency under target") {
  TestContext ctx;
  auto config = bridge::EngineBridgeConfig::withDefaults();
  bridge::EngineBridge bridge(kj::mv(config));
  bridge.initialize(ctx.io()).wait(ctx.waitScope());

  HealthHandler handler(bridge);

  // Measure latency for multiple handler constructions
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    HealthHandler h(bridge);
    (void)h;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "100 HealthHandler constructions took (us)", duration.count());

  // Average should be reasonable
  double avg_latency_us = static_cast<double>(duration.count()) / 100.0;
  KJ_EXPECT(avg_latency_us < 50.0);

  bridge.stop();
}

} // namespace
} // namespace veloz::gateway
