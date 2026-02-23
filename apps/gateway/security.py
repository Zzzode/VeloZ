"""
VeloZ Security Hardening Module

Provides security controls for the gateway:
- CORS configuration
- Security headers
- IP filtering
- Request validation
- TLS configuration
"""

import ipaddress
import logging
import os
import re
from dataclasses import dataclass, field
from typing import Optional

logger = logging.getLogger("veloz.security")


@dataclass
class CORSConfig:
    """CORS configuration."""

    # Allowed origins (empty list = allow all, not recommended for production)
    allowed_origins: list[str] = field(default_factory=list)

    # Allowed methods
    allowed_methods: list[str] = field(
        default_factory=lambda: ["GET", "POST", "DELETE", "OPTIONS"]
    )

    # Allowed headers
    allowed_headers: list[str] = field(
        default_factory=lambda: ["Content-Type", "Authorization", "X-API-Key"]
    )

    # Allow credentials
    allow_credentials: bool = False

    # Max age for preflight cache (seconds)
    max_age: int = 86400

    def is_origin_allowed(self, origin: str) -> bool:
        """Check if an origin is allowed."""
        if not self.allowed_origins:
            return True  # Allow all if no restrictions

        # Exact match
        if origin in self.allowed_origins:
            return True

        # Pattern matching (supports wildcards like *.example.com)
        for allowed in self.allowed_origins:
            if allowed.startswith("*."):
                # Wildcard subdomain match
                domain = allowed[2:]
                if origin.endswith(domain) or origin == f"https://{domain}" or origin == f"http://{domain}":
                    return True
            elif allowed == "*":
                return True

        return False


@dataclass
class SecurityHeadersConfig:
    """Security headers configuration."""

    # Content-Type sniffing protection
    x_content_type_options: str = "nosniff"

    # Clickjacking protection
    x_frame_options: str = "DENY"

    # XSS protection (legacy, but still useful)
    x_xss_protection: str = "1; mode=block"

    # Content Security Policy
    content_security_policy: str = "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'"

    # HTTP Strict Transport Security (only sent over HTTPS)
    strict_transport_security: str = "max-age=31536000; includeSubDomains"

    # Referrer policy
    referrer_policy: str = "strict-origin-when-cross-origin"

    # Permissions policy (formerly Feature-Policy)
    permissions_policy: str = "geolocation=(), microphone=(), camera=()"

    # Cache control for sensitive responses
    cache_control: str = "no-store, no-cache, must-revalidate, private"

    def get_headers(self, is_https: bool = False) -> dict[str, str]:
        """Get all security headers as a dictionary."""
        headers = {
            "X-Content-Type-Options": self.x_content_type_options,
            "X-Frame-Options": self.x_frame_options,
            "X-XSS-Protection": self.x_xss_protection,
            "Content-Security-Policy": self.content_security_policy,
            "Referrer-Policy": self.referrer_policy,
            "Permissions-Policy": self.permissions_policy,
            "Cache-Control": self.cache_control,
        }

        # Only add HSTS over HTTPS
        if is_https:
            headers["Strict-Transport-Security"] = self.strict_transport_security

        return headers


@dataclass
class IPFilterConfig:
    """IP filtering configuration."""

    # IP whitelist (empty = allow all)
    whitelist: list[str] = field(default_factory=list)

    # IP blacklist
    blacklist: list[str] = field(default_factory=list)

    # Allow private/local IPs (127.0.0.1, 10.x.x.x, etc.)
    allow_private: bool = True

    def is_ip_allowed(self, ip_str: str) -> bool:
        """Check if an IP address is allowed."""
        try:
            ip = ipaddress.ip_address(ip_str)
        except ValueError:
            logger.warning("Invalid IP address: %s", ip_str)
            return False

        # Check blacklist first
        if self._ip_in_list(ip, self.blacklist):
            return False

        # Check private IP handling
        if ip.is_private and not self.allow_private:
            return False

        # If whitelist is empty, allow all (except blacklisted)
        if not self.whitelist:
            return True

        # Check whitelist
        return self._ip_in_list(ip, self.whitelist)

    def _ip_in_list(
        self, ip: ipaddress.IPv4Address | ipaddress.IPv6Address, ip_list: list[str]
    ) -> bool:
        """Check if IP is in a list (supports CIDR notation)."""
        for entry in ip_list:
            try:
                if "/" in entry:
                    # CIDR notation
                    network = ipaddress.ip_network(entry, strict=False)
                    if ip in network:
                        return True
                else:
                    # Single IP
                    if ip == ipaddress.ip_address(entry):
                        return True
            except ValueError:
                logger.warning("Invalid IP/CIDR in filter list: %s", entry)
                continue
        return False


