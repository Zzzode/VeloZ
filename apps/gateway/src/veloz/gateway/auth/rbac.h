/**
 * @file rbac.h
 * @brief Role-Based Access Control (RBAC) with bitmask permissions
 *
 * This module provides fine-grained permission checking using bitmask-based
 * permissions and predefined roles. It supports thread-safe role assignment
 * and efficient permission verification.
 *
 * Design decisions:
 * - Bitmask permissions for O(1) permission checks
 * - Predefined roles (Viewer, Trader, Admin)
 * - Thread-safe role management using kj::MutexGuarded
 * - Permission decorators for handler-level authorization
 */

#pragma once

#include <cstdint>
#include <kj/array.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::auth {

/**
 * @brief Permission flags using bitmask for efficient checking
 *
 * Each permission is a bit in a 16-bit unsigned integer.
 * Multiple permissions can be combined using bitwise OR.
 */
enum class Permission : uint16_t {
  // Read permissions
  ReadMarket = 1 << 0,
  ReadOrders = 1 << 1,
  ReadAccount = 1 << 2,
  ReadConfig = 1 << 3,

  // Write permissions
  WriteOrders = 1 << 4,
  WriteCancel = 1 << 5,

  // Admin permissions
  AdminKeys = 1 << 6,
  AdminUsers = 1 << 7,
  AdminConfig = 1 << 8,

  // Full access (all permissions)
  All = 0xFFFF,
};

/**
 * @brief Predefined roles with their permission sets
 *
 * These are constexpr constants for efficient role assignment.
 */
namespace Roles {
constexpr uint16_t Viewer =
    static_cast<uint16_t>(Permission::ReadMarket) | static_cast<uint16_t>(Permission::ReadConfig);

constexpr uint16_t Trader = Viewer | static_cast<uint16_t>(Permission::ReadOrders) |
                            static_cast<uint16_t>(Permission::ReadAccount) |
                            static_cast<uint16_t>(Permission::WriteOrders) |
                            static_cast<uint16_t>(Permission::WriteCancel);

constexpr uint16_t Admin = Trader | static_cast<uint16_t>(Permission::AdminKeys) |
                           static_cast<uint16_t>(Permission::AdminUsers) |
                           static_cast<uint16_t>(Permission::AdminConfig);
} // namespace Roles

/**
 * @brief User role assignment record
 */
struct UserRole {
  kj::String user_id;
  uint16_t permissions;
  int64_t created_at;
  int64_t updated_at;
  kj::Maybe<kj::String> created_by;

  UserRole() : permissions(0), created_at(0), updated_at(0) {}
};

/**
 * @brief RBAC manager for role-based access control
 *
 * Manages user role assignments and provides efficient permission checking.
 *
 * Thread safety: All public methods are thread-safe.
 * Performance target: <1μs for permission checks
 */
class RbacManager {
public:
  /**
   * @brief Default role for new users
   */
  static constexpr uint16_t DEFAULT_PERMISSIONS = Roles::Viewer;

  RbacManager();
  ~RbacManager() = default;

  // Non-copyable, non-movable (contains mutex)
  RbacManager(const RbacManager&) = delete;
  RbacManager& operator=(const RbacManager&) = delete;
  RbacManager(RbacManager&&) = delete;
  RbacManager& operator=(RbacManager&&) = delete;

  /**
   * @brief Assign a role to a user
   *
   * @param user_id User ID to assign the role to
   * @param permissions Permission bitmask to assign
   * @param assigned_by User making the assignment (for audit)
   * @return true if assignment was made, false if user already had the role
   */
  bool assign_role(kj::StringPtr user_id, uint16_t permissions,
                   kj::Maybe<kj::StringPtr> assigned_by = kj::none);

  /**
   * @brief Revoke all roles/permissions from a user
   *
   * @param user_id User ID to revoke permissions from
   * @param revoked_by User making the revocation (for audit)
   * @return true if user was found and revoked, false otherwise
   */
  bool revoke_role(kj::StringPtr user_id, kj::Maybe<kj::StringPtr> revoked_by = kj::none);

  /**
   * @brief Get the permission bitmask for a user
   *
   * @param user_id User ID to look up
   * @return Permission bitmask, or kj::none if user has no assignments
   */
  kj::Maybe<uint16_t> get_role(kj::StringPtr user_id);

  /**
   * @brief Check if a user has a specific permission
   *
   * @param user_id User ID to check
   * @param perm Permission to check for
   * @return true if user has the permission
   */
  bool has_permission(kj::StringPtr user_id, Permission perm);

  /**
   * @brief Check if a user has any of the specified permissions
   *
   * @param user_id User ID to check
   * @param permissions Permission bitmask to check
   * @return true if user has at least one of the permissions
   */
  bool has_any_permission(kj::StringPtr user_id, uint16_t permissions);

  /**
   * @brief Check if a user has all of the specified permissions
   *
   * @param user_id User ID to check
   * @param permissions Permission bitmask to check
   * @return true if user has all the specified permissions
   */
  bool has_all_permissions(kj::StringPtr user_id, uint16_t permissions);

  /**
   * @brief Get all permissions for a user
   *
   * @param user_id User ID to look up
   * @return Permission bitmask (returns DEFAULT_PERMISSIONS if no assignment)
   */
  uint16_t get_permissions(kj::StringPtr user_id);

  /**
   * @brief Get detailed role info for a user
   *
   * @param user_id User ID to look up
   * @return UserRole info, or kj::none if no assignment exists
   */
  kj::Maybe<UserRole> get_user_role_info(kj::StringPtr user_id);

  /**
   * @brief List all user IDs with a specific permission
   *
   * @param perm Permission to check for
   * @return Vector of user IDs that have the permission
   */
  kj::Vector<kj::String> list_users_with_permission(Permission perm);

  /**
   * @brief Get metrics about role assignments
   *
   * @return HashMap mapping permission names to user counts
   */
  kj::HashMap<kj::String, size_t> get_metrics();

  /**
   * @brief Get the human-readable name for a permission
   *
   * @param perm Permission to get name for
   * @return Permission name as a string
   */
  static kj::String permission_name(Permission perm);

  /**
   * @brief Get a list of permission names from a permission bitmask
   *
   * @param permissions Permission bitmask
   * @return Vector of permission names
   */
  static kj::Vector<kj::String> permission_list(uint16_t permissions);

  /**
   * @brief Parse permission names to a bitmask
   *
   * @param names Vector of permission names
   * @return Permission bitmask
   */
  static uint16_t parse_permissions(kj::Vector<kj::StringPtr> names);

private:
  /**
   * @brief Get current Unix timestamp
   */
  int64_t current_timestamp() const;

  kj::MutexGuarded<kj::HashMap<kj::String, UserRole>> user_roles_;
};

} // namespace veloz::gateway::auth
