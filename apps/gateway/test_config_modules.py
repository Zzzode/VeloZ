#!/usr/bin/env python3
"""Unit tests for GUI configuration modules: keychain, config_manager, exchange_tester."""

import unittest
import json
import os
import sys
import tempfile
import shutil
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from keychain import (
    StoredAPIKey,
    KeychainBackend,
    KeyringBackend,
    KeychainManager,
    get_keychain_manager,
)
from config_manager import (
    ConfigPaths,
    ConfigManager,
    DEFAULT_CONFIG,
    get_config_manager,
)
from exchange_tester import (
    ConnectionTestResult,
    ExchangeTester,
    BinanceAdapter,
    get_exchange_tester,
)


class TestStoredAPIKey(unittest.TestCase):
    """Tests for StoredAPIKey dataclass."""

    def test_to_dict(self):
        """Test conversion to dictionary."""
        key = StoredAPIKey(
            id="test-id",
            exchange="binance",
            name="Test Key",
            api_key="api123",
            api_secret="secret456",
            passphrase=None,
            testnet=True,
            permissions=["read", "trade"],
            created_at="2024-01-01T00:00:00",
            last_used=None,
            status="active",
        )
        data = key.to_dict()
        self.assertEqual(data["id"], "test-id")
        self.assertEqual(data["exchange"], "binance")
        self.assertEqual(data["api_key"], "api123")

    def test_from_dict(self):
        """Test creation from dictionary."""
        data = {
            "id": "test-id",
            "exchange": "binance",
            "name": "Test Key",
            "api_key": "api123",
            "api_secret": "secret456",
            "passphrase": None,
            "testnet": True,
            "permissions": ["read", "trade"],
            "created_at": "2024-01-01T00:00:00",
            "last_used": None,
            "status": "active",
        }
        key = StoredAPIKey.from_dict(data)
        self.assertEqual(key.id, "test-id")
        self.assertEqual(key.exchange, "binance")

    def test_to_public_info(self):
        """Test public info excludes sensitive data."""
        key = StoredAPIKey(
            id="test-id",
            exchange="binance",
            name="Test Key",
            api_key="api123",
            api_secret="secret456",
            passphrase="pass789",
            testnet=True,
            permissions=["read", "trade"],
            created_at="2024-01-01T00:00:00",
            last_used=None,
            status="active",
        )
        public = key.to_public_info()
        self.assertIn("id", public)
        self.assertIn("exchange", public)
        self.assertNotIn("api_key", public)
        self.assertNotIn("api_secret", public)
        self.assertNotIn("passphrase", public)


class TestKeyringBackend(unittest.TestCase):
    """Tests for KeyringBackend class."""

    def setUp(self):
        """Set up test backend with fallback storage."""
        self.backend = KeyringBackend()
        # Force fallback mode for testing
        self.backend._available = False
        self.backend._fallback_storage = {}

    def test_store_and_retrieve(self):
        """Test storing and retrieving secrets."""
        result = self.backend.store("TestService", "account1", "secret123")
        self.assertTrue(result)

        retrieved = self.backend.retrieve("TestService", "account1")
        self.assertEqual(retrieved, "secret123")

    def test_retrieve_nonexistent(self):
        """Test retrieving non-existent secret returns None."""
        retrieved = self.backend.retrieve("TestService", "nonexistent")
        self.assertIsNone(retrieved)

    def test_delete(self):
        """Test deleting secrets."""
        self.backend.store("TestService", "account1", "secret123")
        result = self.backend.delete("TestService", "account1")
        self.assertTrue(result)

        retrieved = self.backend.retrieve("TestService", "account1")
        self.assertIsNone(retrieved)

    def test_list_accounts_empty(self):
        """Test listing accounts when empty."""
        accounts = self.backend.list_accounts("TestService")
        self.assertEqual(accounts, [])


