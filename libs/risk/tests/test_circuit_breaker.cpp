#include "veloz/risk/circuit_breaker.h"

#include <atomic>
#include <kj/function.h>
#include <kj/test.h>
#include <unistd.h> // For usleep

namespace {

using namespace veloz::risk;

KJ_TEST("CircuitBreaker: Allow requests when open") {
  CircuitBreaker cb;

  KJ_EXPECT(cb.allow_request());
}

KJ_TEST("CircuitBreaker: Trip on failures") {
  CircuitBreaker cb;
  cb.set_failure_threshold(3);

  cb.record_failure();
  cb.record_failure();
  KJ_EXPECT(cb.allow_request());

  cb.record_failure();
  KJ_EXPECT(!cb.allow_request());
}

KJ_TEST("CircuitBreaker: Auto reset after timeout") {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);
  cb.set_timeout_ms(100);

  cb.record_failure();
  cb.record_failure();
  KJ_EXPECT(!cb.allow_request());

  // Use usleep for timeout test (150ms)
  usleep(150000);
  KJ_EXPECT(cb.allow_request());
}

KJ_TEST("CircuitBreaker: Manual reset") {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  cb.record_failure();
  cb.record_failure();
  KJ_EXPECT(!cb.allow_request());

  cb.reset();
  KJ_EXPECT(cb.allow_request());
}

KJ_TEST("CircuitBreaker: Half-open state") {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  cb.record_failure();
  cb.record_failure();
  KJ_EXPECT(!cb.allow_request());

  cb.reset();
  KJ_EXPECT(cb.allow_request());

  cb.record_success();
  KJ_EXPECT(cb.allow_request());
}

KJ_TEST("CircuitBreaker: Named circuit breaker") {
  CircuitBreaker cb("test_service");
  KJ_EXPECT(cb.name() == "test_service");

  cb.set_name("new_name");
  KJ_EXPECT(cb.name() == "new_name");
}

KJ_TEST("CircuitBreaker: Statistics") {
  CircuitBreaker cb("stats_test");
  cb.set_failure_threshold(3);

  KJ_EXPECT(cb.stats().total_requests.load() == 0);
  KJ_EXPECT(cb.stats().successful_requests.load() == 0);
  KJ_EXPECT(cb.stats().failed_requests.load() == 0);

  (void)cb.allow_request();
  cb.record_success();
  (void)cb.allow_request();
  cb.record_failure();
  (void)cb.allow_request();
  cb.record_success();

  KJ_EXPECT(cb.stats().total_requests.load() == 3);
  KJ_EXPECT(cb.stats().successful_requests.load() == 2);
  KJ_EXPECT(cb.stats().failed_requests.load() == 1);
}

KJ_TEST("CircuitBreaker: Rejected requests tracking") {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  (void)cb.allow_request();
  cb.record_failure();
  (void)cb.allow_request();
  cb.record_failure();

  KJ_EXPECT(!cb.allow_request());
  KJ_EXPECT(!cb.allow_request());

  KJ_EXPECT(cb.stats().rejected_requests.load() == 2);
}

KJ_TEST("CircuitBreaker: State change callback") {
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

  (void)cb.allow_request();
  cb.record_failure();
  (void)cb.allow_request();
  cb.record_failure();

  KJ_EXPECT(callback_count.load() == 1);
  KJ_EXPECT(last_old_state == CircuitState::Closed);
  KJ_EXPECT(last_new_state == CircuitState::Open);

  cb.reset();
  KJ_EXPECT(callback_count.load() == 2);
  KJ_EXPECT(last_old_state == CircuitState::Open);
  KJ_EXPECT(last_new_state == CircuitState::HalfOpen);
}

KJ_TEST("CircuitBreaker: Health check callback") {
  CircuitBreaker cb;

  KJ_EXPECT(cb.check_health());

  cb.set_health_check_callback([]() { return false; });
  KJ_EXPECT(!cb.check_health());

  cb.set_health_check_callback([]() { return true; });
  KJ_EXPECT(cb.check_health());
}

KJ_TEST("CircuitBreaker: Failure rate") {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  KJ_EXPECT(cb.failure_rate() == 0.0);
  KJ_EXPECT(cb.success_rate() == 1.0);

  (void)cb.allow_request();
  cb.record_success();
  (void)cb.allow_request();
  cb.record_success();
  (void)cb.allow_request();
  cb.record_failure();

  KJ_EXPECT(cb.failure_rate() >= 0.33 && cb.failure_rate() <= 0.34);
  KJ_EXPECT(cb.success_rate() >= 0.66 && cb.success_rate() <= 0.67);
}

KJ_TEST("CircuitBreaker: State transition count") {
  CircuitBreaker cb;
  cb.set_failure_threshold(2);

  KJ_EXPECT(cb.stats().state_transitions.load() == 0);

  (void)cb.allow_request();
  cb.record_failure();
  (void)cb.allow_request();
  cb.record_failure();
  KJ_EXPECT(cb.stats().state_transitions.load() == 1);

  cb.reset();
  KJ_EXPECT(cb.stats().state_transitions.load() == 2);

  (void)cb.allow_request();
  cb.record_success();
  (void)cb.allow_request();
  cb.record_success();
  KJ_EXPECT(cb.stats().state_transitions.load() == 3);
}

KJ_TEST("CircuitBreaker: Reset stats") {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  (void)cb.allow_request();
  cb.record_success();
  (void)cb.allow_request();
  cb.record_failure();

  KJ_EXPECT(cb.stats().total_requests.load() > 0);

  cb.reset_stats();

  KJ_EXPECT(cb.stats().total_requests.load() == 0);
  KJ_EXPECT(cb.stats().successful_requests.load() == 0);
  KJ_EXPECT(cb.stats().failed_requests.load() == 0);
}

KJ_TEST("CircuitBreaker: ToString function") {
  KJ_EXPECT(to_string(CircuitState::Closed) == "closed");
  KJ_EXPECT(to_string(CircuitState::Open) == "open");
  KJ_EXPECT(to_string(CircuitState::HalfOpen) == "half_open");
}

KJ_TEST("CircuitBreakerGuard: Mark success") {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  {
    CircuitBreakerGuard guard(cb);
    guard.mark_success();
  }

  KJ_EXPECT(cb.stats().successful_requests.load() == 1);
  KJ_EXPECT(cb.stats().failed_requests.load() == 0);
}

KJ_TEST("CircuitBreakerGuard: Auto failure") {
  CircuitBreaker cb;
  cb.set_failure_threshold(10);

  {
    CircuitBreakerGuard guard(cb);
  }

  KJ_EXPECT(cb.stats().successful_requests.load() == 0);
  KJ_EXPECT(cb.stats().failed_requests.load() == 1);
}

} // namespace
