"""
Tests for VeloZ Vault Client
"""

import os
import threading
import time
import unittest
from unittest.mock import MagicMock, patch

from vault_client import (
    CachedSecret,
    VaultClient,
    VaultConfig,
    get_vault_client,
    init_vault,
    shutdown_vault,
)


class TestVaultConfig(unittest.TestCase):
    """Test VaultConfig dataclass."""

    def test_default_values(self):
        """Test default configuration values."""
        config = VaultConfig()
        self.assertEqual(config.addr, "http://127.0.0.1:8200")
        self.assertEqual(config.auth_method, "token")
        self.assertEqual(config.cache_ttl, 300)
        self.assertTrue(config.fallback_to_env)

    def test_custom_values(self):
        """Test custom configuration values."""
        config = VaultConfig(
            addr="http://vault:8200",
            auth_method="approle",
            role_id="test-role",
            secret_id="test-secret",
            cache_ttl=600,
        )
        self.assertEqual(config.addr, "http://vault:8200")
        self.assertEqual(config.auth_method, "approle")
        self.assertEqual(config.role_id, "test-role")
        self.assertEqual(config.secret_id, "test-secret")
        self.assertEqual(config.cache_ttl, 600)


class TestVaultClientCaching(unittest.TestCase):
    """Test VaultClient caching functionality."""

    def setUp(self):
        """Set up test fixtures."""
        self.config = VaultConfig(
            addr="http://127.0.0.1:8200",
            auth_method="token",
            token="test-token",
            cache_ttl=60,
            fallback_to_env=True,
        )
        self.client = VaultClient(self.config)

    def test_cache_set_and_get(self):
        """Test setting and getting cached secrets."""
        test_data = {"api_key": "test-key", "api_secret": "test-secret"}
        self.client._set_cached("test/path", test_data)

        cached = self.client._get_cached("test/path")
        self.assertEqual(cached, test_data)

    def test_cache_expiration(self):
        """Test cache expiration."""
        # Set a very short TTL for testing
        self.client._config.cache_ttl = 1
        test_data = {"key": "value"}
        self.client._set_cached("test/path", test_data)

        # Should be cached
        self.assertEqual(self.client._get_cached("test/path"), test_data)

        # Wait for expiration
        time.sleep(1.1)

        # Should be expired
        self.assertIsNone(self.client._get_cached("test/path"))

    def test_cache_disabled(self):
        """Test that caching can be disabled."""
        self.client._config.cache_ttl = 0
        test_data = {"key": "value"}
        self.client._set_cached("test/path", test_data)

        # Should not be cached
        self.assertIsNone(self.client._get_cached("test/path"))


class TestVaultClientFallback(unittest.TestCase):
    """Test VaultClient environment variable fallback."""

    def setUp(self):
        """Set up test fixtures."""
        self.config = VaultConfig(
            addr="http://127.0.0.1:8200",
            auth_method="token",
            token="test-token",
            fallback_to_env=True,
        )
        self.client = VaultClient(self.config)

    def test_fallback_binance_credentials(self):
        """Test fallback to environment for Binance credentials."""
        with patch.dict(
            os.environ,
            {
                "VELOZ_BINANCE_API_KEY": "env-api-key",
                "VELOZ_BINANCE_API_SECRET": "env-api-secret",
            },
        ):
            api_key, api_secret = self.client.get_binance_credentials()
            self.assertEqual(api_key, "env-api-key")
            self.assertEqual(api_secret, "env-api-secret")

    def test_fallback_jwt_secret(self):
        """Test fallback to environment for JWT secret."""
        with patch.dict(os.environ, {"VELOZ_JWT_SECRET": "env-jwt-secret"}):
            jwt_secret = self.client.get_jwt_secret()
            self.assertEqual(jwt_secret, "env-jwt-secret")

    def test_fallback_admin_password(self):
        """Test fallback to environment for admin password."""
        with patch.dict(os.environ, {"VELOZ_ADMIN_PASSWORD": "env-admin-pass"}):
            admin_password = self.client.get_admin_password()
            self.assertEqual(admin_password, "env-admin-pass")

    def test_fallback_database_password(self):
        """Test fallback to environment for database password.

        Note: Only password is mapped in _get_from_env for database.
        The full database credentials use the default dict in get_database_credentials.
        """
        with patch.dict(os.environ, {"POSTGRES_PASSWORD": "testpass"}):
            password = self.client.get_secret(
                "veloz/database/postgres", key="password", default=""
            )
            self.assertEqual(password, "testpass")