class TestKeychainManager(unittest.TestCase):
    """Tests for KeychainManager class."""

    def setUp(self):
        """Set up test manager with mock backend."""
        self.mock_backend = Mock(spec=KeychainBackend)
        self.mock_backend.retrieve.return_value = None
        self.mock_backend.store.return_value = True
        self.manager = KeychainManager(backend=self.mock_backend)

    def test_store_api_key(self):
        """Test storing an API key."""
        key_id = self.manager.store_api_key(
            exchange="binance",
            api_key="api123",
            api_secret="secret456",
            name="Test Key",
            testnet=True,
        )
        self.assertIsNotNone(key_id)
        self.mock_backend.store.assert_called()

    def test_store_api_key_with_passphrase(self):
        """Test storing an API key with passphrase (OKX)."""
        key_id = self.manager.store_api_key(
            exchange="okx",
            api_key="api123",
            api_secret="secret456",
            name="OKX Key",
            passphrase="pass789",
            testnet=True,
        )
        self.assertIsNotNone(key_id)

    def test_get_api_key_not_found(self):
        """Test getting non-existent key returns None."""
        key = self.manager.get_api_key("nonexistent-id")
        self.assertIsNone(key)

    def test_list_api_keys_empty(self):
        """Test listing keys when empty."""
        keys = self.manager.list_api_keys()
        self.assertEqual(keys, [])


class TestConfigPaths(unittest.TestCase):
    """Tests for ConfigPaths class."""

    def test_get_default_creates_paths(self):
        """Test that default paths are created correctly."""
        paths = ConfigPaths.get_default()
        self.assertIsInstance(paths.base_dir, Path)
        self.assertIsInstance(paths.config_file, Path)
        self.assertIsInstance(paths.backup_dir, Path)


class TestConfigManager(unittest.TestCase):
    """Tests for ConfigManager class."""

    def setUp(self):
        """Set up test config manager with temp directory."""
        self.temp_dir = tempfile.mkdtemp()
        self.paths = ConfigPaths(
            base_dir=Path(self.temp_dir),
            config_file=Path(self.temp_dir) / "config.json",
            backup_dir=Path(self.temp_dir) / "backups",
        )
        self.manager = ConfigManager(paths=self.paths)

    def tearDown(self):
        """Clean up temp directory."""
        shutil.rmtree(self.temp_dir)

    def test_load_returns_defaults_when_no_file(self):
        """Test loading returns defaults when no config file exists."""
        config = self.manager.load()
        self.assertIn("general", config)
        self.assertIn("trading", config)
        self.assertIn("risk", config)

    def test_save_and_load(self):
        """Test saving and loading configuration."""
        config = DEFAULT_CONFIG.copy()
        config["general"]["language"] = "es"
        self.manager.save(config)

        # Clear cache and reload
        self.manager._config_cache = None
        loaded = self.manager.load()
        self.assertEqual(loaded["general"]["language"], "es")

    def test_update_section(self):
        """Test updating a specific section."""
        self.manager.update("general", {"language": "fr"})
        config = self.manager.load()
        self.assertEqual(config["general"]["language"], "fr")

    def test_reset_to_defaults(self):
        """Test resetting configuration to defaults."""
        # Modify config (use deep copy to avoid modifying DEFAULT_CONFIG)
        import copy
        config = copy.deepcopy(DEFAULT_CONFIG)
        config["general"]["language"] = "de"
        self.manager.save(config)

        # Verify it was changed
        loaded = self.manager.load()
        self.assertEqual(loaded["general"]["language"], "de")

        # Reset
        self.manager._config_cache = None  # Clear cache
        reset_config = self.manager.reset()
        # After reset, should have default language
        self.assertIn("language", reset_config["general"])

    def test_export_config(self):
        """Test exporting configuration as JSON."""
        export_str = self.manager.export_config()
        export_data = json.loads(export_str)
        self.assertIn("general", export_data)

    def test_import_config(self):
        """Test importing configuration from JSON."""
        config = DEFAULT_CONFIG.copy()
        config["general"]["language"] = "ja"
        config_json = json.dumps(config)

        result = self.manager.import_config(config_json)
        self.assertTrue(result)

        loaded = self.manager.load()
        self.assertEqual(loaded["general"]["language"], "ja")

    def test_import_invalid_config(self):
        """Test importing invalid configuration fails."""
        result = self.manager.import_config("not valid json")
        self.assertFalse(result)

    def test_get_value_by_path(self):
        """Test getting value by dot-notation path."""
        value = self.manager.get_value("general.language")
        self.assertEqual(value, "en")

    def test_get_value_nonexistent(self):
        """Test getting non-existent value returns default."""
        value = self.manager.get_value("nonexistent.path", default="fallback")
        self.assertEqual(value, "fallback")

    def test_set_value_by_path(self):
        """Test setting value by dot-notation path."""
        self.manager.set_value("general.language", "pt")
        value = self.manager.get_value("general.language")
        self.assertEqual(value, "pt")

    def test_backup_created_on_save(self):
        """Test that backup is created when saving."""
        config = DEFAULT_CONFIG.copy()
        self.manager.save(config)
        self.manager.save(config)  # Save again to trigger backup

        backups = list(self.paths.backup_dir.glob("config_*.json"))
        self.assertGreater(len(backups), 0)


