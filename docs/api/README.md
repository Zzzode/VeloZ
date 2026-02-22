# API Documentation

This directory contains API reference documentation for VeloZ.

## Documents

### Gateway API
- [HTTP API Reference](http_api.md) - REST API endpoints, authentication, rate limiting
- [SSE API](sse_api.md) - Server-Sent Events stream for real-time updates
- [Engine Protocol](engine_protocol.md) - Engine stdio command format (ORDER/CANCEL) and NDJSON events

### Backtest API
- [Backtest API Reference](backtest_api.md) - Backtest configuration, optimization, and reporting

## Authentication

The gateway supports optional authentication (disabled by default):

1. **JWT Bearer Token** - Obtained via `POST /api/auth/login`
   - Access tokens (short-lived, default 1 hour)
   - Refresh tokens (long-lived, default 7 days)
   - Token refresh via `POST /api/auth/refresh`
   - Logout/revocation via `POST /api/auth/logout`
2. **API Key** - Created via `POST /api/auth/keys` (requires admin)
   - Persistent authentication without expiry
   - Permission-based access control
   - Secure storage (hashed)

Enable authentication by setting `VELOZ_AUTH_ENABLED=true`.

### Permission Levels

- **read**: Read-only access to all GET endpoints
- **write**: Read + write access (POST/DELETE endpoints)
- **admin**: Full access including user/key management

## Rate Limiting

Token bucket rate limiting is enabled when authentication is active:

| Header | Description |
|--------|-------------|
| `X-RateLimit-Limit` | Maximum requests per window |
| `X-RateLimit-Remaining` | Remaining requests |
| `X-RateLimit-Reset` | Unix timestamp when limit resets |

## Audit Logging

When authentication is enabled, security events are automatically logged:

- **Login/logout events** - Authentication attempts and results
- **Token operations** - Refresh token usage and revocation
- **API key management** - Creation and revocation of API keys
- **Permission denials** - Access control violations

**Retention policies** vary by log type:
- Auth logs: 90 days
- Order logs: 365 days
- API key logs: 365 days
- Access logs: 14 days

Enable audit logging with `VELOZ_AUDIT_LOG_ENABLED=true`.

## Quick Links

- [Getting Started](../guides/user/getting-started.md) - Quick start guide with API examples
- [Design Documents](../design) - System architecture and design details
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns used in VeloZ
