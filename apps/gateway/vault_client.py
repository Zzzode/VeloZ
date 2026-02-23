"""
VeloZ Vault Client
Provides secure secrets management using HashiCorp Vault.

This module implements:
- AppRole authentication for service-to-service auth
- Token-based authentication for development
- Automatic token renewal
- Caching with TTL for performance
- Fallback to environment variables when Vault is unavailable
"""

import json
import logging
import os
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

logger = logging.getLogger("veloz.vault")


@dataclass
class VaultConfig:
    """Configuration for Vault client."""

    # Vault server address
    addr: str = "http://127.0.0.1:8200"

    # Authentication method: "token", "approle"
    auth_method: str = "token"

    # Token for token-based auth
    token: str = ""

    # AppRole credentials
    role_id: str = ""
    secret_id: str = ""

    # Namespace (for Vault Enterprise)
    namespace: str = ""

    # TLS verification
    verify_tls: bool = True

    # Cache TTL in seconds (0 to disable)
    cache_ttl: int = 300

    # Token renewal threshold (renew when TTL < this value in seconds)
    renewal_threshold: int = 600

    # Request timeout in seconds
    timeout: int = 30

    # Retry configuration
    max_retries: int = 3
    retry_delay: float = 1.0

    # Fallback to environment variables if Vault unavailable
    fallback_to_env: bool = True


@dataclass
class CachedSecret:
    """Cached secret with expiration."""

    data: dict
    expires_at: float