class TestVaultClientEnvMapping(unittest.TestCase):
    """Test environment variable mapping."""

    def setUp(self):
        """Set up test fixtures."""
        self.config = VaultConfig(fallback_to_env=True)
        self.client = VaultClient(self.config)

    def test_get_from_env_with_key(self):
        """Test getting specific key from environment."""
        with patch.dict(os.environ, {"VELOZ_BINANCE_API_KEY": "test-key"}):
            result = self.client._get_from_env(
                "veloz/binance/credentials", "api_key", "default"
            )
            self.assertEqual(result, "test-key")

    def test_get_from_env_without_key(self):
        """Test getting all keys for a path from environment."""
        with patch.dict(
            os.environ,
            {
                "VELOZ_BINANCE_API_KEY": "test-key",
                "VELOZ_BINANCE_API_SECRET": "test-secret",
            },
        ):
            result = self.client._get_from_env(
                "veloz/binance/credentials", None, "default"
            )
            self.assertEqual(result["api_key"], "test-key")
            self.assertEqual(result["api_secret"], "test-secret")

    def test_get_from_env_default(self):
        """Test default value when env var not set."""
        result = self.client._get_from_env(
            "veloz/unknown/path", "unknown_key", "default-value"
        )
        self.assertEqual(result, "default-value")


class TestVaultClientConnection(unittest.TestCase):
    """Test VaultClient connection handling."""

    def test_is_connected_initially_false(self):
        """Test that client is not connected initially."""
        config = VaultConfig()
        client = VaultClient(config)
        self.assertFalse(client.is_connected())

    def test_disconnect_clears_state(self):
        """Test that disconnect clears client state."""
        config = VaultConfig(token="test-token")
        client = VaultClient(config)
        client._connected = True
        client._token = "test-token"
        client._cache["test"] = CachedSecret(data={}, expires_at=time.time() + 100)

        client.disconnect()

        self.assertFalse(client.is_connected())
        self.assertIsNone(client._token)
        self.assertEqual(len(client._cache), 0)


class TestVaultClientSingleton(unittest.TestCase):
    """Test module-level singleton functions."""

    def tearDown(self):
        """Clean up singleton state."""
        shutdown_vault()

    def test_get_vault_client_creates_instance(self):
        """Test that get_vault_client creates a singleton instance."""
        client1 = get_vault_client()
        client2 = get_vault_client()
        self.assertIs(client1, client2)

    def test_shutdown_vault_clears_singleton(self):
        """Test that shutdown_vault clears the singleton."""
        client1 = get_vault_client()
        shutdown_vault()
        # After shutdown, a new instance should be created
        client2 = get_vault_client()
        self.assertIsNot(client1, client2)


class TestVaultClientConfigFromEnv(unittest.TestCase):
    """Test configuration from environment variables."""

    def test_config_from_env(self):
        """Test creating config from environment variables."""
        env_vars = {
            "VAULT_ADDR": "http://vault.example.com:8200",
            "VAULT_AUTH_METHOD": "approle",
            "VAULT_TOKEN": "env-token",
            "VAULT_ROLE_ID": "env-role-id",
            "VAULT_SECRET_ID": "env-secret-id",
            "VAULT_NAMESPACE": "test-namespace",
            "VAULT_SKIP_VERIFY": "true",
            "VAULT_CACHE_TTL": "600",
            "VAULT_FALLBACK_TO_ENV": "false",
        }

        with patch.dict(os.environ, env_vars):
            config = VaultClient._config_from_env()

            self.assertEqual(config.addr, "http://vault.example.com:8200")
            self.assertEqual(config.auth_method, "approle")
            self.assertEqual(config.token, "env-token")
            self.assertEqual(config.role_id, "env-role-id")
            self.assertEqual(config.secret_id, "env-secret-id")
            self.assertEqual(config.namespace, "test-namespace")
            self.assertFalse(config.verify_tls)
            self.assertEqual(config.cache_ttl, 600)
            self.assertFalse(config.fallback_to_env)


class TestCachedSecret(unittest.TestCase):
    """Test CachedSecret dataclass."""

    def test_cached_secret_creation(self):
        """Test creating a cached secret."""
        data = {"key": "value"}
        expires_at = time.time() + 300
        cached = CachedSecret(data=data, expires_at=expires_at)

        self.assertEqual(cached.data, data)
        self.assertEqual(cached.expires_at, expires_at)

    def test_cached_secret_expiration_check(self):
        """Test checking if cached secret is expired."""
        # Not expired
        cached = CachedSecret(data={}, expires_at=time.time() + 100)
        self.assertGreater(cached.expires_at, time.time())

        # Expired
        cached = CachedSecret(data={}, expires_at=time.time() - 100)
        self.assertLess(cached.expires_at, time.time())


if __name__ == "__main__":
    unittest.main()
