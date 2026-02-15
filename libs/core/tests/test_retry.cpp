#include "kj/test.h"
#include "veloz/core/retry.h"

#include <atomic>
#include <kj/memory.h>
#include <kj/string.h>

using namespace veloz::core;

namespace {

// ============================================================================
// Basic Retry Tests
// ============================================================================

KJ_TEST("RetryHandler: Success on first attempt") {
  RetryHandler handler;

  auto result = handler.execute([]() { return 42; }, "test_op");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value == 42);
  KJ_EXPECT(result.attempts == 1);
  KJ_EXPECT(result.total_delay.count() == 0);
}

KJ_TEST("RetryHandler: Retry on network error") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.retry_on_network_error = true;
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 3) {
          throw NetworkException("Connection failed", 1);
        }
        return 100;
      },
      "network_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value == 100);
  KJ_EXPECT(result.attempts == 3);
  KJ_EXPECT(result.total_delay.count() > 0);
}

KJ_TEST("RetryHandler: Retry on timeout") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.retry_on_timeout = true;
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 2) {
          throw TimeoutException("Request timed out");
        }
        return 200;
      },
      "timeout_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value == 200);
  KJ_EXPECT(result.attempts == 2);
}

KJ_TEST("RetryHandler: Retry on rate limit") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.retry_on_rate_limit = true;
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 2) {
          throw RateLimitException("Rate limited", 50); // 50ms retry-after
        }
        return 300;
      },
      "rate_limit_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value == 300);
  KJ_EXPECT(result.attempts == 2);
}

KJ_TEST("RetryHandler: No retry on circuit breaker") {
  RetryConfig config;
  config.max_attempts = 3;
  RetryHandler handler(config);

  bool caught_exception = false;
  try {
    handler.execute([]() -> int { throw CircuitBreakerException("Circuit open", "test_service"); },
                    "circuit_test");
  } catch (const CircuitBreakerException&) {
    caught_exception = true;
  }
  KJ_EXPECT(caught_exception);
}

KJ_TEST("RetryHandler: Exhausted retries throws original exception") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  RetryHandler handler(config);

  bool caught_exception = false;
  try {
    handler.execute([]() -> int { throw NetworkException("Always fails", 1); }, "exhaust_test");
  } catch (const NetworkException&) {
    caught_exception = true;
  }
  KJ_EXPECT(caught_exception);
}

KJ_TEST("RetryHandler: Void operation") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  RetryHandler handler(config);

  std::atomic<int> call_count{0};

  auto result = handler.execute_void(
      [&]() {
        call_count++;
        if (call_count < 2) {
          throw NetworkException("Temporary failure", 1);
        }
      },
      "void_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value);
  KJ_EXPECT(result.attempts == 2);
  KJ_EXPECT(call_count.load() == 2);
}

KJ_TEST("RetryHandler: Custom retry predicate") {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(10);
  config.should_retry = [](const std::exception& e) {
    // Only retry if the message contains "retry"
    return std::string(e.what()).find("retry") != std::string::npos;
  };
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 2) {
          throw std::runtime_error("Please retry this operation");
        }
        return 400;
      },
      "custom_predicate_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.value == 400);
  KJ_EXPECT(result.attempts == 2);
}

KJ_TEST("RetryHandler: No retry when disabled") {
  RetryConfig config;
  config.max_attempts = 3;
  config.retry_on_network_error = false;
  RetryHandler handler(config);

  bool caught_exception = false;
  try {
    handler.execute([]() -> int { throw NetworkException("Network error", 1); }, "no_retry_test");
  } catch (const NetworkException&) {
    caught_exception = true;
  }
  KJ_EXPECT(caught_exception);
}

KJ_TEST("RetryHandler: Exponential backoff timing") {
  RetryConfig config;
  config.max_attempts = 4;
  config.initial_delay = std::chrono::milliseconds(100);
  config.backoff_multiplier = 2.0;
  config.jitter_factor = 0.0; // No jitter for predictable testing
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 4) {
          throw NetworkException("Temporary failure", 1);
        }
        return 500;
      },
      "backoff_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.attempts == 4);
  // Total delay should be approximately: 100 + 200 + 400 = 700ms
  // Allow some tolerance for execution time
  KJ_EXPECT(result.total_delay.count() >= 600);
  KJ_EXPECT(result.total_delay.count() <= 800);
}

KJ_TEST("RetryHandler: Max delay limit") {
  RetryConfig config;
  config.max_attempts = 5;
  config.initial_delay = std::chrono::milliseconds(100);
  config.max_delay = std::chrono::milliseconds(150);
  config.backoff_multiplier = 2.0;
  config.jitter_factor = 0.0;
  RetryHandler handler(config);

  std::atomic<int> attempt_count{0};

  auto result = handler.execute(
      [&]() {
        attempt_count++;
        if (attempt_count < 5) {
          throw NetworkException("Temporary failure", 1);
        }
        return 600;
      },
      "max_delay_test");

  KJ_EXPECT(result.success);
  KJ_EXPECT(result.attempts == 5);
  // Delays: 100, 150 (capped), 150 (capped), 150 (capped) = 550ms
  KJ_EXPECT(result.total_delay.count() >= 500);
  KJ_EXPECT(result.total_delay.count() <= 650);
}

KJ_TEST("RetryHandler: API retry handler factory") {
  auto handler = make_api_retry_handler();

  KJ_EXPECT(handler.config().max_attempts == 3);
  KJ_EXPECT(handler.config().initial_delay.count() == 100);
}

KJ_TEST("RetryHandler: Critical retry handler factory") {
  auto handler = make_critical_retry_handler();

  KJ_EXPECT(handler.config().max_attempts == 5);
  KJ_EXPECT(handler.config().initial_delay.count() == 50);
}

} // namespace
