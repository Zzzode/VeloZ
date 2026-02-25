"""
Keychain integration for secure credential storage.

Provides platform-specific secure storage for API keys and secrets:
- Windows: Windows Credential Manager via pywin32
- macOS: Keychain via keyring library
- Linux: Secret Service via keyring library
"""

import json
import platform
import logging
from abc import ABC, abstractmethod
from typing import Optional
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
import uuid

logger = logging.getLogger("veloz.keychain")

SERVICE_NAME = "VeloZ"


@dataclass
class StoredAPIKey:
    """API key data stored in keychain."""

    id: str
    exchange: str
    name: str
    api_key: str
    api_secret: str
    passphrase: Optional[str]
    testnet: bool
    permissions: list
    created_at: str
    last_used: Optional[str]
    status: str  # 'active', 'expired', 'revoked'

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> "StoredAPIKey":
        return cls(**data)

    def to_public_info(self) -> dict:
        """Return public info without sensitive data."""
        return {
            "id": self.id,
            "exchange": self.exchange,
            "name": self.name,
            "permissions": self.permissions,
            "createdAt": self.created_at,
            "lastUsed": self.last_used,
            "status": self.status,
            "testnet": self.testnet,
        }


class KeychainBackend(ABC):
    """Abstract base class for keychain backends."""

    @abstractmethod
    def store(self, service: str, account: str, secret: str) -> bool:
        """Store a secret in the keychain."""
        pass

    @abstractmethod
    def retrieve(self, service: str, account: str) -> Optional[str]:
        """Retrieve a secret from the keychain."""
        pass

    @abstractmethod
    def delete(self, service: str, account: str) -> bool:
        """Delete a secret from the keychain."""
        pass

    @abstractmethod
    def list_accounts(self, service: str) -> list:
        """List all accounts for a service."""
        pass


class KeyringBackend(KeychainBackend):
    """Keychain backend using keyring library (works on all platforms)."""

    def __init__(self):
        try:
            import keyring

            self.keyring = keyring
            self._available = True
        except ImportError:
            logger.warning("keyring library not available, using fallback storage")
            self._available = False
            self._fallback_storage = {}

    def store(self, service: str, account: str, secret: str) -> bool:
        try:
            if self._available:
                self.keyring.set_password(service, account, secret)
            else:
                self._fallback_storage[f"{service}/{account}"] = secret
            return True
        except Exception as e:
            logger.error(f"Failed to store secret: {e}")
            return False

    def retrieve(self, service: str, account: str) -> Optional[str]:
        try:
            if self._available:
                return self.keyring.get_password(service, account)
            else:
                return self._fallback_storage.get(f"{service}/{account}")
        except Exception as e:
            logger.error(f"Failed to retrieve secret: {e}")
            return None

    def delete(self, service: str, account: str) -> bool:
        try:
            if self._available:
                self.keyring.delete_password(service, account)
            else:
                key = f"{service}/{account}"
                if key in self._fallback_storage:
                    del self._fallback_storage[key]
            return True
        except Exception as e:
            logger.error(f"Failed to delete secret: {e}")
            return False

    def list_accounts(self, service: str) -> list:
        """List accounts - requires reading the index."""
        # Keyring doesn't have a native list function, so we maintain an index
        index_data = self.retrieve(service, "_index")
        if index_data:
            try:
                return json.loads(index_data)
            except json.JSONDecodeError:
                return []
        return []

    def _update_index(self, service: str, accounts: list) -> bool:
        """Update the account index."""
        return self.store(service, "_index", json.dumps(accounts))


