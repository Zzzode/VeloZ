/**
 * @file test_rbac.cpp
 * @brief Tests for RBAC permission system
 */

#include "veloz/gateway/auth/rbac.h"

#include <atomic>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/test.h>
#include <thread>
#include <vector>

using namespace veloz::gateway::auth;

namespace {

KJ_TEST("RbacManager: Assign and retrieve role") {
  RbacManager manager;

  // Assign Trader role to user
  bool assigned = manager.assign_role("user123"_kj, Roles::Trader, "admin"_kj);
  KJ_EXPECT(assigned, "First assignment should return true");

  // Retrieve role
  KJ_IF_SOME(role, manager.get_role("user123"_kj)) {
    KJ_EXPECT(role == Roles::Trader);
  }
  else {
    KJ_FAIL_EXPECT("Role should be found");
  }

  // Assign same role again should return false
  bool reassigned = manager.assign_role("user123"_kj, Roles::Trader);
  KJ_EXPECT(!reassigned, "Reassignment should return false");
}

KJ_TEST("RbacManager: Revoke role") {
  RbacManager manager;

  // Assign role
  manager.assign_role("user456"_kj, Roles::Admin);

  // Verify role exists
  KJ_IF_SOME(role, manager.get_role("user456"_kj)) {
    (void)role;
  }
  else {
    KJ_FAIL_EXPECT("Role should exist before revocation");
  }

  // Revoke role
  bool revoked = manager.revoke_role("user456"_kj);
  KJ_EXPECT(revoked, "Revocation should succeed");

  // Verify role is gone
  auto role = manager.get_role("user456"_kj);
  KJ_EXPECT(role == kj::none, "Role should not exist after revocation");

  // Revoke again should fail
  bool revoked_again = manager.revoke_role("user456"_kj);
  KJ_EXPECT(!revoked_again, "Re-revocation should fail");
}

KJ_TEST("RbacManager: Has permission check") {
  RbacManager manager;

  // Assign Viewer role
  manager.assign_role("viewer_user"_kj, Roles::Viewer);

  // Check has_permission
  KJ_EXPECT(manager.has_permission("viewer_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("viewer_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(!manager.has_permission("viewer_user"_kj, Permission::ReadOrders));
  KJ_EXPECT(!manager.has_permission("viewer_user"_kj, Permission::WriteOrders));
  KJ_EXPECT(!manager.has_permission("viewer_user"_kj, Permission::AdminKeys));
}

KJ_TEST("RbacManager: Trader role permissions") {
  RbacManager manager;

  // Assign Trader role
  manager.assign_role("trader_user"_kj, Roles::Trader);

  // Check Trader permissions
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::ReadOrders));
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::ReadAccount));
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::WriteOrders));
  KJ_EXPECT(manager.has_permission("trader_user"_kj, Permission::WriteCancel));
  KJ_EXPECT(!manager.has_permission("trader_user"_kj, Permission::AdminKeys));
  KJ_EXPECT(!manager.has_permission("trader_user"_kj, Permission::AdminUsers));
  KJ_EXPECT(!manager.has_permission("trader_user"_kj, Permission::AdminConfig));
}

KJ_TEST("RbacManager: Admin role permissions") {
  RbacManager manager;

  // Assign Admin role
  manager.assign_role("admin_user"_kj, Roles::Admin);

  // Admin should have all permissions
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::ReadOrders));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::ReadAccount));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::WriteOrders));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::WriteCancel));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::AdminKeys));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::AdminUsers));
  KJ_EXPECT(manager.has_permission("admin_user"_kj, Permission::AdminConfig));
}