@dataclass
class RequestValidationConfig:
    """Request validation configuration."""

    # Maximum request body size (bytes)
    max_body_size: int = 1024 * 1024  # 1MB default

    # Maximum URL length
    max_url_length: int = 2048

    # Maximum header size
    max_header_size: int = 8192

    # Maximum number of headers
    max_headers: int = 100

    # Allowed content types for POST/PUT
    allowed_content_types: list[str] = field(
        default_factory=lambda: ["application/json", "application/x-www-form-urlencoded"]
    )

    # Block suspicious patterns in URLs
    block_patterns: list[str] = field(
        default_factory=lambda: [
            r"\.\./",  # Path traversal
            r"<script",  # XSS attempt
            r"javascript:",  # JavaScript protocol
            r"data:",  # Data URI
            r"vbscript:",  # VBScript protocol
        ]
    )

    def validate_request(
        self,
        method: str,
        path: str,
        headers: dict[str, str],
        content_length: int = 0,
    ) -> tuple[bool, str]:
        """
        Validate a request.

        Returns:
            Tuple of (is_valid, error_message)
        """
        # Check URL length
        if len(path) > self.max_url_length:
            return False, "URL too long"

        # Check for suspicious patterns
        for pattern in self.block_patterns:
            if re.search(pattern, path, re.IGNORECASE):
                return False, "Suspicious URL pattern detected"

        # Check content length for POST/PUT
        if method in ("POST", "PUT", "PATCH"):
            if content_length > self.max_body_size:
                return False, f"Request body too large (max {self.max_body_size} bytes)"

            # Check content type
            content_type = headers.get("Content-Type", "").split(";")[0].strip()
            if content_type and content_type not in self.allowed_content_types:
                return False, f"Content type not allowed: {content_type}"

        # Check header count
        if len(headers) > self.max_headers:
            return False, "Too many headers"

        return True, ""


@dataclass
class TLSConfig:
    """TLS configuration."""

    # Enable TLS
    enabled: bool = False

    # Certificate file path
    cert_file: str = ""

    # Private key file path
    key_file: str = ""

    # CA certificate for client verification (mTLS)
    ca_file: str = ""

    # Require client certificate (mTLS)
    require_client_cert: bool = False

    # Minimum TLS version
    min_version: str = "TLSv1.2"

    # Cipher suites (empty = use defaults)
    ciphers: str = ""


@dataclass
class SecurityConfig:
    """Complete security configuration."""

    cors: CORSConfig = field(default_factory=CORSConfig)
    headers: SecurityHeadersConfig = field(default_factory=SecurityHeadersConfig)
    ip_filter: IPFilterConfig = field(default_factory=IPFilterConfig)
    request_validation: RequestValidationConfig = field(
        default_factory=RequestValidationConfig
    )
    tls: TLSConfig = field(default_factory=TLSConfig)

    # Production mode (stricter defaults)
    production_mode: bool = False

    @classmethod
    def from_env(cls) -> "SecurityConfig":
        """Create configuration from environment variables."""
        config = cls()

        # Production mode
        config.production_mode = os.environ.get(
            "VELOZ_PRODUCTION_MODE", "false"
        ).lower() in ("true", "1", "yes")

        # CORS
        allowed_origins = os.environ.get("VELOZ_CORS_ORIGINS", "")
        if allowed_origins:
            config.cors.allowed_origins = [
                o.strip() for o in allowed_origins.split(",") if o.strip()
            ]
        elif config.production_mode:
            # In production, don't allow all origins by default
            config.cors.allowed_origins = []

        config.cors.allow_credentials = os.environ.get(
            "VELOZ_CORS_CREDENTIALS", "false"
        ).lower() in ("true", "1", "yes")

        # IP filtering
        whitelist = os.environ.get("VELOZ_IP_WHITELIST", "")
        if whitelist:
            config.ip_filter.whitelist = [
                ip.strip() for ip in whitelist.split(",") if ip.strip()
            ]

        blacklist = os.environ.get("VELOZ_IP_BLACKLIST", "")
        if blacklist:
            config.ip_filter.blacklist = [
                ip.strip() for ip in blacklist.split(",") if ip.strip()
            ]

        config.ip_filter.allow_private = os.environ.get(
            "VELOZ_ALLOW_PRIVATE_IPS", "true"
        ).lower() in ("true", "1", "yes")

        # Request validation
        max_body = os.environ.get("VELOZ_MAX_BODY_SIZE", "")
        if max_body:
            config.request_validation.max_body_size = int(max_body)

        # TLS
        config.tls.enabled = os.environ.get("VELOZ_TLS_ENABLED", "false").lower() in (
            "true",
            "1",
            "yes",
        )
        config.tls.cert_file = os.environ.get("VELOZ_TLS_CERT", "")
        config.tls.key_file = os.environ.get("VELOZ_TLS_KEY", "")
        config.tls.ca_file = os.environ.get("VELOZ_TLS_CA", "")
        config.tls.require_client_cert = os.environ.get(
            "VELOZ_TLS_CLIENT_CERT", "false"
        ).lower() in ("true", "1", "yes")

        return config