class WindowsKeychain(KeychainBackend):
    """Windows Credential Manager backend."""

    def __init__(self):
        try:
            import win32cred

            self.win32cred = win32cred
            self._available = True
        except ImportError:
            logger.warning("pywin32 not available, falling back to keyring")
            self._fallback = KeyringBackend()
            self._available = False

    def store(self, service: str, account: str, secret: str) -> bool:
        if not self._available:
            return self._fallback.store(service, account, secret)

        try:
            credential = {
                "Type": self.win32cred.CRED_TYPE_GENERIC,
                "TargetName": f"{service}/{account}",
                "CredentialBlob": secret.encode("utf-16-le"),
                "Persist": self.win32cred.CRED_PERSIST_LOCAL_MACHINE,
                "UserName": account,
            }
            self.win32cred.CredWrite(credential, 0)
            return True
        except Exception as e:
            logger.error(f"Failed to store credential: {e}")
            return False

    def retrieve(self, service: str, account: str) -> Optional[str]:
        if not self._available:
            return self._fallback.retrieve(service, account)

        try:
            cred = self.win32cred.CredRead(
                f"{service}/{account}", self.win32cred.CRED_TYPE_GENERIC
            )
            return cred["CredentialBlob"].decode("utf-16-le")
        except Exception:
            return None

    def delete(self, service: str, account: str) -> bool:
        if not self._available:
            return self._fallback.delete(service, account)

        try:
            self.win32cred.CredDelete(
                f"{service}/{account}", self.win32cred.CRED_TYPE_GENERIC
            )
            return True
        except Exception as e:
            logger.error(f"Failed to delete credential: {e}")
            return False

    def list_accounts(self, service: str) -> list:
        if not self._available:
            return self._fallback.list_accounts(service)

        try:
            creds = self.win32cred.CredEnumerate(f"{service}/*", 0)
            return [
                cred["UserName"]
                for cred in creds
                if cred["UserName"] != "_index"
            ]
        except Exception:
            return []


def get_keychain() -> KeychainBackend:
    """Get the appropriate keychain backend for the current platform."""
    system = platform.system()
    if system == "Windows":
        return WindowsKeychain()
    else:
        # macOS and Linux both use keyring
        return KeyringBackend()


class KeychainManager:
    """High-level API key management using the keychain."""

    def __init__(self, backend: Optional[KeychainBackend] = None):
        self.backend = backend or get_keychain()
        self.service = SERVICE_NAME

    def store_api_key(
        self,
        exchange: str,
        api_key: str,
        api_secret: str,
        name: str = "Default",
        passphrase: Optional[str] = None,
        testnet: bool = True,
        permissions: Optional[list] = None,
    ) -> Optional[str]:
        """Store an API key and return its ID."""
        key_id = str(uuid.uuid4())

        stored_key = StoredAPIKey(
            id=key_id,
            exchange=exchange,
            name=name,
            api_key=api_key,
            api_secret=api_secret,
            passphrase=passphrase,
            testnet=testnet,
            permissions=permissions or ["read", "trade"],
            created_at=datetime.now(timezone.utc).isoformat(),
            last_used=None,
            status="active",
        )

        # Store the key data
        if not self.backend.store(
            self.service, key_id, json.dumps(stored_key.to_dict())
        ):
            return None

        # Update the index
        accounts = self._get_index()
        accounts.append(key_id)
        self._update_index(accounts)

        logger.info(f"Stored API key {key_id} for {exchange}")
        return key_id

    def get_api_key(self, key_id: str) -> Optional[StoredAPIKey]:
        """Retrieve an API key by ID."""
        data = self.backend.retrieve(self.service, key_id)
        if not data:
            return None

        try:
            return StoredAPIKey.from_dict(json.loads(data))
        except (json.JSONDecodeError, TypeError) as e:
            logger.error(f"Failed to parse API key data: {e}")
            return None

    def delete_api_key(self, key_id: str) -> bool:
        """Delete an API key by ID."""
        if not self.backend.delete(self.service, key_id):
            return False

        # Update the index
        accounts = self._get_index()
        if key_id in accounts:
            accounts.remove(key_id)
            self._update_index(accounts)

        logger.info(f"Deleted API key {key_id}")
        return True

    def list_api_keys(self) -> list:
        """List all stored API keys (public info only)."""
        accounts = self._get_index()
        keys = []

        for key_id in accounts:
            key = self.get_api_key(key_id)
            if key:
                keys.append(key.to_public_info())

        return keys

    def update_last_used(self, key_id: str) -> bool:
        """Update the last used timestamp for a key."""
        key = self.get_api_key(key_id)
        if not key:
            return False

        key.last_used = datetime.now(timezone.utc).isoformat()
        return self.backend.store(
            self.service, key_id, json.dumps(key.to_dict())
        )

    def _get_index(self) -> list:
        """Get the list of stored key IDs."""
        data = self.backend.retrieve(self.service, "_index")
        if data:
            try:
                return json.loads(data)
            except json.JSONDecodeError:
                return []
        return []

    def _update_index(self, accounts: list) -> bool:
        """Update the key index."""
        return self.backend.store(self.service, "_index", json.dumps(accounts))


# Global instance
_keychain_manager: Optional[KeychainManager] = None


def get_keychain_manager() -> KeychainManager:
    """Get the global keychain manager instance."""
    global _keychain_manager
    if _keychain_manager is None:
        _keychain_manager = KeychainManager()
    return _keychain_manager
