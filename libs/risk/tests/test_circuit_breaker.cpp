#include "veloz/risk/circuit_breaker.h"

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
