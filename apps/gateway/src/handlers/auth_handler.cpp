#include "auth_handler.h"

#include <cstdlib>
#include <cstring>
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/exception.h>
#include <kj/string.h>

// Include yyjson for JSON parsing
extern "C" {
#include <yyjson.h>
}

namespace veloz::gateway {

namespace {

// HTTP headers we'll use
// CONTENT_TYPE is a built-in header, AUTHORIZATION needs custom lookup
static kj::Maybe<kj::HttpHeaderId> HEADER_AUTHORIZATION;
static bool headers_initialized = false;

/**
 * @brief Initialize HTTP header IDs lazily
 */
void init_headers(const kj::HttpHeaderTable& headerTable) {
  if (!headers_initialized) {
    HEADER_AUTHORIZATION = headerTable.stringToId("Authorization"_kj);
    headers_initialized = true;
  }
}

/**
 * @brief Extract Bearer token from Authorization header
 */
kj::Maybe<kj::String> extract_bearer_token(const kj::HttpHeaders& headers) {
  KJ_IF_SOME(auth_header_id, HEADER_AUTHORIZATION) {
    KJ_IF_SOME(auth_header, headers.get(auth_header_id)) {
      // Format: "Bearer <token>"
      const size_t prefix_len = 7; // "Bearer "

      if (auth_header.size() > prefix_len && auth_header.slice(0, prefix_len) == "Bearer "_kj) {
        return kj::str(auth_header.slice(prefix_len));
      }
    }
  }
  return kj::none;
}

/**
 * @brief Parse JSON body and extract string field
 */
kj::Maybe<kj::String> extract_json_string(yyjson_val* obj, kj::StringPtr key) {
  yyjson_val* val = yyjson_obj_get(obj, key.cStr());
  if (!val || !yyjson_is_str(val)) {
    return kj::none;
  }
  return kj::str(yyjson_get_str(val));
}

/**
 * @brief Parse JSON body and extract string array field
 */
kj::Maybe<kj::Vector<kj::String>> extract_json_string_array(yyjson_val* obj, kj::StringPtr key) {
  yyjson_val* val = yyjson_obj_get(obj, key.cStr());
  if (!val || !yyjson_is_arr(val)) {
    return kj::none;
  }

  kj::Vector<kj::String> result;
  size_t idx, max;
  yyjson_val* elem;
  yyjson_arr_foreach(val, idx, max, elem) {
    if (yyjson_is_str(elem)) {
      result.add(kj::str(yyjson_get_str(elem)));
    }
  }

  return kj::mv(result);
}

/**
 * @brief Send a JSON response
 */
kj::Promise<void> send_json(kj::HttpService::Response& response,
                            const kj::HttpHeaderTable& headerTable, int status,
                            kj::StringPtr status_text, kj::StringPtr body) {
  kj::HttpHeaders responseHeaders(headerTable);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json; charset=utf-8"_kj);

  auto stream = response.send(status, status_text, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream));
}

/**
 * @brief Send an error response
 */
kj::Promise<void> send_error(kj::HttpService::Response& response,
                             const kj::HttpHeaderTable& headerTable, int status,
                             kj::StringPtr message) {
  // Secure error messages - don't leak details
  auto body = kj::str("{\"error\":\"", message, "\"}");
  return send_json(response, headerTable, status, "Error"_kj, body);
}

/**
 * @brief Format timestamp as ISO 8601
 */
kj::String format_timestamp(int64_t unix_ts) {
  std::time_t time = static_cast<std::time_t>(unix_ts);
  std::tm tm;
  localtime_r(&time, &tm);

  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);

  return kj::str(buf);
}

} // namespace

// =============================================================================
// AuthHandler Implementation
// =============================================================================

AuthHandler::AuthHandler(auth::JwtManager& jwt, ApiKeyManager& api_keys, audit::AuditLogger& audit)
    : jwt_(jwt), api_keys_(api_keys), audit_(audit),
      admin_password_(
          kj::heapString(getenv("VELOZ_ADMIN_PASSWORD") ? getenv("VELOZ_ADMIN_PASSWORD") : "")) {}

