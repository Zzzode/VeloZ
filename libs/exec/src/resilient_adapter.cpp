#include "veloz/exec/resilient_adapter.h"

#include <cmath>
#include <kj/debug.h>
#include <thread>

namespace veloz::exec {

ResilientExchangeAdapter::ResilientExchangeAdapter(kj::Own<ExchangeAdapter> inner,
                                                   ResilientAdapterConfig config)
    : inner_(kj::mv(inner)), config_(kj::mv(config)),
      circuit_breaker_(kj::str(inner_->name(), "_circuit_breaker").cStr()) {

  // Configure circuit breaker
  circuit_breaker_.set_failure_threshold(config_.failure_threshold);
  circuit_breaker_.set_timeout_ms(config_.circuit_timeout_ms);
  circuit_breaker_.set_success_threshold(config_.success_threshold);

  // Configure retry handler
  // Note: core::RetryConfig uses std::chrono::milliseconds, so we convert from kj::Duration
  core::RetryConfig retry_config;
  retry_config.max_attempts = config_.max_retries;
  retry_config.initial_delay =
      std::chrono::milliseconds(config_.initial_retry_delay / kj::MILLISECONDS);
  retry_config.max_delay = std::chrono::milliseconds(config_.max_retry_delay / kj::MILLISECONDS);
  retry_config.backoff_multiplier = config_.backoff_multiplier;
  retry_config.jitter_factor = config_.jitter_factor;
  retry_config.retry_on_network_error = true;
  retry_config.retry_on_timeout = true;
  retry_config.retry_on_rate_limit = true;
  retry_handler_ = core::RetryHandler(kj::mv(retry_config));

  // Set up circuit breaker state change callback for logging
  // Note: Using kj::String copy for lambda capture
  kj::String adapter_name_copy = kj::str(inner_->name());
  circuit_breaker_.set_state_change_callback(
      [adapter_name = kj::mv(adapter_name_copy)](risk::CircuitState old_state,
                                                 risk::CircuitState new_state) {
        KJ_LOG(WARNING, "Circuit breaker state changed", adapter_name.cStr(),
               risk::to_string(old_state), risk::to_string(new_state));
      });

  // Build adapter name
  adapter_name_ = kj::str("resilient_", inner_->name());
}

namespace {
// Helper to calculate delay with exponential backoff using KJ Duration
kj::Duration calculate_delay(int attempt, kj::Duration initial, kj::Duration max_delay,
                             double multiplier) {
  // Convert to nanoseconds for calculation
  auto initial_ns = initial / kj::NANOSECONDS;
  double delay_ns = static_cast<double>(initial_ns) * std::pow(multiplier, attempt);
  auto max_ns = max_delay / kj::NANOSECONDS;
  delay_ns = std::min(delay_ns, static_cast<double>(max_ns));
  return static_cast<int64_t>(delay_ns) * kj::NANOSECONDS;
}

// Helper to sleep for a KJ Duration
void sleep_for_duration(kj::Duration duration) {
  auto ns = duration / kj::NANOSECONDS;
  std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}
} // namespace

kj::Maybe<ExecutionReport> ResilientExchangeAdapter::place_order(const PlaceOrderRequest& req) {
  stats_.total_requests++;

  // Check circuit breaker first
  if (!circuit_breaker_.allow_request()) {
    stats_.circuit_breaker_rejections++;
    throw core::CircuitBreakerException(kj::str("Circuit breaker open for ", inner_->name()).cStr(),
                                        inner_->name().cStr());
  }

  kj::Maybe<ExecutionReport> result;
  int attempts = 0;
  const int max_attempts = config_.max_retries;

  while (attempts < max_attempts) {
    attempts++;
    try {
      result = inner_->place_order(req);
      circuit_breaker_.record_success();
      stats_.successful_requests++;
      return kj::mv(result);
    } catch (const core::NetworkException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      auto delay = calculate_delay(attempts - 1, config_.initial_retry_delay,
                                   config_.max_retry_delay, config_.backoff_multiplier);
      sleep_for_duration(delay);
    } catch (const core::TimeoutException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      auto delay = calculate_delay(attempts - 1, config_.initial_retry_delay,
                                   config_.max_retry_delay, config_.backoff_multiplier);
      sleep_for_duration(delay);
    } catch (const core::RateLimitException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      kj::Duration delay;
      if (e.retry_after_ms() > 0) {
        delay = e.retry_after_ms() * kj::MILLISECONDS;
      } else {
        delay = calculate_delay(attempts - 1, config_.initial_retry_delay, config_.max_retry_delay,
                                config_.backoff_multiplier);
      }
      sleep_for_duration(delay);
    } catch (const kj::Exception& e) {
      circuit_breaker_.record_failure();
      stats_.failed_requests++;
      throw;
    }
  }

  stats_.failed_requests++;
  throw core::RetryExhaustedException("Retry exhausted for place_order", attempts);
}

kj::Maybe<ExecutionReport> ResilientExchangeAdapter::cancel_order(const CancelOrderRequest& req) {
  stats_.total_requests++;

  // Check circuit breaker first
  if (!circuit_breaker_.allow_request()) {
    stats_.circuit_breaker_rejections++;
    throw core::CircuitBreakerException(kj::str("Circuit breaker open for ", inner_->name()).cStr(),
                                        inner_->name().cStr());
  }

  kj::Maybe<ExecutionReport> result;
  int attempts = 0;
  const int max_attempts = config_.max_retries;

  while (attempts < max_attempts) {
    attempts++;
    try {
      result = inner_->cancel_order(req);
      circuit_breaker_.record_success();
      stats_.successful_requests++;
      return kj::mv(result);
    } catch (const core::NetworkException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      auto delay = calculate_delay(attempts - 1, config_.initial_retry_delay,
                                   config_.max_retry_delay, config_.backoff_multiplier);
      sleep_for_duration(delay);
    } catch (const core::TimeoutException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      auto delay = calculate_delay(attempts - 1, config_.initial_retry_delay,
                                   config_.max_retry_delay, config_.backoff_multiplier);
      sleep_for_duration(delay);
    } catch (const core::RateLimitException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      kj::Duration delay;
      if (e.retry_after_ms() > 0) {
        delay = e.retry_after_ms() * kj::MILLISECONDS;
      } else {
        delay = calculate_delay(attempts - 1, config_.initial_retry_delay, config_.max_retry_delay,
                                config_.backoff_multiplier);
      }
      sleep_for_duration(delay);
    } catch (const kj::Exception& e) {
      circuit_breaker_.record_failure();
      stats_.failed_requests++;
      throw;
    }
  }

  stats_.failed_requests++;
  throw core::RetryExhaustedException("Retry exhausted for cancel_order", attempts);
}

bool ResilientExchangeAdapter::is_connected() const {
  return inner_->is_connected();
}

void ResilientExchangeAdapter::connect() {
  stats_.total_requests++;

  // Check circuit breaker first
  if (!circuit_breaker_.allow_request()) {
    stats_.circuit_breaker_rejections++;
    throw core::CircuitBreakerException(kj::str("Circuit breaker open for ", inner_->name()).cStr(),
                                        inner_->name().cStr());
  }

  int attempts = 0;
  const int max_attempts = config_.max_retries;

  while (attempts < max_attempts) {
    attempts++;
    try {
      inner_->connect();
      circuit_breaker_.record_success();
      stats_.successful_requests++;
      return;
    } catch (const core::NetworkException& e) {
      circuit_breaker_.record_failure();
      if (attempts >= max_attempts) {
        stats_.failed_requests++;
        throw;
      }
      stats_.retried_requests++;
      auto delay = calculate_delay(attempts - 1, config_.initial_retry_delay,
                                   config_.max_retry_delay, config_.backoff_multiplier);
      sleep_for_duration(delay);
    } catch (const kj::Exception& e) {
      circuit_breaker_.record_failure();
      stats_.failed_requests++;
      throw;
    }
  }

  stats_.failed_requests++;
  throw core::RetryExhaustedException("Retry exhausted for connect", attempts);
}

void ResilientExchangeAdapter::disconnect() {
  inner_->disconnect();
}

kj::StringPtr ResilientExchangeAdapter::name() const {
  return adapter_name_;
}

kj::StringPtr ResilientExchangeAdapter::version() const {
  return inner_->version();
}

bool ResilientExchangeAdapter::check_health() {
  if (!config_.enable_health_check) {
    return true;
  }

  // Check circuit breaker health
  if (!circuit_breaker_.check_health()) {
    return false;
  }

  // Check if circuit is open
  if (circuit_breaker_.state() == risk::CircuitState::Open) {
    return false;
  }

  // Check connection status
  return inner_->is_connected();
}

} // namespace veloz::exec
