/**
 * @file rbac.h
 * @brief RBAC integration for gateway request handlers
 *
 * This module provides permission checking decorators and utilities
 * for use in gateway request handlers. It builds on the bitmask-based
 * permission system from veloz/gateway/auth/rbac.h.
 */

#pragma once

#include "../request_context.h"
#include "veloz/gateway/auth/rbac.h"

#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * @brief Permission checking decorator for request handlers
 *
 * Creates a handler wrapper that checks if the authenticated user
 * has the required permission before executing the handler.
 *
 * Usage:
 * ```cpp
 * router.add_route(kj::HttpMethod::GET, "/api/orders",
 *   require_permission(Permission::ReadOrders, [this](RequestContext& ctx) {
 *     return list_orders(ctx);
 *   })
 * );
 *
 * router.add_route(kj::HttpMethod::POST, "/api/orders",
 *   require_permission(Permission::WriteOrders, [this](RequestContext& ctx) {
 *     return submit_order(ctx);
 *   })
 * );
 * ```
 *
 * @param permission The required permission
 * @param rbac_manager The RBAC manager to use for checking
 * @param handler The handler to wrap
 * @return A new handler function that checks permissions first
 */
kj::Function<kj::Promise<void>(RequestContext&)>
require_permission(auth::Permission permission, auth::RbacManager& rbac_manager,
                   kj::Function<kj::Promise<void>(RequestContext&)> handler);

/**
 * @brief Permission checking decorator for multiple permissions (any)
 *
 * Checks if the user has at least one of the specified permissions.
 *
 * @param permissions Bitmask of permissions (user needs any)
 * @param rbac_manager The RBAC manager to use for checking
 * @param handler The handler to wrap
 * @return A new handler function that checks permissions first
 */
kj::Function<kj::Promise<void>(RequestContext&)>
require_any_permission(uint16_t permissions, auth::RbacManager& rbac_manager,
                       kj::Function<kj::Promise<void>(RequestContext&)> handler);

/**
 * @brief Permission checking decorator for multiple permissions (all)
 *
 * Checks if the user has all of the specified permissions.
 *
 * @param permissions Bitmask of permissions (user needs all)
 * @param rbac_manager The RBAC manager to use for checking
 * @param handler The handler to wrap
 * @return A new handler function that checks permissions first
 */
kj::Function<kj::Promise<void>(RequestContext&)>
require_all_permissions(uint16_t permissions, auth::RbacManager& rbac_manager,
                        kj::Function<kj::Promise<void>(RequestContext&)> handler);

/**
 * @brief Helper macro for permission checking
 *
 * Usage:
 * ```cpp
 * router.add_route(kj::HttpMethod::POST, "/api/orders",
 *   REQUIRE_PERMISSION(WriteOrders, rbac_manager, [this](RequestContext& ctx) {
 *     return submit_order(ctx);
 *   })
 * );
 * ```
 */
#define REQUIRE_PERMISSION(perm, rbac_mgr, handler)                                                \
  veloz::gateway::require_permission(veloz::gateway::auth::Permission::perm, rbac_mgr, handler)

/**
 * @brief Extract user ID from request context
 *
 * @param ctx Request context
 * @return User ID if authenticated, kj::none otherwise
 */
kj::Maybe<kj::String> get_user_id(RequestContext& ctx);

/**
 * @brief Check if request context has authentication
 *
 * @param ctx Request context
 * @return true if authenticated
 */
bool is_authenticated(RequestContext& ctx);

/**
 * @brief Send 403 Forbidden response
 *
 * @param ctx Request context
 * @param message Optional error message
 * @return Promise that completes when response is sent
 */
kj::Promise<void> send_forbidden(RequestContext& ctx, kj::StringPtr message = "Forbidden"_kj);

/**
 * @brief Send 401 Unauthorized response
 *
 * @param ctx Request context
 * @param message Optional error message
 * @return Promise that completes when response is sent
 */
kj::Promise<void> send_unauthorized(RequestContext& ctx, kj::StringPtr message = "Unauthorized"_kj);

/**
 * @brief Permission checking helper for string-based permissions
 *
 * Creates a handler wrapper that checks permissions before calling the handler.
 * This is a compatibility layer for the string-based permission system.
 *
 * @param permission The permission string to check
 * @param handler The handler to wrap
 * @return A new handler function that checks permissions first
 */
kj::Function<kj::Promise<void>(RequestContext&)>
require_permission(kj::StringPtr permission,
                   kj::Function<kj::Promise<void>(RequestContext&)> handler);

/**
 * @brief Check if AuthInfo has a specific permission
 *
 * @param auth AuthInfo from RequestContext
 * @param permission Permission string to check
 * @return true if permission is granted
 */
bool has_permission(const AuthInfo& auth, kj::StringPtr permission);

} // namespace veloz::gateway
