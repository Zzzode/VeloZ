"""
Tests for VeloZ Security Module
"""

import os
import unittest
from unittest.mock import patch

from security import (
    CORSConfig,
    IPFilterConfig,
    RequestValidationConfig,
    SecurityConfig,
    SecurityHeadersConfig,
    SecurityManager,
    TLSConfig,
    get_security_manager,
    init_security,
)


class TestCORSConfig(unittest.TestCase):
    """Test CORS configuration."""

    def test_allow_all_origins_when_empty(self):
        """Test that empty allowed_origins allows all."""
        config = CORSConfig(allowed_origins=[])
        self.assertTrue(config.is_origin_allowed("https://example.com"))
        self.assertTrue(config.is_origin_allowed("https://malicious.com"))

    def test_exact_origin_match(self):
        """Test exact origin matching."""
        config = CORSConfig(allowed_origins=["https://example.com"])
        self.assertTrue(config.is_origin_allowed("https://example.com"))
        self.assertFalse(config.is_origin_allowed("https://other.com"))

    def test_wildcard_subdomain_match(self):
        """Test wildcard subdomain matching."""
        config = CORSConfig(allowed_origins=["*.example.com"])
        self.assertTrue(config.is_origin_allowed("https://sub.example.com"))
        self.assertTrue(config.is_origin_allowed("https://api.example.com"))
        self.assertFalse(config.is_origin_allowed("https://example.org"))

    def test_wildcard_all(self):
        """Test wildcard * allows all."""
        config = CORSConfig(allowed_origins=["*"])
        self.assertTrue(config.is_origin_allowed("https://any.com"))

    def test_multiple_origins(self):
        """Test multiple allowed origins."""
        config = CORSConfig(
            allowed_origins=["https://app.example.com", "https://admin.example.com"]
        )
        self.assertTrue(config.is_origin_allowed("https://app.example.com"))
        self.assertTrue(config.is_origin_allowed("https://admin.example.com"))
        self.assertFalse(config.is_origin_allowed("https://other.example.com"))


class TestIPFilterConfig(unittest.TestCase):
    """Test IP filtering configuration."""

    def test_allow_all_when_no_whitelist(self):
        """Test that empty whitelist allows all IPs."""
        config = IPFilterConfig(whitelist=[], blacklist=[])
        self.assertTrue(config.is_ip_allowed("1.2.3.4"))
        self.assertTrue(config.is_ip_allowed("192.168.1.1"))

    def test_whitelist_exact_match(self):
        """Test whitelist with exact IP match."""
        config = IPFilterConfig(whitelist=["1.2.3.4", "5.6.7.8"])
        self.assertTrue(config.is_ip_allowed("1.2.3.4"))
        self.assertTrue(config.is_ip_allowed("5.6.7.8"))
        self.assertFalse(config.is_ip_allowed("9.10.11.12"))

    def test_whitelist_cidr(self):
        """Test whitelist with CIDR notation."""
        config = IPFilterConfig(whitelist=["10.0.0.0/8"])
        self.assertTrue(config.is_ip_allowed("10.0.0.1"))
        self.assertTrue(config.is_ip_allowed("10.255.255.255"))
        self.assertFalse(config.is_ip_allowed("11.0.0.1"))

    def test_blacklist_takes_precedence(self):
        """Test that blacklist takes precedence over whitelist."""
        config = IPFilterConfig(
            whitelist=["10.0.0.0/8"], blacklist=["10.0.0.1"]
        )
        self.assertFalse(config.is_ip_allowed("10.0.0.1"))
        self.assertTrue(config.is_ip_allowed("10.0.0.2"))

    def test_private_ip_blocking(self):
        """Test blocking private IPs."""
        config = IPFilterConfig(allow_private=False)
        self.assertFalse(config.is_ip_allowed("192.168.1.1"))
        self.assertFalse(config.is_ip_allowed("10.0.0.1"))
        self.assertFalse(config.is_ip_allowed("127.0.0.1"))
        self.assertTrue(config.is_ip_allowed("8.8.8.8"))

    def test_invalid_ip_rejected(self):
        """Test that invalid IPs are rejected."""
        config = IPFilterConfig()
        self.assertFalse(config.is_ip_allowed("not-an-ip"))
        self.assertFalse(config.is_ip_allowed(""))