AuthHandler::~AuthHandler() = default;

// -----------------------------------------------------------------------------
// Login Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleLogin(RequestContext& ctx) {
  init_headers(ctx.headerTable);

  return ctx.body.readAllText().then(
      [this, &ctx](kj::String body) { return process_login(kj::mv(body), ctx); });
}

kj::Promise<void> AuthHandler::process_login(kj::String body, RequestContext& ctx) {
  // Parse JSON request
  yyjson_doc* doc = yyjson_read(body.cStr(), body.size(), 0);
  if (!doc) {
    KJ_LOG(WARNING, "Failed to parse login JSON");
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("login_failed"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "invalid_request"_kj);
    co_return;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);

  // Extract username and password
  auto username_opt = extract_json_string(root, "username"_kj);
  auto password_opt = extract_json_string(root, "password"_kj);

  if (username_opt == kj::none || password_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("login_failed"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "invalid_credentials_format"_kj);
    co_return;
  }

  kj::String username = KJ_ASSERT_NONNULL(kj::mv(username_opt));
  kj::String password = KJ_ASSERT_NONNULL(kj::mv(password_opt));

  // Validate credentials
  bool is_valid = validate_admin_password(password);

  if (!is_valid) {
    KJ_LOG(WARNING, "Login attempt with invalid password");
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("login_failed"),
                        kj::heapString(username), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_credentials"_kj);
    co_return;
  }

  // Create tokens
  auto access_token = jwt_.create_access_token(username);
  auto refresh_token = jwt_.create_refresh_token(username);

  // Get token info for response
  auto access_info = jwt_.verify_access_token(access_token);
  KJ_ASSERT(access_info != kj::none, "Fresh access token should verify");

  int64_t expires_in = 0;
  KJ_IF_SOME(info, access_info) {
    expires_in = info.expires_at - info.issued_at;
  }

  // Create response body
  auto response_body =
      kj::str("{\"access_token\":\"", access_token, "\",\"refresh_token\":\"", refresh_token,
              "\",\"expires_in\":", expires_in, ",\"token_type\":\"Bearer\"}");

  // Log successful login
  co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("login_success"),
                      kj::heapString(username), kj::str("unknown"));

  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// Refresh Token Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleRefresh(RequestContext& ctx) {
  init_headers(ctx.headerTable);

  return ctx.body.readAllText().then(
      [this, &ctx](kj::String body) { return process_refresh(kj::mv(body), ctx); });
}

