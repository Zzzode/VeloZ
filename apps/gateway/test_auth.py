#!/usr/bin/env python3
"""Unit tests for JWT token refresh and authentication features."""

import unittest
import time
import sys
import os

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway import JWTManager, APIKeyManager, AuthManager


class TestJWTManager(unittest.TestCase):
    """Tests for JWTManager class."""

    def setUp(self):
        self.jwt = JWTManager("test_secret_key", token_expiry_seconds=3600)

    def test_create_access_token(self):
        """Test access token creation."""
        token = self.jwt.create_token("user123")
        self.assertIsNotNone(token)
        self.assertEqual(len(token.split(".")), 3)  # JWT has 3 parts

    def test_verify_valid_access_token(self):
        """Test verification of valid access token."""
        token = self.jwt.create_token("user123")
        payload = self.jwt.verify_token(token)
        self.assertIsNotNone(payload)
        self.assertEqual(payload["sub"], "user123")
        self.assertEqual(payload["type"], "access")

    def test_verify_expired_token(self):
        """Test that expired tokens are rejected."""
        jwt_short = JWTManager("test_secret", token_expiry_seconds=-1)  # Already expired
        token = jwt_short.create_token("user123")
        payload = jwt_short.verify_token(token)
        self.assertIsNone(payload)

    def test_verify_invalid_signature(self):
        """Test that tokens with invalid signatures are rejected."""
        token = self.jwt.create_token("user123")
        # Tamper with the token
        parts = token.split(".")
        parts[2] = "invalid_signature"
        tampered = ".".join(parts)
        payload = self.jwt.verify_token(tampered)
        self.assertIsNone(payload)

    def test_create_refresh_token(self):
        """Test refresh token creation."""
        token = self.jwt.create_refresh_token("user123")
        self.assertIsNotNone(token)
        self.assertEqual(len(token.split(".")), 3)

    def test_verify_valid_refresh_token(self):
        """Test verification of valid refresh token."""
        token = self.jwt.create_refresh_token("user123")
        payload = self.jwt.verify_refresh_token(token)
        self.assertIsNotNone(payload)
        self.assertEqual(payload["sub"], "user123")
        self.assertEqual(payload["type"], "refresh")
        self.assertIn("jti", payload)

    def test_refresh_token_has_longer_expiry(self):
        """Test that refresh tokens have 7-day expiry."""
        token = self.jwt.create_refresh_token("user123")
        payload = self.jwt.verify_refresh_token(token)
        expected_expiry = 7 * 24 * 3600
        actual_expiry = payload["exp"] - payload["iat"]
        self.assertEqual(actual_expiry, expected_expiry)

    def test_access_token_cannot_be_used_as_refresh(self):
        """Test that access tokens cannot be verified as refresh tokens."""
        access_token = self.jwt.create_token("user123")
        payload = self.jwt.verify_refresh_token(access_token)
        self.assertIsNone(payload)

    def test_refresh_token_cannot_be_used_as_access(self):
        """Test that refresh tokens cannot be verified as access tokens."""
        refresh_token = self.jwt.create_refresh_token("user123")
        payload = self.jwt.verify_token(refresh_token)
        self.assertIsNone(payload)

    def test_revoke_refresh_token(self):
        """Test refresh token revocation."""
        token = self.jwt.create_refresh_token("user123")
        # Token should be valid before revocation
        payload = self.jwt.verify_refresh_token(token)
        self.assertIsNotNone(payload)

        # Revoke the token
        result = self.jwt.revoke_refresh_token(token)
        self.assertTrue(result)

        # Token should be invalid after revocation
        payload = self.jwt.verify_refresh_token(token)
        self.assertIsNone(payload)

    def test_different_users_get_different_tokens(self):
        """Test that different users get different tokens."""
        token1 = self.jwt.create_token("user1")
        token2 = self.jwt.create_token("user2")
        self.assertNotEqual(token1, token2)

    def test_refresh_tokens_have_unique_jti(self):
        """Test that each refresh token has a unique JTI."""
        token1 = self.jwt.create_refresh_token("user123")
        token2 = self.jwt.create_refresh_token("user123")
        payload1 = self.jwt.verify_refresh_token(token1)
        payload2 = self.jwt.verify_refresh_token(token2)
        self.assertNotEqual(payload1["jti"], payload2["jti"])

    def tearDown(self):
        """Stop cleanup thread."""
        self.jwt.stop()


