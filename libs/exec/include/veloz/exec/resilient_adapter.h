#pragma once

#include "veloz/core/error.h"
#include "veloz/core/metrics.h"
#include "veloz/core/retry.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/risk/circuit_breaker.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::exec {

/**
 * @brief Configuration for resilient adapter behavior
 *
 * Note: std::atomic is used for Stats counters because KJ does not provide
 * an atomic counter type. This is acceptable per CLAUDE.md guidelines for
 * cases where KJ does not provide equivalent functionality.
 */
struct ResilientAdapterConfig {
  // Retry configuration
  int max_retries = 3;
  kj::Duration initial_retry_delay{100 * kj::MILLISECONDS};
  kj::Duration max_retry_delay{10 * kj::SECONDS};
  double backoff_multiplier = 2.0;
  double jitter_factor = 0.1;

  // Circuit breaker configuration
  std::size_t failure_threshold = 5;
  std::int64_t circuit_timeout_ms = 60000;
  std::size_t success_threshold = 2;

  // Health check configuration
  bool enable_health_check = true;
  kj::Duration health_check_interval{30 * kj::SECONDS};
};

/**
 * @brief Wrapper that adds resilience (retry + circuit breaker) to any ExchangeAdapter
 *
 * This class wraps an existing ExchangeAdapter and adds:
 * - Automatic retry with exponential backoff
 * - Circuit breaker pattern for fault tolerance
 * - Metrics collection for monitoring
 * - Health check integration
 */
class ResilientExchangeAdapter final : public ExchangeAdapter {
public:
  /**
   * @brief Construct a resilient adapter wrapping an existing adapter
   * @param inner The underlying exchange adapter to wrap (takes ownership)
   * @param config Configuration for resilience behavior
   */
  explicit ResilientExchangeAdapter(kj::Own<ExchangeAdapter> inner,
                                    ResilientAdapterConfig config = {});

  ~ResilientExchangeAdapter() noexcept override = default;

  // ExchangeAdapter interface implementation
  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) override;
  kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) override;
  bool is_connected() const override;
  void connect() override;
  void disconnect() override;
  [[nodiscard]] kj::StringPtr name() const override;
  [[nodiscard]] kj::StringPtr version() const override;

  // Access to underlying adapter
  [[nodiscard]] ExchangeAdapter& inner() noexcept {
    return *inner_;
  }
  [[nodiscard]] const ExchangeAdapter& inner() const noexcept {
    return *inner_;
  }

  // Access to circuit breaker for monitoring
  [[nodiscard]] risk::CircuitBreaker& circuit_breaker() noexcept {
    return circuit_breaker_;
  }
  [[nodiscard]] const risk::CircuitBreaker& circuit_breaker() const noexcept {
    return circuit_breaker_;
  }

  // Access to retry handler for configuration
  [[nodiscard]] core::RetryHandler& retry_handler() noexcept {
    return retry_handler_;
  }
  [[nodiscard]] const core::RetryHandler& retry_handler() const noexcept {
    return retry_handler_;
  }

  // Health check
  [[nodiscard]] bool check_health();

  // Statistics
  struct Stats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> retried_requests{0};
    std::atomic<uint64_t> circuit_breaker_rejections{0};

    void reset() {
      total_requests.store(0);
      successful_requests.store(0);
      failed_requests.store(0);
      retried_requests.store(0);
      circuit_breaker_rejections.store(0);
    }
  };

  [[nodiscard]] const Stats& stats() const noexcept {
    return stats_;
  }
  void reset_stats() {
    stats_.reset();
  }

private:
  // Helper to execute an operation with retry and circuit breaker
  template <typename Func> auto execute_with_resilience(Func&& operation, kj::StringPtr op_name);

  kj::Own<ExchangeAdapter> inner_;
  ResilientAdapterConfig config_;
  risk::CircuitBreaker circuit_breaker_;
  core::RetryHandler retry_handler_;
  Stats stats_;
  kj::String adapter_name_;
};

} // namespace veloz::exec
