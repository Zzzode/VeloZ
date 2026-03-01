#pragma once

#include "api_key_manager.h"
#include "jwt_manager.h"

#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * @brief Authentication information extracted from a request
 *
 * Contains user identity, permissions, and authentication method used.
 * Populated by AuthMiddleware and accessible to handlers.
 */
struct AuthInfo {
  kj::String user_id;
  kj::Vector<kj::String> permissions;
  kj::String auth_method; // "jwt", "api_key", or "disabled"
  kj::Maybe<kj::String> api_key_id;

  AuthInfo() = default;
  AuthInfo(const AuthInfo&) = delete;
  AuthInfo& operator=(const AuthInfo&) = delete;
  AuthInfo(AuthInfo&&) = default;
  AuthInfo& operator=(AuthInfo&&) = default;
};

/**
 * @brief Unified authentication manager
 *
 * Coordinates JWT and API Key authentication methods.
 * Provides a single interface for authentication and permission checking.
 *
 * Thread safety: All public methods are thread-safe.
 * Performance target: <50μs for authentication validation
 */
class AuthManager {
public:
  explicit AuthManager(kj::Own<auth::JwtManager> jwt_manager,
                       kj::Own<ApiKeyManager> api_key_manager);

  ~AuthManager() = default;

  // Non-copyable, non-movable
  AuthManager(const AuthManager&) = delete;
  AuthManager& operator=(const AuthManager&) = delete;
  AuthManager(AuthManager&&) = delete;
  AuthManager& operator=(AuthManager&&) = delete;

  /**
   * @brief Authenticate using an authorization header
   *
   * Supports two formats:
   * - Authorization: Bearer <jwt_token>
   * - X-API-Key: <api_key>
   *
   * Priority: API Key > JWT
   *
   * @param headers The request headers
   * @return AuthInfo if authentication succeeds, kj::none otherwise
   */
  kj::Maybe<AuthInfo> authenticate(const kj::HttpHeaders& headers);

  /**
   * @brief Check if an AuthInfo has a specific permission
   *
   * @param auth The authentication info to check
   * @param permission The permission string to check
   * @return true if permission is granted
   */
  bool has_permission(const AuthInfo& auth, kj::StringPtr permission) const;

  /**
   * @brief Get the JWT manager
   */
  auth::JwtManager& jwt_manager() {
    return *jwt_manager_;
  }

  /**
   * @brief Get the API key manager
   */
  ApiKeyManager& api_key_manager() {
    return *api_key_manager_;
  }

private:
  kj::Own<auth::JwtManager> jwt_manager_;
  kj::Own<ApiKeyManager> api_key_manager_;

  /**
   * @brief Authenticate using JWT token
   *
   * @param token The JWT token to verify
   * @return AuthInfo if valid, kj::none otherwise
   */
  kj::Maybe<AuthInfo> authenticate_jwt(kj::StringPtr token);

  /**
   * @brief Authenticate using API key
   *
   * @param api_key The API key to validate
   * @return AuthInfo if valid, kj::none otherwise
   */
  kj::Maybe<AuthInfo> authenticate_api_key(kj::StringPtr api_key);
};

} // namespace veloz::gateway
