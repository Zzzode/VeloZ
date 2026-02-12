#include "veloz/risk/circuit_breaker.h"

#include <chrono>

namespace veloz::risk {

bool CircuitBreaker::allow_request() {
  std::lock_guard<std::mutex> lock(mutex_);

  check_auto_reset();

  return state_ != CircuitState::Open;
}

void CircuitBreaker::record_success() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ == CircuitState::HalfOpen) {
    success_count_++;
    if (success_count_ >= success_threshold_) {
      state_ = CircuitState::Closed;
      failure_count_ = 0;
      success_count_ = 0;
    }
  } else if (state_ == CircuitState::Closed) {
    failure_count_ = 0;
  }
}

void CircuitBreaker::record_failure() {
  std::lock_guard<std::mutex> lock(mutex_);

  failure_count_++;

  if (state_ == CircuitState::HalfOpen) {
    // Back to open immediately
    state_ = CircuitState::Open;
    success_count_ = 0;
  } else if (state_ == CircuitState::Closed && failure_count_ >= failure_threshold_) {
    state_ = CircuitState::Open;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  last_failure_time_ms_ = now;
}

void CircuitBreaker::reset() {
  std::lock_guard<std::mutex> lock(mutex_);

  state_ = CircuitState::HalfOpen;
  success_count_ = 0;
}

void CircuitBreaker::set_failure_threshold(std::size_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  failure_threshold_ = threshold;
}

void CircuitBreaker::set_timeout_ms(std::int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  timeout_ms_ = timeout_ms;
}

void CircuitBreaker::set_success_threshold(std::size_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  success_threshold_ = threshold;
}

CircuitState CircuitBreaker::state() const {
  // Need to cast away const for mutex since we're just reading state
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
  return state_;
}

void CircuitBreaker::check_auto_reset() {
  if (state_ != CircuitState::Open)
    return;

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  if (now - last_failure_time_ms_ >= timeout_ms_) {
    state_ = CircuitState::HalfOpen;
    success_count_ = 0;
  }
}

} // namespace veloz::risk