kj::Promise<void> AuthHandler::process_refresh(kj::String body, RequestContext& ctx) {
  // Parse JSON request
  yyjson_doc* doc = yyjson_read(body.cStr(), body.size(), 0);
  if (!doc) {
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("refresh_failed"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "invalid_request"_kj);
    co_return;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);

  // Extract refresh token
  auto refresh_token_opt = extract_json_string(root, "refresh_token"_kj);

  if (refresh_token_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("refresh_failed"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "missing_refresh_token"_kj);
    co_return;
  }

  kj::String refresh_token = KJ_ASSERT_NONNULL(kj::mv(refresh_token_opt));

  // Verify refresh token
  auto token_info = jwt_.verify_refresh_token(refresh_token);

  if (token_info == kj::none) {
    KJ_LOG(WARNING, "Invalid refresh token attempt");
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("refresh_invalid"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_token"_kj);
    co_return;
  }

  // Create new access token
  kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);
  auto new_access_token = jwt_.create_access_token(user_id);

  // Get new token expiry
  auto access_info = jwt_.verify_access_token(new_access_token);
  KJ_ASSERT(access_info != kj::none, "Fresh access token should verify");

  int64_t expires_in = 0;
  KJ_IF_SOME(info, access_info) {
    expires_in = info.expires_at - info.issued_at;
  }

  // Create response body
  auto response_body =
      kj::str("{\"access_token\":\"", new_access_token, "\",\"expires_in\":", expires_in, "}");

  // Log successful refresh
  co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("refresh_success"),
                      kj::heapString(user_id), kj::str("unknown"));
  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// Logout Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleLogout(RequestContext& ctx) {
  init_headers(ctx.headerTable);

  // Extract access token from Authorization header
  auto token_opt = extract_bearer_token(ctx.headers);

  if (token_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("logout_failed"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "unauthorized"_kj);
    co_return;
  }

  kj::String token = KJ_ASSERT_NONNULL(kj::mv(token_opt));

  // Verify access token to get user_id
  auto token_info = jwt_.verify_access_token(token);

  if (token_info == kj::none) {
    co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("logout_invalid"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_token"_kj);
    co_return;
  }

  // Note: Access tokens cannot be revoked in this implementation (they're stateless)
  // Only refresh tokens can be revoked. The client should discard the access token.

  kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);

  // Log logout
  co_await audit_.log(audit::AuditLogType::Auth, kj::heapString("logout"), kj::heapString(user_id),
                      kj::str("unknown"));

  auto response_body = kj::str("{\"ok\":true}");
  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// API Key List Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleListApiKeys(RequestContext& ctx) {
  init_headers(ctx.headerTable);

  // Extract access token from Authorization header
  auto token_opt = extract_bearer_token(ctx.headers);

  if (token_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("list_keys_unauthorized"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "unauthorized"_kj);
    co_return;
  }

  kj::String token = KJ_ASSERT_NONNULL(kj::mv(token_opt));

  // Verify access token
  auto token_info = jwt_.verify_access_token(token);

  if (token_info == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("list_keys_invalid"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_token"_kj);
    co_return;
  }

  kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);

  // List API keys for this user
  auto keys = api_keys_.list_keys(user_id);

  // Build JSON response
  kj::Vector<kj::String> key_jsons;
  for (auto& key : keys) {
    auto created_str = format_timestamp(std::chrono::system_clock::to_time_t(key->created_at));
    auto last_used_str = format_timestamp(std::chrono::system_clock::to_time_t(key->last_used));

    kj::String key_json = kj::str(
        "{\"key_id\":\"", key->key_id, "\",\"user_id\":\"", key->user_id, "\",\"name\":\"",
        key->name, "\",\"created_at\":\"", created_str, "\",\"last_used\":\"", last_used_str,
        "\",\"revoked\":", key->revoked ? "true" : "false", ",\"permissions\":[");

    // Add permissions
    for (size_t i = 0; i < key->permissions.size(); ++i) {
      if (i > 0)
        key_json = kj::str(key_json, ",");
      key_json = kj::str(key_json, "\"", key->permissions[i], "\"");
    }
    key_json = kj::str(key_json, "]}");

    key_jsons.add(kj::mv(key_json));
  }

  // Combine all key JSONs into array
  kj::String response_body = kj::str("{\"keys\":[");
  for (size_t i = 0; i < key_jsons.size(); ++i) {
    if (i > 0)
      response_body = kj::str(response_body, ",");
    response_body = kj::str(response_body, key_jsons[i]);
  }
  response_body = kj::str(response_body, "]}");

  co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("list_keys_success"),
                      kj::heapString(user_id), kj::str("unknown"));
  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// API Key Create Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleCreateApiKey(RequestContext& ctx) {
  init_headers(ctx.headerTable);

  // Extract access token from Authorization header
  auto token_opt = extract_bearer_token(ctx.headers);

  if (token_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("create_key_unauthorized"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "unauthorized"_kj);
    co_return;
  }

  kj::String token = KJ_ASSERT_NONNULL(kj::mv(token_opt));

  // Verify access token
  auto token_info = jwt_.verify_access_token(token);

  if (token_info == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("create_key_invalid"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_token"_kj);
    co_return;
  }

  // Read and parse request body
  auto body = co_await ctx.body.readAllText();

  yyjson_doc* doc = yyjson_read(body.cStr(), body.size(), 0);
  if (!doc) {
    kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("create_key_invalid_json"),
                        kj::heapString(user_id), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "invalid_request"_kj);
    co_return;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);

  auto name_opt = extract_json_string(root, "name"_kj);
  auto permissions_opt = extract_json_string_array(root, "permissions"_kj);

  if (name_opt == kj::none || permissions_opt == kj::none) {
    kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("create_key_missing_fields"),
                        kj::heapString(user_id), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 400, "missing_required_fields"_kj);
    co_return;
  }

  kj::String name = KJ_ASSERT_NONNULL(kj::mv(name_opt));
  kj::Vector<kj::String> permissions = KJ_ASSERT_NONNULL(kj::mv(permissions_opt));
  kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);

  // Create API key
  auto key_pair = api_keys_.create_key(user_id, name, kj::mv(permissions));

  // Create response (raw_key is shown only once!)
  auto response_body =
      kj::str("{\"key_id\":\"", key_pair.key_id, "\",\"raw_key\":\"", key_pair.raw_key,
              "\",\"message\":\"Save this key - it will not be shown again\"}");

  co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("create_key_success"),
                      kj::heapString(user_id), kj::str("unknown"));
  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// API Key Revoke Handler
