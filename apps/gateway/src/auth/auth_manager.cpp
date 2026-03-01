#include "auth_manager.h"

#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::gateway {

AuthManager::AuthManager(kj::Own<auth::JwtManager> jwt_manager,
                         kj::Own<ApiKeyManager> api_key_manager)
    : jwt_manager_(kj::mv(jwt_manager)), api_key_manager_(kj::mv(api_key_manager)) {}

kj::Maybe<AuthInfo> AuthManager::authenticate(const kj::HttpHeaders& headers) {
  // Priority: API Key > JWT

  // Check for API key first
  bool found_api_key = false;
  kj::StringPtr api_key_value;

  headers.forEach(
      [&found_api_key, &api_key_value](kj::StringPtr name, kj::StringPtr value) mutable -> void {
        if (name == "X-API-Key"_kj) {
          api_key_value = value;
          found_api_key = true;
        }
      });

  if (found_api_key && api_key_value.size() > 0) {
    auto result = authenticate_api_key(api_key_value);
    if (result != kj::none) {
      return result;
    }
    // API key failed, don't fall back to JWT
    return kj::none;
  }

  // Check for JWT token
  bool found_auth = false;
  kj::StringPtr auth_value;

  headers.forEach(
      [&found_auth, &auth_value](kj::StringPtr name, kj::StringPtr value) mutable -> void {
        if (name == "Authorization"_kj) {
          auth_value = value;
          found_auth = true;
        }
      });

  if (found_auth) {
    const size_t prefix_len = 7; // "Bearer "
    if (auth_value.size() > prefix_len && auth_value.slice(0, prefix_len) == "Bearer "_kj) {
      kj::StringPtr token = auth_value.slice(prefix_len);
      if (token.size() > 0) {
        return authenticate_jwt(token);
      }
    }
  }

  // No valid authentication found
  return kj::none;
}

kj::Maybe<AuthInfo> AuthManager::authenticate_jwt(kj::StringPtr token) {
  auto token_info = jwt_manager_->verify_access_token(token);
  KJ_IF_SOME(info, token_info) {
    // JWT authentication successful
    AuthInfo auth;
    auth.user_id = kj::mv(info.user_id);
    auth.auth_method = kj::str("jwt");

    // If JWT has api_key_id, include it
    KJ_IF_SOME(key_id, info.api_key_id) {
      auth.api_key_id = kj::mv(key_id);
    }

    return kj::mv(auth);
  }

  return kj::none;
}

kj::Maybe<AuthInfo> AuthManager::authenticate_api_key(kj::StringPtr api_key) {
  auto key_info = api_key_manager_->validate(api_key);
  KJ_IF_SOME(key, key_info) {
    // API key authentication successful
    AuthInfo auth;
    auth.user_id = kj::str(key->user_id);
    auth.auth_method = kj::str("api_key");
    auth.api_key_id = kj::str(key->key_id);

    // Copy permissions from API key
    for (auto& perm : key->permissions) {
      auth.permissions.add(kj::str(perm));
    }

    return kj::mv(auth);
  }

  return kj::none;
}

bool AuthManager::has_permission(const AuthInfo& auth, kj::StringPtr permission) const {
  // Check explicit permissions from API key
  for (auto& perm : auth.permissions) {
    if (perm == permission) {
      return true;
    }
  }

  // TODO: Add RBAC lookup for JWT users here
  // For JWT users without explicit permissions, query RBAC

  return false;
}

} // namespace veloz::gateway
