#!/usr/bin/env python3
"""Integration tests for authentication and authorization flows.

Tests the complete authentication workflow:
- JWT token creation and validation
- Token refresh flow
- API key authentication
- Permission enforcement
- Rate limiting
- Audit logging
"""

import json
import os
import sys
import tempfile
import threading
import time
import unittest
from http.server import ThreadingHTTPServer
from io import BytesIO
from unittest.mock import MagicMock, patch

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway import (
    JWTManager,
    APIKeyManager,
    AuthManager,
    RateLimiter,
)
from rbac import RoleManager
from audit import AuditLogStore, create_audit_entry


class MockRequest:
    """Mock HTTP request for testing."""

    def __init__(self, method: str, path: str, headers: dict = None, body: bytes = None):
        self.command = method
        self.path = path
        self.headers = headers or {}
        self.body = body or b""

    def makefile(self, mode):
        if "r" in mode:
            return BytesIO(self.body)
        return BytesIO()


class TestJWTAuthenticationFlow(unittest.TestCase):
    """Tests for JWT authentication workflow."""

    def setUp(self):
        self.jwt = JWTManager("test_secret_key_12345", token_expiry_seconds=3600)

    def tearDown(self):
        self.jwt.stop()

    def test_complete_jwt_flow(self):
        """Test complete JWT flow: create -> verify -> refresh -> revoke."""
        # 1. Create access token
        access_token = self.jwt.create_token("user123")
        self.assertIsNotNone(access_token)

        # 2. Verify access token
        payload = self.jwt.verify_token(access_token)
        self.assertIsNotNone(payload)
        self.assertEqual(payload["sub"], "user123")
        self.assertEqual(payload["type"], "access")

        # 3. Create refresh token
        refresh_token = self.jwt.create_refresh_token("user123")
        self.assertIsNotNone(refresh_token)

        # 4. Verify refresh token
        refresh_payload = self.jwt.verify_refresh_token(refresh_token)
        self.assertIsNotNone(refresh_payload)
        self.assertEqual(refresh_payload["sub"], "user123")
        self.assertEqual(refresh_payload["type"], "refresh")

        # 5. Revoke refresh token
        result = self.jwt.revoke_refresh_token(refresh_token)
        self.assertTrue(result)

        # 6. Verify revoked token is rejected
        revoked_payload = self.jwt.verify_refresh_token(refresh_token)
        self.assertIsNone(revoked_payload)

    def test_access_token_cannot_refresh(self):
        """Test that access tokens cannot be used for refresh."""
        access_token = self.jwt.create_token("user123")
        refresh_payload = self.jwt.verify_refresh_token(access_token)
        self.assertIsNone(refresh_payload)

    def test_refresh_token_cannot_access(self):
        """Test that refresh tokens cannot be used for access."""
        refresh_token = self.jwt.create_refresh_token("user123")
        access_payload = self.jwt.verify_token(refresh_token)
        self.assertIsNone(access_payload)

    def test_token_tampering_detected(self):
        """Test that tampered tokens are rejected."""
        token = self.jwt.create_token("user123")

        # Tamper with payload
        parts = token.split(".")
        parts[1] = parts[1][:-2] + "XX"  # Modify payload
        tampered = ".".join(parts)

        payload = self.jwt.verify_token(tampered)
        self.assertIsNone(payload)

    def test_multiple_users_independent(self):
        """Test that multiple users have independent tokens."""
        token1 = self.jwt.create_token("user1")
        token2 = self.jwt.create_token("user2")

        payload1 = self.jwt.verify_token(token1)
        payload2 = self.jwt.verify_token(token2)

        self.assertEqual(payload1["sub"], "user1")
        self.assertEqual(payload2["sub"], "user2")
        self.assertNotEqual(token1, token2)