KJ_TEST("RbacManager: Has any permission") {
  RbacManager manager;

  // Assign custom role
  uint16_t custom_perms = static_cast<uint16_t>(Permission::ReadMarket) |
                          static_cast<uint16_t>(Permission::WriteOrders);
  manager.assign_role("custom_user"_kj, custom_perms);

  // Check has_any_permission
  uint16_t check_perms =
      static_cast<uint16_t>(Permission::ReadMarket) | static_cast<uint16_t>(Permission::ReadConfig);
  KJ_EXPECT(manager.has_any_permission("custom_user"_kj, check_perms));

  check_perms = static_cast<uint16_t>(Permission::ReadOrders) |
                static_cast<uint16_t>(Permission::WriteCancel);
  KJ_EXPECT(!manager.has_any_permission("custom_user"_kj, check_perms));

  check_perms = static_cast<uint16_t>(Permission::WriteOrders) |
                static_cast<uint16_t>(Permission::WriteCancel);
  KJ_EXPECT(manager.has_any_permission("custom_user"_kj, check_perms));
}

KJ_TEST("RbacManager: Has all permissions") {
  RbacManager manager;

  // Assign custom role
  uint16_t custom_perms = static_cast<uint16_t>(Permission::ReadMarket) |
                          static_cast<uint16_t>(Permission::WriteOrders);
  manager.assign_role("custom_user"_kj, custom_perms);

  // Check has_all_permissions - should match exactly
  uint16_t check_perms = static_cast<uint16_t>(Permission::ReadMarket) |
                         static_cast<uint16_t>(Permission::WriteOrders);
  KJ_EXPECT(manager.has_all_permissions("custom_user"_kj, check_perms));

  // Missing permission should fail
  check_perms =
      static_cast<uint16_t>(Permission::ReadMarket) | static_cast<uint16_t>(Permission::ReadConfig);
  KJ_EXPECT(!manager.has_all_permissions("custom_user"_kj, check_perms));

  // Extra permission should fail
  check_perms = static_cast<uint16_t>(Permission::ReadMarket) |
                static_cast<uint16_t>(Permission::WriteOrders) |
                static_cast<uint16_t>(Permission::ReadConfig);
  KJ_EXPECT(!manager.has_all_permissions("custom_user"_kj, check_perms));
}

