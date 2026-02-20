#pragma once

#include "veloz/core/error.h"
#include "veloz/core/metrics.h"

#include <chrono>
#include <cmath>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/function.h>
#include <kj/string.h>
#include <random>
#include <thread>

namespace veloz::core {

/**
 * @brief Configuration for retry behavior
 *
 * Note: The should_retry predicate uses kj::Exception instead of std::exception
 * to integrate with KJ's exception handling infrastructure.
 *
 * This struct is move-only because kj::Function is not copyable.
 */
struct RetryConfig {
  int max_attempts = 3;                         // Maximum number of retry attempts
  std::chrono::milliseconds initial_delay{100}; // Initial delay before first retry
  std::chrono::milliseconds max_delay{30000};   // Maximum delay between retries
  double backoff_multiplier = 2.0;              // Exponential backoff multiplier
  double jitter_factor = 0.1;                   // Random jitter factor (0.0 - 1.0)
  bool retry_on_timeout = true;                 // Retry on timeout errors
  bool retry_on_network_error = true;           // Retry on network errors
  bool retry_on_rate_limit = true;              // Retry on rate limit errors
  kj::Maybe<kj::Function<bool(const kj::Exception&)>>
      should_retry; // Custom retry predicate (KJ-based)

  // Default constructor
  RetryConfig() = default;

  // Move operations
  RetryConfig(RetryConfig&&) = default;
  RetryConfig& operator=(RetryConfig&&) = default;

  // Disable copy operations (kj::Function is not copyable)
  RetryConfig(const RetryConfig&) = delete;
  RetryConfig& operator=(const RetryConfig&) = delete;
};

/**
 * @brief Result of a retry operation
 */
template <typename T> struct RetryResult {
  bool success = false;
  T value{};
  int attempts = 0;
  std::chrono::milliseconds total_delay{0};
  kj::String last_error;
};

/**
 * @brief Retry handler with exponential backoff and jitter
 *
 * Implements the "full jitter" algorithm for exponential backoff:
 * delay = random(0, min(max_delay, initial_delay * 2^attempt))
 */
class RetryHandler final {
public:
  // Default constructor with default config
  RetryHandler() : config_() {}

  // Move constructor for custom config
  explicit RetryHandler(RetryConfig&& config) : config_(kj::mv(config)) {}

  /**
   * @brief Execute a function with retry logic
   * @param operation The operation to execute
   * @param operation_name Name for metrics/logging
   * @return RetryResult containing success status and value
   */
  template <typename Func>
  auto execute(Func&& operation, kj::StringPtr operation_name = "operation")
      -> RetryResult<decltype(operation())> {
    using ResultType = decltype(operation());
    RetryResult<ResultType> result;

    for (int attempt = 0; attempt < config_.max_attempts; ++attempt) {
      result.attempts = attempt + 1;

      try {
        result.value = operation();
        result.success = true;
        record_success(operation_name);
        return result;
      } catch (const RateLimitException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_rate_limit || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "rate_limit");
          throw;
        }
        // Use retry_after_ms if provided, otherwise use calculated delay
        auto delay = e.retry_after_ms() > 0 ? std::chrono::milliseconds(e.retry_after_ms())
                                            : calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "rate_limit");
        sleep_with_jitter(delay);
      } catch (const TimeoutException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_timeout || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "timeout");
          throw;
        }
        auto delay = calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "timeout");
        sleep_with_jitter(delay);
      } catch (const NetworkException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_network_error || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "network");
          throw;
        }
        auto delay = calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "network");
        sleep_with_jitter(delay);
      } catch (const CircuitBreakerException& e) {
        // Never retry circuit breaker errors
        result.last_error = kj::str(e.what());
        record_failure(operation_name, "circuit_breaker");
        throw;
      } catch (...) {
        // Catch any other exception and convert to kj::Exception
        auto kjException = kj::getCaughtExceptionAsKj();
        result.last_error = kj::str(kjException.getDescription());
        // Use KJ_IF_SOME pattern for kj::Maybe<kj::Function>
        bool should_retry_result = false;
        KJ_IF_SOME(retry_fn, config_.should_retry) {
          should_retry_result = retry_fn(kjException);
        }
        if (should_retry_result) {
          if (attempt < config_.max_attempts - 1) {
            auto delay = calculate_delay(attempt);
            result.total_delay += delay;
            record_retry(operation_name, "custom");
            sleep_with_jitter(delay);
            continue;
          }
        }
        record_failure(operation_name, "unknown");
        kj::throwFatalException(kj::mv(kjException));
      }
    }

    // Should not reach here, but if we do, throw retry exhausted
    throw RetryExhaustedException(
        kj::str("Retry exhausted after ", result.attempts, " attempts: ", result.last_error),
        result.attempts);
  }

  /**
   * @brief Execute a void function with retry logic
   */
  template <typename Func>
  auto execute_void(Func&& operation, kj::StringPtr operation_name = "operation")
      -> RetryResult<bool> {
    RetryResult<bool> result;

    for (int attempt = 0; attempt < config_.max_attempts; ++attempt) {
      result.attempts = attempt + 1;

      try {
        operation();
        result.success = true;
        result.value = true;
        record_success(operation_name);
        return result;
      } catch (const RateLimitException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_rate_limit || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "rate_limit");
          throw;
        }
        auto delay = e.retry_after_ms() > 0 ? std::chrono::milliseconds(e.retry_after_ms())
                                            : calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "rate_limit");
        sleep_with_jitter(delay);
      } catch (const TimeoutException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_timeout || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "timeout");
          throw;
        }
        auto delay = calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "timeout");
        sleep_with_jitter(delay);
      } catch (const NetworkException& e) {
        result.last_error = kj::str(e.what());
        if (!config_.retry_on_network_error || attempt == config_.max_attempts - 1) {
          record_failure(operation_name, "network");
          throw;
        }
        auto delay = calculate_delay(attempt);
        result.total_delay += delay;
        record_retry(operation_name, "network");
        sleep_with_jitter(delay);
      } catch (const CircuitBreakerException& e) {
        result.last_error = kj::str(e.what());
        record_failure(operation_name, "circuit_breaker");
        throw;
      } catch (...) {
        // Catch any other exception and convert to kj::Exception
        auto kjException = kj::getCaughtExceptionAsKj();
        result.last_error = kj::str(kjException.getDescription());
        // Use KJ_IF_SOME pattern for kj::Maybe<kj::Function>
        bool should_retry_result = false;
        KJ_IF_SOME(retry_fn, config_.should_retry) {
          should_retry_result = retry_fn(kjException);
        }
        if (should_retry_result) {
          if (attempt < config_.max_attempts - 1) {
            auto delay = calculate_delay(attempt);
            result.total_delay += delay;
            record_retry(operation_name, "custom");
            sleep_with_jitter(delay);
            continue;
          }
        }
        record_failure(operation_name, "unknown");
        kj::throwFatalException(kj::mv(kjException));
      }
    }

    throw RetryExhaustedException(
        kj::str("Retry exhausted after ", result.attempts, " attempts: ", result.last_error),
        result.attempts);
  }

  [[nodiscard]] const RetryConfig& config() const noexcept {
    return config_;
  }