class TestAPIKeyAuthenticationFlow(unittest.TestCase):
    """Tests for API key authentication workflow."""

    def setUp(self):
        self.manager = APIKeyManager()

    def test_complete_api_key_flow(self):
        """Test complete API key flow: create -> validate -> revoke."""
        # 1. Create API key
        key_id, raw_key = self.manager.create_key("user123", "test-key")
        self.assertTrue(key_id.startswith("vk_"))
        self.assertTrue(raw_key.startswith("veloz_"))

        # 2. Validate API key
        api_key = self.manager.validate_key(raw_key)
        self.assertIsNotNone(api_key)
        self.assertEqual(api_key.user_id, "user123")
        self.assertEqual(api_key.key_id, key_id)

        # 3. Revoke API key
        result = self.manager.revoke_key(key_id)
        self.assertTrue(result)

        # 4. Verify revoked key is rejected
        revoked_key = self.manager.validate_key(raw_key)
        self.assertIsNone(revoked_key)

    def test_api_key_permissions(self):
        """Test API key with specific permissions."""
        key_id, raw_key = self.manager.create_key(
            "user123", "admin-key", permissions=["read", "write", "admin"]
        )

        api_key = self.manager.validate_key(raw_key)
        self.assertEqual(api_key.permissions, ["read", "write", "admin"])

    def test_multiple_keys_per_user(self):
        """Test multiple API keys for same user."""
        key_id1, raw_key1 = self.manager.create_key("user123", "key1")
        key_id2, raw_key2 = self.manager.create_key("user123", "key2")

        # Both keys should be valid
        api_key1 = self.manager.validate_key(raw_key1)
        api_key2 = self.manager.validate_key(raw_key2)

        self.assertIsNotNone(api_key1)
        self.assertIsNotNone(api_key2)
        self.assertNotEqual(key_id1, key_id2)

        # List keys for user
        user_keys = self.manager.list_keys("user123")
        self.assertEqual(len(user_keys), 2)


class TestAuthManagerIntegration(unittest.TestCase):
    """Tests for AuthManager integration."""

    def setUp(self):
        os.environ["VELOZ_ADMIN_PASSWORD"] = "test_password_123"
        self.auth = AuthManager(
            jwt_secret="test_secret_key_12345",
            token_expiry=3600,
            auth_enabled=True,
        )

    def tearDown(self):
        self.auth.stop()
        if "VELOZ_ADMIN_PASSWORD" in os.environ:
            del os.environ["VELOZ_ADMIN_PASSWORD"]

    def test_login_and_authenticate_flow(self):
        """Test complete login and authentication flow."""
        # 1. Login with credentials
        tokens = self.auth.create_token("admin", "test_password_123")
        self.assertIsNotNone(tokens)
        self.assertIn("access_token", tokens)
        self.assertIn("refresh_token", tokens)

        # 2. Authenticate with access token
        headers = {"Authorization": f"Bearer {tokens['access_token']}"}
        auth_info, error = self.auth.authenticate(headers)

        self.assertIsNotNone(auth_info)
        self.assertEqual(auth_info["user_id"], "admin")
        self.assertEqual(auth_info["auth_method"], "jwt")
        self.assertEqual(error, "")

    def test_api_key_authentication_flow(self):
        """Test API key authentication flow."""
        # 1. Create API key
        key_id, raw_key = self.auth.create_api_key("user123", "test-key")

        # 2. Authenticate with API key
        headers = {"X-API-Key": raw_key}
        auth_info, error = self.auth.authenticate(headers)

        self.assertIsNotNone(auth_info)
        self.assertEqual(auth_info["user_id"], "user123")
        self.assertEqual(auth_info["auth_method"], "api_key")

    def test_token_refresh_flow(self):
        """Test token refresh flow."""
        # 1. Login
        tokens = self.auth.create_token("admin", "test_password_123")

        # 2. Refresh token
        new_tokens = self.auth.refresh_access_token(tokens["refresh_token"])
        self.assertIsNotNone(new_tokens)
        self.assertIn("access_token", new_tokens)

        # 3. New token should work
        headers = {"Authorization": f"Bearer {new_tokens['access_token']}"}
        auth_info, error = self.auth.authenticate(headers)
        self.assertIsNotNone(auth_info)

    def test_invalid_credentials_rejected(self):
        """Test that invalid credentials are rejected."""
        tokens = self.auth.create_token("admin", "wrong_password")
        self.assertIsNone(tokens)

    def test_missing_auth_rejected(self):
        """Test that missing authentication is rejected."""
        auth_info, error = self.auth.authenticate({})
        self.assertIsNone(auth_info)
        self.assertEqual(error, "authentication_required")

    def test_invalid_token_rejected(self):
        """Test that invalid tokens are rejected."""
        headers = {"Authorization": "Bearer invalid_token_here"}
        auth_info, error = self.auth.authenticate(headers)
        self.assertIsNone(auth_info)

    def test_public_endpoints_bypass_auth(self):
        """Test that public endpoints bypass authentication."""
        self.assertTrue(self.auth.is_public_endpoint("/health"))
        self.assertTrue(self.auth.is_public_endpoint("/api/stream"))
        self.assertFalse(self.auth.is_public_endpoint("/api/orders"))


