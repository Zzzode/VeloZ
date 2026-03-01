/**
 * @file test_config.h
 * @brief Unified test configuration for VeloZ C++ tests
 *
 * This file provides:
 * - Default timeout constants (must match cmake/VeloZTestTimeouts.cmake)
 * - Common test context with KJ async I/O
 * - Timer-based timeout helper for async tests
 *
 * Usage:
 *   KJ_TEST("MyModule::MyClass: description") {
 *     veloz::test::TestContext ctx;
 *     // Use ctx.waitScope for async operations
 *   }
 *
 * For async tests with timeout:
 *   KJ_TEST("Async operation with timeout") {
 *     veloz::test::TestContext ctx;
 *     auto promise = ctx.timeoutAfter(veloz::test::DEFAULT_UNIT_TEST_TIMEOUT,
 *                                     runAsyncTest(ctx.io));
 *     // Will throw OVERLOADED exception on timeout
 *   }
 */

#pragma once

#include <kj/async-io.h>
#include <kj/test.h>
#include <kj/time.h>

namespace veloz::test {

// ============================================================================
// Timeout Constants (sync with cmake/VeloZTestTimeouts.cmake)
// ============================================================================

/// Timeout for fast unit tests (no I/O, pure computation)
constexpr auto TIMEOUT_SHORT_MS = 10000; // 10 seconds

/// Default timeout for standard unit tests
constexpr auto TIMEOUT_DEFAULT_MS = 30000; // 30 seconds

/// Timeout for integration tests and I/O tests
constexpr auto TIMEOUT_LONG_MS = 120000; // 120 seconds

/// Timeout for load tests and performance tests
constexpr auto TIMEOUT_EXTENDED_MS = 300000; // 300 seconds

/// KJ duration constants (multiply by integer to get Duration)
/// Usage: 5 * veloz::test::SECONDS
constexpr auto SECONDS = kj::SECONDS;
constexpr auto MILLISECONDS = kj::MILLISECONDS;
constexpr auto SHORT = TIMEOUT_SHORT_MS * kj::MILLISECONDS;
constexpr auto DEFAULT = TIMEOUT_DEFAULT_MS * kj::MILLISECONDS;
constexpr auto LONG = TIMEOUT_LONG_MS * kj::MILLISECONDS;
constexpr auto EXTENDED = TIMEOUT_EXTENDED_MS * kj::MILLISECONDS;

// ============================================================================
// Test Context
// ============================================================================

/**
 * @brief Common test context with KJ async I/O support
 *
 * Provides a ready-to-use async I/O context for tests that need:
 * - Timer access
 * - Network operations
 * - Event loop
 *
 * Example:
 *   KJ_TEST("MyTest: async operation") {
 *     veloz::test::TestContext ctx;
 *     auto promise = someAsyncOperation(ctx.io);
 *     promise.wait(ctx.waitScope);
 *   }
 */
class TestContext {
public:
  TestContext() : io_(kj::setupAsyncIo()) {}

  /// Get the wait scope for synchronous waiting on promises
  kj::WaitScope& waitScope() {
    return io_.waitScope;
  }

  /// Get the async I/O provider for network/timer access
  kj::AsyncIoProvider& ioProvider() {
    return *io_.provider;
  }

  /// Get the low-level async I/O context
  kj::AsyncIoContext& io() {
    return io_;
  }

  /// Get the timer for time-based operations
  kj::Timer& timer() {
    return io_.provider->getTimer();
  }

  /// Execute a promise with a timeout
  /// @param timeout Maximum time to wait
  /// @param promise The promise to execute
  /// @throws kj::Exception with type OVERLOADED on timeout
  template <typename T>
  kj::Promise<T> timeoutAfter(kj::Duration timeout, kj::Promise<T>&& promise) {
    return timer().timeoutAfter(timeout, kj::mv(promise));
  }

  /// Execute a void promise with a timeout
  kj::Promise<void> timeoutAfter(kj::Duration timeout, kj::Promise<void>&& promise) {
    return timer().timeoutAfter(timeout, kj::mv(promise));
  }

private:
  kj::AsyncIoContext io_;
};

// ============================================================================
// Test Naming Conventions
// ============================================================================

/**
 * @brief Test naming convention guidelines
 *
 * Test names should follow this pattern:
 *   KJ_TEST("ModuleName::ClassName: scenario description")
 *
 * Examples:
 *   KJ_TEST("Core::Logger: logs messages with timestamp")
 *   KJ_TEST("Market::OrderBook: updates bid price correctly")
 *   KJ_TEST("Exec::BinanceAdapter: handles connection timeout")
 *
 * This pattern enables:
 * - Easy filtering by module
 * - Clear identification of what's being tested
 * - Consistent test output formatting
 */

// ============================================================================
// Assertion Helpers
// ============================================================================

/**
 * @brief Assert that a promise completes within the specified timeout
 *
 * Usage:
 *   KJ_ASSERT_TIMEOUT(ctx, promise, 5 * kj::SECONDS);
 */
#define KJ_ASSERT_TIMEOUT(ctx, promise, timeout)                                                   \
  do {                                                                                             \
    auto& _ctx = (ctx);                                                                            \
    auto _promise = _ctx.timeoutAfter((timeout), (promise));                                       \
    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() { _promise.wait(_ctx.waitScope()); })) { \
      if (exception.getType() == kj::Exception::Type::OVERLOADED) {                                \
        KJ_FAIL_EXPECT("Test timed out after", (timeout));                                         \
      }                                                                                            \
      throw exception;                                                                             \
    }                                                                                              \
  } while (0)

/**
 * @brief Expect that an async operation throws an exception
 *
 * Usage:
 *   VELOZ_EXPECT_THROW(ctx, riskyOperation(ctx.io), kj::Exception::Type::FAILED);
 */
#define VELOZ_EXPECT_THROW(ctx, promise, expected_type)                                            \
  do {                                                                                             \
    auto& _ctx = (ctx);                                                                            \
    auto _result = kj::runCatchingExceptions([&]() { (promise).wait(_ctx.waitScope()); });         \
    KJ_IF_SOME(exception, _result) {                                                               \
      if (exception.getType() != (expected_type)) {                                                \
        KJ_FAIL_EXPECT("Expected exception type", (int)(expected_type), "but got",                 \
                       (int)exception.getType(), ":", exception.getDescription());                 \
      }                                                                                            \
    }                                                                                              \
    else {                                                                                         \
      KJ_FAIL_EXPECT("Expected exception but none was thrown");                                    \
    }                                                                                              \
  } while (0)

} // namespace veloz::test
