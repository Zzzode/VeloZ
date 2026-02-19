#include "kj/test.h"
#include "veloz/core/error.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/resilient_adapter.h"
#include "veloz/risk/circuit_breaker.h"

#include <atomic>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/time.h>
#include <thread>

using namespace veloz::exec;
using namespace veloz::risk;
using namespace veloz::core;

namespace {

// Mock exchange adapter for testing
class MockExchangeAdapter : public ExchangeAdapter {
public:
  MockExchangeAdapter() = default;

  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
    call_count_++;
    if (should_fail_) {
      if (fail_with_network_error_) {
        throw NetworkException("Network error", 1);
      }
      return kj::none;
    }
    ExecutionReport report;
    report.venue_order_id = kj::str("order_", call_count_);
    report.client_order_id = kj::str(req.client_order_id);
    report.status = OrderStatus::New;
    return kj::mv(report);
  }

  kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
    call_count_++;
    if (should_fail_) {
      if (fail_with_network_error_) {
        throw NetworkException("Network error", 1);
      }
      return kj::none;
    }
    ExecutionReport report;
    report.venue_order_id = kj::str(req.client_order_id);
    report.status = OrderStatus::Canceled;
    return kj::mv(report);
  }

  bool is_connected() const override {
    return connected_;
  }

  void connect() override {
    if (should_fail_connect_) {
      throw NetworkException("Connection failed", 1);
    }
    connected_ = true;
  }

  void disconnect() override {
    connected_ = false;
  }

  kj::StringPtr name() const override {
    return "mock_exchange"_kj;
  }

  kj::StringPtr version() const override {
    return "1.0.0"_kj;
  }

  // Test control methods
  void set_should_fail(bool fail) {
    should_fail_ = fail;
  }
  void set_fail_with_network_error(bool fail) {
    fail_with_network_error_ = fail;
    if (fail) {
      should_fail_ = true; // Network errors require should_fail_ to be true
    }
  }
  void set_should_fail_connect(bool fail) {
    should_fail_connect_ = fail;
  }
  int call_count() const {
    return call_count_;
  }
  void reset_call_count() {
    call_count_ = 0;
  }

private:
  bool connected_ = false;
  bool should_fail_ = false;
  bool fail_with_network_error_ = false;
  bool should_fail_connect_ = false;
  std::atomic<int> call_count_{0};
};

// ============================================================================
// ResilientExchangeAdapter Tests
// ============================================================================

KJ_TEST("ResilientExchangeAdapter: Successful place order") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();

  ResilientAdapterConfig config;
  config.max_retries = 3;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 3;

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect(); // This counts as 1 successful request

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_1");

  auto result = resilient_adapter->place_order(req);

  KJ_IF_SOME(report, result) {
    KJ_EXPECT(report.status == OrderStatus::New);
  }
  else {
    KJ_FAIL_EXPECT("Expected execution report");
  }

  // connect() + place_order() = 2 successful requests
  KJ_EXPECT(resilient_adapter->stats().successful_requests.load() == 2);
  KJ_EXPECT(resilient_adapter->stats().failed_requests.load() == 0);
}

KJ_TEST("ResilientExchangeAdapter: Retry on network error") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto* mock_ptr = mock_adapter.get(); // Keep raw pointer before move

  ResilientAdapterConfig config;
  config.max_retries = 3;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 10; // High threshold to prevent circuit breaker tripping

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect();
  mock_ptr->set_fail_with_network_error(true);

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_2");

  bool caught_exception = false;
  try {
    resilient_adapter->place_order(req);
  } catch (const NetworkException&) {
    caught_exception = true;
  }
  KJ_EXPECT(caught_exception);

  // Should have attempted multiple times (at least max_retries attempts)
  KJ_EXPECT(mock_ptr->call_count() >= config.max_retries);

  // Verify that request was tracked as failed
  KJ_EXPECT(resilient_adapter->stats().failed_requests.load() == 1);
}

KJ_TEST("ResilientExchangeAdapter: Circuit breaker integration") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto* mock_ptr = mock_adapter.get(); // Keep raw pointer before move

  ResilientAdapterConfig config;
  config.max_retries = 1;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 2; // Low threshold for quick tripping
  config.circuit_timeout_ms = 60000;

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect();
  mock_ptr->set_fail_with_network_error(true);

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_3");

  // Cause failures - circuit breaker should eventually trip
  int network_failures = 0;
  int circuit_breaker_rejections = 0;
  for (int i = 0; i < 10; ++i) {
    try {
      resilient_adapter->place_order(req);
    } catch (const NetworkException&) {
      network_failures++;
    } catch (const CircuitBreakerException&) {
      circuit_breaker_rejections++;
      break;
    }
  }

  // Verify that we got some failures (either network or circuit breaker)
  KJ_EXPECT(network_failures > 0 || circuit_breaker_rejections > 0);

  // Verify that total requests were tracked
  KJ_EXPECT(resilient_adapter->stats().total_requests.load() > 0);
}

