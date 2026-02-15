#pragma once

// std::atomic for lightweight flag (KJ MutexGuarded has overhead)
#include <atomic>
#include <cstdint>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/mutex.h>
#include <kj/string.h>

namespace veloz::risk {

enum class CircuitState {
  Closed,   // Normal operation
  Open,     // Circuit is tripped, blocking requests
  HalfOpen, // Testing if service recovered
};

/**
 * @brief Convert CircuitState to string
 */
[[nodiscard]] inline kj::StringPtr to_string(CircuitState state) {
  switch (state) {
  case CircuitState::Closed:
    return "closed"_kj;
  case CircuitState::Open:
    return "open"_kj;
  case CircuitState::HalfOpen:
    return "half_open"_kj;
  }
  return "unknown"_kj;
}

/**
 * @brief Circuit breaker statistics
 */
struct CircuitBreakerStats {
  std::atomic<uint64_t> total_requests{0};
  std::atomic<uint64_t> successful_requests{0};
  std::atomic<uint64_t> failed_requests{0};
  std::atomic<uint64_t> rejected_requests{0};
  std::atomic<uint64_t> state_transitions{0};
  std::atomic<int64_t> last_state_change_ms{0};
  std::atomic<int64_t> time_in_open_ms{0};

  void reset() {
    total_requests.store(0);
    successful_requests.store(0);
    failed_requests.store(0);
    rejected_requests.store(0);
    state_transitions.store(0);
    last_state_change_ms.store(0);
    time_in_open_ms.store(0);
  }
};

/**
 * @brief Callback types for circuit breaker events
 */
using StateChangeCallback = kj::Function<void(CircuitState old_state, CircuitState new_state)>;
using HealthCheckCallback = kj::Function<bool()>;

class CircuitBreaker final {
public:
  CircuitBreaker() = default;
  explicit CircuitBreaker(kj::StringPtr name) : name_(kj::heapString(name)) {}

  // Check if request is allowed
  [[nodiscard]] bool allow_request();

  // Record outcome
  void record_success();
  void record_failure();

  // Manual reset
  void reset();

  // Configuration
  void set_failure_threshold(std::size_t threshold);
  void set_timeout_ms(std::int64_t timeout_ms);
  void set_success_threshold(std::size_t threshold);

  // Query state
  [[nodiscard]] CircuitState state() const;

  // Name for metrics/logging
  [[nodiscard]] kj::StringPtr name() const noexcept {
    return name_;
  }
  void set_name(kj::StringPtr name) {
    name_ = kj::heapString(name);
  }

  // Statistics
  [[nodiscard]] const CircuitBreakerStats& stats() const noexcept {
    return stats_;
  }
  void reset_stats() {
    stats_.reset();
  }

  // Callbacks
  void set_state_change_callback(StateChangeCallback callback) {
    auto lock = guarded_.lockExclusive();
    lock->on_state_change = kj::mv(callback);
  }

  void set_health_check_callback(HealthCheckCallback callback) {
    auto lock = guarded_.lockExclusive();
    lock->health_check = kj::mv(callback);
  }

  /**
   * @brief Execute a health check if configured
   * @return true if healthy or no health check configured
   */
  [[nodiscard]] bool check_health();

  /**
   * @brief Get failure rate (0.0 - 1.0)
   */
  [[nodiscard]] double failure_rate() const {
    auto total = stats_.total_requests.load();
    if (total == 0)
      return 0.0;
    return static_cast<double>(stats_.failed_requests.load()) / static_cast<double>(total);
  }

  /**
   * @brief Get success rate (0.0 - 1.0)
   */
  [[nodiscard]] double success_rate() const {
    return 1.0 - failure_rate();
  }

private:
  void check_auto_reset();

  // Internal state structure
  struct BreakerState {
    CircuitState state{CircuitState::Closed};
    std::size_t failure_count{0};
    std::size_t success_count{0};
    std::int64_t last_failure_time_ms{0};
    std::size_t failure_threshold{5};
    std::int64_t timeout_ms{60000};   // 1 minute default
    std::size_t success_threshold{2}; // Need 2 successes in half-open to close
    kj::Maybe<StateChangeCallback> on_state_change;
    kj::Maybe<HealthCheckCallback> health_check;
  };

  void check_auto_reset_internal(BreakerState& state);
  void transition_state(BreakerState& state, CircuitState new_state);

  kj::String name_{kj::heapString("default")};
  kj::MutexGuarded<BreakerState> guarded_;
  CircuitBreakerStats stats_;
};

/**
 * @brief RAII guard for circuit breaker operations
 *
 * Automatically records success/failure based on scope exit
 */
class CircuitBreakerGuard final {
public:
  explicit CircuitBreakerGuard(CircuitBreaker& breaker) : breaker_(breaker), success_(false) {}

  ~CircuitBreakerGuard() {
    if (success_) {
      breaker_.record_success();
    } else {
      breaker_.record_failure();
    }
  }

  void mark_success() noexcept {
    success_ = true;
  }

  CircuitBreakerGuard(const CircuitBreakerGuard&) = delete;
  CircuitBreakerGuard& operator=(const CircuitBreakerGuard&) = delete;

private:
  CircuitBreaker& breaker_;
  bool success_;
};

} // namespace veloz::risk
