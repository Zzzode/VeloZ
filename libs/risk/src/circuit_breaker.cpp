#include "veloz/risk/circuit_breaker.h"

#include <chrono>

namespace veloz::risk {

bool CircuitBreaker::allow_request() {
  auto lock = guarded_.lockExclusive();

  check_auto_reset_internal(*lock);

  return lock->state != CircuitState::Open;
}

void CircuitBreaker::record_success() {
  auto lock = guarded_.lockExclusive();

  if (lock->state == CircuitState::HalfOpen) {
    lock->success_count++;
    if (lock->success_count >= lock->success_threshold) {
      lock->state = CircuitState::Closed;
      lock->failure_count = 0;
      lock->success_count = 0;
    }
  } else if (lock->state == CircuitState::Closed) {
    lock->failure_count = 0;
  }
}

void CircuitBreaker::record_failure() {
  auto lock = guarded_.lockExclusive();

  lock->failure_count++;

  if (lock->state == CircuitState::HalfOpen) {
    // Back to open immediately
    lock->state = CircuitState::Open;
    lock->success_count = 0;
  } else if (lock->state == CircuitState::Closed &&
             lock->failure_count >= lock->failure_threshold) {
    lock->state = CircuitState::Open;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  lock->last_failure_time_ms = now;
}

void CircuitBreaker::reset() {
  auto lock = guarded_.lockExclusive();

  lock->state = CircuitState::HalfOpen;
  lock->success_count = 0;
}

void CircuitBreaker::set_failure_threshold(std::size_t threshold) {
  auto lock = guarded_.lockExclusive();
  lock->failure_threshold = threshold;
}

void CircuitBreaker::set_timeout_ms(std::int64_t timeout_ms) {
  auto lock = guarded_.lockExclusive();
  lock->timeout_ms = timeout_ms;
}

void CircuitBreaker::set_success_threshold(std::size_t threshold) {
  auto lock = guarded_.lockExclusive();
  lock->success_threshold = threshold;
}

CircuitState CircuitBreaker::state() const {
  return guarded_.lockExclusive()->state;
}

void CircuitBreaker::check_auto_reset() {
  auto lock = guarded_.lockExclusive();
  check_auto_reset_internal(*lock);
}

void CircuitBreaker::check_auto_reset_internal(BreakerState& state) {
  if (state.state != CircuitState::Open)
    return;

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  if (now - state.last_failure_time_ms >= state.timeout_ms) {
    state.state = CircuitState::HalfOpen;
    state.success_count = 0;
  }
}

} // namespace veloz::risk