class TestJWTManagerCleanup(unittest.TestCase):
    """Tests for JWTManager cleanup functionality."""

    def test_get_revocation_metrics_initial(self):
        """Test initial revocation metrics."""
        jwt = JWTManager("test_secret", cleanup_interval=3600)
        try:
            metrics = jwt.get_revocation_metrics()
            self.assertEqual(metrics["revoked_token_count"], 0)
            self.assertEqual(metrics["total_cleaned_up"], 0)
            self.assertEqual(metrics["cleanup_interval_seconds"], 3600)
        finally:
            jwt.stop()

    def test_get_revocation_metrics_after_revoke(self):
        """Test revocation metrics after revoking tokens."""
        jwt = JWTManager("test_secret", cleanup_interval=3600)
        try:
            # Revoke some tokens
            token1 = jwt.create_refresh_token("user1")
            token2 = jwt.create_refresh_token("user2")
            jwt.revoke_refresh_token(token1)
            jwt.revoke_refresh_token(token2)

            metrics = jwt.get_revocation_metrics()
            self.assertEqual(metrics["revoked_token_count"], 2)
        finally:
            jwt.stop()

    def test_cleanup_expired_tokens(self):
        """Test that expired revoked tokens are cleaned up."""
        jwt = JWTManager("test_secret", cleanup_interval=3600)
        try:
            # Manually add an expired revoked token
            expired_time = int(time.time()) - (8 * 24 * 3600)  # 8 days ago (beyond 7-day expiry)
            with jwt._mu:
                jwt._revoked_refresh_tokens["expired_jti"] = expired_time
                jwt._revoked_refresh_tokens["valid_jti"] = int(time.time())

            # Run cleanup
            removed = jwt._cleanup_expired_tokens()

            # Should have removed the expired token
            self.assertEqual(removed, 1)
            metrics = jwt.get_revocation_metrics()
            self.assertEqual(metrics["revoked_token_count"], 1)
            self.assertEqual(metrics["total_cleaned_up"], 1)
        finally:
            jwt.stop()

    def test_cleanup_preserves_valid_tokens(self):
        """Test that valid revoked tokens are preserved during cleanup."""
        jwt = JWTManager("test_secret", cleanup_interval=3600)
        try:
            # Revoke a token
            token = jwt.create_refresh_token("user1")
            jwt.revoke_refresh_token(token)

            # Run cleanup
            removed = jwt._cleanup_expired_tokens()

            # Should not have removed anything
            self.assertEqual(removed, 0)
            metrics = jwt.get_revocation_metrics()
            self.assertEqual(metrics["revoked_token_count"], 1)
        finally:
            jwt.stop()

    def test_stop_cleanup_thread(self):
        """Test that cleanup thread can be stopped gracefully."""
        jwt = JWTManager("test_secret", cleanup_interval=1)  # Short interval for testing
        self.assertTrue(jwt._cleanup_thread.is_alive())

        jwt.stop()

        # Thread should stop
        self.assertFalse(jwt._cleanup_thread.is_alive())

    def test_custom_cleanup_interval(self):
        """Test custom cleanup interval configuration."""
        jwt = JWTManager("test_secret", cleanup_interval=7200)  # 2 hours
        try:
            metrics = jwt.get_revocation_metrics()
            self.assertEqual(metrics["cleanup_interval_seconds"], 7200)
        finally:
            jwt.stop()


class TestAPIKeyManager(unittest.TestCase):
    """Tests for APIKeyManager class."""

    def setUp(self):
        self.manager = APIKeyManager()

    def test_create_key(self):
        """Test API key creation."""
        key_id, raw_key = self.manager.create_key("user123", "test-key")
        self.assertTrue(key_id.startswith("vk_"))
        self.assertTrue(raw_key.startswith("veloz_"))

    def test_validate_valid_key(self):
        """Test validation of valid API key."""
        key_id, raw_key = self.manager.create_key("user123", "test-key")
        api_key = self.manager.validate_key(raw_key)
        self.assertIsNotNone(api_key)
        self.assertEqual(api_key.user_id, "user123")
        self.assertEqual(api_key.key_id, key_id)

    def test_validate_invalid_key(self):
        """Test validation of invalid API key."""
        api_key = self.manager.validate_key("invalid_key")
        self.assertIsNone(api_key)

    def test_revoke_key(self):
        """Test API key revocation."""
        key_id, raw_key = self.manager.create_key("user123", "test-key")
        # Key should be valid before revocation
        self.assertIsNotNone(self.manager.validate_key(raw_key))

        # Revoke the key
        result = self.manager.revoke_key(key_id)
        self.assertTrue(result)

        # Key should be invalid after revocation
        self.assertIsNone(self.manager.validate_key(raw_key))

    def test_list_keys(self):
        """Test listing API keys."""
        self.manager.create_key("user1", "key1")
        self.manager.create_key("user2", "key2")
        self.manager.create_key("user1", "key3")

        # List all keys
        all_keys = self.manager.list_keys()
        self.assertEqual(len(all_keys), 3)

        # List keys for specific user
        user1_keys = self.manager.list_keys("user1")
        self.assertEqual(len(user1_keys), 2)

    def test_key_permissions(self):
        """Test API key permissions."""
        key_id, raw_key = self.manager.create_key(
            "user123", "admin-key", permissions=["read", "write", "admin"]
        )
        api_key = self.manager.validate_key(raw_key)
        self.assertEqual(api_key.permissions, ["read", "write", "admin"])