class TestRBACIntegration(unittest.TestCase):
    """Tests for Role-Based Access Control integration."""

    def setUp(self):
        self.role_manager = RoleManager()

    def test_role_assignment_flow(self):
        """Test complete role assignment flow."""
        # 1. Assign role
        result = self.role_manager.assign_role("user123", "trader")
        self.assertTrue(result)

        # 2. Check role
        has_role = self.role_manager.has_role("user123", "trader")
        self.assertTrue(has_role)

        # 3. Get all roles
        roles = self.role_manager.get_roles("user123")
        self.assertIn("trader", roles)

        # 4. Remove role
        result = self.role_manager.remove_role("user123", "trader")
        self.assertTrue(result)

        # 5. Verify removed
        has_role = self.role_manager.has_role("user123", "trader")
        self.assertFalse(has_role)

    def test_multiple_roles(self):
        """Test user with multiple roles."""
        self.role_manager.assign_role("user123", "viewer")
        self.role_manager.assign_role("user123", "trader")

        roles = self.role_manager.get_roles("user123")
        self.assertIn("viewer", roles)
        self.assertIn("trader", roles)

    def test_set_roles_replaces_all(self):
        """Test that set_roles replaces all existing roles."""
        self.role_manager.assign_role("user123", "viewer")
        self.role_manager.assign_role("user123", "trader")

        # Set new roles
        self.role_manager.set_roles("user123", ["admin"])

        roles = self.role_manager.get_roles("user123")
        self.assertEqual(roles, ["admin"])


class TestRateLimitingIntegration(unittest.TestCase):
    """Tests for rate limiting integration."""

    def test_rate_limit_enforcement(self):
        """Test that rate limits are enforced."""
        limiter = RateLimiter(capacity=5, refill_rate=5.0)  # 5 requests capacity

        # First 5 requests should pass
        for i in range(5):
            allowed, headers = limiter.check_rate_limit("user123")
            self.assertTrue(allowed, f"Request {i+1} should be allowed")

        # 6th request should be rate limited
        allowed, headers = limiter.check_rate_limit("user123")
        self.assertFalse(allowed)
        self.assertIn("Retry-After", headers)
        self.assertGreater(int(headers["Retry-After"]), 0)

    def test_rate_limit_per_user(self):
        """Test that rate limits are per-user."""
        limiter = RateLimiter(capacity=2, refill_rate=2.0)

        # User 1 uses up their limit
        limiter.check_rate_limit("user1")
        limiter.check_rate_limit("user1")
        allowed, _ = limiter.check_rate_limit("user1")
        self.assertFalse(allowed)

        # User 2 should still have capacity
        allowed, _ = limiter.check_rate_limit("user2")
        self.assertTrue(allowed)

    def test_rate_limit_recovery(self):
        """Test that rate limits recover over time."""
        limiter = RateLimiter(capacity=2, refill_rate=10.0)  # Fast refill

        # Use up capacity
        limiter.check_rate_limit("user123")
        limiter.check_rate_limit("user123")

        # Wait for refill
        time.sleep(0.3)

        # Should have capacity again
        allowed, _ = limiter.check_rate_limit("user123")
        self.assertTrue(allowed)


