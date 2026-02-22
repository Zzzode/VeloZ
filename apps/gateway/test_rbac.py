#!/usr/bin/env python3
"""Unit tests for Role-Based Access Control (RBAC) module."""

import unittest
import sys
import os

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rbac import RoleManager, role_manager, get_user_permissions


class TestRoleManager(unittest.TestCase):
    """Tests for RoleManager class."""

    def setUp(self):
        self.manager = RoleManager()

    def test_assign_role(self):
        """Test assigning a role to a user."""
        result = self.manager.assign_role("user1", "trader")
        self.assertTrue(result)
        self.assertIn("trader", self.manager.get_roles("user1"))

    def test_assign_role_duplicate(self):
        """Test that assigning duplicate role returns False."""
        self.manager.assign_role("user1", "trader")
        result = self.manager.assign_role("user1", "trader")
        self.assertFalse(result)

    def test_assign_multiple_roles(self):
        """Test assigning multiple roles to a user."""
        self.manager.assign_role("user1", "viewer")
        self.manager.assign_role("user1", "trader")
        roles = self.manager.get_roles("user1")
        self.assertIn("viewer", roles)
        self.assertIn("trader", roles)

    def test_remove_role(self):
        """Test removing a role from a user."""
        self.manager.assign_role("user1", "trader")
        result = self.manager.remove_role("user1", "trader")
        self.assertTrue(result)
        self.assertNotIn("trader", self.manager.get_roles("user1"))

    def test_remove_role_not_assigned(self):
        """Test removing a role that wasn't assigned."""
        result = self.manager.remove_role("user1", "trader")
        self.assertFalse(result)

    def test_remove_role_nonexistent_user(self):
        """Test removing role from nonexistent user."""
        result = self.manager.remove_role("nonexistent", "trader")
        self.assertFalse(result)

    def test_set_roles(self):
        """Test setting all roles for a user."""
        self.manager.assign_role("user1", "viewer")
        self.manager.set_roles("user1", ["admin", "trader"])
        roles = self.manager.get_roles("user1")
        self.assertIn("admin", roles)
        self.assertIn("trader", roles)
        self.assertNotIn("viewer", roles)

    def test_set_roles_new_user(self):
        """Test setting roles for a new user."""
        self.manager.set_roles("newuser", ["trader"])
        roles = self.manager.get_roles("newuser")
        self.assertEqual(roles, ["trader"])

    def test_get_roles_default(self):
        """Test that new users get default roles."""
        roles = self.manager.get_roles("newuser")
        self.assertEqual(roles, RoleManager.DEFAULT_ROLES)

    def test_has_role_true(self):
        """Test has_role returns True when user has role."""
        self.manager.assign_role("user1", "admin")
        self.assertTrue(self.manager.has_role("user1", "admin"))

    def test_has_role_false(self):
        """Test has_role returns False when user doesn't have role."""
        self.manager.assign_role("user1", "viewer")
        self.assertFalse(self.manager.has_role("user1", "admin"))

    def test_has_role_default(self):
        """Test has_role with default roles."""
        # New user should have default 'viewer' role
        self.assertTrue(self.manager.has_role("newuser", "viewer"))
        self.assertFalse(self.manager.has_role("newuser", "admin"))

    def test_get_user_role_info(self):
        """Test getting detailed user role info."""
        self.manager.assign_role("user1", "trader", assigned_by="admin")
        info = self.manager.get_user_role_info("user1")
        self.assertIsNotNone(info)
        self.assertEqual(info["user_id"], "user1")
        self.assertIn("trader", info["roles"])
        self.assertIn("created_at", info)
        self.assertIn("updated_at", info)
        self.assertEqual(info["created_by"], "admin")

    def test_get_user_role_info_nonexistent(self):
        """Test getting info for nonexistent user."""
        info = self.manager.get_user_role_info("nonexistent")
        self.assertIsNone(info)

    def test_list_users_with_role(self):
        """Test listing users with a specific role."""
        self.manager.assign_role("user1", "trader")
        self.manager.assign_role("user2", "trader")
        self.manager.assign_role("user3", "admin")

        traders = self.manager.list_users_with_role("trader")
        self.assertIn("user1", traders)
        self.assertIn("user2", traders)
        self.assertNotIn("user3", traders)

    def test_list_users_with_role_empty(self):
        """Test listing users when no one has the role."""
        users = self.manager.list_users_with_role("nonexistent_role")
        self.assertEqual(users, [])

    def test_list_all_user_roles(self):
        """Test listing all user role assignments."""
        self.manager.assign_role("user1", "trader")
        self.manager.assign_role("user2", "admin")

        all_roles = self.manager.list_all_user_roles()
        self.assertEqual(len(all_roles), 2)
        user_ids = [r["user_id"] for r in all_roles]
        self.assertIn("user1", user_ids)
        self.assertIn("user2", user_ids)

    def test_delete_user(self):
        """Test deleting user role assignments."""
        self.manager.assign_role("user1", "trader")
        result = self.manager.delete_user("user1")
        self.assertTrue(result)
        # User should now get default roles
        roles = self.manager.get_roles("user1")
        self.assertEqual(roles, RoleManager.DEFAULT_ROLES)

    def test_delete_user_nonexistent(self):
        """Test deleting nonexistent user."""
        result = self.manager.delete_user("nonexistent")
        self.assertFalse(result)

    def test_get_metrics(self):
        """Test getting role assignment metrics."""
        self.manager.assign_role("user1", "trader")
        self.manager.assign_role("user2", "trader")
        self.manager.assign_role("user3", "admin")

        metrics = self.manager.get_metrics()
        self.assertEqual(metrics["total_users"], 3)
        self.assertEqual(metrics["role_counts"]["trader"], 2)
        self.assertEqual(metrics["role_counts"]["admin"], 1)

    def test_get_metrics_empty(self):
        """Test getting metrics with no assignments."""
        metrics = self.manager.get_metrics()
        self.assertEqual(metrics["total_users"], 0)
        self.assertEqual(metrics["role_counts"], {})