class TestRequestValidationConfig(unittest.TestCase):
    """Test request validation configuration."""

    def test_valid_request(self):
        """Test valid request passes validation."""
        config = RequestValidationConfig()
        valid, error = config.validate_request(
            method="GET",
            path="/api/orders",
            headers={"Content-Type": "application/json"},
            content_length=0,
        )
        self.assertTrue(valid)
        self.assertEqual(error, "")

    def test_url_too_long(self):
        """Test URL length validation."""
        config = RequestValidationConfig(max_url_length=100)
        valid, error = config.validate_request(
            method="GET",
            path="/" + "a" * 200,
            headers={},
            content_length=0,
        )
        self.assertFalse(valid)
        self.assertIn("URL too long", error)

    def test_body_too_large(self):
        """Test body size validation."""
        config = RequestValidationConfig(max_body_size=1000)
        valid, error = config.validate_request(
            method="POST",
            path="/api/orders",
            headers={"Content-Type": "application/json"},
            content_length=2000,
        )
        self.assertFalse(valid)
        self.assertIn("too large", error)

    def test_path_traversal_blocked(self):
        """Test path traversal is blocked."""
        config = RequestValidationConfig()
        valid, error = config.validate_request(
            method="GET",
            path="/api/../../../etc/passwd",
            headers={},
            content_length=0,
        )
        self.assertFalse(valid)
        self.assertIn("Suspicious", error)

    def test_xss_attempt_blocked(self):
        """Test XSS attempts are blocked."""
        config = RequestValidationConfig()
        valid, error = config.validate_request(
            method="GET",
            path="/api/search?q=<script>alert(1)</script>",
            headers={},
            content_length=0,
        )
        self.assertFalse(valid)
        self.assertIn("Suspicious", error)

    def test_invalid_content_type(self):
        """Test invalid content type is rejected."""
        config = RequestValidationConfig()
        valid, error = config.validate_request(
            method="POST",
            path="/api/orders",
            headers={"Content-Type": "text/html"},
            content_length=100,
        )
        self.assertFalse(valid)
        self.assertIn("Content type not allowed", error)

    def test_too_many_headers(self):
        """Test header count validation."""
        config = RequestValidationConfig(max_headers=5)
        headers = {f"Header-{i}": f"value-{i}" for i in range(10)}
        valid, error = config.validate_request(
            method="GET",
            path="/api/orders",
            headers=headers,
            content_length=0,
        )
        self.assertFalse(valid)
        self.assertIn("Too many headers", error)


class TestSecurityHeadersConfig(unittest.TestCase):
    """Test security headers configuration."""

    def test_default_headers(self):
        """Test default security headers."""
        config = SecurityHeadersConfig()
        headers = config.get_headers(is_https=False)

        self.assertEqual(headers["X-Content-Type-Options"], "nosniff")
        self.assertEqual(headers["X-Frame-Options"], "DENY")
        self.assertIn("X-XSS-Protection", headers)
        self.assertIn("Content-Security-Policy", headers)
        self.assertIn("Referrer-Policy", headers)

    def test_hsts_only_on_https(self):
        """Test HSTS header only added for HTTPS."""
        config = SecurityHeadersConfig()

        # HTTP - no HSTS
        headers_http = config.get_headers(is_https=False)
        self.assertNotIn("Strict-Transport-Security", headers_http)

        # HTTPS - has HSTS
        headers_https = config.get_headers(is_https=True)
        self.assertIn("Strict-Transport-Security", headers_https)


