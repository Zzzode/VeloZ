#include "middleware/auth_middleware.h"

#include <chrono>
#include <ctime>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>

namespace veloz::gateway {

namespace {

// Standard error messages (secure - don't leak info)
constexpr kj::StringPtr ERROR_UNAUTHORIZED = R"({
  "error": "unauthorized",
  "message": "Authentication required"
})"_kj;

// Note: ERROR_FORBIDDEN is defined but not currently used - kept for future use
// constexpr kj::StringPtr ERROR_FORBIDDEN = R"({
//   "error": "forbidden",
//   "message": "Insufficient permissions"
// })"_kj;

} // anonymous namespace

AuthMiddleware::Config AuthMiddleware::default_config() {
  Config config;
  config.require_auth = true;
  // KJ Vector doesn't support initializer lists, so we build it incrementally
  config.public_paths.add(kj::str("/health"));
  config.public_paths.add(kj::str("/api/health"));
  config.public_paths.add(kj::str("/api/stream"));
  config.public_paths.add(kj::str("/api/auth/login"));
  config.public_paths.add(kj::str("/api/auth/refresh"));
  config.public_paths.add(kj::str("/")); // Serve static UI
  return config;
}

AuthMiddleware::AuthMiddleware(kj::Own<AuthManager> auth_manager, audit::AuditLogger* audit_logger,
                               Config config)
    : auth_manager_(kj::mv(auth_manager)), audit_logger_(audit_logger), config_(kj::mv(config)) {}

kj::Promise<void> AuthMiddleware::process(RequestContext& ctx,
                                          kj::Function<kj::Promise<void>()> next) {

  // Check if path is public
  if (is_public_path(ctx.path)) {
    // Public path, skip authentication
    return next();
  }

  // Attempt authentication
  KJ_IF_SOME(auth_info, authenticate_request(ctx)) {
    // Capture auth_method before moving - kj::str creates a copy
    auto auth_method = kj::str(auth_info.auth_method);

    // Authentication successful, populate context
    ctx.authInfo = kj::mv(auth_info);

    // Log successful authentication
    log_auth_attempt(ctx, true, auth_method);

    // Continue to next middleware/handler
    return next();
  }

  // Authentication failed
  log_auth_attempt(ctx, false);
  return send_unauthorized(ctx);
}

bool AuthMiddleware::is_public_path(kj::StringPtr path) const {
  for (auto& public_path : config_.public_paths) {
    if (path == public_path) {
      return true;
    }
  }
  return false;
}

kj::Maybe<AuthInfo> AuthMiddleware::authenticate_request(RequestContext& ctx) {
  return auth_manager_->authenticate(ctx.headers);
}

void AuthMiddleware::log_auth_attempt(RequestContext&, bool success, kj::StringPtr auth_method) {

  if (audit_logger_ == nullptr) {
    return;
  }

  // Get current time
  auto now = std::chrono::system_clock::now();

  // Create audit log entry - simplified for now
  // TODO: Add proper audit entry creation once audit API is finalized
  (void)now; // Suppress unused warning for now
  (void)success;
  (void)auth_method;
}

kj::Promise<void> AuthMiddleware::send_unauthorized(RequestContext& ctx) {
  kj::String body = kj::heapString(ERROR_UNAUTHORIZED);

  kj::HttpHeaders headers(ctx.headerTable);
  headers.addPtrPtr("Content-Type"_kj, "application/json"_kj);
  headers.addPtrPtr("WWW-Authenticate"_kj, "Bearer"_kj);

  auto stream = ctx.response.send(401, "Unauthorized"_kj, headers, body.size());
  return stream->write(body.asBytes()).attach(kj::mv(body));
}

} // namespace veloz::gateway