class TestAuditLoggingIntegration(unittest.TestCase):
    """Tests for audit logging integration."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.store = AuditLogStore(log_dir=self.temp_dir)

    def tearDown(self):
        self.store.close()
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_auth_events_logged(self):
        """Test that authentication events are logged."""
        # Log auth event
        entry = create_audit_entry(
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
            details={"method": "password"},
        )
        self.store.write(entry)

        # Verify logged
        logs = self.store.get_recent_logs(log_type="auth")
        self.assertEqual(len(logs), 1)
        self.assertEqual(logs[0]["action"], "login")
        self.assertEqual(logs[0]["user_id"], "user123")

    def test_order_events_logged(self):
        """Test that order events are logged."""
        entry = create_audit_entry(
            log_type="order",
            action="place_order",
            user_id="user123",
            ip_address="192.168.1.1",
            details={"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1},
        )
        self.store.write(entry)

        logs = self.store.get_recent_logs(log_type="order")
        self.assertEqual(len(logs), 1)
        self.assertEqual(logs[0]["details"]["symbol"], "BTCUSDT")

    def test_audit_trail_completeness(self):
        """Test that audit trail captures all events."""
        # Log multiple events
        events = [
            ("auth", "login", "user1"),
            ("order", "place_order", "user1"),
            ("order", "cancel_order", "user1"),
            ("auth", "logout", "user1"),
        ]

        for log_type, action, user_id in events:
            entry = create_audit_entry(
                log_type=log_type,
                action=action,
                user_id=user_id,
                ip_address="192.168.1.1",
            )
            self.store.write(entry)

        # Verify all logged
        all_logs = self.store.get_recent_logs(limit=10)
        self.assertEqual(len(all_logs), 4)

        # Verify user filter works
        user_logs = self.store.get_recent_logs(user_id="user1")
        self.assertEqual(len(user_logs), 4)


class TestEndToEndAuthFlow(unittest.TestCase):
    """End-to-end tests for authentication flows."""

    def setUp(self):
        os.environ["VELOZ_ADMIN_PASSWORD"] = "test_password_123"
        self.temp_dir = tempfile.mkdtemp()
        self.auth = AuthManager(
            jwt_secret="test_secret_key_12345",
            token_expiry=3600,
            auth_enabled=True,
        )
        self.role_manager = RoleManager()
        self.audit_store = AuditLogStore(log_dir=self.temp_dir)

    def tearDown(self):
        self.auth.stop()
        self.audit_store.close()
        if "VELOZ_ADMIN_PASSWORD" in os.environ:
            del os.environ["VELOZ_ADMIN_PASSWORD"]
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_complete_user_session(self):
        """Test complete user session: login -> actions -> logout."""
        # 1. Login
        tokens = self.auth.create_token("admin", "test_password_123")
        self.assertIsNotNone(tokens)

        # Log login event
        self.audit_store.write(create_audit_entry(
            "auth", "login", "admin", "192.168.1.1"
        ))

        # 2. Authenticate and perform action
        headers = {"Authorization": f"Bearer {tokens['access_token']}"}
        auth_info, _ = self.auth.authenticate(headers)
        self.assertIsNotNone(auth_info)

        # Log action
        self.audit_store.write(create_audit_entry(
            "order", "place_order", "admin", "192.168.1.1",
            details={"symbol": "BTCUSDT"}
        ))

        # 3. Refresh token
        new_tokens = self.auth.refresh_access_token(tokens["refresh_token"])
        self.assertIsNotNone(new_tokens)

        # 4. Logout (revoke refresh token)
        self.auth.revoke_refresh_token(tokens["refresh_token"])

        # Log logout
        self.audit_store.write(create_audit_entry(
            "auth", "logout", "admin", "192.168.1.1"
        ))

        # 5. Verify session ended
        result = self.auth.refresh_access_token(tokens["refresh_token"])
        self.assertIsNone(result)

        # 6. Verify audit trail
        logs = self.audit_store.get_recent_logs(user_id="admin")
        self.assertEqual(len(logs), 3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