class TestSecurityManager(unittest.TestCase):
    """Test SecurityManager class."""

    def setUp(self):
        """Set up test fixtures."""
        self.config = SecurityConfig(
            cors=CORSConfig(allowed_origins=["https://app.example.com"]),
            ip_filter=IPFilterConfig(blacklist=["1.2.3.4"]),
        )
        self.manager = SecurityManager(self.config)

    def test_check_ip(self):
        """Test IP checking."""
        self.assertTrue(self.manager.check_ip("5.6.7.8"))
        self.assertFalse(self.manager.check_ip("1.2.3.4"))

    def test_validate_request(self):
        """Test request validation."""
        valid, error = self.manager.validate_request(
            method="GET",
            path="/api/orders",
            headers={},
            content_length=0,
        )
        self.assertTrue(valid)

    def test_is_origin_allowed(self):
        """Test origin checking."""
        self.assertTrue(self.manager.is_origin_allowed("https://app.example.com"))
        self.assertFalse(self.manager.is_origin_allowed("https://other.com"))

    def test_get_cors_headers(self):
        """Test CORS header generation."""
        headers = self.manager.get_cors_headers("https://app.example.com")
        self.assertEqual(
            headers["Access-Control-Allow-Origin"], "https://app.example.com"
        )
        self.assertIn("Access-Control-Allow-Methods", headers)

    def test_get_cors_headers_disallowed_origin(self):
        """Test CORS headers for disallowed origin."""
        headers = self.manager.get_cors_headers("https://malicious.com")
        self.assertNotIn("Access-Control-Allow-Origin", headers)

    def test_get_security_headers(self):
        """Test security header generation."""
        headers = self.manager.get_security_headers(is_https=True)
        self.assertIn("X-Content-Type-Options", headers)
        self.assertIn("Strict-Transport-Security", headers)

    def test_get_response_headers(self):
        """Test combined response headers."""
        headers = self.manager.get_response_headers(
            origin="https://app.example.com", is_https=True
        )
        # Should have both CORS and security headers
        self.assertIn("Access-Control-Allow-Origin", headers)
        self.assertIn("X-Content-Type-Options", headers)


class TestSecurityConfigFromEnv(unittest.TestCase):
    """Test SecurityConfig.from_env()."""

    def test_production_mode(self):
        """Test production mode from environment."""
        with patch.dict(os.environ, {"VELOZ_PRODUCTION_MODE": "true"}):
            config = SecurityConfig.from_env()
            self.assertTrue(config.production_mode)

    def test_cors_origins_from_env(self):
        """Test CORS origins from environment."""
        with patch.dict(
            os.environ,
            {"VELOZ_CORS_ORIGINS": "https://app.example.com,https://admin.example.com"},
        ):
            config = SecurityConfig.from_env()
            self.assertIn("https://app.example.com", config.cors.allowed_origins)
            self.assertIn("https://admin.example.com", config.cors.allowed_origins)

    def test_ip_whitelist_from_env(self):
        """Test IP whitelist from environment."""
        with patch.dict(os.environ, {"VELOZ_IP_WHITELIST": "10.0.0.0/8,192.168.1.1"}):
            config = SecurityConfig.from_env()
            self.assertIn("10.0.0.0/8", config.ip_filter.whitelist)
            self.assertIn("192.168.1.1", config.ip_filter.whitelist)

    def test_ip_blacklist_from_env(self):
        """Test IP blacklist from environment."""
        with patch.dict(os.environ, {"VELOZ_IP_BLACKLIST": "1.2.3.4,5.6.7.8"}):
            config = SecurityConfig.from_env()
            self.assertIn("1.2.3.4", config.ip_filter.blacklist)
            self.assertIn("5.6.7.8", config.ip_filter.blacklist)

    def test_tls_config_from_env(self):
        """Test TLS configuration from environment."""
        with patch.dict(
            os.environ,
            {
                "VELOZ_TLS_ENABLED": "true",
                "VELOZ_TLS_CERT": "/path/to/cert.pem",
                "VELOZ_TLS_KEY": "/path/to/key.pem",
            },
        ):
            config = SecurityConfig.from_env()
            self.assertTrue(config.tls.enabled)
            self.assertEqual(config.tls.cert_file, "/path/to/cert.pem")
            self.assertEqual(config.tls.key_file, "/path/to/key.pem")


class TestSecurityManagerSingleton(unittest.TestCase):
    """Test module-level singleton functions."""

    def test_get_security_manager(self):
        """Test singleton access."""
        manager1 = get_security_manager()
        manager2 = get_security_manager()
        self.assertIs(manager1, manager2)

    def test_init_security(self):
        """Test initialization with custom config."""
        config = SecurityConfig(production_mode=True)
        manager = init_security(config)
        self.assertTrue(manager.config.production_mode)


if __name__ == "__main__":
    unittest.main()
