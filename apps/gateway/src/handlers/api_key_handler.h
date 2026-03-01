#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declarations
class AuthManager;

/**
 * API key management handler.
 *
 * Handles endpoints:
 * - GET /api/auth/keys - List API keys
 * - POST /api/auth/keys - Create new API key
 * - DELETE /api/auth/keys/:id - Revoke API key
 */
class APIKeyHandler {
public:
  explicit APIKeyHandler(AuthManager* authManager);

  /**
   * Handle GET /api/auth/keys
   *
   * Query parameters:
   * - user_id: Filter by user ID (optional)
   *
   * Returns JSON array of API key info (public info only).
   */
  kj::Promise<void> handleListKeys(RequestContext& ctx);

  /**
   * Handle POST /api/auth/keys
   *
   * Request body (JSON):
   * {
   *   "user_id": string,
   *   "name": string,
   *   "permissions": ["read", "write"]
   * }
   *
   * Returns JSON with key_id and raw_key (only shown once).
   */
  kj::Promise<void> handleCreateKey(RequestContext& ctx);

  /**
   * Handle DELETE /api/auth/keys/:id
   *
   * Path parameter: id (key_id)
   *
   * Returns JSON: {"ok": true/false}
   */
  kj::Promise<void> handleRevokeKey(RequestContext& ctx);

private:
  AuthManager* authManager_;
};

} // namespace veloz::gateway
