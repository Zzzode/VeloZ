#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declarations
class RBACManager;

/**
 * Role management handler.
 *
 * Handles endpoints:
 * - GET /api/auth/roles - Get user roles
 * - POST /api/auth/roles - Assign/remove roles
 */
class RoleHandler {
public:
  explicit RoleHandler(RBACManager* rbacManager);

  /**
   * Handle GET /api/auth/roles
   *
   * Query parameters:
   * - user_id: User ID to get roles for (required)
   *
   * Returns JSON with user role info:
   * {
   *   "user_id": string,
   *   "roles": [string],
   *   "created_at": timestamp,
   *   "updated_at": timestamp
   * }
   */
  kj::Promise<void> handleGetRoles(RequestContext& ctx);

  /**
   * Handle POST /api/auth/roles
   *
   * Request body (JSON):
   * {
   *   "user_id": string,
   *   "action": "assign" | "remove" | "set",
   *   "role": string,
   *   "roles": [string]  // for "set" action
   * }
   *
   * Returns JSON: {"ok": true/false}
   */
  kj::Promise<void> handleManageRoles(RequestContext& ctx);

private:
  RBACManager* rbacManager_;
};

} // namespace veloz::gateway
