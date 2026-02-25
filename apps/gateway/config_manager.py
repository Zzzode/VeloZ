"""
Configuration management for VeloZ.

Handles loading, saving, validation, and backup of configuration files.
"""

import json
import os
import time
import logging
from pathlib import Path
from typing import Any, Dict, Optional
from dataclasses import dataclass

logger = logging.getLogger("veloz.config")


@dataclass
class ConfigPaths:
    """Configuration file paths."""

    base_dir: Path
    config_file: Path
    backup_dir: Path

    @classmethod
    def get_default(cls) -> "ConfigPaths":
        """Get default paths based on platform."""
        if os.name == "nt":  # Windows
            base = Path(os.environ.get("APPDATA", "")) / "VeloZ"
        elif os.name == "darwin":  # macOS
            base = Path.home() / "Library" / "Application Support" / "VeloZ"
        else:  # Linux
            base = Path.home() / ".config" / "veloz"

        return cls(
            base_dir=base,
            config_file=base / "config.json",
            backup_dir=base / "backups",
        )


# Default configuration
DEFAULT_CONFIG: Dict[str, Any] = {
    "general": {
        "language": "en",
        "theme": "system",
        "timezone": "UTC",
        "dateFormat": "ISO",
        "notifications": {
            "orderFills": True,
            "priceAlerts": True,
            "systemAlerts": True,
            "sound": True,
        },
    },
    "trading": {
        "defaultSymbol": "BTCUSDT",
        "orderConfirmation": True,
        "doubleClickToTrade": False,
        "defaultOrderType": "limit",
        "defaultTimeInForce": "GTC",
        "slippageTolerance": 0.005,
    },
    "risk": {
        "maxPositionSize": 0.1,
        "maxPositionValue": 10000,
        "dailyLossLimit": 0.05,
        "maxOpenOrders": 10,
        "circuitBreaker": {
            "enabled": True,
            "threshold": 0.1,
            "cooldownMinutes": 60,
        },
        "requireConfirmation": {
            "largeOrders": True,
            "largeOrderThreshold": 1000,
            "marketOrders": True,
        },
    },
    "advanced": {
        "apiRateLimit": 10,
        "websocketReconnectDelay": 5000,
        "orderBookDepth": 20,
        "logLevel": "info",
        "telemetry": True,
    },
    "security": {
        "twoFactorEnabled": False,
        "recoveryEmail": "",
        "sessionTimeout": 30,
        "requirePasswordForTrades": False,
    },
}


