#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/string.h>
#include <kj/vector.h>

// Forward declaration
namespace veloz::core {
class MetricsRegistry;
}

namespace veloz::gateway {

/**
 * Prometheus metrics handler.
 *
 * Handles endpoint:
 * - GET /metrics - Prometheus metrics exposition
 *
 * Performance target: <50μs response time
 */
class MetricsHandler {
public:
  explicit MetricsHandler(core::MetricsRegistry& registry);

  /**
   * Handle GET /metrics
   *
   * Returns Prometheus format metrics text.
   * Format:
   * # HELP metric_name Description
   * # TYPE metric_name counter|gauge|histogram
   * metric_name value
   * metric_name_bucket{le="0.001"} count
   * ...
   */
  kj::Promise<void> handleMetrics(RequestContext& ctx);

private:
  core::MetricsRegistry& registry_;
};

} // namespace veloz::gateway