class TestConnectionTestResult(unittest.TestCase):
    """Tests for ConnectionTestResult dataclass."""

    def test_to_dict(self):
        """Test conversion to dictionary."""
        result = ConnectionTestResult(
            success=True,
            exchange="binance",
            permissions=["read", "trade"],
            balance_available=True,
            latency_ms=50.5,
            warnings=["High latency"],
        )
        data = result.to_dict()
        self.assertTrue(data["success"])
        self.assertEqual(data["exchange"], "binance")
        self.assertEqual(data["latencyMs"], 50.5)
        self.assertIn("High latency", data["warnings"])

    def test_to_dict_with_error(self):
        """Test conversion with error."""
        result = ConnectionTestResult(
            success=False,
            exchange="binance",
            permissions=[],
            balance_available=False,
            latency_ms=0,
            error="Invalid API key",
        )
        data = result.to_dict()
        self.assertFalse(data["success"])
        self.assertEqual(data["error"], "Invalid API key")


class TestExchangeTester(unittest.TestCase):
    """Tests for ExchangeTester class."""

    def test_supported_exchanges(self):
        """Test that all expected exchanges are supported."""
        tester = ExchangeTester()
        expected = ["binance", "binance_futures", "okx", "bybit", "coinbase"]
        for exchange in expected:
            self.assertIn(exchange, tester.adapters)

    def test_unsupported_exchange(self):
        """Test that unsupported exchange returns error."""
        import asyncio

        tester = ExchangeTester()
        loop = asyncio.new_event_loop()
        result = loop.run_until_complete(
            tester.test_connection("unsupported", "key", "secret")
        )
        loop.close()

        self.assertFalse(result.success)
        self.assertIn("Unsupported exchange", result.error)


class TestBinanceAdapter(unittest.TestCase):
    """Tests for BinanceAdapter class."""

    def test_get_base_url_mainnet(self):
        """Test mainnet URL selection."""
        adapter = BinanceAdapter()
        url = adapter._get_base_url(testnet=False)
        self.assertEqual(url, "https://api.binance.com")

    def test_get_base_url_testnet(self):
        """Test testnet URL selection."""
        adapter = BinanceAdapter()
        url = adapter._get_base_url(testnet=True)
        self.assertEqual(url, "https://testnet.binance.vision")

    def test_sign_request(self):
        """Test request signing."""
        adapter = BinanceAdapter()
        params = {"timestamp": 1234567890}
        signature = adapter._sign_request(params, "secret")
        self.assertIsInstance(signature, str)
        self.assertEqual(len(signature), 64)  # SHA256 hex digest


class TestGlobalInstances(unittest.TestCase):
    """Tests for global instance getters."""

    def test_get_keychain_manager_singleton(self):
        """Test keychain manager is singleton."""
        manager1 = get_keychain_manager()
        manager2 = get_keychain_manager()
        self.assertIs(manager1, manager2)

    def test_get_exchange_tester_singleton(self):
        """Test exchange tester is singleton."""
        tester1 = get_exchange_tester()
        tester2 = get_exchange_tester()
        self.assertIs(tester1, tester2)


if __name__ == "__main__":
    unittest.main()