KJ_TEST("RbacManager: Get permissions returns default for unassigned") {
  RbacManager manager;

  // Unassigned user should get default permissions
  uint16_t perms = manager.get_permissions("unassigned_user"_kj);
  KJ_EXPECT(perms == RbacManager::DEFAULT_PERMISSIONS);
  KJ_EXPECT(perms == Roles::Viewer);

  // Check default permissions
  KJ_EXPECT(manager.has_permission("unassigned_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("unassigned_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(!manager.has_permission("unassigned_user"_kj, Permission::WriteOrders));
}

KJ_TEST("RbacManager: Get user role info") {
  RbacManager manager;

  // Assign role with audit info
  manager.assign_role("user_info"_kj, Roles::Trader, "admin123"_kj);

  // Get user role info
  KJ_IF_SOME(role_info, manager.get_user_role_info("user_info"_kj)) {
    KJ_EXPECT(role_info.user_id == "user_info"_kj);
    KJ_EXPECT(role_info.permissions == Roles::Trader);
    KJ_EXPECT(role_info.created_at > 0);
    KJ_EXPECT(role_info.updated_at > 0);

    KJ_IF_SOME(created_by, role_info.created_by) {
      KJ_EXPECT(created_by == "admin123"_kj);
    }
    else {
      KJ_FAIL_EXPECT("Created by should be set");
    }
  }
  else {
    KJ_FAIL_EXPECT("User role info should be found");
  }
}

KJ_TEST("RbacManager: List users with permission") {
  RbacManager manager;

  // Assign various roles
  manager.assign_role("viewer1"_kj, Roles::Viewer);
  manager.assign_role("viewer2"_kj, Roles::Viewer);
  manager.assign_role("trader1"_kj, Roles::Trader);
  manager.assign_role("admin1"_kj, Roles::Admin);

  // List users with ReadMarket permission (Viewer, Trader, Admin have it)
  auto market_users = manager.list_users_with_permission(Permission::ReadMarket);
  KJ_EXPECT(market_users.size() == 4);

  // List users with WriteOrders permission (Trader, Admin have it)
  auto write_users = manager.list_users_with_permission(Permission::WriteOrders);
  KJ_EXPECT(write_users.size() == 2);

  // List users with AdminKeys permission (only Admin has it)
  auto admin_users = manager.list_users_with_permission(Permission::AdminKeys);
  KJ_EXPECT(admin_users.size() == 1);
}

KJ_TEST("RbacManager: Get metrics") {
  RbacManager manager;

  // Assign roles
  manager.assign_role("viewer1"_kj, Roles::Viewer);
  manager.assign_role("viewer2"_kj, Roles::Viewer);
  manager.assign_role("trader1"_kj, Roles::Trader);
  manager.assign_role("admin1"_kj, Roles::Admin);

  // Get metrics
  auto metrics = manager.get_metrics();

  // Check counts (Viewer: 2 + Trader: 1 + Admin: 1 = 4 users with ReadMarket)
  KJ_IF_SOME(market_count, metrics.find("read:market"_kj)) {
    KJ_EXPECT(market_count == 4);
  }
  else {
    KJ_FAIL_EXPECT("ReadMarket metric not found");
  }

  // Only Trader and Admin have WriteOrders (2 users)
  KJ_IF_SOME(write_count, metrics.find("write:orders"_kj)) {
    KJ_EXPECT(write_count == 2);
  }
  else {
    KJ_FAIL_EXPECT("WriteOrders metric not found");
  }

  // Only Admin has AdminKeys (1 user)
  KJ_IF_SOME(admin_keys_count, metrics.find("admin:keys"_kj)) {
    KJ_EXPECT(admin_keys_count == 1);
  }
  else {
    KJ_FAIL_EXPECT("AdminKeys metric not found");
  }
}

KJ_TEST("RbacManager: Permission name") {
  kj::String name = RbacManager::permission_name(Permission::ReadMarket);
  KJ_EXPECT(name == "read:market"_kj);

  name = RbacManager::permission_name(Permission::WriteOrders);
  KJ_EXPECT(name == "write:orders"_kj);

  name = RbacManager::permission_name(Permission::AdminKeys);
  KJ_EXPECT(name == "admin:keys"_kj);
}

KJ_TEST("RbacManager: Permission list") {
  // Viewer role permissions
  auto viewer_perms = RbacManager::permission_list(Roles::Viewer);
  KJ_EXPECT(viewer_perms.size() == 2);
  KJ_EXPECT(viewer_perms[0] == "read:market"_kj || viewer_perms[1] == "read:market"_kj);
  KJ_EXPECT(viewer_perms[0] == "read:config"_kj || viewer_perms[1] == "read:config"_kj);

  // Trader role permissions
  auto trader_perms = RbacManager::permission_list(Roles::Trader);
  KJ_EXPECT(trader_perms.size() == 6);

  // Admin role permissions
  auto admin_perms = RbacManager::permission_list(Roles::Admin);
  KJ_EXPECT(admin_perms.size() == 9);

  // Empty permissions
  auto empty_perms = RbacManager::permission_list(0);
  KJ_EXPECT(empty_perms.size() == 0);
}

KJ_TEST("RbacManager: Parse permissions") {
  kj::Vector<kj::StringPtr> names;
  names.add("read:market"_kj);
  names.add("read:orders"_kj);
  names.add("write:orders"_kj);

  uint16_t perms = RbacManager::parse_permissions(kj::mv(names));

  KJ_EXPECT(perms & static_cast<uint16_t>(Permission::ReadMarket));
  KJ_EXPECT(perms & static_cast<uint16_t>(Permission::ReadOrders));
  KJ_EXPECT(perms & static_cast<uint16_t>(Permission::WriteOrders));
  KJ_EXPECT(!(perms & static_cast<uint16_t>(Permission::WriteCancel)));
}

KJ_TEST("RbacManager: Parse permissions with unknown names") {
  kj::Vector<kj::StringPtr> names;
  names.add("read:market"_kj);
  names.add("unknown:permission"_kj); // Unknown, should be ignored
  names.add("write:orders"_kj);

  uint16_t perms = RbacManager::parse_permissions(kj::mv(names));

  KJ_EXPECT(perms & static_cast<uint16_t>(Permission::ReadMarket));
  KJ_EXPECT(perms & static_cast<uint16_t>(Permission::WriteOrders));
}

KJ_TEST("RbacManager: Custom permissions") {
  RbacManager manager;

  // Create custom permission set
  uint16_t custom_perms = static_cast<uint16_t>(Permission::ReadMarket) |
                          static_cast<uint16_t>(Permission::WriteCancel) |
                          static_cast<uint16_t>(Permission::AdminUsers);

  manager.assign_role("custom_user"_kj, custom_perms);

  // Verify only specified permissions
  KJ_EXPECT(manager.has_permission("custom_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("custom_user"_kj, Permission::WriteCancel));
  KJ_EXPECT(manager.has_permission("custom_user"_kj, Permission::AdminUsers));
  KJ_EXPECT(!manager.has_permission("custom_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(!manager.has_permission("custom_user"_kj, Permission::WriteOrders));
  KJ_EXPECT(!manager.has_permission("custom_user"_kj, Permission::AdminKeys));
}

KJ_TEST("RbacManager: Thread safety - concurrent role assignment") {
  RbacManager manager;

  // Test concurrent role assignment
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, &success_count, i]() {
      for (int j = 0; j < 10; ++j) {
        kj::String user_id = kj::str("user-", i, "-", j);
        bool assigned = manager.assign_role(user_id, Roles::Trader);
        if (assigned) {
          success_count++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  KJ_EXPECT(success_count == 100, "All assignments should succeed");
}

KJ_TEST("RbacManager: Thread safety - concurrent permission checks") {
  RbacManager manager;

  // Create some users
  manager.assign_role("user1"_kj, Roles::Viewer);
  manager.assign_role("user2"_kj, Roles::Trader);
  manager.assign_role("user3"_kj, Roles::Admin);

  // Test concurrent permission checks
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, &success_count]() {
      for (int j = 0; j < 100; ++j) {
        bool has_perm = manager.has_permission("user1"_kj, Permission::ReadMarket);
        bool no_perm = !manager.has_permission("user1"_kj, Permission::WriteOrders);
        if (has_perm && no_perm) {
          success_count++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  KJ_EXPECT(success_count == 1000, "All checks should succeed");
}

KJ_TEST("RbacManager: Thread safety - concurrent role modification and checking") {
  RbacManager manager;

  // Assign initial roles
  manager.assign_role("user1"_kj, Roles::Viewer);
  manager.assign_role("user2"_kj, Roles::Trader);

  std::vector<std::thread> threads;

  // Thread 1: Modify roles
  threads.emplace_back([&manager]() {
    for (int i = 0; i < 100; ++i) {
      manager.assign_role("user1"_kj, i % 2 == 0 ? Roles::Viewer : Roles::Trader);
    }
  });

  // Thread 2: Check permissions
  threads.emplace_back([&manager]() {
    for (int i = 0; i < 100; ++i) {
      // Just verify no crashes
      (void)manager.has_permission("user1"_kj, Permission::ReadMarket);
      (void)manager.get_permissions("user1"_kj);
    }
  });

  for (auto& t : threads) {
    t.join();
  }
  // Test passes if no crashes
}

KJ_TEST("RbacManager: User role info timestamps") {
  RbacManager manager;

  auto user_id = "time_user"_kj;

  // Assign role
  manager.assign_role(user_id, Roles::Trader);

  KJ_IF_SOME(role_info, manager.get_user_role_info(user_id)) {
    int64_t created_at = role_info.created_at;
    int64_t updated_at = role_info.updated_at;

    KJ_EXPECT(created_at > 0);
    KJ_EXPECT(updated_at > 0);
    KJ_EXPECT(updated_at == created_at); // Should be same on first assignment

    // Small delay (not ideal test but demonstrates the concept)
    // In production, use clock mocking for deterministic testing
  }
  else {
    KJ_FAIL_EXPECT("User role info should be found");
  }
}

KJ_TEST("RbacManager: Update existing role") {
  RbacManager manager;

  auto user_id = "update_user"_kj;

  // Assign Viewer role
  manager.assign_role(user_id, Roles::Viewer);
  KJ_IF_SOME(role, manager.get_role(user_id)) {
    KJ_EXPECT(role == Roles::Viewer);
  }
  else {
    KJ_FAIL_EXPECT("Role should be found");
  }

  // Update to Trader role
  bool assigned = manager.assign_role(user_id, Roles::Trader);
  KJ_EXPECT(!assigned, "Reassignment should return false");
  KJ_IF_SOME(role, manager.get_role(user_id)) {
    KJ_EXPECT(role == Roles::Trader);
  }
  else {
    KJ_FAIL_EXPECT("Role should be found");
  }

  // Update to Admin role
  assigned = manager.assign_role(user_id, Roles::Admin);
  KJ_EXPECT(!assigned);
  KJ_IF_SOME(role, manager.get_role(user_id)) {
    KJ_EXPECT(role == Roles::Admin);
  }
  else {
    KJ_FAIL_EXPECT("Role should be found");
  }
}

KJ_TEST("RbacManager: Empty user ID handling") {
  RbacManager manager;

  // Assign to empty user ID (edge case)
  bool assigned = manager.assign_role(""_kj, Roles::Viewer);
  KJ_EXPECT(assigned);

  KJ_IF_SOME(role, manager.get_role(""_kj)) {
    KJ_EXPECT(role == Roles::Viewer);
  }
  else {
    KJ_FAIL_EXPECT("Empty user ID role should be found");
  }
}

KJ_TEST("RbacManager: Permission bitmask operations") {
  // Test bitmask operations
  uint16_t perms =
      static_cast<uint16_t>(Permission::ReadMarket) | static_cast<uint16_t>(Permission::ReadOrders);

  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::ReadMarket)) != 0);
  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::ReadOrders)) != 0);
  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::WriteOrders)) == 0);

  // Add permission
  perms |= static_cast<uint16_t>(Permission::WriteOrders);
  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::WriteOrders)) != 0);

  // Remove permission
  perms &= ~static_cast<uint16_t>(Permission::ReadMarket);
  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::ReadMarket)) == 0);
  KJ_EXPECT((perms & static_cast<uint16_t>(Permission::ReadOrders)) != 0);
}