class VaultClient:
    """
    HashiCorp Vault client for VeloZ.

    Supports AppRole and Token authentication methods.
    Implements automatic token renewal and secret caching.
    """

    def __init__(self, config: Optional[VaultConfig] = None):
        """Initialize Vault client with configuration."""
        self._config = config or self._config_from_env()
        self._token: Optional[str] = None
        self._token_expires_at: float = 0
        self._cache: dict[str, CachedSecret] = {}
        self._cache_lock = threading.Lock()
        self._renewal_thread: Optional[threading.Thread] = None
        self._shutdown = threading.Event()
        self._connected = False

        # Initialize token if provided directly
        if self._config.auth_method == "token" and self._config.token:
            self._token = self._config.token
            self._token_expires_at = time.time() + 3600  # Assume 1 hour

    @staticmethod
    def _config_from_env() -> VaultConfig:
        """Create configuration from environment variables."""
        return VaultConfig(
            addr=os.environ.get("VAULT_ADDR", "http://127.0.0.1:8200"),
            auth_method=os.environ.get("VAULT_AUTH_METHOD", "token"),
            token=os.environ.get("VAULT_TOKEN", ""),
            role_id=os.environ.get("VAULT_ROLE_ID", ""),
            secret_id=os.environ.get("VAULT_SECRET_ID", ""),
            namespace=os.environ.get("VAULT_NAMESPACE", ""),
            verify_tls=os.environ.get("VAULT_SKIP_VERIFY", "").lower() not in (
                "true",
                "1",
                "yes",
            ),
            cache_ttl=int(os.environ.get("VAULT_CACHE_TTL", "300")),
            fallback_to_env=os.environ.get("VAULT_FALLBACK_TO_ENV", "true").lower()
            in ("true", "1", "yes"),
        )

    def connect(self) -> bool:
        """
        Connect to Vault and authenticate.

        Returns True if connection successful, False otherwise.
        """
        try:
            # Check Vault health
            if not self._check_health():
                logger.warning("Vault health check failed")
                return False

            # Authenticate based on method
            if self._config.auth_method == "approle":
                if not self._authenticate_approle():
                    logger.error("AppRole authentication failed")
                    return False
            elif self._config.auth_method == "token":
                if not self._config.token:
                    logger.error("Token authentication requires VAULT_TOKEN")
                    return False
                self._token = self._config.token
                # Verify token is valid
                if not self._lookup_token():
                    logger.error("Token validation failed")
                    return False

            self._connected = True

            # Start token renewal thread
            self._start_renewal_thread()

            logger.info("Connected to Vault at %s", self._config.addr)
            return True

        except Exception as e:
            logger.error("Failed to connect to Vault: %s", e)
            return False

    def disconnect(self) -> None:
        """Disconnect from Vault and cleanup."""
        self._shutdown.set()
        if self._renewal_thread and self._renewal_thread.is_alive():
            self._renewal_thread.join(timeout=5)
        self._connected = False
        self._token = None
        with self._cache_lock:
            self._cache.clear()
        logger.info("Disconnected from Vault")

    def is_connected(self) -> bool:
        """Check if connected to Vault."""
        return self._connected and self._token is not None

    def get_secret(
        self, path: str, key: Optional[str] = None, default: Any = None
    ) -> Any:
        """
        Get a secret from Vault.

        Args:
            path: Secret path (e.g., "veloz/binance/credentials")
            key: Optional specific key within the secret
            default: Default value if secret not found

        Returns:
            Secret value or default if not found
        """
        # Check cache first
        cached = self._get_cached(path)
        if cached is not None:
            if key:
                return cached.get(key, default)
            return cached

        # Try Vault
        if self.is_connected():
            try:
                data = self._read_secret(path)
                if data is not None:
                    self._set_cached(path, data)
                    if key:
                        return data.get(key, default)
                    return data
            except Exception as e:
                logger.warning("Failed to read secret %s from Vault: %s", path, e)

        # Fallback to environment variables
        if self._config.fallback_to_env:
            return self._get_from_env(path, key, default)

        return default

    def get_binance_credentials(self) -> tuple[str, str]:
        """
        Get Binance API credentials.

        Returns:
            Tuple of (api_key, api_secret)
        """
        secret = self.get_secret("veloz/binance/credentials", default={})
        api_key = secret.get("api_key", "")
        api_secret = secret.get("api_secret", "")

        # Fallback to environment variables
        if not api_key:
            api_key = os.environ.get("VELOZ_BINANCE_API_KEY", "")
        if not api_secret:
            api_secret = os.environ.get("VELOZ_BINANCE_API_SECRET", "")

        return api_key, api_secret

    def get_jwt_secret(self) -> str:
        """Get JWT signing secret."""
        secret = self.get_secret("veloz/gateway/auth", key="jwt_secret", default="")
        if not secret:
            secret = os.environ.get("VELOZ_JWT_SECRET", "")
        return secret

    def get_admin_password(self) -> str:
        """Get admin password."""
        secret = self.get_secret("veloz/gateway/auth", key="admin_password", default="")
        if not secret:
            secret = os.environ.get("VELOZ_ADMIN_PASSWORD", "")
        return secret

    def get_database_credentials(self) -> dict:
        """Get database credentials."""
        return self.get_secret(
            "veloz/database/postgres",
            default={
                "host": os.environ.get("POSTGRES_HOST", "localhost"),
                "port": os.environ.get("POSTGRES_PORT", "5432"),
                "database": os.environ.get("POSTGRES_DB", "veloz"),
                "username": os.environ.get("POSTGRES_USER", "veloz"),
                "password": os.environ.get("POSTGRES_PASSWORD", ""),
            },
        )

    # =========================================================================
    # Private Methods
    # =========================================================================

    def _request(
        self,
        method: str,
        path: str,
        data: Optional[dict] = None,
        authenticated: bool = True,
    ) -> Optional[dict]:
        """Make HTTP request to Vault."""
        url = f"{self._config.addr}/v1/{path}"

        headers = {"Content-Type": "application/json"}

        if authenticated and self._token:
            headers["X-Vault-Token"] = self._token

        if self._config.namespace:
            headers["X-Vault-Namespace"] = self._config.namespace

        body = json.dumps(data).encode("utf-8") if data else None

        req = urllib.request.Request(url, data=body, headers=headers, method=method)

        for attempt in range(self._config.max_retries):
            try:
                with urllib.request.urlopen(
                    req, timeout=self._config.timeout
                ) as response:
                    if response.status == 204:
                        return {}
                    return json.loads(response.read().decode("utf-8"))
            except urllib.error.HTTPError as e:
                if e.code == 404:
                    return None
                if e.code >= 500 and attempt < self._config.max_retries - 1:
                    time.sleep(self._config.retry_delay * (attempt + 1))
                    continue
                raise
            except urllib.error.URLError as e:
                if attempt < self._config.max_retries - 1:
                    time.sleep(self._config.retry_delay * (attempt + 1))
                    continue
                raise

        return None

    def _check_health(self) -> bool:
        """Check Vault health status."""
        try:
            result = self._request("GET", "sys/health", authenticated=False)
            return result is not None
        except Exception:
            return False

    def _authenticate_approle(self) -> bool:
        """Authenticate using AppRole."""
        try:
            result = self._request(
                "POST",
                "auth/approle/login",
                data={
                    "role_id": self._config.role_id,
                    "secret_id": self._config.secret_id,
                },
                authenticated=False,
            )

            if result and "auth" in result:
                self._token = result["auth"]["client_token"]
                lease_duration = result["auth"].get("lease_duration", 3600)
                self._token_expires_at = time.time() + lease_duration
                logger.info("AppRole authentication successful")
                return True

            return False
        except Exception as e:
            logger.error("AppRole authentication error: %s", e)
            return False

    def _lookup_token(self) -> bool:
        """Lookup and validate current token."""
        try:
            result = self._request("GET", "auth/token/lookup-self")
            if result and "data" in result:
                ttl = result["data"].get("ttl", 0)
                self._token_expires_at = time.time() + ttl
                return True
            return False
        except Exception:
            return False

    def _renew_token(self) -> bool:
        """Renew current token."""
        try:
            result = self._request("POST", "auth/token/renew-self")
            if result and "auth" in result:
                lease_duration = result["auth"].get("lease_duration", 3600)
                self._token_expires_at = time.time() + lease_duration
                logger.debug("Token renewed, new TTL: %d seconds", lease_duration)
                return True
            return False
        except Exception as e:
            logger.warning("Token renewal failed: %s", e)
            return False

    def _read_secret(self, path: str) -> Optional[dict]:
        """Read secret from KV v2 engine."""
        # KV v2 uses data/ prefix
        kv_path = f"secret/data/{path}"
        result = self._request("GET", kv_path)
        if result and "data" in result and "data" in result["data"]:
            return result["data"]["data"]
        return None

    def _get_cached(self, path: str) -> Optional[dict]:
        """Get secret from cache if not expired."""
        if self._config.cache_ttl <= 0:
            return None

        with self._cache_lock:
            cached = self._cache.get(path)
            if cached and cached.expires_at > time.time():
                return cached.data
            elif cached:
                del self._cache[path]

        return None

    def _set_cached(self, path: str, data: dict) -> None:
        """Cache secret with TTL."""
        if self._config.cache_ttl <= 0:
            return

        with self._cache_lock:
            self._cache[path] = CachedSecret(
                data=data, expires_at=time.time() + self._config.cache_ttl
            )

    def _get_from_env(self, path: str, key: Optional[str], default: Any) -> Any:
        """
        Fallback to environment variables.

        Maps Vault paths to environment variable names:
        - veloz/binance/credentials -> VELOZ_BINANCE_*
        - veloz/gateway/auth -> VELOZ_*
        """
        env_mapping = {
            ("veloz/binance/credentials", "api_key"): "VELOZ_BINANCE_API_KEY",
            ("veloz/binance/credentials", "api_secret"): "VELOZ_BINANCE_API_SECRET",
            ("veloz/gateway/auth", "jwt_secret"): "VELOZ_JWT_SECRET",
            ("veloz/gateway/auth", "admin_password"): "VELOZ_ADMIN_PASSWORD",
            ("veloz/database/postgres", "password"): "POSTGRES_PASSWORD",
        }

        if key:
            env_var = env_mapping.get((path, key))
            if env_var:
                return os.environ.get(env_var, default)
        else:
            # Return dict of all matching env vars for path
            result = {}
            for (p, k), env_var in env_mapping.items():
                if p == path:
                    value = os.environ.get(env_var, "")
                    if value:
                        result[k] = value
            if result:
                return result

        return default

    def _start_renewal_thread(self) -> None:
        """Start background token renewal thread."""
        if self._renewal_thread and self._renewal_thread.is_alive():
            return

        self._shutdown.clear()
        self._renewal_thread = threading.Thread(
            target=self._renewal_loop, daemon=True, name="vault-token-renewal"
        )
        self._renewal_thread.start()

    def _renewal_loop(self) -> None:
        """Background loop for token renewal."""
        while not self._shutdown.is_set():
            try:
                # Check if token needs renewal
                time_to_expiry = self._token_expires_at - time.time()
                if time_to_expiry < self._config.renewal_threshold:
                    logger.debug(
                        "Token expires in %d seconds, renewing...", int(time_to_expiry)
                    )
                    if not self._renew_token():
                        # If renewal fails, try re-authentication
                        if self._config.auth_method == "approle":
                            self._authenticate_approle()
            except Exception as e:
                logger.error("Error in token renewal loop: %s", e)

            # Sleep for a bit before next check
            self._shutdown.wait(60)


# =============================================================================
# Module-level singleton
# =============================================================================

_vault_client: Optional[VaultClient] = None
_vault_lock = threading.Lock()


def get_vault_client() -> VaultClient:
    """Get or create the global Vault client instance."""
    global _vault_client
    with _vault_lock:
        if _vault_client is None:
            _vault_client = VaultClient()
        return _vault_client


def init_vault(config: Optional[VaultConfig] = None) -> bool:
    """
    Initialize the global Vault client.

    Args:
        config: Optional configuration (uses environment if not provided)

    Returns:
        True if Vault connection successful, False otherwise
    """
    global _vault_client
    with _vault_lock:
        _vault_client = VaultClient(config)
        return _vault_client.connect()


def shutdown_vault() -> None:
    """Shutdown the global Vault client."""
    global _vault_client
    with _vault_lock:
        if _vault_client:
            _vault_client.disconnect()
            _vault_client = None
