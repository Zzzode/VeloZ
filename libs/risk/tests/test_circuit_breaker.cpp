#include "veloz/risk/circuit_breaker.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace veloz::risk;

TEST(CircuitBreaker, AllowRequestsWhenOpen) {
  CircuitBreaker cb;

  EXPECT_TRUE(cb.allow_request());
}

TEST(CircuitBreaker, TripOnFailures) {
  CircuitBreaker cb;
  cb.set_failure_threshold(3);

  cb.record_failure();
  cb.record_failure();
  EXPECT_TRUE(cb.allow_request()); // Still open

  cb.record_failure();              // Trip!
  EXPECT_FALSE(cb.allow_request()); // Now open
}

TEST(CircuitBreaker, AutoResetAfterTimeout) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);
  cb.set_timeout_ms(100);

  cb.record_failure();
  cb.record_failure();
  EXPECT_FALSE(cb.allow_request());

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_TRUE(cb.allow_request()); // Should be auto-reset
}

TEST(CircuitBreaker, ManualReset) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  cb.record_failure();
  cb.record_failure();
  EXPECT_FALSE(cb.allow_request());

  cb.reset();
  EXPECT_TRUE(cb.allow_request());
}

TEST(CircuitBreaker, HalfOpenState) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  cb.record_failure();
  cb.record_failure();
  EXPECT_FALSE(cb.allow_request());

  cb.reset(); // Go to half-open
  EXPECT_TRUE(cb.allow_request());

  cb.record_success();
  EXPECT_TRUE(cb.allow_request()); // Stay closed
}

TEST(CircuitBreaker, NamedCircuitBreaker) {
  CircuitBreaker cb("test_service");
  EXPECT_EQ(cb.name(), "test_service");

  cb.set_name("new_name");
  EXPECT_EQ(cb.name(), "new_name");
}

TEST(CircuitBreaker, Statistics) {
  CircuitBreaker cb("stats_test");
  cb.set_failure_threshold(3);

  // Initial stats should be zero
  EXPECT_EQ(cb.stats().total_requests.load(), 0);
  EXPECT_EQ(cb.stats().successful_requests.load(), 0);
  EXPECT_EQ(cb.stats().failed_requests.load(), 0);

  // Make some requests
  cb.allow_request();
  cb.record_success();
  cb.allow_request();
  cb.record_failure();
  cb.allow_request();
  cb.record_success();

  EXPECT_EQ(cb.stats().total_requests.load(), 3);
  EXPECT_EQ(cb.stats().successful_requests.load(), 2);
  EXPECT_EQ(cb.stats().failed_requests.load(), 1);
}

TEST(CircuitBreaker, RejectedRequestsTracking) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  // Trip the circuit
  cb.allow_request();
  cb.record_failure();
  cb.allow_request();
  cb.record_failure();

  // Now requests should be rejected
  EXPECT_FALSE(cb.allow_request());
  EXPECT_FALSE(cb.allow_request());

  EXPECT_EQ(cb.stats().rejected_requests.load(), 2);
}

TEST(CircuitBreaker, StateChangeCallback) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  std::atomic<int> callback_count{0};
  CircuitState last_old_state = CircuitState::Closed;
  CircuitState last_new_state = CircuitState::Closed;

  cb.set_state_change_callback([&](CircuitState old_state, CircuitState new_state) {
    callback_count++;
    last_old_state = old_state;
    last_new_state = new_state;
  });

  // Trip the circuit
  cb.allow_request();
  cb.record_failure();
  cb.allow_request();
  cb.record_failure();

  EXPECT_EQ(callback_count.load(), 1);
  EXPECT_EQ(last_old_state, CircuitState::Closed);
  EXPECT_EQ(last_new_state, CircuitState::Open);

  // Reset to half-open
  cb.reset();
  EXPECT_EQ(callback_count.load(), 2);
  EXPECT_EQ(last_old_state, CircuitState::Open);
  EXPECT_EQ(last_new_state, CircuitState::HalfOpen);
}

TEST(CircuitBreaker, HealthCheckCallback) {
  CircuitBreaker cb;

  // No health check configured - should return true
  EXPECT_TRUE(cb.check_health());

  // Set health check that returns false
  cb.set_health_check_callback([]() { return false; });
  EXPECT_FALSE(cb.check_health());

  // Set health check that returns true
  cb.set_health_check_callback([]() { return true; });
  EXPECT_TRUE(cb.check_health());
}

TEST(CircuitBreaker, FailureRate) {
  CircuitBreaker cb;
  cb.set_failure_threshold(10); // High threshold to prevent tripping

  // No requests yet
  EXPECT_DOUBLE_EQ(cb.failure_rate(), 0.0);
  EXPECT_DOUBLE_EQ(cb.success_rate(), 1.0);

  // 2 successes, 1 failure = 33% failure rate
  cb.allow_request();
  cb.record_success();
  cb.allow_request();
  cb.record_success();
  cb.allow_request();
  cb.record_failure();

  EXPECT_NEAR(cb.failure_rate(), 1.0 / 3.0, 0.01);
  EXPECT_NEAR(cb.success_rate(), 2.0 / 3.0, 0.01);
}

TEST(CircuitBreaker, StateTransitionCount) {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  EXPECT_EQ(cb.stats().state_transitions.load(), 0);

  // Trip the circuit (Closed -> Open)
  cb.allow_request();
  cb.record_failure();
  cb.allow_request();
  cb.record_failure();
  EXPECT_EQ(cb.stats().state_transitions.load(), 1);

  // Reset (Open -> HalfOpen)
  cb.reset();
  EXPECT_EQ(cb.stats().state_transitions.load(), 2);

  // Success in half-open (HalfOpen -> Closed)
  cb.allow_request();
  cb.record_success();
  cb.allow_request();
  cb.record_success();
  EXPECT_EQ(cb.stats().state_transitions.load(), 3);
}

TEST(CircuitBreaker, ResetStats) {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  cb.allow_request();
  cb.record_success();
  cb.allow_request();
  cb.record_failure();

  EXPECT_GT(cb.stats().total_requests.load(), 0);

  cb.reset_stats();

  EXPECT_EQ(cb.stats().total_requests.load(), 0);
  EXPECT_EQ(cb.stats().successful_requests.load(), 0);
  EXPECT_EQ(cb.stats().failed_requests.load(), 0);
}

TEST(CircuitBreaker, ToStringFunction) {
  EXPECT_EQ(to_string(CircuitState::Closed), "closed");
  EXPECT_EQ(to_string(CircuitState::Open), "open");
  EXPECT_EQ(to_string(CircuitState::HalfOpen), "half_open");
}

TEST(CircuitBreakerGuard, MarkSuccess) {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  {
    CircuitBreakerGuard guard(cb);
    guard.mark_success();
  }

  EXPECT_EQ(cb.stats().successful_requests.load(), 1);
  EXPECT_EQ(cb.stats().failed_requests.load(), 0);
}

TEST(CircuitBreakerGuard, AutoFailure) {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  {
    CircuitBreakerGuard guard(cb);
    // Don't mark success - should record failure on destruction
  }

  EXPECT_EQ(cb.stats().successful_requests.load(), 0);
  EXPECT_EQ(cb.stats().failed_requests.load(), 1);
}
