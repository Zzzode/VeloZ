#pragma once

#include "audit/audit_logger.h"
#include "auth/auth_manager.h"
#include "middleware.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * Authentication middleware.
 *
 * Validates JWT tokens and API keys from request headers.
 * Populates RequestContext::authInfo with authentication details.
 * Logs authentication attempts to audit log.
 *
 * Supported authentication methods:
 * - JWT via Authorization: Bearer <token>
 * - API Key via X-API-Key: <key>
 *
 * Priority: API Key > JWT
 */
class AuthMiddleware : public Middleware {
public:
  /**
   * @brief Configuration for auth middleware
   */
  struct Config {
    bool require_auth;                   // Require authentication for all endpoints
    kj::Vector<kj::String> public_paths; // Paths that don't require auth
  };

  /**
   * @brief Create auth middleware
   *
   * @param auth_manager The authentication manager
   * @param audit_logger Optional audit logger for auth events
   * @param config Configuration options
   */
  explicit AuthMiddleware(kj::Own<AuthManager> auth_manager,
                          audit::AuditLogger* audit_logger = nullptr,
                          Config config = default_config());

  ~AuthMiddleware() noexcept = default;

  // Non-copyable, non-movable
  AuthMiddleware(const AuthMiddleware&) = delete;
  AuthMiddleware& operator=(const AuthMiddleware&) = delete;
  AuthMiddleware(AuthMiddleware&&) = delete;
  AuthMiddleware& operator=(AuthMiddleware&&) = delete;

  /**
   * @brief Process request through authentication middleware
   *
   * @param ctx The request context
   * @param next Function to call next middleware/handler
   * @return Promise that completes when processing is done
   */
  kj::Promise<void> process(RequestContext& ctx, kj::Function<kj::Promise<void>()> next) override;

  /**
   * @brief Create default configuration
   *
   * Default: auth required for all endpoints except:
   * - /health
   * - /api/health
   * - /api/stream
   * - /api/auth/login
   * - /api/auth/refresh
   */
  static Config default_config();

private:
  kj::Own<AuthManager> auth_manager_;
  audit::AuditLogger* audit_logger_;
  Config config_;

  /**
   * @brief Check if a path is public (doesn't require auth)
   */
  bool is_public_path(kj::StringPtr path) const;

  /**
   * @brief Authenticate request and populate auth info
   *
   * @param ctx The request context
   * @return AuthInfo if authentication succeeds
   */
  kj::Maybe<AuthInfo> authenticate_request(RequestContext& ctx);

  /**
   * @brief Log authentication attempt
   *
   * @param ctx The request context
   * @param success Whether authentication succeeded
   * @param auth_method The method attempted
   */
  void log_auth_attempt(RequestContext& ctx, bool success, kj::StringPtr auth_method = "none"_kj);

  /**
   * @brief Send 401 Unauthorized response
   *
   * @param ctx The request context
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> send_unauthorized(RequestContext& ctx);
};

} // namespace veloz::gateway
