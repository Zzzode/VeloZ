#!/usr/bin/env python3
"""Unit tests for permission-based access control."""

import unittest
import sys
import os

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway import (
    Permission,
    PermissionManager,
    ROLE_PERMISSIONS,
    ENDPOINT_PERMISSIONS,
    permission_manager,
)


class TestPermission(unittest.TestCase):
    """Tests for Permission constants."""

    def test_read_permissions_exist(self):
        """Test that read permissions are defined."""
        self.assertEqual(Permission.READ_MARKET, "read:market")
        self.assertEqual(Permission.READ_ORDERS, "read:orders")
        self.assertEqual(Permission.READ_ACCOUNT, "read:account")
        self.assertEqual(Permission.READ_CONFIG, "read:config")

    def test_write_permissions_exist(self):
        """Test that write permissions are defined."""
        self.assertEqual(Permission.WRITE_ORDERS, "write:orders")
        self.assertEqual(Permission.WRITE_CANCEL, "write:cancel")

    def test_admin_permissions_exist(self):
        """Test that admin permissions are defined."""
        self.assertEqual(Permission.ADMIN_KEYS, "admin:keys")
        self.assertEqual(Permission.ADMIN_USERS, "admin:users")
        self.assertEqual(Permission.ADMIN_CONFIG, "admin:config")

    def test_legacy_permissions_exist(self):
        """Test that legacy permissions are defined."""
        self.assertEqual(Permission.LEGACY_READ, "read")
        self.assertEqual(Permission.LEGACY_WRITE, "write")
        self.assertEqual(Permission.LEGACY_ADMIN, "admin")


class TestRolePermissions(unittest.TestCase):
    """Tests for role-based permission definitions."""

    def test_viewer_role_has_limited_permissions(self):
        """Test that viewer role has read-only permissions."""
        viewer_perms = ROLE_PERMISSIONS["viewer"]
        self.assertIn(Permission.READ_MARKET, viewer_perms)
        self.assertIn(Permission.READ_CONFIG, viewer_perms)
        self.assertNotIn(Permission.WRITE_ORDERS, viewer_perms)
        self.assertNotIn(Permission.ADMIN_KEYS, viewer_perms)

    def test_trader_role_has_trading_permissions(self):
        """Test that trader role has trading permissions."""
        trader_perms = ROLE_PERMISSIONS["trader"]
        self.assertIn(Permission.READ_MARKET, trader_perms)
        self.assertIn(Permission.READ_ORDERS, trader_perms)
        self.assertIn(Permission.READ_ACCOUNT, trader_perms)
        self.assertIn(Permission.WRITE_ORDERS, trader_perms)
        self.assertIn(Permission.WRITE_CANCEL, trader_perms)
        self.assertNotIn(Permission.ADMIN_KEYS, trader_perms)

    def test_admin_role_has_all_permissions(self):
        """Test that admin role has all permissions."""
        admin_perms = ROLE_PERMISSIONS["admin"]
        self.assertIn(Permission.READ_MARKET, admin_perms)
        self.assertIn(Permission.WRITE_ORDERS, admin_perms)
        self.assertIn(Permission.ADMIN_KEYS, admin_perms)
        self.assertIn(Permission.ADMIN_USERS, admin_perms)
        self.assertIn(Permission.ADMIN_CONFIG, admin_perms)


class TestPermissionManager(unittest.TestCase):
    """Tests for PermissionManager class."""

    def setUp(self):
        self.manager = PermissionManager()

    def test_expand_role_permissions(self):
        """Test expanding role to granular permissions."""
        expanded = self.manager.expand_permissions(["trader"])
        self.assertIn(Permission.READ_MARKET, expanded)
        self.assertIn(Permission.WRITE_ORDERS, expanded)
        self.assertNotIn(Permission.ADMIN_KEYS, expanded)

    def test_expand_legacy_read_permission(self):
        """Test expanding legacy 'read' permission."""
        expanded = self.manager.expand_permissions(["read"])
        self.assertIn(Permission.READ_MARKET, expanded)
        self.assertIn(Permission.READ_ORDERS, expanded)
        self.assertIn(Permission.READ_ACCOUNT, expanded)
        self.assertIn(Permission.READ_CONFIG, expanded)
        self.assertNotIn(Permission.WRITE_ORDERS, expanded)

    def test_expand_legacy_write_permission(self):
        """Test expanding legacy 'write' permission."""
        expanded = self.manager.expand_permissions(["write"])
        self.assertIn(Permission.WRITE_ORDERS, expanded)
        self.assertIn(Permission.WRITE_CANCEL, expanded)
        self.assertNotIn(Permission.READ_MARKET, expanded)

    def test_expand_legacy_admin_permission(self):
        """Test expanding legacy 'admin' permission."""
        expanded = self.manager.expand_permissions(["admin"])
        self.assertIn(Permission.ADMIN_KEYS, expanded)
        self.assertIn(Permission.ADMIN_USERS, expanded)
        self.assertIn(Permission.ADMIN_CONFIG, expanded)

    def test_expand_granular_permissions(self):
        """Test that granular permissions pass through unchanged."""
        expanded = self.manager.expand_permissions([Permission.READ_MARKET])
        self.assertIn(Permission.READ_MARKET, expanded)
        self.assertEqual(len(expanded), 1)

    def test_expand_mixed_permissions(self):
        """Test expanding a mix of roles, legacy, and granular permissions."""
        expanded = self.manager.expand_permissions([
            "viewer",
            "write",
            Permission.ADMIN_KEYS,
        ])
        # From viewer role
        self.assertIn(Permission.READ_MARKET, expanded)
        # From legacy write
        self.assertIn(Permission.WRITE_ORDERS, expanded)
        # Direct granular permission
        self.assertIn(Permission.ADMIN_KEYS, expanded)

    def test_check_permission_with_matching_permission(self):
        """Test permission check with matching permission."""
        result = self.manager.check_permission(
            user_permissions=["read"],
            required_permissions=[Permission.READ_MARKET],
        )
        self.assertTrue(result)

    def test_check_permission_with_role(self):
        """Test permission check with role."""
        result = self.manager.check_permission(
            user_permissions=["trader"],
            required_permissions=[Permission.WRITE_ORDERS],
        )
        self.assertTrue(result)

    def test_check_permission_denied(self):
        """Test permission check when permission is denied."""
        result = self.manager.check_permission(
            user_permissions=["viewer"],
            required_permissions=[Permission.WRITE_ORDERS],
        )
        self.assertFalse(result)

    def test_check_permission_empty_required(self):
        """Test permission check with no required permissions."""
        result = self.manager.check_permission(
            user_permissions=[],
            required_permissions=[],
        )
        self.assertTrue(result)

    def test_check_permission_any_of_required(self):
        """Test that having any of the required permissions grants access."""
        result = self.manager.check_permission(
            user_permissions=[Permission.READ_MARKET],
            required_permissions=[Permission.READ_MARKET, Permission.READ_ORDERS],
        )
        self.assertTrue(result)