class TestAuthManager(unittest.TestCase):
    """Tests for AuthManager class."""

    def setUp(self):
        os.environ["VELOZ_ADMIN_PASSWORD"] = "test_password"
        self.auth = AuthManager(
            jwt_secret="test_secret",
            token_expiry=3600,
            auth_enabled=True,
        )

    def tearDown(self):
        self.auth.stop()
        if "VELOZ_ADMIN_PASSWORD" in os.environ:
            del os.environ["VELOZ_ADMIN_PASSWORD"]

    def test_create_token_valid_credentials(self):
        """Test token creation with valid credentials."""
        tokens = self.auth.create_token("admin", "test_password")
        self.assertIsNotNone(tokens)
        self.assertIn("access_token", tokens)
        self.assertIn("refresh_token", tokens)
        self.assertIn("expires_in", tokens)

    def test_create_token_invalid_credentials(self):
        """Test token creation with invalid credentials."""
        tokens = self.auth.create_token("admin", "wrong_password")
        self.assertIsNone(tokens)

    def test_refresh_access_token(self):
        """Test refreshing access token."""
        tokens = self.auth.create_token("admin", "test_password")
        refresh_token = tokens["refresh_token"]

        result = self.auth.refresh_access_token(refresh_token)
        self.assertIsNotNone(result)
        self.assertIn("access_token", result)
        self.assertIn("expires_in", result)

    def test_refresh_with_invalid_token(self):
        """Test refresh with invalid token."""
        result = self.auth.refresh_access_token("invalid_token")
        self.assertIsNone(result)

    def test_revoke_refresh_token(self):
        """Test refresh token revocation via AuthManager."""
        tokens = self.auth.create_token("admin", "test_password")
        refresh_token = tokens["refresh_token"]

        # Should be able to refresh before revocation
        self.assertIsNotNone(self.auth.refresh_access_token(refresh_token))

        # Revoke
        self.auth.revoke_refresh_token(refresh_token)

        # Should not be able to refresh after revocation
        self.assertIsNone(self.auth.refresh_access_token(refresh_token))

    def test_authenticate_with_jwt(self):
        """Test authentication with JWT token."""
        tokens = self.auth.create_token("admin", "test_password")
        headers = {"Authorization": f"Bearer {tokens['access_token']}"}

        auth_info, error = self.auth.authenticate(headers)
        self.assertIsNotNone(auth_info)
        self.assertEqual(auth_info["user_id"], "admin")
        self.assertEqual(auth_info["auth_method"], "jwt")
        self.assertEqual(error, "")

    def test_authenticate_with_api_key(self):
        """Test authentication with API key."""
        key_id, raw_key = self.auth.create_api_key("user123", "test-key")
        headers = {"X-API-Key": raw_key}

        auth_info, error = self.auth.authenticate(headers)
        self.assertIsNotNone(auth_info)
        self.assertEqual(auth_info["user_id"], "user123")
        self.assertEqual(auth_info["auth_method"], "api_key")

    def test_authenticate_missing_credentials(self):
        """Test authentication with missing credentials."""
        auth_info, error = self.auth.authenticate({})
        self.assertIsNone(auth_info)
        self.assertEqual(error, "authentication_required")

    def test_public_endpoints(self):
        """Test public endpoint detection."""
        self.assertTrue(self.auth.is_public_endpoint("/health"))
        self.assertTrue(self.auth.is_public_endpoint("/api/stream"))
        self.assertFalse(self.auth.is_public_endpoint("/api/orders"))

    def test_get_revocation_metrics(self):
        """Test getting revocation metrics from AuthManager."""
        metrics = self.auth.get_revocation_metrics()
        self.assertIn("revoked_token_count", metrics)
        self.assertIn("total_cleaned_up", metrics)
        self.assertIn("cleanup_interval_seconds", metrics)


if __name__ == "__main__":
    unittest.main()