private:
  [[nodiscard]] std::chrono::milliseconds calculate_delay(int attempt) const {
    // Exponential backoff: initial_delay * multiplier^attempt
    double delay_ms = static_cast<double>(config_.initial_delay.count()) *
                      std::pow(config_.backoff_multiplier, attempt);

    // Cap at max_delay
    delay_ms = kj::min(delay_ms, static_cast<double>(config_.max_delay.count()));

    return std::chrono::milliseconds(static_cast<int64_t>(delay_ms));
  }

  void sleep_with_jitter(std::chrono::milliseconds base_delay) {
    if (config_.jitter_factor > 0.0) {
      std::random_device rd;
      std::mt19937 gen(rd());
      double jitter_range = base_delay.count() * config_.jitter_factor;
      std::uniform_real_distribution<> dis(-jitter_range, jitter_range);
      auto jittered_delay =
          std::chrono::milliseconds(static_cast<int64_t>(base_delay.count() + dis(gen)));
      std::this_thread::sleep_for(jittered_delay);
    } else {
      std::this_thread::sleep_for(base_delay);
    }
  }

  void record_success(kj::StringPtr operation_name) {
    counter_inc("api_requests_total");
    counter_inc(kj::str("api_success_", operation_name).cStr());
  }

  void record_failure(kj::StringPtr operation_name, kj::StringPtr error_type) {
    counter_inc("api_errors_total");
    counter_inc(kj::str("api_error_", error_type).cStr());
    counter_inc(kj::str("api_failure_", operation_name).cStr());
  }

  void record_retry(kj::StringPtr operation_name, kj::StringPtr error_type) {
    counter_inc("api_retries_total");
    counter_inc(kj::str("api_retry_", error_type).cStr());
    counter_inc(kj::str("api_retry_", operation_name).cStr());
  }

  RetryConfig config_;
};

/**
 * @brief Create a default retry handler for API calls
 */
[[nodiscard]] inline RetryHandler make_api_retry_handler() {
  RetryConfig config;
  config.max_attempts = 3;
  config.initial_delay = std::chrono::milliseconds(100);
  config.max_delay = std::chrono::milliseconds(10000);
  config.backoff_multiplier = 2.0;
  config.jitter_factor = 0.1;
  return RetryHandler(kj::mv(config));
}

/**
 * @brief Create a retry handler for critical operations
 */
[[nodiscard]] inline RetryHandler make_critical_retry_handler() {
  RetryConfig config;
  config.max_attempts = 5;
  config.initial_delay = std::chrono::milliseconds(50);
  config.max_delay = std::chrono::milliseconds(5000);
  config.backoff_multiplier = 1.5;
  config.jitter_factor = 0.2;
  return RetryHandler(kj::mv(config));
}

} // namespace veloz::core
