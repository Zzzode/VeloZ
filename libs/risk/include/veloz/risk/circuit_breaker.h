#pragma once

#include <cstdint>
#include <kj/mutex.h>

namespace veloz::risk {

enum class CircuitState {
  Closed,   // Normal operation
  Open,     // Circuit is tripped, blocking requests
  HalfOpen, // Testing if service recovered
};

class CircuitBreaker final {
public:
  CircuitBreaker() = default;

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
  };

  void check_auto_reset_internal(BreakerState& state);

  kj::MutexGuarded<BreakerState> guarded_;
};

} // namespace veloz::risk
