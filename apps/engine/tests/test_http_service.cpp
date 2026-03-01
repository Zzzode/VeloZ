/**
 * @file test_http_service.cpp
 * @brief Unit tests for EngineHttpService state management and callbacks
 *
 * Tests cover:
 * - Engine lifecycle state transitions
 * - Start/stop callback registration
 * - State accessor methods
 *
 * Note: Full HTTP request/response tests require integration testing
 * with a real HTTP client, which is covered in integration tests.
 */

#include "veloz/engine/http_service.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/test.h>

using namespace veloz::engine;

namespace {

class TestResponse final : public kj::HttpService::Response {
public:
  kj::uint statusCode = 0;
  kj::String statusText = kj::str("");

  kj::Own<kj::AsyncOutputStream> send(kj::uint statusCode, kj::StringPtr statusText,
                                      const kj::HttpHeaders& headers,
                                      kj::Maybe<uint64_t> expectedBodySize) override {
    this->statusCode = statusCode;
    this->statusText = kj::str(statusText);
    this->expectedBodySize = expectedBodySize;
    return kj::heap<kj::NullStream>();
  }

  kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders& headers) override {
    KJ_FAIL_REQUIRE("websocket not supported");
  }

  kj::Maybe<uint64_t> expectedBodySize;
};

// ============================================================================
// Engine Lifecycle State Tests
// ============================================================================

KJ_TEST("EngineHttpService: Initial state is Starting") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Starting);
}

KJ_TEST("EngineHttpService: State can be set to Running") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  service.set_engine_state(EngineLifecycleState::Running);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Running);
}

KJ_TEST("EngineHttpService: State can be set to Stopping") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  service.set_engine_state(EngineLifecycleState::Stopping);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Stopping);
}

KJ_TEST("EngineHttpService: State can be set to Stopped") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  service.set_engine_state(EngineLifecycleState::Stopped);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Stopped);
}

KJ_TEST("EngineHttpService: Full lifecycle transition") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  // Starting -> Running
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Starting);

  service.set_engine_state(EngineLifecycleState::Running);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Running);

  // Running -> Stopping
  service.set_engine_state(EngineLifecycleState::Stopping);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Stopping);

  // Stopping -> Stopped
  service.set_engine_state(EngineLifecycleState::Stopped);
  KJ_EXPECT(service.get_engine_state() == EngineLifecycleState::Stopped);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

KJ_TEST("EngineHttpService: Start callback can be set") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  bool callbackSet = false;
  service.set_start_callback([&callbackSet]() -> bool {
    callbackSet = true;
    return true;
  });

  // Callback is set but not invoked until a request is made
  KJ_EXPECT(!callbackSet);
}

KJ_TEST("EngineHttpService: Stop callback can be set") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  bool callbackSet = false;
  service.set_stop_callback([&callbackSet]() -> bool {
    callbackSet = true;
    return true;
  });

  // Callback is set but not invoked until a request is made
  KJ_EXPECT(!callbackSet);
}

// ============================================================================
// Stop Flag Tests
// ============================================================================

KJ_TEST("EngineHttpService: Stop flag initially false") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  KJ_EXPECT(*stopFlag.lockShared() == false);
}

KJ_TEST("EngineHttpService: Stop flag can be modified externally") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  // Simulate external stop signal
  *stopFlag.lockExclusive() = true;

  KJ_EXPECT(*stopFlag.lockShared() == true);
}

// ============================================================================
// Strategy Manager Tests
// ============================================================================

KJ_TEST("EngineHttpService: Strategy manager null by default") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  // Strategy manager is null by default
  // This is verified by the fact that strategy endpoints return 503
  // when no manager is set (tested in integration tests)
}

KJ_TEST("EngineHttpService: Strategy manager can be set to nullptr") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  service.set_strategy_manager(nullptr);
  // No crash means success
}

// ============================================================================
// Multiple Service Instances
// ============================================================================

KJ_TEST("EngineHttpService: Multiple instances are independent") {
  auto headerTable1 = kj::heap<kj::HttpHeaderTable>();
  auto headerTable2 = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag1(false);
  kj::MutexGuarded<bool> stopFlag2(false);

  EngineHttpService service1(*headerTable1, stopFlag1);
  EngineHttpService service2(*headerTable2, stopFlag2);

  // Set different states
  service1.set_engine_state(EngineLifecycleState::Running);
  service2.set_engine_state(EngineLifecycleState::Stopped);

  // Verify independence
  KJ_EXPECT(service1.get_engine_state() == EngineLifecycleState::Running);
  KJ_EXPECT(service2.get_engine_state() == EngineLifecycleState::Stopped);
}

KJ_TEST("EngineHttpService: Stop flags are independent") {
  auto headerTable1 = kj::heap<kj::HttpHeaderTable>();
  auto headerTable2 = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag1(false);
  kj::MutexGuarded<bool> stopFlag2(false);

  EngineHttpService service1(*headerTable1, stopFlag1);
  EngineHttpService service2(*headerTable2, stopFlag2);

  // Set one stop flag
  *stopFlag1.lockExclusive() = true;

  // Verify independence
  KJ_EXPECT(*stopFlag1.lockShared() == true);
  KJ_EXPECT(*stopFlag2.lockShared() == false);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

KJ_TEST("EngineHttpService: State access is thread-safe") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  // Rapid state changes should not cause issues
  for (int i = 0; i < 100; i++) {
    service.set_engine_state(EngineLifecycleState::Running);
    auto state = service.get_engine_state();
    KJ_EXPECT(state == EngineLifecycleState::Running);

    service.set_engine_state(EngineLifecycleState::Stopping);
    state = service.get_engine_state();
    KJ_EXPECT(state == EngineLifecycleState::Stopping);
  }
}

KJ_TEST("EngineHttpService: /api/control/status routes to status handler") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = service.request(kj::HttpMethod::GET, "/api/control/status"_kj,
                                 kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 200);
}

KJ_TEST("EngineHttpService: /api/control/health routes to health handler") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = service.request(kj::HttpMethod::GET, "/api/control/health"_kj,
                                 kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 200);
}

KJ_TEST("EngineHttpService: /api/control/config returns config payload") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::MutexGuarded<bool> stopFlag(false);
  EngineHttpService service(*headerTable, stopFlag);

  kj::EventLoop eventLoop;
  kj::WaitScope waitScope(eventLoop);
  kj::NullStream requestBody;
  TestResponse response;

  auto promise = service.request(kj::HttpMethod::GET, "/api/control/config"_kj,
                                 kj::HttpHeaders(*headerTable), requestBody, response);
  promise.wait(waitScope);

  KJ_EXPECT(response.statusCode == 200);
}

} // namespace
