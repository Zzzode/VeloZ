#include "veloz/risk/circuit_breaker.h"

#include <chrono>

namespace veloz::risk {

namespace {
int64_t current_time_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
} // namespace

bool CircuitBreaker::allow_request() {
  auto lock = guarded_.lockExclusive();

  check_auto_reset_internal(*lock);

  stats_.total_requests++;

  if (lock->state == CircuitState::Open) {
    stats_.rejected_requests++;
    return false;
  }

  return true;
}

void CircuitBreaker::record_success() {
  auto lock = guarded_.lockExclusive();

  stats_.successful_requests++;

  if (lock->state == CircuitState::HalfOpen) {
    lock->success_count++;
    if (lock->success_count >= lock->success_threshold) {
      transition_state(*lock, CircuitState::Closed);
      lock->failure_count = 0;
      lock->success_count = 0;
    }
  } else if (lock->state == CircuitState::Closed) {
    lock->failure_count = 0;
  }
}

void CircuitBreaker::record_failure() {
  auto lock = guarded_.lockExclusive();

  stats_.failed_requests++;
  lock->failure_count++;

  if (lock->state == CircuitState::HalfOpen) {
    // Back to open immediately
    transition_state(*lock, CircuitState::Open);
    lock->success_count = 0;
  } else if (lock->state == CircuitState::Closed &&
             lock->failure_count >= lock->failure_threshold) {
    transition_state(*lock, CircuitState::Open);
  }

  lock->last_failure_time_ms = current_time_ms();
}

void CircuitBreaker::reset() {
  auto lock = guarded_.lockExclusive();

  transition_state(*lock, CircuitState::HalfOpen);
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

bool CircuitBreaker::check_health() {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME (health_check, lock->health_check) {
    return health_check();
  }
  return true; // No health check configured, assume healthy
}

void CircuitBreaker::check_auto_reset() {
  auto lock = guarded_.lockExclusive();
  check_auto_reset_internal(*lock);
}

void CircuitBreaker::check_auto_reset_internal(BreakerState& state) {
  if (state.state != CircuitState::Open)
    return;

  auto now = current_time_ms();

  // Track time spent in open state
  if (state.last_failure_time_ms > 0) {
    auto time_in_open = now - state.last_failure_time_ms;
    stats_.time_in_open_ms.fetch_add(time_in_open);
  }

  if (now - state.last_failure_time_ms >= state.timeout_ms) {
    transition_state(state, CircuitState::HalfOpen);
    state.success_count = 0;
  }
}

void CircuitBreaker::transition_state(BreakerState& state, CircuitState new_state) {
  if (state.state == new_state) {
    return;
  }

  CircuitState old_state = state.state;
  state.state = new_state;
  stats_.state_transitions++;
  stats_.last_state_change_ms.store(current_time_ms());

  // Invoke callback if set
  KJ_IF_SOME (callback, state.on_state_change) {
    // Note: callback is called with lock held - keep callbacks fast
    callback(old_state, new_state);
  }
}

} // namespace veloz::risk
