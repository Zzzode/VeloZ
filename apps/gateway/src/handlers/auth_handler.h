#pragma once

#include "audit/audit_logger.h"
#include "auth/api_key_manager.h"
#include "auth/jwt_manager.h"
#include "request_context.h"

#include <kj/async.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * @brief Authentication handler for login, token refresh, logout, and API key management
 *
 * Handles the following endpoints:
 * - POST /api/auth/login - Username/password authentication
 * - POST /api/auth/refresh - Refresh access token
 * - POST /api/auth/logout - Logout (token info logging)
 * - GET /api/auth/keys - List API keys for user
 * - POST /api/auth/keys - Create new API key
 * - DELETE /api/auth/keys/{id} - Revoke API key
 *
 * Security features:
 * - Constant-time password comparison to prevent timing attacks
 * - Secure error messages that don't leak information
 * - Audit logging of all authentication attempts
 * - API key shown only once at creation
 */
class AuthHandler {
public:
  /**
   * @brief Construct AuthHandler with dependencies
   *
   * @param jwt JWT manager for token creation/validation
   * @param api_keys API key manager
   * @param audit Audit logger for authentication events
   */
  explicit AuthHandler(auth::JwtManager& jwt, ApiKeyManager& api_keys, audit::AuditLogger& audit);

  ~AuthHandler();

  // Non-copyable, non-movable
  AuthHandler(const AuthHandler&) = delete;
  AuthHandler& operator=(const AuthHandler&) = delete;
  AuthHandler(AuthHandler&&) = delete;
  AuthHandler& operator=(AuthHandler&&) = delete;

  // ---------------------------------------------------------------------------
  // Authentication Endpoints
  // ---------------------------------------------------------------------------

  /**
   * @brief Handle POST /api/auth/login
   *
   * Request body: {"username": "admin", "password": "..."}
   * Response: {"access_token": "...", "refresh_token": "...", "expires_in": 3600, "token_type":
   * "Bearer"}
   *
   * Validates credentials against VELOZ_ADMIN_PASSWORD environment variable.
   *
   * @param ctx Request context containing body, headers, response
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleLogin(RequestContext& ctx);

  /**
   * @brief Handle POST /api/auth/refresh
   *
   * Request body: {"refresh_token": "..."}
   * Response: {"access_token": "...", "expires_in": 3600}
   *
   * Validates the refresh token and issues a new access token.
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleRefresh(RequestContext& ctx);

  /**
   * @brief Handle POST /api/auth/logout
   *
   * Headers: Authorization: Bearer <token>
   * Response: {"ok": true}
   *
   * Logs the logout event. Note: access tokens cannot be revoked (stateless JWT).
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleLogout(RequestContext& ctx);

  // ---------------------------------------------------------------------------
  // API Key Endpoints
  // ---------------------------------------------------------------------------

  /**
   * @brief Handle GET /api/auth/keys
   *
   * Headers: Authorization: Bearer <token>
   * Response: {"keys": [...]}
   *
   * Lists all API keys for the authenticated user.
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleListApiKeys(RequestContext& ctx);

  /**
   * @brief Handle POST /api/auth/keys
   *
   * Headers: Authorization: Bearer <token>
   * Request body: {"name": "...", "permissions": ["read", "write"]}
   * Response: {"key_id": "...", "raw_key": "...", "message": "Save this key - it will not be shown
   * again"}
   *
   * Creates a new API key for the authenticated user.
   * The raw_key is shown only once and must be saved by the client.
   *
   * @param ctx Request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleCreateApiKey(RequestContext& ctx);

  /**
   * @brief Handle DELETE /api/auth/keys/{id}
   *
   * Headers: Authorization: Bearer <token>
   * Response: {"ok": true}
   *
   * Revokes an API key by its ID.
   *
   * @param ctx Request context
   * @param key_id The API key ID to revoke
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleRevokeApiKey(RequestContext& ctx, kj::StringPtr key_id);

private:
  // ---------------------------------------------------------------------------
  // Private Helper Methods
  // ---------------------------------------------------------------------------

  /**
   * @brief Process login request (internal)
   */
  kj::Promise<void> process_login(kj::String body, RequestContext& ctx);

  /**
   * @brief Process refresh request (internal)
   */
  kj::Promise<void> process_refresh(kj::String body, RequestContext& ctx);

  /**
   * @brief Validate admin password using constant-time comparison
   *
   * @param password Password to validate
   * @return true if password matches VELOZ_ADMIN_PASSWORD
   */
  bool validate_admin_password(kj::StringPtr password);

  // ---------------------------------------------------------------------------
  // Request Parsing Helpers
  // ---------------------------------------------------------------------------

  struct LoginRequest {
    kj::String userId;
    kj::String password;
  };

  struct RefreshRequest {
    kj::String refreshToken;
  };

  kj::Maybe<LoginRequest> parseLoginRequest(const kj::String& body);
  kj::Maybe<RefreshRequest> parseRefreshRequest(const kj::String& body);

  // ---------------------------------------------------------------------------
  // Member Variables
  // ---------------------------------------------------------------------------

  auth::JwtManager& jwt_;
  ApiKeyManager& api_keys_;
  audit::AuditLogger& audit_;
  kj::String admin_password_; // From VELOZ_ADMIN_PASSWORD env var
};

} // namespace veloz::gateway