class SecurityManager:
    """
    Manages security controls for the gateway.

    Usage:
        security = SecurityManager(SecurityConfig.from_env())

        # In request handler:
        if not security.check_ip(client_ip):
            return 403, "Forbidden"

        if not security.validate_request(method, path, headers, content_length):
            return 400, "Bad Request"

        # Add security headers to response
        for name, value in security.get_response_headers(origin, is_https).items():
            response.add_header(name, value)
    """

    def __init__(self, config: Optional[SecurityConfig] = None):
        """Initialize security manager."""
        self._config = config or SecurityConfig.from_env()
        logger.info(
            "Security manager initialized (production_mode=%s)",
            self._config.production_mode,
        )

    @property
    def config(self) -> SecurityConfig:
        """Get the security configuration."""
        return self._config

    def check_ip(self, ip: str) -> bool:
        """Check if an IP address is allowed."""
        return self._config.ip_filter.is_ip_allowed(ip)

    def validate_request(
        self,
        method: str,
        path: str,
        headers: dict[str, str],
        content_length: int = 0,
    ) -> tuple[bool, str]:
        """Validate a request."""
        return self._config.request_validation.validate_request(
            method, path, headers, content_length
        )

    def is_origin_allowed(self, origin: str) -> bool:
        """Check if a CORS origin is allowed."""
        return self._config.cors.is_origin_allowed(origin)

    def get_cors_headers(self, origin: str, method: str = "GET") -> dict[str, str]:
        """Get CORS headers for a response."""
        headers = {}

        if not origin:
            return headers

        if self._config.cors.is_origin_allowed(origin):
            # Use the actual origin instead of * when credentials are allowed
            if self._config.cors.allow_credentials:
                headers["Access-Control-Allow-Origin"] = origin
                headers["Access-Control-Allow-Credentials"] = "true"
            else:
                # If all origins allowed and no credentials, can use *
                if not self._config.cors.allowed_origins:
                    headers["Access-Control-Allow-Origin"] = "*"
                else:
                    headers["Access-Control-Allow-Origin"] = origin

            headers["Access-Control-Allow-Methods"] = ", ".join(
                self._config.cors.allowed_methods
            )
            headers["Access-Control-Allow-Headers"] = ", ".join(
                self._config.cors.allowed_headers
            )

            if method == "OPTIONS":
                headers["Access-Control-Max-Age"] = str(self._config.cors.max_age)

        return headers

    def get_security_headers(self, is_https: bool = False) -> dict[str, str]:
        """Get security headers for a response."""
        return self._config.headers.get_headers(is_https)

    def get_response_headers(
        self, origin: str = "", is_https: bool = False, method: str = "GET"
    ) -> dict[str, str]:
        """Get all response headers (CORS + security)."""
        headers = {}
        headers.update(self.get_security_headers(is_https))
        headers.update(self.get_cors_headers(origin, method))
        return headers


# Module-level singleton
_security_manager: Optional[SecurityManager] = None


def get_security_manager() -> SecurityManager:
    """Get or create the global security manager."""
    global _security_manager
    if _security_manager is None:
        _security_manager = SecurityManager()
    return _security_manager


def init_security(config: Optional[SecurityConfig] = None) -> SecurityManager:
    """Initialize the global security manager with configuration."""
    global _security_manager
    _security_manager = SecurityManager(config)
    return _security_manager
