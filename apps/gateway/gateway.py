#!/usr/bin/env python3
import json
import hmac
import hashlib
import base64
import os
import socket
import ssl
import subprocess
import struct
import threading
import time
import urllib.error
import urllib.request
import secrets
import logging
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse, urlencode
from collections import deque
from dataclasses import dataclass, asdict, field
from typing import Optional
from datetime import datetime

# Import RBAC module
from rbac import role_manager, RoleManager

# Import audit module
from audit import (
    AuditLogStore,
    AuditLogRetentionManager,
    AuditLogEntry,
    create_audit_entry,
    get_audit_store,
    get_retention_manager,
    init_audit_system,
    DEFAULT_POLICIES,
)

# Import metrics module
from metrics import (
    get_metrics_manager,
    init_metrics,
    RequestTimer,
    metrics_available,
)

# Import configuration modules
from keychain import get_keychain_manager, KeychainManager
from config_manager import get_config_manager, ConfigManager
from exchange_tester import get_exchange_tester, test_exchange_connection

# Configure audit logger
audit_logger = logging.getLogger("veloz.audit")
audit_logger.setLevel(logging.INFO)
_audit_handler = logging.StreamHandler()
_audit_handler.setFormatter(
    logging.Formatter("%(asctime)s [AUDIT] %(message)s", datefmt="%Y-%m-%d %H:%M:%S")
)
audit_logger.addHandler(_audit_handler)


# =============================================================================
# JWT Implementation (simplified, using HMAC-SHA256)
# =============================================================================


class JWTManager:
    """Simple JWT implementation using HMAC-SHA256."""

    # Refresh token expiry: 7 days
    REFRESH_TOKEN_EXPIRY = 7 * 24 * 3600
    # Default cleanup interval: 1 hour
    DEFAULT_CLEANUP_INTERVAL = 3600

    def __init__(self, secret_key: str, token_expiry_seconds: int = 3600, cleanup_interval: int = DEFAULT_CLEANUP_INTERVAL):
        self._secret = secret_key.encode("utf-8") if secret_key else secrets.token_bytes(32)
        self._expiry = token_expiry_seconds
        self._refresh_secret = hashlib.sha256(self._secret + b"_refresh").digest()
        self._mu = threading.Lock()
        # Store revoked refresh tokens (token_id -> revocation_time)
        self._revoked_refresh_tokens: dict[str, int] = {}
        # Cleanup configuration
        self._cleanup_interval = cleanup_interval
        self._cleanup_stop_event = threading.Event()
        self._cleanup_thread: Optional[threading.Thread] = None
        self._last_cleanup_time: int = 0
        self._cleanup_count: int = 0  # Total tokens cleaned up
        # Start background cleanup thread
        self._start_cleanup_thread()

    def _b64url_encode(self, data: bytes) -> str:
        return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")

    def _b64url_decode(self, data: str) -> bytes:
        padding = 4 - len(data) % 4
        if padding != 4:
            data += "=" * padding
        return base64.urlsafe_b64decode(data)

    def create_token(self, user_id: str, api_key_id: Optional[str] = None) -> str:
        """Create a JWT access token for the given user."""
        header = {"alg": "HS256", "typ": "JWT"}
        now = int(time.time())
        payload = {
            "sub": user_id,
            "iat": now,
            "exp": now + self._expiry,
            "type": "access",
        }
        if api_key_id:
            payload["kid"] = api_key_id

        header_b64 = self._b64url_encode(json.dumps(header).encode("utf-8"))
        payload_b64 = self._b64url_encode(json.dumps(payload).encode("utf-8"))
        message = f"{header_b64}.{payload_b64}"
        signature = hmac.new(self._secret, message.encode("utf-8"), hashlib.sha256).digest()
        sig_b64 = self._b64url_encode(signature)
        return f"{message}.{sig_b64}"

    def create_refresh_token(self, user_id: str) -> str:
        """Create a JWT refresh token for the given user."""
        header = {"alg": "HS256", "typ": "JWT"}
        now = int(time.time())
        token_id = secrets.token_hex(16)
        payload = {
            "sub": user_id,
            "iat": now,
            "exp": now + self.REFRESH_TOKEN_EXPIRY,
            "type": "refresh",
            "jti": token_id,  # JWT ID for revocation tracking
        }

        header_b64 = self._b64url_encode(json.dumps(header).encode("utf-8"))
        payload_b64 = self._b64url_encode(json.dumps(payload).encode("utf-8"))
        message = f"{header_b64}.{payload_b64}"
        signature = hmac.new(self._refresh_secret, message.encode("utf-8"), hashlib.sha256).digest()
        sig_b64 = self._b64url_encode(signature)
        return f"{message}.{sig_b64}"

    def verify_token(self, token: str) -> Optional[dict]:
        """Verify a JWT access token and return the payload if valid."""
        try:
            parts = token.split(".")
            if len(parts) != 3:
                return None

            header_b64, payload_b64, sig_b64 = parts
            message = f"{header_b64}.{payload_b64}"
            expected_sig = hmac.new(self._secret, message.encode("utf-8"), hashlib.sha256).digest()
            actual_sig = self._b64url_decode(sig_b64)

            if not hmac.compare_digest(expected_sig, actual_sig):
                return None

            payload = json.loads(self._b64url_decode(payload_b64))

            # Check token type (must be access token or old token without type)
            token_type = payload.get("type")
            if token_type and token_type != "access":
                return None

            if payload.get("exp", 0) < int(time.time()):
                return None

            return payload
        except Exception:
            return None

    def verify_refresh_token(self, token: str) -> Optional[dict]:
        """Verify a JWT refresh token and return the payload if valid."""
        try:
            parts = token.split(".")
            if len(parts) != 3:
                return None

            header_b64, payload_b64, sig_b64 = parts
            message = f"{header_b64}.{payload_b64}"
            expected_sig = hmac.new(self._refresh_secret, message.encode("utf-8"), hashlib.sha256).digest()
            actual_sig = self._b64url_decode(sig_b64)

            if not hmac.compare_digest(expected_sig, actual_sig):
                return None

            payload = json.loads(self._b64url_decode(payload_b64))

            # Must be a refresh token
            if payload.get("type") != "refresh":
                return None

            if payload.get("exp", 0) < int(time.time()):
                return None

            # Check if token has been revoked
            jti = payload.get("jti")
            if jti:
                with self._mu:
                    if jti in self._revoked_refresh_tokens:
                        return None

            return payload
        except Exception:
            return None

    def revoke_refresh_token(self, token: str) -> bool:
        """Revoke a refresh token by its JTI."""
        try:
            parts = token.split(".")
            if len(parts) != 3:
                return False

            payload_b64 = parts[1]
            payload = json.loads(self._b64url_decode(payload_b64))
            jti = payload.get("jti")
            if not jti:
                return False

            with self._mu:
                self._revoked_refresh_tokens[jti] = int(time.time())
            return True
        except Exception:
            return False

    def _start_cleanup_thread(self) -> None:
        """Start the background cleanup thread."""
        self._cleanup_thread = threading.Thread(
            target=self._cleanup_loop,
            name="jwt-cleanup",
            daemon=True,
        )
        self._cleanup_thread.start()

    def _cleanup_loop(self) -> None:
        """Background loop that periodically cleans up expired revoked tokens."""
        while not self._cleanup_stop_event.wait(timeout=self._cleanup_interval):
            self._cleanup_expired_tokens()

    def _cleanup_expired_tokens(self) -> int:
        """Remove expired tokens from the revocation list. Returns count of removed tokens."""
        now = int(time.time())
        cutoff = now - self.REFRESH_TOKEN_EXPIRY
        removed_count = 0

        with self._mu:
            before_count = len(self._revoked_refresh_tokens)
            self._revoked_refresh_tokens = {
                k: v for k, v in self._revoked_refresh_tokens.items() if v > cutoff
            }
            removed_count = before_count - len(self._revoked_refresh_tokens)
            self._cleanup_count += removed_count
            self._last_cleanup_time = now

        if removed_count > 0:
            audit_logger.info(f"Refresh token cleanup: removed {removed_count} expired revoked tokens")

        return removed_count

    def stop(self) -> None:
        """Stop the cleanup thread gracefully."""
        self._cleanup_stop_event.set()
        if self._cleanup_thread and self._cleanup_thread.is_alive():
            self._cleanup_thread.join(timeout=5.0)

    def get_revocation_metrics(self) -> dict:
        """Get metrics about revoked refresh tokens."""
        with self._mu:
            return {
                "revoked_token_count": len(self._revoked_refresh_tokens),
                "total_cleaned_up": self._cleanup_count,
                "last_cleanup_time": self._last_cleanup_time,
                "cleanup_interval_seconds": self._cleanup_interval,
            }


# =============================================================================
# API Key Management
# =============================================================================


@dataclass
class APIKey:
    key_id: str
    key_hash: str  # Store hash, not the actual key
    user_id: str
    name: str
    created_at: int
    last_used_at: Optional[int] = None
    revoked: bool = False
    permissions: list = field(default_factory=lambda: ["read", "write"])


class APIKeyManager:
    """Manages API keys for authentication."""

    def __init__(self):
        self._mu = threading.Lock()
        self._keys: dict[str, APIKey] = {}  # key_id -> APIKey
        self._key_lookup: dict[str, str] = {}  # key_hash -> key_id

    def _hash_key(self, key: str) -> str:
        return hashlib.sha256(key.encode("utf-8")).hexdigest()

    def create_key(self, user_id: str, name: str, permissions: Optional[list] = None) -> tuple[str, str]:
        """Create a new API key. Returns (key_id, raw_key)."""
        key_id = f"vk_{secrets.token_hex(8)}"
        raw_key = f"veloz_{secrets.token_hex(32)}"
        key_hash = self._hash_key(raw_key)

        api_key = APIKey(
            key_id=key_id,
            key_hash=key_hash,
            user_id=user_id,
            name=name,
            created_at=int(time.time()),
            permissions=permissions or ["read", "write"],
        )

        with self._mu:
            self._keys[key_id] = api_key
            self._key_lookup[key_hash] = key_id

        audit_logger.info(f"API key created: key_id={key_id}, user={user_id}, name={name}")
        return key_id, raw_key

    def validate_key(self, raw_key: str) -> Optional[APIKey]:
        """Validate an API key and return the APIKey object if valid."""
        key_hash = self._hash_key(raw_key)
        with self._mu:
            key_id = self._key_lookup.get(key_hash)
            if not key_id:
                return None
            api_key = self._keys.get(key_id)
            if not api_key or api_key.revoked:
                return None
            api_key.last_used_at = int(time.time())
            return api_key

    def revoke_key(self, key_id: str) -> bool:
        """Revoke an API key."""
        with self._mu:
            api_key = self._keys.get(key_id)
            if not api_key:
                return False
            api_key.revoked = True
            if api_key.key_hash in self._key_lookup:
                del self._key_lookup[api_key.key_hash]
        audit_logger.info(f"API key revoked: key_id={key_id}")
        return True

    def list_keys(self, user_id: Optional[str] = None) -> list[dict]:
        """List all API keys, optionally filtered by user."""
        with self._mu:
            keys = []
            for api_key in self._keys.values():
                if user_id and api_key.user_id != user_id:
                    continue
                keys.append({
                    "key_id": api_key.key_id,
                    "user_id": api_key.user_id,
                    "name": api_key.name,
                    "created_at": api_key.created_at,
                    "last_used_at": api_key.last_used_at,
                    "revoked": api_key.revoked,
                    "permissions": api_key.permissions,
                })
            return keys


# =============================================================================
# Permission-Based Access Control
# =============================================================================


class Permission:
    """Permission constants for access control."""
    # Read permissions
    READ_MARKET = "read:market"
    READ_ORDERS = "read:orders"
    READ_ACCOUNT = "read:account"
    READ_CONFIG = "read:config"

    # Write permissions
    WRITE_ORDERS = "write:orders"
    WRITE_CANCEL = "write:cancel"

    # Admin permissions
    ADMIN_KEYS = "admin:keys"
    ADMIN_USERS = "admin:users"
    ADMIN_CONFIG = "admin:config"

    # Legacy permission mappings
    LEGACY_READ = "read"
    LEGACY_WRITE = "write"
    LEGACY_ADMIN = "admin"


