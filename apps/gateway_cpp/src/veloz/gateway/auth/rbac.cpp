/**
 * @file rbac.cpp
 * @brief Implementation of Role-Based Access Control (RBAC)
 */

#include "veloz/gateway/auth/rbac.h"

#include <chrono>
#include <cstring>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::auth {

// Static permission name mapping
namespace {
struct PermissionInfo {
  kj::StringPtr name;
  Permission perm;
};

constexpr PermissionInfo PERMISSION_INFOS[] = {
    {"read:market"_kj, Permission::ReadMarket},   {"read:orders"_kj, Permission::ReadOrders},
    {"read:account"_kj, Permission::ReadAccount}, {"read:config"_kj, Permission::ReadConfig},
    {"write:orders"_kj, Permission::WriteOrders}, {"write:cancel"_kj, Permission::WriteCancel},
    {"admin:keys"_kj, Permission::AdminKeys},     {"admin:users"_kj, Permission::AdminUsers},
    {"admin:config"_kj, Permission::AdminConfig},
};

// Note: NUM_PERMISSIONS computed at compile time for validation
[[maybe_unused]] constexpr size_t NUM_PERMISSIONS =
    sizeof(PERMISSION_INFOS) / sizeof(PermissionInfo);
} // namespace

RbacManager::RbacManager() {
  // Initialize with empty user roles map
}

bool RbacManager::assign_role(kj::StringPtr user_id, uint16_t permissions,
                              kj::Maybe<kj::StringPtr> assigned_by) {
  auto guard = user_roles_.lockExclusive();

  int64_t now = current_timestamp();

  KJ_IF_SOME(existing, guard->find(user_id)) {
    // User exists - update permissions
    existing.permissions = permissions;
    existing.updated_at = now;

    KJ_IF_SOME(by, assigned_by) {
      existing.created_by = kj::str(by);
    }

    return false;
  }
  else {
    // New user - create assignment
    UserRole role;
    role.user_id = kj::str(user_id);
    role.permissions = permissions;
    role.created_at = now;
    role.updated_at = now;

    KJ_IF_SOME(by, assigned_by) {
      role.created_by = kj::str(by);
    }

    guard->upsert(kj::str(user_id), kj::mv(role));
    return true;
  }
}

bool RbacManager::revoke_role(kj::StringPtr user_id, kj::Maybe<kj::StringPtr> revoked_by) {
  auto guard = user_roles_.lockExclusive();

  KJ_IF_SOME(found, guard->find(user_id)) {
    (void)found;
    (void)revoked_by; // Audit info not stored for revocation
    guard->erase(user_id);
    return true;
  }

  return false;
}

kj::Maybe<uint16_t> RbacManager::get_role(kj::StringPtr user_id) {
  auto guard = user_roles_.lockShared();

  KJ_IF_SOME(found, guard->find(user_id)) {
    return found.permissions;
  }

  return kj::none;
}

bool RbacManager::has_permission(kj::StringPtr user_id, Permission perm) {
  return has_all_permissions(user_id, static_cast<uint16_t>(perm));
}

bool RbacManager::has_any_permission(kj::StringPtr user_id, uint16_t permissions) {
  auto guard = user_roles_.lockShared();

  KJ_IF_SOME(found, guard->find(user_id)) {
    return (found.permissions & permissions) != 0;
  }

  // No assignment - check default permissions
  return (DEFAULT_PERMISSIONS & permissions) != 0;
}

bool RbacManager::has_all_permissions(kj::StringPtr user_id, uint16_t permissions) {
  auto guard = user_roles_.lockShared();

  KJ_IF_SOME(found, guard->find(user_id)) {
    return (found.permissions & permissions) == permissions;
  }

  // No assignment - check default permissions
  return (DEFAULT_PERMISSIONS & permissions) == permissions;
}

uint16_t RbacManager::get_permissions(kj::StringPtr user_id) {
  auto guard = user_roles_.lockShared();

  KJ_IF_SOME(found, guard->find(user_id)) {
    return found.permissions;
  }

  // Return default permissions if no assignment
  return DEFAULT_PERMISSIONS;
}

kj::Maybe<UserRole> RbacManager::get_user_role_info(kj::StringPtr user_id) {
  auto guard = user_roles_.lockShared();

  KJ_IF_SOME(found, guard->find(user_id)) {
    // Deep copy since UserRole contains kj::String which is not copyable
    UserRole copy;
    copy.user_id = kj::str(found.user_id);
    copy.permissions = found.permissions;
    copy.created_at = found.created_at;
    copy.updated_at = found.updated_at;

    KJ_IF_SOME(created_by, found.created_by) {
      copy.created_by = kj::str(created_by);
    }

    return kj::mv(copy);
  }

  return kj::none;
}

kj::Vector<kj::String> RbacManager::list_users_with_permission(Permission perm) {
  auto guard = user_roles_.lockShared();
  kj::Vector<kj::String> users;

  uint16_t perm_mask = static_cast<uint16_t>(perm);

  for (auto& entry : *guard) {
    if (entry.value.permissions & perm_mask) {
      users.add(kj::str(entry.key));
    }
  }

  return users;
}

kj::HashMap<kj::String, size_t> RbacManager::get_metrics() {
  auto guard = user_roles_.lockShared();
  kj::HashMap<kj::String, size_t> metrics;

  // Count users per permission
  for (auto& entry : *guard) {
    auto perm_list = permission_list(entry.value.permissions);
    for (auto& perm : perm_list) {
      KJ_IF_SOME(count, metrics.find(perm)) {
        metrics.upsert(kj::str(perm), count + 1,
                       [](size_t& val, size_t&& new_val) { val = new_val; });
      }
      else {
        metrics.upsert(kj::str(perm), 1, [](size_t& val, size_t&& new_val) { val = new_val; });
      }
    }
  }

  return metrics;
}

kj::String RbacManager::permission_name(Permission perm) {
  for (const auto& info : PERMISSION_INFOS) {
    if (info.perm == perm) {
      return kj::str(info.name);
    }
  }
  return kj::str("unknown");
}

kj::Vector<kj::String> RbacManager::permission_list(uint16_t permissions) {
  kj::Vector<kj::String> names;

  for (const auto& info : PERMISSION_INFOS) {
    if (permissions & static_cast<uint16_t>(info.perm)) {
      names.add(kj::str(info.name));
    }
  }

  return names;
}

uint16_t RbacManager::parse_permissions(kj::Vector<kj::StringPtr> names) {
  uint16_t permissions = 0;

  for (auto& name : names) {
    for (const auto& info : PERMISSION_INFOS) {
      if (name == info.name) {
        permissions |= static_cast<uint16_t>(info.perm);
        break;
      }
    }
  }

  return permissions;
}

int64_t RbacManager::current_timestamp() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

} // namespace veloz::gateway::auth