class TestEndpointPermissions(unittest.TestCase):
    """Tests for endpoint permission mappings."""

    def test_market_endpoint_requires_read_market(self):
        """Test that /api/market requires read:market."""
        perms = ENDPOINT_PERMISSIONS.get("/api/market", [])
        self.assertIn(Permission.READ_MARKET, perms)

    def test_order_endpoint_requires_write_orders(self):
        """Test that /api/order requires write:orders."""
        perms = ENDPOINT_PERMISSIONS.get("/api/order", [])
        self.assertIn(Permission.WRITE_ORDERS, perms)

    def test_cancel_endpoint_requires_write_cancel(self):
        """Test that /api/cancel requires write:cancel."""
        perms = ENDPOINT_PERMISSIONS.get("/api/cancel", [])
        self.assertIn(Permission.WRITE_CANCEL, perms)

    def test_auth_keys_endpoint_requires_admin(self):
        """Test that /api/auth/keys requires admin:keys."""
        perms = ENDPOINT_PERMISSIONS.get("/api/auth/keys", [])
        self.assertIn(Permission.ADMIN_KEYS, perms)

    def test_get_endpoint_permissions_exact_match(self):
        """Test getting permissions for exact endpoint match."""
        perms = permission_manager.get_endpoint_permissions("/api/market", "GET")
        self.assertIn(Permission.READ_MARKET, perms)

    def test_get_endpoint_permissions_default_get(self):
        """Test default permissions for unknown GET endpoint."""
        perms = permission_manager.get_endpoint_permissions("/api/unknown", "GET")
        self.assertIn(Permission.READ_CONFIG, perms)

    def test_get_endpoint_permissions_default_post(self):
        """Test default permissions for unknown POST endpoint."""
        perms = permission_manager.get_endpoint_permissions("/api/unknown", "POST")
        self.assertIn(Permission.WRITE_ORDERS, perms)


class TestPermissionIntegration(unittest.TestCase):
    """Integration tests for permission system."""

    def test_viewer_can_access_market_data(self):
        """Test that viewer can access market data."""
        result = permission_manager.check_permission(
            user_permissions=["viewer"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/market", "GET"),
        )
        self.assertTrue(result)

    def test_viewer_cannot_place_orders(self):
        """Test that viewer cannot place orders."""
        result = permission_manager.check_permission(
            user_permissions=["viewer"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/order", "POST"),
        )
        self.assertFalse(result)

    def test_trader_can_place_orders(self):
        """Test that trader can place orders."""
        result = permission_manager.check_permission(
            user_permissions=["trader"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/order", "POST"),
        )
        self.assertTrue(result)

    def test_trader_cannot_manage_api_keys(self):
        """Test that trader cannot manage API keys."""
        result = permission_manager.check_permission(
            user_permissions=["trader"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/auth/keys", "POST"),
        )
        self.assertFalse(result)

    def test_admin_can_manage_api_keys(self):
        """Test that admin can manage API keys."""
        result = permission_manager.check_permission(
            user_permissions=["admin"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/auth/keys", "POST"),
        )
        self.assertTrue(result)

    def test_legacy_read_write_permissions(self):
        """Test that legacy read+write permissions work for trading."""
        result = permission_manager.check_permission(
            user_permissions=["read", "write"],
            required_permissions=permission_manager.get_endpoint_permissions("/api/order", "POST"),
        )
        self.assertTrue(result)


if __name__ == "__main__":
    unittest.main()