# Role definitions with their permissions
ROLE_PERMISSIONS = {
    "viewer": [
        Permission.READ_MARKET,
        Permission.READ_CONFIG,
    ],
    "trader": [
        Permission.READ_MARKET,
        Permission.READ_ORDERS,
        Permission.READ_ACCOUNT,
        Permission.READ_CONFIG,
        Permission.WRITE_ORDERS,
        Permission.WRITE_CANCEL,
    ],
    "admin": [
        Permission.READ_MARKET,
        Permission.READ_ORDERS,
        Permission.READ_ACCOUNT,
        Permission.READ_CONFIG,
        Permission.WRITE_ORDERS,
        Permission.WRITE_CANCEL,
        Permission.ADMIN_KEYS,
        Permission.ADMIN_USERS,
        Permission.ADMIN_CONFIG,
    ],
}

# Endpoint permission requirements
ENDPOINT_PERMISSIONS = {
    # Market data endpoints (read-only)
    "/api/market": [Permission.READ_MARKET],
    "/api/stream": [Permission.READ_MARKET],
    "/api/config": [Permission.READ_CONFIG],
    "/api/health": [Permission.READ_CONFIG],

    # Order endpoints
    "/api/orders": [Permission.READ_ORDERS],
    "/api/orders_state": [Permission.READ_ORDERS],
    "/api/order_state": [Permission.READ_ORDERS],
    "/api/order": [Permission.WRITE_ORDERS],
    "/api/cancel": [Permission.WRITE_CANCEL],

    # Account endpoints
    "/api/account": [Permission.READ_ACCOUNT],

    # Execution endpoints
    "/api/execution/ping": [Permission.READ_CONFIG],

    # Admin endpoints
    "/api/auth/keys": [Permission.ADMIN_KEYS],
    "/api/auth/roles": [Permission.ADMIN_USERS],

    # Audit endpoints
    "/api/audit/logs": [Permission.ADMIN_CONFIG],
    "/api/audit/stats": [Permission.ADMIN_CONFIG],
    "/api/audit/archive": [Permission.ADMIN_CONFIG],
}


class PermissionManager:
    """Manages permission checks for access control."""

    def __init__(self):
        # Map legacy permissions to new granular permissions
        self._legacy_map = {
            Permission.LEGACY_READ: [
                Permission.READ_MARKET,
                Permission.READ_ORDERS,
                Permission.READ_ACCOUNT,
                Permission.READ_CONFIG,
            ],
            Permission.LEGACY_WRITE: [
                Permission.WRITE_ORDERS,
                Permission.WRITE_CANCEL,
            ],
            Permission.LEGACY_ADMIN: [
                Permission.ADMIN_KEYS,
                Permission.ADMIN_USERS,
                Permission.ADMIN_CONFIG,
            ],
        }

    def expand_permissions(self, permissions: list) -> set:
        """Expand legacy permissions and roles to granular permissions."""
        expanded = set()
        for perm in permissions:
            # Check if it's a role
            if perm in ROLE_PERMISSIONS:
                expanded.update(ROLE_PERMISSIONS[perm])
            # Check if it's a legacy permission
            elif perm in self._legacy_map:
                expanded.update(self._legacy_map[perm])
            else:
                # It's already a granular permission
                expanded.add(perm)
        return expanded

    def check_permission(self, user_permissions: list, required_permissions: list) -> bool:
        """Check if user has all required permissions."""
        if not required_permissions:
            return True

        expanded = self.expand_permissions(user_permissions)

        # Check if user has any of the required permissions
        for required in required_permissions:
            if required in expanded:
                return True

        return False

    def get_endpoint_permissions(self, path: str, method: str) -> list:
        """Get required permissions for an endpoint."""
        # Check exact match first
        if path in ENDPOINT_PERMISSIONS:
            return ENDPOINT_PERMISSIONS[path]

        # Check prefix matches for dynamic paths
        for endpoint, perms in ENDPOINT_PERMISSIONS.items():
            if path.startswith(endpoint):
                return perms

        # Default: require at least read permission for GET, write for others
        if method == "GET":
            return [Permission.READ_CONFIG]
        return [Permission.WRITE_ORDERS]


# Global permission manager instance
permission_manager = PermissionManager()


def require_permission(*required_permissions):
    """Decorator factory for permission-based access control."""
    def decorator(handler_method):
        def wrapper(self, *args, **kwargs):
            auth_info = getattr(self, "_auth_info", None)
            if not auth_info:
                self._send_json(401, {"error": "authentication_required"})
                return

            user_permissions = auth_info.get("permissions", [])
            if not permission_manager.check_permission(user_permissions, list(required_permissions)):
                audit_logger.warning(
                    f"Permission denied: user={auth_info.get('user_id')}, "
                    f"required={required_permissions}, has={user_permissions}"
                )
                self._send_json(403, {
                    "error": "forbidden",
                    "message": f"Required permissions: {', '.join(required_permissions)}",
                })
                return

            return handler_method(self, *args, **kwargs)
        return wrapper
    return decorator


# =============================================================================
# Token Bucket Rate Limiter
# =============================================================================


@dataclass
class TokenBucket:
    tokens: float
    last_update: float
    capacity: float
    refill_rate: float  # tokens per second


class RateLimiter:
    """Token bucket rate limiter."""

    def __init__(
        self,
        capacity: int = 100,
        refill_rate: float = 10.0,
        window_seconds: int = 60,
    ):
        self._mu = threading.Lock()
        self._buckets: dict[str, TokenBucket] = {}
        self._capacity = capacity
        self._refill_rate = refill_rate
        self._window = window_seconds
        self._cleanup_interval = 300  # Cleanup old buckets every 5 minutes
        self._last_cleanup = time.time()

    def _get_bucket(self, key: str) -> TokenBucket:
        now = time.time()
        bucket = self._buckets.get(key)
        if bucket is None:
            bucket = TokenBucket(
                tokens=float(self._capacity),
                last_update=now,
                capacity=float(self._capacity),
                refill_rate=self._refill_rate,
            )
            self._buckets[key] = bucket
        return bucket

    def _refill(self, bucket: TokenBucket) -> None:
        now = time.time()
        elapsed = now - bucket.last_update
        bucket.tokens = min(bucket.capacity, bucket.tokens + elapsed * bucket.refill_rate)
        bucket.last_update = now

    def check_rate_limit(self, key: str, cost: float = 1.0) -> tuple[bool, dict]:
        """
        Check if request is allowed under rate limit.
        Returns (allowed, headers_dict).
        """
        with self._mu:
            # Periodic cleanup
            now = time.time()
            if now - self._last_cleanup > self._cleanup_interval:
                self._cleanup_old_buckets()
                self._last_cleanup = now

            bucket = self._get_bucket(key)
            self._refill(bucket)

            allowed = bucket.tokens >= cost
            if allowed:
                bucket.tokens -= cost

            # Calculate reset time (when bucket will be full again)
            tokens_needed = bucket.capacity - bucket.tokens
            reset_seconds = int(tokens_needed / bucket.refill_rate) if bucket.refill_rate > 0 else 0

            headers = {
                "X-RateLimit-Limit": str(int(bucket.capacity)),
                "X-RateLimit-Remaining": str(max(0, int(bucket.tokens))),
                "X-RateLimit-Reset": str(int(now) + reset_seconds),
            }

            if not allowed:
                headers["Retry-After"] = str(int((cost - bucket.tokens) / bucket.refill_rate) + 1)

            return allowed, headers

    def _cleanup_old_buckets(self) -> None:
        """Remove buckets that haven't been used recently."""
        now = time.time()
        stale_keys = [
            key for key, bucket in self._buckets.items()
            if now - bucket.last_update > self._window * 10
        ]
        for key in stale_keys:
            del self._buckets[key]


# =============================================================================
# Authentication Middleware
# =============================================================================


class AuthManager:
    """Manages authentication for the gateway."""

    def __init__(
        self,
        jwt_secret: Optional[str] = None,
        token_expiry: int = 3600,
        rate_limit_capacity: int = 100,
        rate_limit_refill: float = 10.0,
        auth_enabled: bool = True,
        admin_password: Optional[str] = None,
    ):
        self._enabled = auth_enabled
        self._jwt = JWTManager(jwt_secret or secrets.token_hex(32), token_expiry)
        self._api_keys = APIKeyManager()
        self._rate_limiter = RateLimiter(capacity=rate_limit_capacity, refill_rate=rate_limit_refill)
        self._admin_password = admin_password if admin_password is not None else os.environ.get("VELOZ_ADMIN_PASSWORD", "")

        # Public endpoints that don't require authentication
        self._public_endpoints = {"/health", "/api/health", "/api/stream"}

        # Create default admin API key if password is set
        if self._admin_password:
            key_id, raw_key = self._api_keys.create_key("admin", "default-admin-key", ["read", "write", "admin"])
            audit_logger.info(f"Default admin API key created: {key_id}")

    def is_enabled(self) -> bool:
        return self._enabled

    def is_public_endpoint(self, path: str) -> bool:
        return path in self._public_endpoints

    def authenticate(self, headers: dict) -> tuple[Optional[dict], str]:
        """
        Authenticate a request.
        Returns (user_info, error_message).
        user_info contains: user_id, permissions, auth_method
        """
        if not self._enabled:
            return {"user_id": "anonymous", "permissions": ["read", "write"], "auth_method": "disabled"}, ""

        # Check for Bearer token (JWT)
        auth_header = headers.get("Authorization", "")
        if auth_header.startswith("Bearer "):
            token = auth_header[7:]
            payload = self._jwt.verify_token(token)
            if payload:
                return {
                    "user_id": payload.get("sub", "unknown"),
                    "permissions": ["read", "write"],
                    "auth_method": "jwt",
                }, ""
            return None, "invalid_token"

        # Check for API key
        api_key = headers.get("X-API-Key", "")
        if api_key:
            key_obj = self._api_keys.validate_key(api_key)
            if key_obj:
                return {
                    "user_id": key_obj.user_id,
                    "permissions": key_obj.permissions,
                    "auth_method": "api_key",
                    "key_id": key_obj.key_id,
                }, ""
            return None, "invalid_api_key"

        return None, "authentication_required"

    def check_rate_limit(self, identifier: str) -> tuple[bool, dict]:
        """Check rate limit for the given identifier."""
        return self._rate_limiter.check_rate_limit(identifier)

    def create_token(self, user_id: str, password: str) -> Optional[dict]:
        """Create JWT access and refresh tokens if credentials are valid."""
        # Simple password check for admin
        if user_id == "admin" and self._admin_password and password == self._admin_password:
            access_token = self._jwt.create_token(user_id)
            refresh_token = self._jwt.create_refresh_token(user_id)
            audit_logger.info(f"JWT tokens created for user: {user_id}")
            return {
                "access_token": access_token,
                "refresh_token": refresh_token,
                "expires_in": self._jwt._expiry,
            }
        return None

    def refresh_access_token(self, refresh_token: str) -> Optional[dict]:
        """Create a new access token using a valid refresh token."""
        payload = self._jwt.verify_refresh_token(refresh_token)
        if not payload:
            return None

        user_id = payload.get("sub")
        if not user_id:
            return None

        # Create new access token
        access_token = self._jwt.create_token(user_id)
        audit_logger.info(f"Access token refreshed for user: {user_id}")
        return {
            "access_token": access_token,
            "expires_in": self._jwt._expiry,
        }

    def revoke_refresh_token(self, refresh_token: str) -> bool:
        """Revoke a refresh token."""
        return self._jwt.revoke_refresh_token(refresh_token)

    def create_api_key(self, user_id: str, name: str, permissions: Optional[list] = None) -> tuple[str, str]:
        """Create a new API key."""
        return self._api_keys.create_key(user_id, name, permissions)

    def revoke_api_key(self, key_id: str) -> bool:
        """Revoke an API key."""
        return self._api_keys.revoke_key(key_id)

    def list_api_keys(self, user_id: Optional[str] = None) -> list[dict]:
        """List API keys."""
        return self._api_keys.list_keys(user_id)

    def get_revocation_metrics(self) -> dict:
        """Get metrics about revoked refresh tokens."""
        return self._jwt.get_revocation_metrics()

    def stop(self) -> None:
        """Stop background threads gracefully."""
        self._jwt.stop()