KJ_TEST("RbacManager: All permission") {
  RbacManager manager;

  manager.assign_role("super_user"_kj, static_cast<uint16_t>(Permission::All));

  // Should have all permissions
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::ReadMarket));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::ReadOrders));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::ReadAccount));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::ReadConfig));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::WriteOrders));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::WriteCancel));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::AdminKeys));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::AdminUsers));
  KJ_EXPECT(manager.has_permission("super_user"_kj, Permission::AdminConfig));
}

KJ_TEST("RbacManager: List users with permission - empty result") {
  RbacManager manager;

  // No users assigned
  auto users = manager.list_users_with_permission(Permission::ReadMarket);
  KJ_EXPECT(users.size() == 0);

  // Assign users without the permission
  manager.assign_role("user1"_kj, Roles::Viewer);
  manager.assign_role("user2"_kj, Roles::Viewer);

  auto write_users = manager.list_users_with_permission(Permission::WriteOrders);
  KJ_EXPECT(write_users.size() == 0);
}

KJ_TEST("RbacManager: Get permissions returns assigned not default") {
  RbacManager manager;

  auto user_id = "assigned_user"_kj;

  // Assign explicit role
  manager.assign_role(user_id, Roles::Admin);

  // Get permissions
  uint16_t perms = manager.get_permissions(user_id);

  // Should return assigned role, not default
  KJ_EXPECT(perms == Roles::Admin);
  KJ_EXPECT(perms != RbacManager::DEFAULT_PERMISSIONS);
}

} // anonymous namespace