class TestRoleManagerAudit(unittest.TestCase):
    """Tests for audit logging in RoleManager."""

    def setUp(self):
        self.manager = RoleManager()

    def test_assign_role_with_audit(self):
        """Test that role assignment includes audit info."""
        self.manager.assign_role("user1", "trader", assigned_by="admin")
        info = self.manager.get_user_role_info("user1")
        self.assertEqual(info["created_by"], "admin")

    def test_set_roles_with_audit(self):
        """Test that set_roles includes audit info."""
        self.manager.set_roles("user1", ["trader"], set_by="admin")
        info = self.manager.get_user_role_info("user1")
        self.assertEqual(info["created_by"], "admin")


class TestGetUserPermissions(unittest.TestCase):
    """Tests for get_user_permissions helper function."""

    def setUp(self):
        # Reset the global role manager
        role_manager._user_roles.clear()

    def test_get_user_permissions_default(self):
        """Test getting permissions for user with default roles."""
        perms = get_user_permissions("newuser", {})
        self.assertEqual(perms, RoleManager.DEFAULT_ROLES)

    def test_get_user_permissions_assigned(self):
        """Test getting permissions for user with assigned roles."""
        role_manager.assign_role("user1", "trader")
        perms = get_user_permissions("user1", {})
        self.assertIn("trader", perms)


class TestGlobalRoleManager(unittest.TestCase):
    """Tests for the global role_manager instance."""

    def setUp(self):
        # Clear any existing assignments
        role_manager._user_roles.clear()

    def test_global_instance_exists(self):
        """Test that global role_manager is available."""
        self.assertIsNotNone(role_manager)
        self.assertIsInstance(role_manager, RoleManager)

    def test_global_instance_works(self):
        """Test that global instance can be used."""
        role_manager.assign_role("testuser", "admin")
        self.assertTrue(role_manager.has_role("testuser", "admin"))


if __name__ == "__main__":
    unittest.main()