class EngineBridge:
    def __init__(
        self,
        engine_cmd,
        market_source,
        market_symbol,
        binance_base_url,
        order_store,
        account_store,
    ):
        self._proc = subprocess.Popen(
            engine_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._lock = threading.Lock()
        self._cv = threading.Condition(self._lock)
        self._latest_market = {"symbol": "BTCUSDT", "price": None, "ts_ns": None}
        self._orders = []
        self._max_orders = 200
        self._events = deque(maxlen=500)
        self._next_event_id = 0
        self._market_source = market_source
        self._market_symbol = market_symbol
        self._binance_base_url = binance_base_url
        self._market_thread = None
        self._external_market_last_ok_ns = 0
        self._order_store = order_store
        self._account_store = account_store

        self._stdout_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self._stderr_thread = threading.Thread(target=self._drain_stderr, daemon=True)
        self._stdout_thread.start()
        self._stderr_thread.start()

        if self._market_source == "binance_rest":
            self._market_thread = threading.Thread(
                target=self._poll_binance_price, daemon=True
            )
            self._market_thread.start()

    def stop(self):
        try:
            if self._proc.poll() is None:
                self._proc.terminate()
        except Exception:
            pass

    def market(self):
        with self._lock:
            return dict(self._latest_market)

    def orders(self):
        with self._lock:
            return list(self._orders)

    def config(self):
        return {
            "market_source": self._market_source,
            "market_symbol": self._market_symbol,
            "binance_base_url": (
                self._binance_base_url
                if self._market_source == "binance_rest"
                else None
            ),
        }

    def is_connected(self):
        return self._proc.poll() is None

    def pop_events(self, last_id):
        with self._lock:
            if last_id is None:
                return list(self._events), (
                    self._events[-1]["_id"] if self._events else 0
                )

            out = []
            for e in self._events:
                if e["_id"] > last_id:
                    out.append(e)
            new_last = out[-1]["_id"] if out else last_id
            return out, new_last

    def wait_for_event(self, last_id, timeout_s):
        end = time.time() + timeout_s
        with self._lock:
            while True:
                if self._events and (
                    last_id is None or self._events[-1]["_id"] > last_id
                ):
                    return True
                remaining = end - time.time()
                if remaining <= 0:
                    return False
                self._cv.wait(timeout=remaining)

    def place_order(self, side, symbol, qty, price, client_order_id):
        if self._proc.stdin is None:
            raise RuntimeError("engine stdin unavailable")
        cmd = f"ORDER {side} {symbol} {qty} {price} {client_order_id}\n"
        self._proc.stdin.write(cmd)
        self._proc.stdin.flush()

    def cancel_order(self, client_order_id):
        if self._proc.stdin is None:
            raise RuntimeError("engine stdin unavailable")
        cmd = f"CANCEL {client_order_id}\n"
        self._proc.stdin.write(cmd)
        self._proc.stdin.flush()

    def _read_stdout(self):
        if self._proc.stdout is None:
            return
        for line in self._proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                evt = json.loads(line)
            except Exception:
                continue
            t = evt.get("type")
            with self._lock:
                if (
                    t == "market"
                    and self._market_source == "binance_rest"
                    and (time.time_ns() - self._external_market_last_ok_ns)
                    < 5_000_000_000
                ):
                    continue
                self._next_event_id += 1
                evt["_id"] = self._next_event_id
                self._events.append(evt)
                if t == "market":
                    self._latest_market = {
                        "symbol": evt.get("symbol"),
                        "price": evt.get("price"),
                        "ts_ns": evt.get("ts_ns"),
                    }
                elif t in ("order_received", "order_update", "fill", "error"):
                    self._orders.append(evt)
                    if len(self._orders) > self._max_orders:
                        self._orders = self._orders[-self._max_orders :]
                if (
                    t in ("order_received", "order_update", "fill", "order_state")
                    and self._order_store is not None
                ):
                    self._order_store.apply_event(
                        {k: v for k, v in evt.items() if k != "_id"}
                    )
                if t == "account" and self._account_store is not None:
                    self._account_store.apply_event(
                        {k: v for k, v in evt.items() if k != "_id"}
                    )
                self._cv.notify_all()

    def _drain_stderr(self):
        if self._proc.stderr is None:
            return
        for _ in self._proc.stderr:
            pass

    def _emit_event(self, evt):
        with self._lock:
            self._next_event_id += 1
            evt["_id"] = self._next_event_id
            self._events.append(evt)
            if evt.get("type") == "market":
                self._latest_market = {
                    "symbol": evt.get("symbol"),
                    "price": evt.get("price"),
                    "ts_ns": evt.get("ts_ns"),
                }
            self._cv.notify_all()

    def _poll_binance_price(self):
        last_error_emit = 0.0
        while True:
            if self._proc.poll() is not None:
                return
            try:
                symbol = self._market_symbol
                base = self._binance_base_url.rstrip("/")
                url = f"{base}/api/v3/ticker/price?symbol={symbol}"
                req = urllib.request.Request(url, headers={"User-Agent": "VeloZ/0.1"})
                with urllib.request.urlopen(req, timeout=2.0) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                data = json.loads(raw)
                price = float(data["price"])
                self._external_market_last_ok_ns = time.time_ns()
                self._emit_event(
                    {
                        "type": "market",
                        "symbol": symbol,
                        "ts_ns": time.time_ns(),
                        "price": price,
                    }
                )
            except Exception:
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._emit_event(
                        {
                            "type": "error",
                            "ts_ns": time.time_ns(),
                            "message": "market_source_error",
                        }
                    )
            time.sleep(0.5)

    def emit_event(self, evt):
        t = evt.get("type")
        with self._lock:
            self._next_event_id += 1
            evt["_id"] = self._next_event_id
            self._events.append(evt)
            if t == "market":
                self._latest_market = {
                    "symbol": evt.get("symbol"),
                    "price": evt.get("price"),
                    "ts_ns": evt.get("ts_ns"),
                }
            elif t in ("order_update", "fill", "error"):
                self._orders.append(evt)
                if len(self._orders) > self._max_orders:
                    self._orders = self._orders[-self._max_orders :]
            self._cv.notify_all()


@dataclass
class BalanceState:
    asset: str
    free: float
    locked: float


class AccountStore:
    def __init__(self):
        self._mu = threading.Lock()
        self._balances: dict[str, BalanceState] = {}
        self._last_ts_ns: int | None = None

    def set_balances(self, balances: list[BalanceState], ts_ns: int | None):
        with self._mu:
            self._balances = {b.asset: b for b in balances}
            self._last_ts_ns = ts_ns

    def apply_event(self, evt: dict):
        if evt.get("type") != "account":
            return
        ts_ns = evt.get("ts_ns")
        balances = evt.get("balances")
        if not isinstance(balances, list):
            if ts_ns is not None:
                with self._mu:
                    self._last_ts_ns = ts_ns
            return
        items = []
        for b in balances:
            if not isinstance(b, dict):
                continue
            asset = b.get("asset")
            if not asset:
                continue
            try:
                free = float(b.get("free", 0.0))
            except Exception:
                free = 0.0
            try:
                locked = float(b.get("locked", 0.0))
            except Exception:
                locked = 0.0
            items.append(BalanceState(asset=str(asset), free=free, locked=locked))
        if items:
            self.set_balances(items, ts_ns)
        elif ts_ns is not None:
            with self._mu:
                self._last_ts_ns = ts_ns

    def snapshot(self):
        with self._mu:
            return {
                "ts_ns": self._last_ts_ns,
                "balances": [asdict(b) for b in self._balances.values()],
            }


@dataclass
class OrderState:
    client_order_id: str
    symbol: str | None = None
    side: str | None = None
    order_type: str | None = None  # "limit" or "market"
    order_qty: float | None = None
    limit_price: float | None = None
    venue_order_id: str | None = None
    status: str | None = None
    reason: str | None = None
    executed_qty: float = 0.0
    avg_price: float | None = None
    last_ts_ns: int | None = None


class OrderStore:
    def __init__(self):
        self._mu = threading.Lock()
        self._orders: dict[str, OrderState] = {}

    def note_order_params(self, *, client_order_id, symbol, side, qty, price, order_type=None):
        if not client_order_id:
            return
        with self._mu:
            st = self._orders.get(client_order_id)
            if st is None:
                st = OrderState(client_order_id=str(client_order_id))
                self._orders[client_order_id] = st
            if symbol:
                st.symbol = str(symbol)
            if side:
                st.side = str(side)
            if order_type:
                st.order_type = str(order_type)
            try:
                if qty is not None:
                    st.order_qty = float(qty)
            except Exception:
                pass
            try:
                if price is not None:
                    st.limit_price = float(price)
            except Exception:
                pass

    def apply_event(self, evt: dict):
        t = evt.get("type")
        if t not in ("order_received", "order_update", "fill", "order_state"):
            return

        cid = evt.get("client_order_id")
        if not cid:
            return

        with self._mu:
            st = self._orders.get(cid)
            if st is None:
                st = OrderState(client_order_id=str(cid))
                self._orders[cid] = st

            if t == "order_received":
                # Handle order_received event from engine
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                if evt.get("side"):
                    st.side = evt.get("side")
                if evt.get("order_type"):
                    st.order_type = evt.get("order_type")
                if evt.get("quantity") is not None:
                    try:
                        st.order_qty = float(evt.get("quantity"))
                    except Exception:
                        pass
                if evt.get("price") is not None:
                    try:
                        st.limit_price = float(evt.get("price"))
                    except Exception:
                        pass
                st.status = "PENDING"
                return

            if t == "order_state":
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                if evt.get("side"):
                    st.side = evt.get("side")
                if evt.get("order_qty") is not None:
                    try:
                        st.order_qty = float(evt.get("order_qty"))
                    except Exception:
                        pass
                if evt.get("limit_price") is not None:
                    try:
                        st.limit_price = float(evt.get("limit_price"))
                    except Exception:
                        pass
                if evt.get("venue_order_id"):
                    st.venue_order_id = evt.get("venue_order_id")
                if evt.get("status"):
                    st.status = evt.get("status")
                if evt.get("reason"):
                    st.reason = evt.get("reason")
                if evt.get("executed_qty") is not None:
                    try:
                        st.executed_qty = float(evt.get("executed_qty"))
                    except Exception:
                        pass
                if evt.get("avg_price") is not None:
                    try:
                        st.avg_price = float(evt.get("avg_price"))
                    except Exception:
                        pass
                if evt.get("last_ts_ns"):
                    st.last_ts_ns = evt.get("last_ts_ns")
                return

            if t == "order_update":
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                if evt.get("side"):
                    st.side = evt.get("side")
                if evt.get("qty") is not None:
                    try:
                        st.order_qty = float(evt.get("qty"))
                    except Exception:
                        pass
                if evt.get("price") is not None:
                    try:
                        st.limit_price = float(evt.get("price"))
                    except Exception:
                        pass
                if evt.get("venue_order_id"):
                    st.venue_order_id = evt.get("venue_order_id")
                if evt.get("status"):
                    st.status = evt.get("status")
                if evt.get("reason"):
                    st.reason = evt.get("reason")
                if evt.get("ts_ns"):
                    st.last_ts_ns = evt.get("ts_ns")
                return

            if t == "fill":
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                qty = evt.get("qty")
                price = evt.get("price")
                try:
                    if qty is not None:
                        st.executed_qty += float(qty)
                except Exception:
                    pass
                try:
                    if st.executed_qty > 0 and price is not None:
                        st.avg_price = float(price)
                except Exception:
                    pass
                if evt.get("ts_ns"):
                    st.last_ts_ns = evt.get("ts_ns")
                if st.order_qty is not None and st.order_qty > 0:
                    if st.executed_qty + 1e-12 >= st.order_qty:
                        if st.status not in ("CANCELLED", "REJECTED", "EXPIRED"):
                            st.status = "FILLED"
                    elif st.executed_qty > 0:
                        if st.status not in ("CANCELLED", "REJECTED", "EXPIRED"):
                            st.status = "PARTIALLY_FILLED"

    def list(self):
        with self._mu:
            return [asdict(v) for v in self._orders.values()]

    def get(self, client_order_id: str):
        with self._mu:
            v = self._orders.get(client_order_id)
            return asdict(v) if v else None


class BinanceSpotRestClient:
    def __init__(self, base_url, api_key, api_secret):
        self._base_url = (base_url or "").rstrip("/")
        self._api_key = api_key or ""
        self._api_secret = api_secret or ""

    def update_credentials(self, api_key, api_secret):
        self._api_key = api_key or ""
        self._api_secret = api_secret or ""

    def enabled(self):
        return bool(self._base_url and self._api_key and self._api_secret)

    def _sign(self, query_string):
        mac = hmac.new(
            self._api_secret.encode("utf-8"),
            query_string.encode("utf-8"),
            hashlib.sha256,
        )
        return mac.hexdigest()

    def _request(self, method, path, params, signed, api_key_only=False):
        base_params = dict(params or {})
        headers = {"User-Agent": "VeloZ/0.1"}

        if api_key_only:
            headers["X-MBX-APIKEY"] = self._api_key
            query = urlencode(sorted(base_params.items()), doseq=True)
        elif signed:
            base_params.setdefault("timestamp", int(time.time() * 1000))
            base_params.setdefault("recvWindow", 5000)
            query = urlencode(sorted(base_params.items()), doseq=True)
            sig = self._sign(query)
            query = query + "&signature=" + sig
            headers["X-MBX-APIKEY"] = self._api_key
        else:
            query = urlencode(sorted(base_params.items()), doseq=True)

        url = f"{self._base_url}{path}"
        if query:
            url += "?" + query

        req = urllib.request.Request(url, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=5.0) as resp:
                body = resp.read().decode("utf-8", errors="replace")
            return json.loads(body) if body else {}
        except urllib.error.HTTPError as e:
            raw = e.read().decode("utf-8", errors="replace")
            try:
                return {"_http_error": e.code, "_body": json.loads(raw) if raw else {}}
            except Exception:
                return {"_http_error": e.code, "_body": raw}
        except Exception as e:
            return {"_error": str(e)}

    def ping(self):
        return self._request("GET", "/api/v3/ping", {}, signed=False)

    def place_order(self, *, symbol, side, qty, price, client_order_id):
        params = {
            "symbol": symbol,
            "side": side,
            "type": "LIMIT",
            "timeInForce": "GTC",
            "quantity": qty,
            "price": price,
            "newClientOrderId": client_order_id,
        }
        return self._request("POST", "/api/v3/order", params, signed=True)

    def cancel_order(self, *, symbol, client_order_id):
        params = {"symbol": symbol, "origClientOrderId": client_order_id}
        return self._request("DELETE", "/api/v3/order", params, signed=True)

    def get_order(self, *, symbol, client_order_id):
        params = {"symbol": symbol, "origClientOrderId": client_order_id}
        return self._request("GET", "/api/v3/order", params, signed=True)

    def new_listen_key(self):
        return self._request(
            "POST", "/api/v3/userDataStream", {}, signed=False, api_key_only=True
        )

    def keepalive_listen_key(self, listen_key):
        return self._request(
            "PUT",
            "/api/v3/userDataStream",
            {"listenKey": listen_key},
            signed=False,
            api_key_only=True,
        )

    def close_listen_key(self, listen_key):
        return self._request(
            "DELETE",
            "/api/v3/userDataStream",
            {"listenKey": listen_key},
            signed=False,
            api_key_only=True,
        )


class SimpleWebSocketClient:
    def __init__(self, ws_url, on_text, on_error):
        self._ws_url = ws_url
        self._on_text = on_text
        self._on_error = on_error
        self._sock = None
        self._mu = threading.Lock()
        self._closed = False

    def close(self):
        with self._mu:
            self._closed = True
            sock = self._sock
            self._sock = None
        try:
            if sock is not None:
                sock.close()
        except Exception:
            pass

    def run(self):
        try:
            self._connect()
            while True:
                with self._mu:
                    if self._closed:
                        return
                opcode, payload = self._recv_frame()
                if opcode is None:
                    return
                if opcode == 0x1:
                    try:
                        self._on_text(payload.decode("utf-8", errors="replace"))
                    except Exception:
                        pass
                elif opcode == 0x8:
                    return
                elif opcode == 0x9:
                    self._send_pong(payload)
        except Exception as e:
            try:
                self._on_error(str(e))
            except Exception:
                pass
            return
        finally:
            self.close()

    def _parse_ws_url(self):
        u = urlparse(self._ws_url)
        if u.scheme not in ("ws", "wss"):
            raise ValueError("bad_ws_url")
        host = u.hostname
        if not host:
            raise ValueError("bad_ws_url")
        port = u.port or (443 if u.scheme == "wss" else 80)
        path = u.path or "/"
        if u.query:
            path += "?" + u.query
        return u.scheme, host, port, path

    def _connect(self):
        scheme, host, port, path = self._parse_ws_url()
        raw = socket.create_connection((host, port), timeout=10.0)
        if scheme == "wss":
            ctx = ssl.create_default_context()
            s = ctx.wrap_socket(raw, server_hostname=host)
        else:
            s = raw

        key = base64.b64encode(os.urandom(16)).decode("ascii")
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        ).encode("utf-8")
        s.sendall(req)

        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("ws_handshake_failed")
            buf += chunk
            if len(buf) > 64 * 1024:
                raise RuntimeError("ws_handshake_too_large")

        head, rest = buf.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise RuntimeError("ws_handshake_not_101")

        with self._mu:
            self._sock = s
        self._recv_buffer = rest

    def _recv_exact(self, n):
        out = b""
        while len(out) < n:
            if self._recv_buffer:
                take = min(n - len(out), len(self._recv_buffer))
                out += self._recv_buffer[:take]
                self._recv_buffer = self._recv_buffer[take:]
                continue
            with self._mu:
                s = self._sock
            if s is None:
                raise RuntimeError("ws_closed")
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("ws_closed")
            self._recv_buffer += chunk
        return out

    def _recv_frame(self):
        b1, b2 = self._recv_exact(2)
        fin = (b1 & 0x80) != 0
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        ln = b2 & 0x7F
        if ln == 126:
            ln = int.from_bytes(self._recv_exact(2), "big")
        elif ln == 127:
            ln = int.from_bytes(self._recv_exact(8), "big")
        mask_key = b""
        if masked:
            mask_key = self._recv_exact(4)
        payload = self._recv_exact(ln) if ln else b""
        if masked and payload:
            payload = bytes(payload[i] ^ mask_key[i % 4] for i in range(len(payload)))
        if not fin:
            raise RuntimeError("ws_fragments_unsupported")
        return opcode, payload

    def _send_frame(self, opcode, payload):
        with self._mu:
            s = self._sock
            closed = self._closed
        if closed or s is None:
            return

        fin_opcode = 0x80 | (opcode & 0x0F)
        payload = payload or b""
        ln = len(payload)
        mask_bit = 0x80
        if ln < 126:
            header = bytes([fin_opcode, mask_bit | ln])
        elif ln < (1 << 16):
            header = bytes([fin_opcode, mask_bit | 126]) + ln.to_bytes(2, "big")
        else:
            header = bytes([fin_opcode, mask_bit | 127]) + ln.to_bytes(8, "big")
        mask_key = os.urandom(4)
        masked = bytes(payload[i] ^ mask_key[i % 4] for i in range(ln))
        s.sendall(header + mask_key + masked)

    def _send_pong(self, payload):
        self._send_frame(0xA, payload)


