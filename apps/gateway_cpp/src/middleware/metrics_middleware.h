#pragma once

#include "request_context.h"

#include <cstdint>
#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/string.h>
#include <veloz/core/metrics.h>

namespace veloz::gateway {

/**
 * Metrics collection middleware.
 *
 * Records request timing, count, and error rate using MetricsRegistry.
 * Features:
 * - Request duration histogram
 * - Request counter by status code
 * - Active connections gauge
 * - Path normalization for metric aggregation
 * - Status code categorization (2xx, 3xx, 4xx, 5xx)
 * - Automatic timing with RAII pattern
 */
class MetricsMiddleware {
public:
  explicit MetricsMiddleware(veloz::core::MetricsRegistry& registry);

  ~MetricsMiddleware() noexcept;

  /**
   * Process request and collect metrics.
   *
   * @param ctx Request context
   * @param next Next middleware/handler in chain
   * @return Promise that completes when metrics are recorded
   */
  kj::Promise<void> process(RequestContext& ctx, kj::Function<kj::Promise<void>()> next);

  /**
   * Record request metrics (public for testing).
   *
   * @param method HTTP method
   * @param path Request path
   * @param status Response status code
   * @param duration_sec Request duration in seconds
   */
  void record_request(kj::HttpMethod method, kj::StringPtr path, unsigned int status,
                      double duration_sec);

  /**
   * Categorize HTTP status code (public for testing).
   *
   * @param status HTTP status code
   * @return Category string (2xx, 3xx, 4xx, 5xx, unknown)
   */
  kj::String categorize_status(unsigned int status) const;

  /**
   * Normalize path by replacing numeric IDs with placeholders (public for testing).
   *
   * @param path Original path
   * @return Normalized path (e.g., /api/orders/123 -> /api/orders/{id})
   */
  kj::String normalize_path(kj::StringPtr path) const;

private:
  veloz::core::MetricsRegistry& registry_;

  // Cached metric pointers for performance
  veloz::core::Counter* requests_total_ = nullptr;
  veloz::core::Histogram* request_duration_ = nullptr;
  veloz::core::Counter* requests_by_status_ = nullptr;
  veloz::core::Gauge* active_connections_ = nullptr;
};

} // namespace veloz::gateway
