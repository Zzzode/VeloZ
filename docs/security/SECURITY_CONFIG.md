# VeloZ Security Configuration Guide

This document describes all security-related configuration options for VeloZ.

## Environment Variables

### Production Mode

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_PRODUCTION_MODE` | `false` | Enable production security defaults |

When `VELOZ_PRODUCTION_MODE=true`:
- CORS is restricted (no wildcard origins)
- Verbose errors are disabled
- Stricter validation is applied

### CORS Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_CORS_ORIGINS` | `*` (dev) / empty (prod) | Comma-separated list of allowed origins |
| `VELOZ_CORS_CREDENTIALS` | `false` | Allow credentials in CORS requests |

Examples:
```bash
# Allow specific origins
export VELOZ_CORS_ORIGINS="https://app.example.com,https://admin.example.com"

# Allow all subdomains
export VELOZ_CORS_ORIGINS="*.example.com"

# Allow credentials (requires specific origins, not *)
export VELOZ_CORS_CREDENTIALS=true
```

### IP Filtering

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_IP_WHITELIST` | empty | Comma-separated list of allowed IPs/CIDRs |
| `VELOZ_IP_BLACKLIST` | empty | Comma-separated list of blocked IPs/CIDRs |
| `VELOZ_ALLOW_PRIVATE_IPS` | `true` | Allow private IP ranges (10.x, 192.168.x, etc.) |

Examples:
```bash
# Only allow specific IPs
export VELOZ_IP_WHITELIST="10.0.0.0/8,192.168.1.100"

# Block specific IPs
export VELOZ_IP_BLACKLIST="1.2.3.4,5.6.7.8"

# Block private IPs (for public-facing deployments)
export VELOZ_ALLOW_PRIVATE_IPS=false
```

### Request Validation

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_MAX_BODY_SIZE` | `1048576` (1MB) | Maximum request body size in bytes |

### TLS Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_TLS_ENABLED` | `false` | Enable TLS/HTTPS |
| `VELOZ_TLS_CERT` | empty | Path to TLS certificate file |
| `VELOZ_TLS_KEY` | empty | Path to TLS private key file |
| `VELOZ_TLS_CA` | empty | Path to CA certificate (for mTLS) |
| `VELOZ_TLS_CLIENT_CERT` | `false` | Require client certificate (mTLS) |

Example:
```bash
export VELOZ_TLS_ENABLED=true
export VELOZ_TLS_CERT=/etc/veloz/tls/server.crt
export VELOZ_TLS_KEY=/etc/veloz/tls/server.key
```

### Authentication

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_AUTH_ENABLED` | `false` | Enable authentication |
| `VELOZ_JWT_SECRET` | auto-generated | JWT signing secret |
| `VELOZ_TOKEN_EXPIRY` | `3600` | JWT token expiry in seconds |
| `VELOZ_ADMIN_PASSWORD` | empty | Admin password for initial setup |

### Rate Limiting

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_RATE_LIMIT_CAPACITY` | `100` | Max requests per window |
| `VELOZ_RATE_LIMIT_REFILL` | `10.0` | Requests added per second |

### Secrets Management (Vault)

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_VAULT_ENABLED` | `false` | Enable HashiCorp Vault |
| `VAULT_ADDR` | `http://127.0.0.1:8200` | Vault server address |
| `VAULT_AUTH_METHOD` | `token` | Auth method: `token` or `approle` |
| `VAULT_TOKEN` | empty | Vault token (for token auth) |
| `VAULT_ROLE_ID` | empty | AppRole role ID |
| `VAULT_SECRET_ID` | empty | AppRole secret ID |

## Security Headers

The following security headers are automatically added to all responses:

| Header | Value | Purpose |
|--------|-------|---------|
| `X-Content-Type-Options` | `nosniff` | Prevent MIME sniffing |
| `X-Frame-Options` | `DENY` | Prevent clickjacking |
| `X-XSS-Protection` | `1; mode=block` | XSS filter (legacy) |
| `Content-Security-Policy` | `default-src 'self'...` | Content restrictions |
| `Referrer-Policy` | `strict-origin-when-cross-origin` | Referrer control |
| `Permissions-Policy` | `geolocation=()...` | Feature restrictions |
| `Cache-Control` | `no-store, no-cache...` | Prevent caching |
| `Strict-Transport-Security` | `max-age=31536000...` | HTTPS enforcement (HTTPS only) |

## Production Checklist

Before deploying to production:

- [ ] Set `VELOZ_PRODUCTION_MODE=true`
- [ ] Configure specific CORS origins (not `*`)
- [ ] Enable TLS with valid certificates
- [ ] Set strong `VELOZ_JWT_SECRET`
- [ ] Enable Vault for secrets management
- [ ] Configure IP whitelist if applicable
- [ ] Set appropriate rate limits
- [ ] Enable authentication (`VELOZ_AUTH_ENABLED=true`)
- [ ] Review and set `VELOZ_MAX_BODY_SIZE`

## Example Production Configuration

```bash
# Production mode
export VELOZ_PRODUCTION_MODE=true

# CORS
export VELOZ_CORS_ORIGINS="https://app.veloz.io,https://admin.veloz.io"
export VELOZ_CORS_CREDENTIALS=true

# TLS
export VELOZ_TLS_ENABLED=true
export VELOZ_TLS_CERT=/etc/veloz/tls/server.crt
export VELOZ_TLS_KEY=/etc/veloz/tls/server.key

# Authentication
export VELOZ_AUTH_ENABLED=true
export VELOZ_TOKEN_EXPIRY=1800

# Vault
export VELOZ_VAULT_ENABLED=true
export VAULT_ADDR=https://vault.internal:8200
export VAULT_AUTH_METHOD=approle
export VAULT_ROLE_ID=<role-id>
export VAULT_SECRET_ID=<secret-id>

# Rate limiting
export VELOZ_RATE_LIMIT_CAPACITY=60
export VELOZ_RATE_LIMIT_REFILL=1.0

# Request limits
export VELOZ_MAX_BODY_SIZE=102400
```

## Monitoring Security Events

Security events are logged to the audit log. Monitor for:

- Failed authentication attempts
- Rate limit violations
- Blocked IP addresses
- Invalid request patterns
- CORS violations

See `docs/security/INCIDENT_RESPONSE.md` for incident handling procedures.
