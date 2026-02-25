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

The gateway supports comprehensive authentication and authorization:

### JWT Authentication

1. **Access Tokens** - Short-lived (1 hour by default)
   - Obtained via `POST /api/auth/login`
   - Used for API requests via `Authorization: Bearer <token>`
   - Automatically expire after configured duration (default: 3600 seconds / 1 hour)
   - Expiry configurable via `VELOZ_TOKEN_EXPIRY` environment variable

2. **Refresh Tokens** - Long-lived (7 days)
   - Obtained alongside access tokens
   - Used to get new access tokens via `POST /api/auth/refresh`
   - Can be revoked via `POST /api/auth/logout`
   - Background cleanup removes expired tokens

3. **Token Revocation**
   - Logout revokes refresh token immediately
   - Revoked tokens cannot be used to obtain new access tokens
   - Background thread cleans up expired tokens every hour

### API Keys

Created via `POST /api/auth/keys` (requires admin role):
- Persistent authentication without expiry
- Permission-based access control
- Secure storage (hashed with bcrypt)
- Can be revoked individually

Enable authentication by setting `VELOZ_AUTH_ENABLED=true`.

### RBAC (Role-Based Access Control)

Three built-in roles with hierarchical permissions:

- **viewer**: Read-only access to all GET endpoints
- **trader**: Viewer + trading operations (place/cancel orders)
- **admin**: Trader + user management, API key management, system configuration

Roles can be assigned via `POST /api/auth/users/{user_id}/roles`.

## Rate Limiting

Token bucket rate limiting is enabled when authentication is active:

| Header | Description |
|--------|-------------|
| `X-RateLimit-Limit` | Maximum requests per window |
| `X-RateLimit-Remaining` | Remaining requests |
| `X-RateLimit-Reset` | Unix timestamp when limit resets |

## Audit Logging

Comprehensive audit logging system with configurable retention policies.

### Logged Events

- **Authentication** - Login/logout attempts and results
- **Token operations** - Refresh token usage and revocation
- **API key management** - Creation and revocation of API keys
- **Permission denials** - Access control violations
- **Order operations** - All trading activity
- **Errors** - System errors and failures

### Retention Policies

Configurable retention periods by log type:
- **Auth logs**: 90 days (archive before delete)
- **Order logs**: 365 days (archive before delete)
- **API key logs**: 365 days (archive before delete)
- **Error logs**: 30 days (archive before delete)
- **Access logs**: 14 days (no archive)

### Features

- **NDJSON format** - Newline-delimited JSON for easy parsing
- **Automatic archiving** - Gzip compression before deletion
- **Background cleanup** - Scheduled cleanup jobs (default: every 24 hours)
- **Query API** - Search and retrieve audit logs
  - `GET /api/audit/logs` - Query logs with filters
  - `GET /api/audit/stats` - Get retention statistics
  - `POST /api/audit/archive` - Manually trigger archiving

### Storage

- **Active logs**: `/var/log/veloz/audit/*.ndjson`
- **Archives**: `/var/log/veloz/audit/archive/*.ndjson.gz`

Enable audit logging with `VELOZ_AUDIT_LOG_ENABLED=true`.

## Quick Links

- [Getting Started](../guides/user/getting-started.md) - Quick start guide with API examples
- [Design Documents](../design) - System architecture and design details
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns used in VeloZ