class ExecutionRouter:
    def __init__(
        self, *, bridge, execution_mode, binance_client, order_store, account_store=None
    ):
        self._bridge = bridge
        self._execution_mode = execution_mode
        self._binance = binance_client
        self._orders = order_store
        self._account = account_store or AccountStore()
        self._poll_mu = threading.Lock()
        self._poll: dict[str, dict] = {}
        self._poll_thread = None
        self._user_stream_mu = threading.Lock()
        self._user_stream_connected = False
        self._user_stream_thread = None
        if (
            self._execution_mode == "binance_testnet_spot"
            and self._binance
            and self._binance.enabled()
        ):
            self._poll_thread = threading.Thread(
                target=self._poll_binance_orders, daemon=True
            )
            self._poll_thread.start()
            self._user_stream_thread = threading.Thread(
                target=self._run_binance_user_stream, daemon=True
            )
            self._user_stream_thread.start()

    def config(self):
        with self._user_stream_mu:
            ws_connected = self._user_stream_connected
        return {
            "execution_mode": self._execution_mode,
            "binance_trade_enabled": bool(self._binance and self._binance.enabled()),
            "binance_trade_base_url": (
                self._binance._base_url
                if self._execution_mode == "binance_testnet_spot"
                else None
            ),
            "binance_user_stream_connected": (
                ws_connected
                if self._execution_mode == "binance_testnet_spot"
                else False
            ),
        }

    def orders_state(self):
        return self._orders.list()

    def order_state(self, client_order_id):
        return self._orders.get(client_order_id)

    def account_state(self):
        return self._account.snapshot()

    def ping(self):
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                return {"ok": False, "error": "binance_not_configured"}
            r = self._binance.ping()
            ok = not (isinstance(r, dict) and (r.get("_error") or r.get("_http_error")))
            return {"ok": ok, "result": r}
        return {"ok": True}

    def place_order(self, side, symbol, qty, price, client_order_id):
        self._orders.note_order_params(
            client_order_id=client_order_id,
            symbol=symbol,
            side=side,
            qty=qty,
            price=price,
        )
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                raise RuntimeError("binance_not_configured")
            r = self._binance.place_order(
                symbol=symbol,
                side=side,
                qty=qty,
                price=price,
                client_order_id=client_order_id,
            )
            if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                evt = {
                    "type": "order_update",
                    "ts_ns": time.time_ns(),
                    "client_order_id": client_order_id,
                    "status": "REJECTED",
                    "reason": "binance_error",
                    "symbol": symbol,
                }
                self._bridge.emit_event(evt)
                self._orders.apply_event(evt)
                return {"ok": False, "error": "binance_error", "details": r}

            venue_order_id = (
                str(r.get("orderId"))
                if isinstance(r, dict) and "orderId" in r
                else None
            )
            evt = {
                "type": "order_update",
                "ts_ns": time.time_ns(),
                "client_order_id": client_order_id,
                "venue_order_id": venue_order_id,
                "status": "ACCEPTED",
                "symbol": symbol,
            }
            self._bridge.emit_event(evt)
            self._orders.apply_event(evt)
            with self._poll_mu:
                self._poll[client_order_id] = {
                    "symbol": symbol,
                    "last_exec_qty": 0.0,
                    "last_status": "ACCEPTED",
                }
            return {
                "ok": True,
                "client_order_id": client_order_id,
                "venue_order_id": venue_order_id,
            }

        self._bridge.place_order(side, symbol, qty, price, client_order_id)
        return {"ok": True, "client_order_id": client_order_id}

    def cancel_order(self, client_order_id, symbol=None):
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                raise RuntimeError("binance_not_configured")
            market_symbol = (
                (symbol or self._bridge.config().get("market_symbol") or "BTCUSDT")
                .strip()
                .upper()
            )
            r = self._binance.cancel_order(
                symbol=market_symbol, client_order_id=client_order_id
            )
            if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                evt = {
                    "type": "order_update",
                    "ts_ns": time.time_ns(),
                    "client_order_id": client_order_id,
                    "status": "REJECTED",
                    "reason": "binance_error",
                }
                self._bridge.emit_event(evt)
                self._orders.apply_event(evt)
                return {"ok": False, "error": "binance_error", "details": r}
            evt = {
                "type": "order_update",
                "ts_ns": time.time_ns(),
                "client_order_id": client_order_id,
                "status": "CANCELLED",
            }
            self._bridge.emit_event(evt)
            self._orders.apply_event(evt)
            with self._poll_mu:
                self._poll.pop(client_order_id, None)
            return {"ok": True, "client_order_id": client_order_id}

        self._bridge.cancel_order(client_order_id)
        return {"ok": True, "client_order_id": client_order_id}

    def _map_binance_status(self, status):
        s = (status or "").upper()
        if s == "NEW":
            return "ACCEPTED"
        if s == "PARTIALLY_FILLED":
            return "PARTIALLY_FILLED"
        if s == "FILLED":
            return "FILLED"
        if s == "CANCELED":
            return "CANCELLED"
        if s == "REJECTED":
            return "REJECTED"
        if s == "EXPIRED":
            return "EXPIRED"
        return s or "UNKNOWN"

    def _poll_binance_orders(self):
        last_error_emit = 0.0
        while True:
            time.sleep(0.5)
            if not (self._binance and self._binance.enabled()):
                continue
            with self._user_stream_mu:
                if self._user_stream_connected:
                    continue

            with self._poll_mu:
                items = list(self._poll.items())

            for cid, meta in items:
                symbol = meta.get("symbol") or (
                    self._bridge.config().get("market_symbol") or "BTCUSDT"
                )
                r = self._binance.get_order(symbol=symbol, client_order_id=cid)
                if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                    now = time.time()
                    if now - last_error_emit > 5.0:
                        last_error_emit = now
                        self._bridge.emit_event(
                            {
                                "type": "error",
                                "ts_ns": time.time_ns(),
                                "message": "binance_poll_error",
                            }
                        )
                    continue

                status = self._map_binance_status(
                    r.get("status") if isinstance(r, dict) else None
                )
                try:
                    exec_qty = float(r.get("executedQty", 0.0))
                except Exception:
                    exec_qty = 0.0
                try:
                    orig_qty = float(r.get("origQty", 0.0))
                except Exception:
                    orig_qty = 0.0
                try:
                    cq = float(r.get("cummulativeQuoteQty", 0.0))
                except Exception:
                    cq = 0.0
                avg_price = (cq / exec_qty) if exec_qty > 0 else None
                if orig_qty > 0:
                    self._orders.note_order_params(
                        client_order_id=cid,
                        symbol=symbol,
                        side=None,
                        qty=orig_qty,
                        price=None,
                    )

                last_exec = float(meta.get("last_exec_qty", 0.0) or 0.0)
                if exec_qty > last_exec:
                    delta = exec_qty - last_exec
                    fill_evt = {
                        "type": "fill",
                        "ts_ns": time.time_ns(),
                        "client_order_id": cid,
                        "symbol": symbol,
                        "qty": delta,
                        "price": avg_price,
                    }
                    self._bridge.emit_event(fill_evt)
                    self._orders.apply_event(fill_evt)
                    meta["last_exec_qty"] = exec_qty

                last_status = meta.get("last_status")
                if status and status != last_status:
                    upd = {
                        "type": "order_update",
                        "ts_ns": time.time_ns(),
                        "client_order_id": cid,
                        "symbol": symbol,
                        "status": status,
                    }
                    self._bridge.emit_event(upd)
                    self._orders.apply_event(upd)
                    meta["last_status"] = status

                if status in ("FILLED", "CANCELLED", "REJECTED", "EXPIRED"):
                    with self._poll_mu:
                        self._poll.pop(cid, None)
                else:
                    with self._poll_mu:
                        if cid in self._poll:
                            self._poll[cid] = meta

    def _handle_binance_execution_report(self, msg: dict):
        try:
            cid = str(msg.get("c") or "")
            if not cid:
                return
            symbol = str(msg.get("s") or "")
            side = str(msg.get("S") or "")
            venue_order_id = str(msg.get("i")) if msg.get("i") is not None else None
            status = self._map_binance_status(msg.get("X"))
            ts_ns = (
                int(msg.get("E", 0)) * 1_000_000
                if msg.get("E") is not None
                else time.time_ns()
            )

            try:
                orig_qty = float(msg.get("q", 0.0))
            except Exception:
                orig_qty = 0.0
            try:
                limit_price = float(msg.get("p", 0.0))
            except Exception:
                limit_price = 0.0
            if orig_qty > 0:
                self._orders.note_order_params(
                    client_order_id=cid,
                    symbol=symbol,
                    side=side,
                    qty=orig_qty,
                    price=limit_price,
                )

            upd = {
                "type": "order_update",
                "ts_ns": ts_ns,
                "client_order_id": cid,
                "venue_order_id": venue_order_id,
                "status": status,
                "symbol": symbol,
                "side": side,
                "qty": orig_qty if orig_qty > 0 else None,
                "price": limit_price if limit_price > 0 else None,
            }
            self._bridge.emit_event(upd)
            self._orders.apply_event({k: v for k, v in upd.items() if v is not None})

            try:
                last_fill_qty = float(msg.get("l", 0.0))
            except Exception:
                last_fill_qty = 0.0
            if last_fill_qty > 0:
                try:
                    last_fill_price = float(msg.get("L", 0.0))
                except Exception:
                    last_fill_price = 0.0
                fill_evt = {
                    "type": "fill",
                    "ts_ns": ts_ns,
                    "client_order_id": cid,
                    "symbol": symbol,
                    "qty": last_fill_qty,
                    "price": last_fill_price if last_fill_price > 0 else None,
                }
                self._bridge.emit_event(fill_evt)
                self._orders.apply_event(
                    {k: v for k, v in fill_evt.items() if v is not None}
                )
        except Exception:
            return

    def _handle_binance_account_update(self, msg: dict):
        try:
            ts_ns = (
                int(msg.get("E", 0)) * 1_000_000
                if msg.get("E") is not None
                else time.time_ns()
            )
            balances = []
            for b in msg.get("B", []) or []:
                asset = str(b.get("a") or "")
                if not asset:
                    continue
                try:
                    free = float(b.get("f", 0.0))
                except Exception:
                    free = 0.0
                try:
                    locked = float(b.get("l", 0.0))
                except Exception:
                    locked = 0.0
                balances.append(BalanceState(asset=asset, free=free, locked=locked))
            self._account.set_balances(balances, ts_ns)
            self._bridge.emit_event({"type": "account", "ts_ns": ts_ns})
        except Exception:
            return

    def _run_binance_user_stream(self):
        ws_base = (
            os.environ.get(
                "VELOZ_BINANCE_WS_BASE_URL", "wss://testnet.binance.vision/ws"
            ).strip()
            or "wss://testnet.binance.vision/ws"
        )
        last_error_emit = 0.0

        while True:
            time.sleep(0.2)
            if not (self._binance and self._binance.enabled()):
                with self._user_stream_mu:
                    self._user_stream_connected = False
                continue

            lk = self._binance.new_listen_key()
            listen_key = lk.get("listenKey") if isinstance(lk, dict) else None
            if not listen_key:
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._bridge.emit_event(
                        {
                            "type": "error",
                            "ts_ns": time.time_ns(),
                            "message": "listen_key_error",
                        }
                    )
                continue

            stop_flag = {"stop": False}

            def on_text(txt):
                try:
                    msg = json.loads(txt)
                except Exception:
                    return
                et = msg.get("e")
                if et == "executionReport":
                    self._handle_binance_execution_report(msg)
                elif et == "outboundAccountPosition":
                    self._handle_binance_account_update(msg)

            def on_error(err):
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._bridge.emit_event(
                        {
                            "type": "error",
                            "ts_ns": time.time_ns(),
                            "message": "binance_ws_error",
                        }
                    )

            ws_url = ws_base.rstrip("/") + "/" + listen_key
            ws = SimpleWebSocketClient(ws_url, on_text=on_text, on_error=on_error)

            def keepalive():
                while not stop_flag["stop"]:
                    time.sleep(25 * 60)
                    if stop_flag["stop"]:
                        return
                    self._binance.keepalive_listen_key(listen_key)

            ka = threading.Thread(target=keepalive, daemon=True)
            ka.start()

            with self._user_stream_mu:
                self._user_stream_connected = True
            ws.run()
            stop_flag["stop"] = True
            try:
                ws.close()
            except Exception:
                pass
            try:
                self._binance.close_listen_key(listen_key)
            except Exception:
                pass
            with self._user_stream_mu:
                self._user_stream_connected = False


