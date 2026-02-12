#pragma once

#include <cstdint>
#include <mutex>

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

  std::mutex mutex_;
  CircuitState state_{CircuitState::Closed};
  std::size_t failure_count_{0};
  std::size_t success_count_{0};
  std::int64_t last_failure_time_ms_{0};

  std::size_t failure_threshold_{5};
  std::int64_t timeout_ms_{60000};   // 1 minute default
  std::size_t success_threshold_{2}; // Need 2 successes in half-open to close
};

} // namespace veloz::risk
