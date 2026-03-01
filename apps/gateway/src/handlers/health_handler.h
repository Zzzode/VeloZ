#pragma once

#include "request_context.h"

#include <kj/async-io.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::gateway {

// Forward declarations
namespace bridge {
class EngineBridge;
}

/**
 * Health check handler.
 *
 * Handles endpoints:
 * - GET /health - Simple health check (public, no auth required)
 * - GET /api/health - Detailed health with engine status (requires Read permission)
 *
 * Performance targets:
 * - Simple health: <10μs
 * - Detailed health: <50μs
 */
class HealthHandler {
public:
  /**
   * @brief Construct HealthHandler with EngineBridge reference
   *
   * @param bridge EngineBridge for engine status queries
   */
  explicit HealthHandler(bridge::EngineBridge& bridge);

  /**
   * @brief Handle GET /health
   *
   * Simple health check - returns 200 if the process is alive.
   * No authentication required.
   *
   * Response format:
   * {
   *   "status": "ok",
   *   "timestamp": "2026-02-27T09:26:00Z"
   * }
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleSimpleHealth(RequestContext& ctx);

  /**
   * @brief Handle GET /api/health
   *
   * Detailed health check with engine status, uptime, and metrics.
   * Requires Read permission.
   *
   * Response format:
   * {
   *   "status": "ok",
   *   "timestamp": "2026-02-27T09:26:00Z",
   *   "engine": {
   *     "running": true,
   *     "uptime_seconds": 3600,
   *     "orders_processed": 1234
   *   },
   *   "memory_mb": 45.2,
   *   "version": "1.0.0"
   * }
   *
   * @param ctx Request context with auth info
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleDetailedHealth(RequestContext& ctx);

  /**
   * @brief Handle GET /api/execution/ping
   *
   * Engine connectivity check - returns pong with engine connection status.
   * No authentication required.
   *
   * Response format:
   * {
   *   "pong": true,
   *   "engine_connected": true
   * }
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleExecutionPing(RequestContext& ctx);

private:
  bridge::EngineBridge& bridge_;
  kj::Date start_time_;

  /**
   * @brief Format a timestamp as ISO 8601 string
   */
  static kj::String format_timestamp(std::int64_t unix_ts);

  /**
   * @brief Get current process memory usage in MB
   */
  static double get_memory_usage_mb();
};

} // namespace veloz::gateway