class WebSocketManager:
    """Manages WebSocket connections and broadcasts."""
    def __init__(self):
        self._connections: dict[str, list[tuple[socket.socket, str, str]]] = {}  # path -> [(sock, address, port)]
        self._lock = threading.Lock()

    def add_connection(self, path: str, sock: socket.socket, address: str, port: int):
        """Add a new WebSocket connection."""
        with self._lock:
            self._connections.setdefault(path, []).append((sock, address, port))

    def remove_connection(self, path: str, sock: socket.socket):
        """Remove a WebSocket connection."""
        with self._lock:
            self._connections[path] = [(s, a, p) for s, a, p in self._connections.get(path, [])
                                       if s != sock]

    def broadcast(self, path: str, message: str, exclude_sock: socket.socket = None):
        """Broadcast message to all connections on a path."""
        with self._lock:
            connections = self._connections.get(path, [])
            for sock, _, _ in connections:
                if sock != exclude_sock:
                    try:
                        self._send_ws_message(sock, message)
                    except:
                        pass

    def _send_ws_message(self, sock: socket.socket, message: str):
        """Send a WebSocket message."""
        data = message.encode('utf-8')
        frame = self._create_ws_frame(data)
        try:
            sock.sendall(frame)
        except:
            pass

    def _create_ws_frame(self, data: bytes, opcode: int = 0x1) -> bytes:
        """Create a WebSocket frame."""
        fin = 0x80
        frame = bytearray()
        frame.append(fin | opcode)

        payload_len = len(data)
        if payload_len <= 125:
            frame.append(payload_len)
        elif payload_len <= 65535:
            frame.append(126 | (payload_len & 0xFF))
            frame.append((payload_len >> 8) & 0xFF)
        else:
            frame.append(127)
            frame.append((payload_len >> 24) & 0xFF)
            frame.append((payload_len >> 16) & 0xFF)
            frame.append((payload_len >> 8) & 0xFF)
            frame.append(payload_len & 0xFF)

        return bytes(frame) + data


# Global WebSocket manager
ws_manager = WebSocketManager()