class ConfigManager:
    """Manages application configuration."""

    def __init__(self, paths: Optional[ConfigPaths] = None):
        self.paths = paths or ConfigPaths.get_default()
        self._ensure_directories()
        self._config_cache: Optional[Dict[str, Any]] = None

    def _ensure_directories(self):
        """Create necessary directories if they don't exist."""
        self.paths.base_dir.mkdir(parents=True, exist_ok=True)
        self.paths.backup_dir.mkdir(parents=True, exist_ok=True)

    def load(self) -> Dict[str, Any]:
        """Load configuration from file."""
        if self._config_cache is not None:
            return self._config_cache

        if not self.paths.config_file.exists():
            logger.info("No config file found, using defaults")
            self._config_cache = DEFAULT_CONFIG.copy()
            return self._config_cache

        try:
            content = self.paths.config_file.read_text()
            config = json.loads(content)
            # Merge with defaults to ensure all keys exist
            self._config_cache = self._merge_with_defaults(config)
            return self._config_cache
        except (json.JSONDecodeError, IOError) as e:
            logger.error(f"Failed to load config: {e}")
            # Try to restore from backup
            restored = self._restore_from_backup()
            if restored:
                self._config_cache = restored
                return self._config_cache
            # Fall back to defaults
            self._config_cache = DEFAULT_CONFIG.copy()
            return self._config_cache

    def save(self, config: Dict[str, Any]) -> bool:
        """Save configuration to file."""
        # Validate config
        if not self._validate(config):
            raise ValueError("Invalid configuration")

        # Create backup before saving
        self._create_backup()

        try:
            content = json.dumps(config, indent=2)
            self.paths.config_file.write_text(content)
            self._config_cache = config
            logger.info("Configuration saved successfully")
            return True
        except IOError as e:
            logger.error(f"Failed to save config: {e}")
            raise RuntimeError(f"Failed to save config: {e}")

    def update(self, section: str, data: Dict[str, Any]) -> Dict[str, Any]:
        """Update a section of the configuration."""
        config = self.load()
        if section in config:
            config[section] = {**config[section], **data}
        else:
            config[section] = data

        self.save(config)
        return config

    def reset(self) -> Dict[str, Any]:
        """Reset configuration to defaults."""
        self._create_backup()
        self._config_cache = DEFAULT_CONFIG.copy()
        self.save(self._config_cache)
        return self._config_cache

    def _create_backup(self):
        """Create a backup of the current config file."""
        if not self.paths.config_file.exists():
            return

        try:
            timestamp = int(time.time())
            backup_file = self.paths.backup_dir / f"config_{timestamp}.json"
            backup_file.write_text(self.paths.config_file.read_text())

            # Keep only last 10 backups
            backups = sorted(self.paths.backup_dir.glob("config_*.json"))
            for old_backup in backups[:-10]:
                old_backup.unlink()

            logger.debug(f"Created backup: {backup_file}")
        except IOError as e:
            logger.warning(f"Failed to create backup: {e}")

    def _restore_from_backup(self) -> Optional[Dict[str, Any]]:
        """Restore configuration from the most recent backup."""
        backups = sorted(self.paths.backup_dir.glob("config_*.json"))
        if not backups:
            return None

        try:
            content = backups[-1].read_text()
            config = json.loads(content)
            logger.info(f"Restored config from backup: {backups[-1]}")
            return self._merge_with_defaults(config)
        except (json.JSONDecodeError, IOError) as e:
            logger.error(f"Failed to restore from backup: {e}")
            return None

    def _validate(self, config: Dict[str, Any]) -> bool:
        """Validate configuration structure."""
        required_sections = ["general", "trading", "risk", "advanced"]
        return all(section in config for section in required_sections)

    def _merge_with_defaults(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Merge config with defaults to ensure all keys exist."""
        result = DEFAULT_CONFIG.copy()
        for key, value in config.items():
            if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                result[key] = {**result[key], **value}
            else:
                result[key] = value
        return result

    def export_config(self, include_sensitive: bool = False) -> str:
        """Export configuration as JSON string."""
        config = self.load()
        if not include_sensitive:
            # Remove any sensitive fields (API keys are stored separately)
            config = config.copy()
            config.pop("apiKeys", None)
        return json.dumps(config, indent=2)

    def import_config(self, config_json: str) -> bool:
        """Import configuration from JSON string."""
        try:
            config = json.loads(config_json)
            if self._validate(config):
                self.save(config)
                return True
            return False
        except json.JSONDecodeError:
            return False

    def get_value(self, path: str, default: Any = None) -> Any:
        """Get a configuration value by dot-notation path."""
        config = self.load()
        keys = path.split(".")
        value = config

        for key in keys:
            if isinstance(value, dict) and key in value:
                value = value[key]
            else:
                return default

        return value

    def set_value(self, path: str, value: Any) -> bool:
        """Set a configuration value by dot-notation path."""
        config = self.load()
        keys = path.split(".")
        target = config

        for key in keys[:-1]:
            if key not in target:
                target[key] = {}
            target = target[key]

        target[keys[-1]] = value
        return self.save(config)


# Global instance
_config_manager: Optional[ConfigManager] = None


def get_config_manager() -> ConfigManager:
    """Get the global configuration manager instance."""
    global _config_manager
    if _config_manager is None:
        _config_manager = ConfigManager()
    return _config_manager
