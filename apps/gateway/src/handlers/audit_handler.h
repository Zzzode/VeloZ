#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/string.h>
#include <kj/vector.h>

// Forward declarations
namespace veloz::gateway::audit {
class AuditStore;
}

namespace veloz::gateway {

/**
 * Audit log handler.
 *
 * Handles endpoints:
 * - GET /api/audit/logs - Query audit logs
 * - GET /api/audit/stats - Get audit statistics
 * - POST /api/audit/archive - Trigger log archive
 *
 * Performance target: <100μs response time
 */
class AuditHandler {
public:
  explicit AuditHandler(audit::AuditStore& store);

  /**
   * Handle GET /api/audit/logs
   *
   * Query parameters:
   * - type: Filter by log type (Auth, Order, ApiKey, Error, Access)
   * - user_id: Filter by user ID (optional)
   * - ip_address: Filter by IP address (optional)
   * - start_time: Filter by start time (ISO8601, optional)
   * - end_time: Filter by end time (ISO8601, optional)
   * - limit: Maximum number of results (default: 100)
   * - offset: Pagination offset (default: 0)
   *
   * Returns JSON array of audit log entries.
   */
  kj::Promise<void> handleQueryLogs(RequestContext& ctx);

  /**
   * Handle GET /api/audit/stats
   *
   * Returns JSON object with audit statistics:
   * - total_entries: Total log entries
   * - auth_count: Authentication events
   * - order_count: Order events
   * - apikey_count: API key events
   * - error_count: Error events
   * - access_count: Access events
   */
  kj::Promise<void> handleGetStats(RequestContext& ctx);

  /**
   * Handle POST /api/audit/archive
   *
   * Triggers manual archive of audit logs.
   */
  kj::Promise<void> handleTriggerArchive(RequestContext& ctx);

private:
  audit::AuditStore& store_;
};

} // namespace veloz::gateway