KJ_TEST("ResilientExchangeAdapter: Circuit breaker trips") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto* mock_ptr = mock_adapter.get(); // Keep raw pointer before move

  ResilientAdapterConfig config;
  config.max_retries = 1;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 2;

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect();
  mock_ptr->set_fail_with_network_error(true);

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_4");

  // Trigger circuit breaker
  for (int i = 0; i < 3; ++i) {
    try {
      resilient_adapter->place_order(req);
    } catch (...) {
      // Expected to fail
    }
  }

  // Verify circuit breaker is open
  KJ_EXPECT(!resilient_adapter->circuit_breaker().allow_request());
}

KJ_TEST("ResilientExchangeAdapter: Circuit breaker recovery") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto* mock_ptr = mock_adapter.get(); // Keep raw pointer before move

  ResilientAdapterConfig config;
  config.max_retries = 1;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 2;
  config.circuit_timeout_ms = 100; // Short timeout for testing

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect();
  mock_ptr->set_fail_with_network_error(true);

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_5");

  // Trigger circuit breaker
  for (int i = 0; i < 3; ++i) {
    try {
      resilient_adapter->place_order(req);
    } catch (...) {
      // Expected to fail
    }
  }

  // Wait for circuit timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // After timeout, circuit breaker transitions to HalfOpen (not Closed)
  // It only transitions to Closed after successful requests in HalfOpen state
  // Calling allow_request() triggers auto-reset check
  resilient_adapter->circuit_breaker().allow_request();
  CircuitState state_after_timeout = resilient_adapter->circuit_breaker().state();
  KJ_EXPECT(state_after_timeout == CircuitState::HalfOpen);
}

KJ_TEST("ResilientExchangeAdapter: Success after failures") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto* mock_ptr = mock_adapter.get(); // Keep raw pointer before move

  ResilientAdapterConfig config;
  config.max_retries = 3;
  config.initial_retry_delay = 10 * kj::MILLISECONDS;
  config.failure_threshold = 10; // High threshold to prevent circuit breaker tripping

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect();

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_6");

  // First, cause some failures
  mock_ptr->set_fail_with_network_error(true);
  bool had_failure = false;
  try {
    resilient_adapter->place_order(req);
  } catch (const NetworkException&) {
    had_failure = true;
  }
  KJ_EXPECT(had_failure);

  // Now allow success
  mock_ptr->set_fail_with_network_error(false);

  // Request should succeed
  auto result = resilient_adapter->place_order(req);
  KJ_IF_SOME(report, result) {
    KJ_EXPECT(report.status == OrderStatus::New);
  }

  // Verify successes were tracked (after failure)
  KJ_EXPECT(resilient_adapter->stats().successful_requests.load() > 0);
  // Verify total requests includes both failed and successful attempts
  KJ_EXPECT(resilient_adapter->stats().total_requests.load() >= 2);
}

KJ_TEST("ResilientExchangeAdapter: Health check") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();

  ResilientAdapterConfig config;
  config.enable_health_check = true;

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  KJ_EXPECT(!resilient_adapter->check_health()); // Not connected

  resilient_adapter->connect();
  KJ_EXPECT(resilient_adapter->check_health());
}

KJ_TEST("ResilientExchangeAdapter: Statistics") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();

  ResilientAdapterConfig config;
  config.max_retries = 3;

  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter), config);

  resilient_adapter->connect(); // This counts as 1 successful request

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT");
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 1.0;
  req.client_order_id = kj::str("test_order_7");

  resilient_adapter->place_order(req);
  resilient_adapter->place_order(req);

  // connect() + 2 place_order() = 3 total requests
  KJ_EXPECT(resilient_adapter->stats().total_requests.load() == 3);
  KJ_EXPECT(resilient_adapter->stats().successful_requests.load() == 3);

  resilient_adapter->reset_stats();
  KJ_EXPECT(resilient_adapter->stats().total_requests.load() == 0);
}

KJ_TEST("ResilientExchangeAdapter: Adapter name") {
  auto mock_adapter = kj::heap<MockExchangeAdapter>();
  auto resilient_adapter = kj::heap<ResilientExchangeAdapter>(kj::mv(mock_adapter));

  KJ_EXPECT(kj::StringPtr(resilient_adapter->name()) == "resilient_mock_exchange"_kj);
  KJ_EXPECT(kj::StringPtr(resilient_adapter->version()) == "1.0.0"_kj);
}

} // namespace
