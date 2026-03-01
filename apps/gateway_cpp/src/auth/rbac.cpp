/**
 * @file rbac.cpp
 * @brief Implementation of RBAC integration for gateway handlers
 */

#include "auth/rbac.h"

#include <kj/common.h>
#include <kj/string.h>

namespace veloz::gateway {

kj::Function<kj::Promise<void>(RequestContext&)>
require_permission(auth::Permission permission, auth::RbacManager& rbac_manager,
                   kj::Function<kj::Promise<void>(RequestContext&)> handler) {
  return [permission, &rbac_manager,
          handler = kj::mv(handler)](RequestContext& ctx) mutable -> kj::Promise<void> {
    KJ_IF_SOME(auth_info, ctx.authInfo) {
      if (rbac_manager.has_permission(auth_info.user_id, permission)) {
        return handler(ctx);
      }
      return send_forbidden(ctx);
    }
    return send_unauthorized(ctx);
  };
}

kj::Function<kj::Promise<void>(RequestContext&)>
require_any_permission(uint16_t permissions, auth::RbacManager& rbac_manager,
                       kj::Function<kj::Promise<void>(RequestContext&)> handler) {
  return [permissions, &rbac_manager,
          handler = kj::mv(handler)](RequestContext& ctx) mutable -> kj::Promise<void> {
    KJ_IF_SOME(auth_info, ctx.authInfo) {
      if (rbac_manager.has_any_permission(auth_info.user_id, permissions)) {
        return handler(ctx);
      }
      return send_forbidden(ctx);
    }
    return send_unauthorized(ctx);
  };
}

kj::Function<kj::Promise<void>(RequestContext&)>
require_all_permissions(uint16_t permissions, auth::RbacManager& rbac_manager,
                        kj::Function<kj::Promise<void>(RequestContext&)> handler) {
  return [permissions, &rbac_manager,
          handler = kj::mv(handler)](RequestContext& ctx) mutable -> kj::Promise<void> {
    KJ_IF_SOME(auth_info, ctx.authInfo) {
      if (rbac_manager.has_all_permissions(auth_info.user_id, permissions)) {
        return handler(ctx);
      }
      return send_forbidden(ctx);
    }
    return send_unauthorized(ctx);
  };
}

kj::Function<kj::Promise<void>(RequestContext&)>
require_permission(kj::StringPtr permission,
                   kj::Function<kj::Promise<void>(RequestContext&)> handler) {
  return [perm = kj::str(permission),
          handler = kj::mv(handler)](RequestContext& ctx) mutable -> kj::Promise<void> {
    KJ_IF_SOME(auth_info, ctx.authInfo) {
      // Check if permission exists in user's permissions list
      for (const auto& user_perm : auth_info.permissions) {
        if (user_perm == perm) {
          return handler(ctx);
        }
      }
      return send_forbidden(ctx);
    }
    return send_unauthorized(ctx);
  };
}

kj::Maybe<kj::String> get_user_id(RequestContext& ctx) {
  KJ_IF_SOME(auth_info, ctx.authInfo) {
    return kj::str(auth_info.user_id);
  }
  return kj::none;
}

bool is_authenticated(RequestContext& ctx) {
  return ctx.authInfo != kj::none;
}

kj::Promise<void> send_forbidden(RequestContext& ctx, kj::StringPtr message) {
  kj::String body = kj::str(R"({"error":")", message, R"("})");
  kj::HttpHeaders response_headers(ctx.headerTable);
  response_headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);
  auto stream = ctx.response.send(403, "Forbidden"_kj, response_headers, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> send_unauthorized(RequestContext& ctx, kj::StringPtr message) {
  kj::String body = kj::str(R"({"error":")", message, R"("})");
  kj::HttpHeaders response_headers(ctx.headerTable);
  response_headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);
  auto stream = ctx.response.send(401, "Unauthorized"_kj, response_headers, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

bool has_permission(const AuthInfo& auth, kj::StringPtr permission) {
  for (const auto& perm : auth.permissions) {
    if (perm == permission) {
      return true;
    }
  }
  return false;
}

} // namespace veloz::gateway