// -----------------------------------------------------------------------------

kj::Promise<void> AuthHandler::handleRevokeApiKey(RequestContext& ctx, kj::StringPtr key_id) {
  init_headers(ctx.headerTable);

  // Extract access token from Authorization header
  auto token_opt = extract_bearer_token(ctx.headers);

  if (token_opt == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("revoke_key_unauthorized"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "unauthorized"_kj);
    co_return;
  }

  kj::String token = KJ_ASSERT_NONNULL(kj::mv(token_opt));

  // Verify access token
  auto token_info = jwt_.verify_access_token(token);

  if (token_info == kj::none) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("revoke_key_invalid"),
                        kj::heapString("unknown"), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 401, "invalid_token"_kj);
    co_return;
  }

  kj::String user_id = kj::str(KJ_ASSERT_NONNULL(token_info).user_id);

  // Revoke the key
  bool revoked = api_keys_.revoke(key_id);

  if (!revoked) {
    co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("revoke_key_not_found"),
                        kj::heapString(user_id), kj::str("unknown"));
    co_await send_error(ctx.response, ctx.headerTable, 404, "key_not_found"_kj);
    co_return;
  }

  // Create response
  auto response_body = kj::str("{\"ok\":true}");

  co_await audit_.log(audit::AuditLogType::ApiKey, kj::heapString("revoke_key_success"),
                      kj::heapString(user_id), kj::str("unknown"));
  co_await send_json(ctx.response, ctx.headerTable, 200, "OK"_kj, response_body);
  co_return;
}

// -----------------------------------------------------------------------------
// Private Methods
// -----------------------------------------------------------------------------

bool AuthHandler::validate_admin_password(kj::StringPtr password) {
  // Constant-time comparison to prevent timing attacks
  if (admin_password_.size() != password.size()) {
    return false;
  }

  // Simple constant-time comparison
  volatile int result = 0;
  for (size_t i = 0; i < admin_password_.size(); ++i) {
    result |= admin_password_[i] ^ password[i];
  }

  return result == 0;
}

kj::Maybe<AuthHandler::LoginRequest> AuthHandler::parseLoginRequest(const kj::String& body) {
  yyjson_doc* doc = yyjson_read(body.cStr(), body.size(), 0);
  if (!doc) {
    return kj::none;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);

  auto username = extract_json_string(root, "username"_kj);
  auto password = extract_json_string(root, "password"_kj);

  if (username == kj::none || password == kj::none) {
    return kj::none;
  }

  LoginRequest req;
  req.userId = KJ_ASSERT_NONNULL(kj::mv(username));
  req.password = KJ_ASSERT_NONNULL(kj::mv(password));

  return kj::mv(req);
}

kj::Maybe<AuthHandler::RefreshRequest> AuthHandler::parseRefreshRequest(const kj::String& body) {
  yyjson_doc* doc = yyjson_read(body.cStr(), body.size(), 0);
  if (!doc) {
    return kj::none;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);

  auto refreshToken = extract_json_string(root, "refresh_token"_kj);

  if (refreshToken == kj::none) {
    return kj::none;
  }

  RefreshRequest req;
  req.refreshToken = KJ_ASSERT_NONNULL(kj::mv(refreshToken));

  return kj::mv(req);
}

} // namespace veloz::gateway