class Handler(SimpleHTTPRequestHandler):
    static_dir = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=self.static_dir, **kwargs)

    def end_headers(self):
        # Get security manager from server if available
        security_manager = getattr(self.server, "security_manager", None)
        if security_manager:
            # Get origin from request headers
            origin = self.headers.get("Origin", "")
            is_https = self.headers.get("X-Forwarded-Proto", "").lower() == "https"

            # Add security and CORS headers
            for name, value in security_manager.get_response_headers(
                origin=origin, is_https=is_https, method=self.command
            ).items():
                self.send_header(name, value)
        else:
            # Fallback to permissive CORS (development mode)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type,Authorization,X-API-Key")
        super().end_headers()

    def _get_client_identifier(self) -> str:
        """Get a unique identifier for rate limiting."""
        # Use API key ID if authenticated, otherwise use IP
        auth_info = getattr(self, "_auth_info", None)
        if auth_info and auth_info.get("key_id"):
            return f"key:{auth_info['key_id']}"
        return f"ip:{self.client_address[0]}"

    def _check_auth_and_rate_limit(self, check_permissions: bool = True) -> bool:
        """
        Check authentication, permissions, and rate limit.
        Returns True if request should proceed, False if rejected.
        Sets self._auth_info and self._rate_limit_headers.
        """
        parsed = urlparse(self.path)
        auth_manager = getattr(self.server, "auth_manager", None)

        # Default rate limit headers
        self._rate_limit_headers = {}
        self._auth_info = None

        if not auth_manager or not auth_manager.is_enabled():
            # Populate dummy auth info when auth is disabled
            self._auth_info = {
                "user_id": "anonymous", 
                "permissions": ["read", "write", "admin", "admin:keys", "admin:users", "admin:config"], 
                "auth_method": "disabled"
            }
            return True

        # Public endpoints skip auth but still rate limit
        if auth_manager.is_public_endpoint(parsed.path):
            identifier = f"ip:{self.client_address[0]}"
            allowed, headers = auth_manager.check_rate_limit(identifier)
            self._rate_limit_headers = headers
            if not allowed:
                self._send_json(429, {"error": "rate_limit_exceeded"}, headers)
                return False
            return True

        # Authenticate
        request_headers = {k: v for k, v in self.headers.items()}
        auth_info, error = auth_manager.authenticate(request_headers)

        if not auth_info:
            audit_logger.warning(
                f"Auth failed: path={parsed.path}, ip={self.client_address[0]}, error={error}"
            )
            self._send_json(401, {"error": error})
            return False

        self._auth_info = auth_info

        # Check permissions for the endpoint
        if check_permissions:
            method = self.command  # GET, POST, DELETE, etc.
            required_perms = permission_manager.get_endpoint_permissions(parsed.path, method)
            user_perms = auth_info.get("permissions", [])

            if not permission_manager.check_permission(user_perms, required_perms):
                audit_logger.warning(
                    f"Permission denied: user={auth_info.get('user_id')}, "
                    f"path={parsed.path}, required={required_perms}, has={user_perms}"
                )
                self._send_json(403, {
                    "error": "forbidden",
                    "message": f"Required permissions: {', '.join(required_perms)}",
                })
                return False

        # Check rate limit
        identifier = self._get_client_identifier()
        allowed, headers = auth_manager.check_rate_limit(identifier)
        self._rate_limit_headers = headers

        if not allowed:
            audit_logger.warning(
                f"Rate limit exceeded: user={auth_info.get('user_id')}, identifier={identifier}"
            )
            self._send_json(429, {"error": "rate_limit_exceeded"}, headers)
            return False

        return True

    def _audit_log(self, action: str, details: Optional[dict] = None) -> None:
        """Log an authenticated action for audit."""
        auth_info = getattr(self, "_auth_info", None)
        user_id = auth_info.get("user_id", "anonymous") if auth_info else "anonymous"
        auth_method = auth_info.get("auth_method", "none") if auth_info else "none"
        ip = self.client_address[0]

        log_data = {
            "action": action,
            "user": user_id,
            "auth_method": auth_method,
            "ip": ip,
        }
        if details:
            log_data.update(details)

        audit_logger.info(json.dumps(log_data))

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)

        # Health check is always public
        if parsed.path == "/health":
            self._send_json(200, {"ok": True})
            return

        # Metrics endpoint for Prometheus scraping (public)
        if parsed.path == "/metrics":
            self._handle_metrics()
            return

        # Check auth and rate limit for all other endpoints
        if not self._check_auth_and_rate_limit():
            return

        if parsed.path == "/api/config":
            out = dict(self.server.bridge.config())
            out.update(self.server.router.config())
            auth_manager = getattr(self.server, "auth_manager", None)
            out["auth_enabled"] = auth_manager.is_enabled() if auth_manager else False
            self._send_json(200, out)
            return
        if parsed.path == "/api/orders_state":
            self._audit_log("list_orders_state")
            self._send_json(200, {"items": self.server.router.orders_state()})
            return
        if parsed.path == "/api/order_state":
            qs = parse_qs(parsed.query or "")
            cid = (qs.get("client_order_id") or [""])[0].strip()
            if not cid:
                self._send_json(400, {"error": "bad_params"})
                return
            st = self.server.router.order_state(cid)
            if not st:
                self._send_json(404, {"error": "not_found"})
                return
            self._audit_log("get_order_state", {"client_order_id": cid})
            self._send_json(200, st)
            return
        if parsed.path == "/api/execution/ping":
            self._send_json(200, self.server.router.ping())
            return
        if parsed.path == "/api/health":
            self._send_json(
                200,
                {"ok": True, "engine": {"connected": self.server.bridge.is_connected()}},
            )
            return
        if parsed.path == "/api/stream":
            self._handle_sse(parsed)
            return
        if parsed.path == "/api/market":
            self._send_json(200, self.server.bridge.market())
            return
        if parsed.path == "/api/orders":
            self._audit_log("list_orders")
            self._send_json(200, {"items": self.server.bridge.orders()})
            return
        if parsed.path == "/api/account":
            self._audit_log("get_account")
            self._send_json(200, self.server.router.account_state())
            return
        if parsed.path == "/api/auth/keys":
            self._handle_list_api_keys()
            return
        if parsed.path == "/api/auth/roles":
            self._handle_list_roles()
            return
        if parsed.path == "/api/audit/logs":
            self._handle_query_audit_logs()
            return
        if parsed.path == "/api/audit/stats":
            self._handle_get_audit_stats()
            return
        # Configuration endpoints
        if parsed.path == "/api/exchange-keys":
            self._handle_list_exchange_keys()
            return
        if parsed.path == "/api/settings":
            self._handle_get_settings()
            return
        if parsed.path == "/api/settings/export":
            self._handle_export_settings()
            return
            
        # SPA Routing: If path is not an API endpoint and file doesn't exist, serve index.html
        if not parsed.path.startswith("/api/") and not parsed.path.startswith("/metrics") and not parsed.path.startswith("/ws/"):
            # Check if it's a static file request
            # translate_path uses the configured directory
            path = self.translate_path(self.path)
            if not os.path.exists(path) or os.path.isdir(path):
                # Serve index.html for client-side routing
                # Manually serve index.html to avoid issues with modifying self.path
                index_path = os.path.join(Handler.static_dir, "index.html")
                if os.path.exists(index_path):
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html")
                    fs = os.stat(index_path)
                    self.send_header("Content-Length", str(fs.st_size))
                    self.end_headers()
                    with open(index_path, "rb") as f:
                        self.wfile.write(f.read())
                    return
        
        return super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)

        # Auth endpoints (login and refresh don't require prior auth)
        if parsed.path == "/api/auth/login":
            self._handle_login()
            return

        if parsed.path == "/api/auth/refresh":
            self._handle_refresh_token()
            return

        if parsed.path == "/api/auth/logout":
            self._handle_logout()
            return

        # Check auth and rate limit
        if not self._check_auth_and_rate_limit():
            return

        if parsed.path == "/api/cancel":
            self._handle_cancel()
            return
        if parsed.path == "/api/auth/keys":
            self._handle_create_api_key()
            return
        if parsed.path == "/api/auth/roles":
            self._handle_assign_role()
            return
        if parsed.path == "/api/audit/archive":
            self._handle_trigger_archive()
            return
        if parsed.path == "/api/config":
            self._handle_update_config()
            return
        # Configuration endpoints
        if parsed.path == "/api/exchange-keys":
            self._handle_create_exchange_key()
            return
        if parsed.path == "/api/exchange-keys/test":
            self._handle_test_exchange_connection()
            return
        if parsed.path == "/api/settings":
            self._handle_update_settings()
            return
        if parsed.path == "/api/settings/import":
            self._handle_import_settings()
            return
        if parsed.path != "/api/order":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        data = None
        ct = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        if ct == "application/json":
            try:
                data = json.loads(body) if body else {}
            except Exception:
                self._send_json(400, {"error": "bad_json"})
                return
        else:
            data = {k: v[0] for k, v in parse_qs(body).items()}

        try:
            side = str(data.get("side", "BUY")).upper()
            symbol = str(data.get("symbol", "BTCUSDT"))
            qty = float(data.get("qty", 0.0))
            price = float(data.get("price", 0.0))
            client_order_id = str(
                data.get("client_order_id", f"web-{int(time.time()*1000)}")
            )
        except Exception:
            self._send_json(400, {"error": "bad_params"})
            return

        if side not in ("BUY", "SELL") or qty <= 0.0 or price <= 0.0 or not symbol:
            self._send_json(400, {"error": "bad_params"})
            return

        try:
            r = self.server.router.place_order(
                side, symbol, qty, price, client_order_id
            )
        except RuntimeError as e:
            if str(e) == "binance_not_configured":
                self._send_json(400, {"error": "binance_not_configured"})
                return
            self._send_json(500, {"error": "execution_unavailable"})
            return

        self._audit_log("place_order", {
            "client_order_id": client_order_id,
            "symbol": symbol,
            "side": side,
            "qty": qty,
            "price": price,
        })
        self._send_json(200, r)

    def _handle_cancel(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        data = None
        ct = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        if ct == "application/json":
            try:
                data = json.loads(body) if body else {}
            except Exception:
                self._send_json(400, {"error": "bad_json"})
                return
        else:
            data = {k: v[0] for k, v in parse_qs(body).items()}

        client_order_id = str(data.get("client_order_id", "")).strip()
        if not client_order_id:
            self._send_json(400, {"error": "bad_params"})
            return
        symbol = str(data.get("symbol", "")).strip().upper() or None

        try:
            r = self.server.router.cancel_order(client_order_id, symbol=symbol)
        except RuntimeError as e:
            if str(e) == "binance_not_configured":
                self._send_json(400, {"error": "binance_not_configured"})
                return
            self._send_json(500, {"error": "execution_unavailable"})
            return

        self._audit_log("cancel_order", {"client_order_id": client_order_id, "symbol": symbol})
        self._send_json(200, r)

    def _handle_login(self):
        """Handle login request to get JWT tokens."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        user_id = str(data.get("user_id", "")).strip()
        password = str(data.get("password", ""))

        if not user_id or not password:
            self._send_json(400, {"error": "bad_params"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        tokens = auth_manager.create_token(user_id, password)
        if not tokens:
            audit_logger.warning(f"Login failed: user={user_id}, ip={self.client_address[0]}")
            self._send_json(401, {"error": "invalid_credentials"})
            return

        audit_logger.info(f"Login success: user={user_id}, ip={self.client_address[0]}")
        self._send_json(200, {
            "access_token": tokens["access_token"],
            "refresh_token": tokens["refresh_token"],
            "token_type": "Bearer",
            "expires_in": tokens["expires_in"],
        })

    def _handle_refresh_token(self):
        """Handle token refresh request."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        refresh_token = str(data.get("refresh_token", "")).strip()
        if not refresh_token:
            self._send_json(400, {"error": "bad_params", "message": "refresh_token is required"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        result = auth_manager.refresh_access_token(refresh_token)
        if not result:
            audit_logger.warning(f"Token refresh failed: ip={self.client_address[0]}")
            self._send_json(401, {"error": "invalid_refresh_token"})
            return

        audit_logger.info(f"Token refresh success: ip={self.client_address[0]}")
        self._send_json(200, {
            "access_token": result["access_token"],
            "token_type": "Bearer",
            "expires_in": result["expires_in"],
        })

    def _handle_logout(self):
        """Handle logout request to revoke refresh token."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        refresh_token = str(data.get("refresh_token", "")).strip()
        if not refresh_token:
            self._send_json(400, {"error": "bad_params", "message": "refresh_token is required"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        success = auth_manager.revoke_refresh_token(refresh_token)
        if success:
            audit_logger.info(f"Logout success: ip={self.client_address[0]}")
        self._send_json(200, {"ok": True})

    def _handle_create_api_key(self):
        """Handle API key creation."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        name = str(data.get("name", "")).strip()
        if not name:
            self._send_json(400, {"error": "bad_params", "message": "name is required"})
            return

        permissions = data.get("permissions")
        if permissions and not isinstance(permissions, list):
            self._send_json(400, {"error": "bad_params", "message": "permissions must be a list"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission to create keys
        user_permissions = auth_info.get("permissions", [])
        if "admin" not in user_permissions:
            self._send_json(403, {"error": "forbidden", "message": "admin permission required"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        user_id = auth_info.get("user_id", "unknown")
        key_id, raw_key = auth_manager.create_api_key(user_id, name, permissions)

        self._audit_log("create_api_key", {"key_id": key_id, "name": name})
        self._send_json(201, {
            "key_id": key_id,
            "api_key": raw_key,
            "message": "Store this key securely. It will not be shown again.",
        })

    def _handle_list_api_keys(self):
        """Handle listing API keys."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        # Admin can see all keys, others only their own
        user_permissions = auth_info.get("permissions", [])
        if "admin" in user_permissions:
            keys = auth_manager.list_api_keys()
        else:
            keys = auth_manager.list_api_keys(auth_info.get("user_id"))

        self._audit_log("list_api_keys")
        self._send_json(200, {"keys": keys})

    def do_DELETE(self):
        """Handle DELETE requests."""
        parsed = urlparse(self.path)

        if not self._check_auth_and_rate_limit():
            return

        if parsed.path == "/api/auth/keys":
            self._handle_revoke_api_key()
            return

        if parsed.path == "/api/auth/roles":
            self._handle_remove_role()
            return

        if parsed.path == "/api/exchange-keys":
            self._handle_delete_exchange_key()
            return

        self.send_error(404)

    def _handle_revoke_api_key(self):
        """Handle API key revocation."""
        qs = parse_qs(urlparse(self.path).query or "")
        key_id = (qs.get("key_id") or [""])[0].strip()

        if not key_id:
            self._send_json(400, {"error": "bad_params", "message": "key_id is required"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if "admin" not in user_permissions:
            self._send_json(403, {"error": "forbidden", "message": "admin permission required"})
            return

        auth_manager = getattr(self.server, "auth_manager", None)
        if not auth_manager:
            self._send_json(500, {"error": "auth_not_configured"})
            return

        success = auth_manager.revoke_api_key(key_id)
        if not success:
            self._send_json(404, {"error": "not_found"})
            return

        self._audit_log("revoke_api_key", {"key_id": key_id})
        self._send_json(200, {"ok": True, "key_id": key_id})

    # =========================================================================
    # Role Management Endpoints (RBAC)
    # =========================================================================

    def _handle_list_roles(self):
        """Handle listing user roles."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_USERS]):
            self._send_json(403, {"error": "forbidden", "message": "admin:users permission required"})
            return

        qs = parse_qs(urlparse(self.path).query or "")
        user_id = (qs.get("user_id") or [""])[0].strip()

        if user_id:
            # Get roles for specific user
            info = role_manager.get_user_role_info(user_id)
            if info:
                self._send_json(200, info)
            else:
                # Return default roles for users without explicit assignment
                self._send_json(200, {
                    "user_id": user_id,
                    "roles": role_manager.get_roles(user_id),
                    "is_default": True,
                })
        else:
            # List all user role assignments
            all_roles = role_manager.list_all_user_roles()
            metrics = role_manager.get_metrics()
            self._send_json(200, {
                "users": all_roles,
                "metrics": metrics,
                "available_roles": list(ROLE_PERMISSIONS.keys()),
            })

    def _handle_assign_role(self):
        """Handle role assignment to a user."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        user_id = str(data.get("user_id", "")).strip()
        role = str(data.get("role", "")).strip()
        roles = data.get("roles")  # For setting multiple roles at once

        if not user_id:
            self._send_json(400, {"error": "bad_params", "message": "user_id is required"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_USERS]):
            self._send_json(403, {"error": "forbidden", "message": "admin:users permission required"})
            return

        assigned_by = auth_info.get("user_id", "unknown")

        if roles is not None:
            # Set multiple roles at once
            if not isinstance(roles, list):
                self._send_json(400, {"error": "bad_params", "message": "roles must be a list"})
                return
            # Validate roles
            invalid_roles = [r for r in roles if r not in ROLE_PERMISSIONS]
            if invalid_roles:
                self._send_json(400, {
                    "error": "bad_params",
                    "message": f"Invalid roles: {invalid_roles}. Valid roles: {list(ROLE_PERMISSIONS.keys())}",
                })
                return
            role_manager.set_roles(user_id, roles, set_by=assigned_by)
            self._audit_log("set_roles", {"user_id": user_id, "roles": roles})
            self._send_json(200, {
                "ok": True,
                "user_id": user_id,
                "roles": role_manager.get_roles(user_id),
            })
        elif role:
            # Assign single role
            if role not in ROLE_PERMISSIONS:
                self._send_json(400, {
                    "error": "bad_params",
                    "message": f"Invalid role: {role}. Valid roles: {list(ROLE_PERMISSIONS.keys())}",
                })
                return
            result = role_manager.assign_role(user_id, role, assigned_by=assigned_by)
            if result:
                self._audit_log("assign_role", {"user_id": user_id, "role": role})
                self._send_json(200, {
                    "ok": True,
                    "user_id": user_id,
                    "role": role,
                    "roles": role_manager.get_roles(user_id),
                })
            else:
                self._send_json(200, {
                    "ok": False,
                    "message": "User already has this role",
                    "user_id": user_id,
                    "roles": role_manager.get_roles(user_id),
                })
        else:
            self._send_json(400, {"error": "bad_params", "message": "role or roles is required"})

    def _handle_remove_role(self):
        """Handle role removal from a user."""
        qs = parse_qs(urlparse(self.path).query or "")
        user_id = (qs.get("user_id") or [""])[0].strip()
        role = (qs.get("role") or [""])[0].strip()

        if not user_id:
            self._send_json(400, {"error": "bad_params", "message": "user_id is required"})
            return

        if not role:
            self._send_json(400, {"error": "bad_params", "message": "role is required"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_USERS]):
            self._send_json(403, {"error": "forbidden", "message": "admin:users permission required"})
            return

        removed_by = auth_info.get("user_id", "unknown")
        result = role_manager.remove_role(user_id, role, removed_by=removed_by)

        if result:
            self._audit_log("remove_role", {"user_id": user_id, "role": role})
            self._send_json(200, {
                "ok": True,
                "user_id": user_id,
                "role": role,
                "roles": role_manager.get_roles(user_id),
            })
        else:
            self._send_json(404, {
                "error": "not_found",
                "message": "User does not have this role",
            })

    # =========================================================================
    # Audit Log Query Endpoints
    # =========================================================================

    def _handle_query_audit_logs(self):
        """Handle querying audit logs with filters and pagination."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_CONFIG]):
            self._send_json(403, {"error": "forbidden", "message": "admin:config permission required"})
            return

        qs = parse_qs(urlparse(self.path).query or "")

        # Parse query parameters
        log_type = (qs.get("type") or [""])[0].strip() or None
        user_id = (qs.get("user_id") or [""])[0].strip() or None
        action = (qs.get("action") or [""])[0].strip() or None

        # Pagination parameters
        try:
            limit = int((qs.get("limit") or ["100"])[0])
            limit = min(max(1, limit), 1000)  # Clamp between 1 and 1000
        except ValueError:
            limit = 100

        try:
            offset = int((qs.get("offset") or ["0"])[0])
            offset = max(0, offset)
        except ValueError:
            offset = 0

        # Time range parameters
        start_time = None
        end_time = None
        try:
            start_str = (qs.get("start_time") or [""])[0].strip()
            if start_str:
                start_time = datetime.fromisoformat(start_str.replace("Z", "+00:00"))
        except ValueError:
            pass

        try:
            end_str = (qs.get("end_time") or [""])[0].strip()
            if end_str:
                end_time = datetime.fromisoformat(end_str.replace("Z", "+00:00"))
        except ValueError:
            pass

        # Get audit store
        audit_store = get_audit_store()
        if not audit_store:
            # Fall back to in-memory recent logs if store not initialized
            self._send_json(200, {
                "logs": [],
                "total": 0,
                "limit": limit,
                "offset": offset,
                "message": "Audit store not initialized",
            })
            return

        # Query logs
        if log_type:
            # Query specific log type from files
            logs = audit_store.query_logs(
                log_type=log_type,
                start_time=start_time,
                end_time=end_time,
                user_id=user_id,
                action=action,
                limit=limit + offset + 1,  # Get extra to check if more exist
            )
        else:
            # Get recent logs from memory (all types)
            logs = audit_store.get_recent_logs(
                log_type=None,
                limit=limit + offset + 1,
                user_id=user_id,
            )

        # Apply offset and limit
        total = len(logs)
        has_more = total > offset + limit
        logs = logs[offset:offset + limit]

        self._send_json(200, {
            "logs": logs,
            "total": total if total <= offset + limit else -1,  # -1 indicates more available
            "limit": limit,
            "offset": offset,
            "has_more": has_more,
        })

    def _handle_get_audit_stats(self):
        """Handle getting audit log statistics."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_CONFIG]):
            self._send_json(403, {"error": "forbidden", "message": "admin:config permission required"})
            return

        retention_manager = get_retention_manager()
        if not retention_manager:
            self._send_json(200, {
                "status": "not_initialized",
                "message": "Audit retention manager not initialized",
            })
            return

        status = retention_manager.get_retention_status()
        self._send_json(200, {
            "status": "active",
            **status,
        })

    def _handle_trigger_archive(self):
        """Handle manual trigger of audit log archival."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check if user has admin permission
        user_permissions = auth_info.get("permissions", [])
        if not permission_manager.check_permission(user_permissions, [Permission.ADMIN_CONFIG]):
            self._send_json(403, {"error": "forbidden", "message": "admin:config permission required"})
            return

        retention_manager = get_retention_manager()
        if not retention_manager:
            self._send_json(500, {
                "error": "not_initialized",
                "message": "Audit retention manager not initialized",
            })
            return

        # Run cleanup/archive
        try:
            stats = retention_manager.run_cleanup()
            self._audit_log("trigger_archive", stats)
            self._send_json(200, {
                "ok": True,
                "stats": stats,
            })
        except Exception as e:
            self._send_json(500, {
                "error": "archive_failed",
                "message": str(e),
            })

    def _handle_update_config(self):
        """Handle configuration update request."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        # Note: This endpoint saves configuration to a file
        # The gateway must be restarted for changes to take effect
        config_file = os.path.join(os.path.dirname(__file__), ".veloz_config.json")

        try:
            # Read existing config if it exists
            existing_config = {}
            if os.path.exists(config_file):
                with open(config_file, 'r') as f:
                    existing_config = json.load(f)

            # Update with new values
            existing_config.update(data)

            # Write updated config
            with open(config_file, 'w') as f:
                json.dump(existing_config, f, indent=2)

            self._audit_log("update_config", {"fields": list(data.keys())})
            self._send_json(200, {
                "ok": True,
                "message": "Configuration saved. Restart the gateway for changes to take effect.",
                "config_file": config_file,
            })
        except Exception as e:
            self._send_json(500, {
                "error": "config_save_failed",
                "message": str(e),
            })

    # =========================================================================
    # Exchange Key Management Endpoints (GUI Configuration)
    # =========================================================================

    def _handle_list_exchange_keys(self):
        """Handle listing exchange API keys."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        try:
            keychain = get_keychain_manager()
            keys = keychain.list_api_keys()
            self._audit_log("list_exchange_keys")
            self._send_json(200, {"keys": keys})
        except Exception as e:
            self._send_json(500, {"error": "keychain_error", "message": str(e)})

    def _handle_create_exchange_key(self):
        """Handle creating a new exchange API key."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        # Validate required fields
        exchange = str(data.get("exchange", "")).strip().lower()
        api_key = str(data.get("apiKey", "")).strip()
        api_secret = str(data.get("apiSecret", "")).strip()
        name = str(data.get("name", "Default")).strip()
        testnet = data.get("testnet", True)
        passphrase = data.get("passphrase")  # Required for OKX

        if not exchange or not api_key or not api_secret:
            self._send_json(400, {
                "error": "bad_params",
                "message": "exchange, apiKey, and apiSecret are required",
            })
            return

        # Validate exchange
        valid_exchanges = ["binance", "binance_futures", "okx", "bybit", "coinbase"]
        if exchange not in valid_exchanges:
            self._send_json(400, {
                "error": "bad_params",
                "message": f"Invalid exchange. Valid options: {valid_exchanges}",
            })
            return

        # OKX requires passphrase
        if exchange == "okx" and not passphrase:
            self._send_json(400, {
                "error": "bad_params",
                "message": "passphrase is required for OKX",
            })
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        try:
            keychain = get_keychain_manager()
            key_id = keychain.store_api_key(
                exchange=exchange,
                api_key=api_key,
                api_secret=api_secret,
                name=name,
                passphrase=passphrase,
                testnet=testnet,
            )

            if not key_id:
                self._send_json(500, {"error": "keychain_error", "message": "Failed to store key"})
                return

            self._audit_log("create_exchange_key", {"key_id": key_id, "exchange": exchange})

            # Update active credentials if applicable
            if exchange == "binance":
                router = getattr(self.server, "router", None)
                if router and router._execution_mode == "binance_testnet_spot":
                    if router._binance:
                        router._binance.update_credentials(api_key, api_secret)
                        audit_logger.info("Updated active Binance credentials")

            self._send_json(201, {
                "ok": True,
                "keyId": key_id,
                "message": "API key stored securely",
            })
        except Exception as e:
            self._send_json(500, {"error": "keychain_error", "message": str(e)})

    def _handle_delete_exchange_key(self):
        """Handle deleting an exchange API key."""
        qs = parse_qs(urlparse(self.path).query or "")
        key_id = (qs.get("key_id") or qs.get("keyId") or [""])[0].strip()

        if not key_id:
            self._send_json(400, {"error": "bad_params", "message": "key_id is required"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        try:
            keychain = get_keychain_manager()
            success = keychain.delete_api_key(key_id)

            if not success:
                self._send_json(404, {"error": "not_found"})
                return

            self._audit_log("delete_exchange_key", {"key_id": key_id})
            self._send_json(200, {"ok": True, "keyId": key_id})
        except Exception as e:
            self._send_json(500, {"error": "keychain_error", "message": str(e)})

    def _handle_test_exchange_connection(self):
        """Handle testing an exchange connection."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        # Can test by key_id (stored key) or by providing credentials directly
        key_id = str(data.get("keyId", "")).strip()
        exchange = str(data.get("exchange", "")).strip().lower()
        api_key = str(data.get("apiKey", "")).strip()
        api_secret = str(data.get("apiSecret", "")).strip()
        testnet = data.get("testnet", True)
        passphrase = data.get("passphrase")

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # If key_id provided, look up the stored key
        if key_id:
            try:
                keychain = get_keychain_manager()
                stored_key = keychain.get_api_key(key_id)
                if not stored_key:
                    self._send_json(404, {"error": "not_found", "message": "Key not found"})
                    return
                exchange = stored_key.exchange
                api_key = stored_key.api_key
                api_secret = stored_key.api_secret
                testnet = stored_key.testnet
                passphrase = stored_key.passphrase
            except Exception as e:
                self._send_json(500, {"error": "keychain_error", "message": str(e)})
                return
        elif not exchange or not api_key or not api_secret:
            self._send_json(400, {
                "error": "bad_params",
                "message": "Either keyId or (exchange, apiKey, apiSecret) required",
            })
            return

        # Run the connection test
        import asyncio
        try:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            result = loop.run_until_complete(
                test_exchange_connection(
                    exchange=exchange,
                    api_key=api_key,
                    api_secret=api_secret,
                    testnet=testnet,
                    passphrase=passphrase,
                )
            )
            loop.close()

            self._audit_log("test_exchange_connection", {
                "exchange": exchange,
                "success": result.success,
            })
            self._send_json(200, result.to_dict())
        except Exception as e:
            self._send_json(500, {"error": "test_failed", "message": str(e)})

    # =========================================================================
    # Settings Management Endpoints (GUI Configuration)
    # =========================================================================

    def _handle_get_settings(self):
        """Handle getting application settings."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        try:
            config_mgr = get_config_manager()
            settings = config_mgr.load()
            self._send_json(200, settings)
        except Exception as e:
            self._send_json(500, {"error": "config_error", "message": str(e)})

    def _handle_update_settings(self):
        """Handle updating application settings."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        try:
            config_mgr = get_config_manager()

            # If updating a specific section
            section = str(data.get("section", "")).strip()
            if section and "data" in data:
                settings = config_mgr.update(section, data["data"])
            else:
                # Full config update
                config_mgr.save(data)
                settings = config_mgr.load()

            self._audit_log("update_settings", {"sections": list(data.keys())})
            self._send_json(200, {
                "ok": True,
                "settings": settings,
            })
        except ValueError as e:
            self._send_json(400, {"error": "validation_error", "message": str(e)})
        except Exception as e:
            self._send_json(500, {"error": "config_error", "message": str(e)})

    def _handle_export_settings(self):
        """Handle exporting settings as JSON."""
        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        qs = parse_qs(urlparse(self.path).query or "")
        include_sensitive = (qs.get("include_sensitive") or ["false"])[0].lower() == "true"

        try:
            config_mgr = get_config_manager()
            export_data = config_mgr.export_config(include_sensitive=include_sensitive)

            self._audit_log("export_settings", {"include_sensitive": include_sensitive})

            # Return as downloadable JSON
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Disposition", "attachment; filename=veloz_config.json")
            self.send_header("Content-Length", str(len(export_data)))
            self.end_headers()
            self.wfile.write(export_data.encode("utf-8"))
        except Exception as e:
            self._send_json(500, {"error": "export_error", "message": str(e)})

    def _handle_import_settings(self):
        """Handle importing settings from JSON."""
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        try:
            data = json.loads(body) if body else {}
        except Exception:
            self._send_json(400, {"error": "bad_json"})
            return

        auth_info = getattr(self, "_auth_info", None)
        if not auth_info:
            self._send_json(401, {"error": "authentication_required"})
            return

        # Check for admin permission for import
        user_permissions = auth_info.get("permissions", [])
        if "admin" not in user_permissions:
            self._send_json(403, {"error": "forbidden", "message": "admin permission required"})
            return

        config_json = data.get("config")
        if not config_json:
            self._send_json(400, {"error": "bad_params", "message": "config field required"})
            return

        try:
            config_mgr = get_config_manager()

            # If config is a string, parse it
            if isinstance(config_json, str):
                success = config_mgr.import_config(config_json)
            else:
                # If it's already a dict, save directly
                config_mgr.save(config_json)
                success = True

            if not success:
                self._send_json(400, {"error": "import_error", "message": "Invalid configuration format"})
                return

            self._audit_log("import_settings")
            self._send_json(200, {
                "ok": True,
                "message": "Configuration imported successfully",
                "settings": config_mgr.load(),
            })
        except ValueError as e:
            self._send_json(400, {"error": "validation_error", "message": str(e)})
        except Exception as e:
            self._send_json(500, {"error": "import_error", "message": str(e)})

    def _handle_sse(self, parsed):
        qs = parse_qs(parsed.query or "")
        last_id = None
        try:
            if "last_id" in qs:
                last_id = int(qs["last_id"][0])
        except Exception:
            last_id = None

        try:
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()

            while True:
                ok = self.server.bridge.wait_for_event(last_id, timeout_s=10.0)
                if not ok:
                    self.wfile.write(b": keep-alive\n\n")
                    self.wfile.flush()
                    continue

                events, last_id = self.server.bridge.pop_events(last_id)
                for e in events:
                    eid = e.get("_id", 0)
                    payload = json.dumps({k: v for k, v in e.items() if k != "_id"})
                    msg = f"id: {eid}\nevent: {e.get('type','event')}\ndata: {payload}\n\n"
                    self.wfile.write(msg.encode("utf-8"))
                self.wfile.flush()
        except (ConnectionResetError, BrokenPipeError):
            # Normal client disconnect
            return
        except Exception as e:
            # Log other errors but don't crash
            audit_logger.error(f"SSE error: {e}")
            return

    def _send_json(self, status, obj, extra_headers: Optional[dict] = None):
        payload = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))

        # Add rate limit headers if available
        rate_limit_headers = getattr(self, "_rate_limit_headers", {})
        for key, value in rate_limit_headers.items():
            self.send_header(key, value)

        # Add any extra headers
        if extra_headers:
            for key, value in extra_headers.items():
                self.send_header(key, value)

        self.end_headers()
        self.wfile.write(payload)

    def _handle_metrics(self):
        """Handle Prometheus metrics endpoint."""
        metrics_manager = get_metrics_manager()

        # Update engine running status
        bridge = getattr(self.server, "bridge", None)
        if bridge:
            metrics_manager.set_engine_running(bridge.is_running())

        # Get metrics output
        metrics_output = metrics_manager.get_metrics()
        content_type = metrics_manager.get_content_type()

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(metrics_output)))
        self.end_headers()
        self.wfile.write(metrics_output)

    def log_message(self, format, *args):
        return


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    ui_dir = os.path.join(repo_root, "apps", "ui")
    ui_dist_dir = os.path.join(ui_dir, "dist")
    static_dir = ui_dir
    if os.path.exists(os.path.join(ui_dist_dir, "index.html")):
        static_dir = ui_dist_dir
    Handler.static_dir = static_dir
    audit_logger.info(f"Serving UI from {static_dir}")

    # Initialize metrics system
    metrics_manager = init_metrics()
    if metrics_available():
        audit_logger.info("Prometheus metrics enabled at /metrics")
    else:
        audit_logger.warning("prometheus_client not installed, metrics will be limited")

    # Initialize Vault client for secrets management
    vault_client = None
    vault_enabled = os.environ.get("VELOZ_VAULT_ENABLED", "false").lower() in ("true", "1", "yes")
    if vault_enabled:
        try:
            from vault_client import VaultClient, VaultConfig
            vault_config = VaultConfig(
                addr=os.environ.get("VAULT_ADDR", "http://127.0.0.1:8200"),
                auth_method=os.environ.get("VAULT_AUTH_METHOD", "token"),
                token=os.environ.get("VAULT_TOKEN", ""),
                role_id=os.environ.get("VAULT_ROLE_ID", ""),
                secret_id=os.environ.get("VAULT_SECRET_ID", ""),
                fallback_to_env=True,
            )
            vault_client = VaultClient(vault_config)
            if vault_client.connect():
                audit_logger.info("Connected to HashiCorp Vault for secrets management")
            else:
                audit_logger.warning("Failed to connect to Vault, falling back to environment variables")
                vault_client = None
        except ImportError:
            audit_logger.warning("Vault client not available, using environment variables")
        except Exception as e:
            audit_logger.warning(f"Vault initialization failed: {e}, using environment variables")

    preset = os.environ.get("VELOZ_PRESET", "dev")
    market_source = os.environ.get("VELOZ_MARKET_SOURCE", "sim").strip().lower()
    if market_source not in ("sim", "binance_rest"):
        market_source = "sim"
    market_symbol = (
        os.environ.get("VELOZ_MARKET_SYMBOL", "BTCUSDT").strip().upper() or "BTCUSDT"
    )
    binance_base_url = (
        os.environ.get("VELOZ_BINANCE_BASE_URL", "https://api.binance.com").strip()
        or "https://api.binance.com"
    )
    execution_mode = (
        os.environ.get("VELOZ_EXECUTION_MODE", "sim_engine").strip().lower()
    )
    if execution_mode not in ("sim_engine", "binance_testnet_spot"):
        execution_mode = "sim_engine"
    binance_trade_base_url = (
        os.environ.get(
            "VELOZ_BINANCE_TRADE_BASE_URL", "https://testnet.binance.vision"
        ).strip()
        or "https://testnet.binance.vision"
    )

    # Get Binance credentials from Vault or environment
    if vault_client:
        binance_api_key, binance_api_secret = vault_client.get_binance_credentials()
    else:
        binance_api_key = os.environ.get("VELOZ_BINANCE_API_KEY", "")
        binance_api_secret = os.environ.get("VELOZ_BINANCE_API_SECRET", "")

    # Auth configuration
    auth_enabled = os.environ.get("VELOZ_AUTH_ENABLED", "false").lower() in ("true", "1", "yes")
    # Get JWT secret from Vault or environment
    if vault_client:
        jwt_secret = vault_client.get_jwt_secret()
    else:
        jwt_secret = os.environ.get("VELOZ_JWT_SECRET", "")
    token_expiry = int(os.environ.get("VELOZ_TOKEN_EXPIRY", "3600"))
    rate_limit_capacity = int(os.environ.get("VELOZ_RATE_LIMIT_CAPACITY", "100"))
    rate_limit_refill = float(os.environ.get("VELOZ_RATE_LIMIT_REFILL", "10.0"))

    engine_path = os.path.join(
        repo_root, "build", preset, "apps", "engine", "veloz_engine"
    )
    if not os.path.exists(engine_path):
        raise SystemExit(f"engine not found: {engine_path}")

    order_store = OrderStore()
    account_store = AccountStore()
    bridge = EngineBridge(
        [engine_path, "--stdio"],
        market_source,
        market_symbol,
        binance_base_url,
        order_store,
        account_store,
    )
    binance_client = BinanceSpotRestClient(
        binance_trade_base_url, binance_api_key, binance_api_secret
    )
    router = ExecutionRouter(
        bridge=bridge,
        execution_mode=execution_mode,
        binance_client=binance_client,
        order_store=order_store,
        account_store=account_store,
    )

    # Get admin password from Vault or environment
    if vault_client:
        admin_password = vault_client.get_admin_password()
    else:
        admin_password = os.environ.get("VELOZ_ADMIN_PASSWORD", "")

    # Initialize auth manager
    auth_manager = AuthManager(
        jwt_secret=jwt_secret,
        token_expiry=token_expiry,
        rate_limit_capacity=rate_limit_capacity,
        rate_limit_refill=rate_limit_refill,
        auth_enabled=auth_enabled,
        admin_password=admin_password,
    )

    # Initialize security manager
    security_manager = None
    try:
        from security import SecurityConfig, SecurityManager
        security_config = SecurityConfig.from_env()
        security_manager = SecurityManager(security_config)
        if security_config.production_mode:
            audit_logger.info("Security hardening enabled (production mode)")
        else:
            audit_logger.info("Security module loaded (development mode)")
    except ImportError:
        audit_logger.warning("Security module not available")
    except Exception as e:
        audit_logger.warning(f"Security module initialization failed: {e}")

    host = os.environ.get("VELOZ_HOST", "0.0.0.0")
    port = int(os.environ.get("VELOZ_PORT", "8080"))

    httpd = ThreadingHTTPServer((host, port), Handler)
    httpd.bridge = bridge
    httpd.router = router
    httpd.auth_manager = auth_manager
    httpd.security_manager = security_manager

    if auth_enabled:
        audit_logger.info(f"Authentication enabled, rate limit: {rate_limit_capacity} req/min")
    else:
        audit_logger.info("Authentication disabled (set VELOZ_AUTH_ENABLED=true to enable)")

    display_host = host
    if host == "0.0.0.0":
        display_host = "127.0.0.1"

    audit_logger.info(f"Gateway listening on http://{host}:{port}")
    audit_logger.info(f"Open http://{display_host}:{port} in your browser to access the UI")
    try:
        httpd.serve_forever()
    finally:
        bridge.stop()
        # Cleanup Vault client
        if vault_client:
            vault_client.disconnect()


if __name__ == "__main__":
    main()
