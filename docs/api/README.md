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
2. **API Key** - Created via `POST /api/auth/keys` (requires admin)

Enable authentication by setting `VELOZ_AUTH_ENABLED=true`.

## Rate Limiting

Token bucket rate limiting is enabled when authentication is active:

| Header | Description |
|--------|-------------|
| `X-RateLimit-Limit` | Maximum requests per window |
| `X-RateLimit-Remaining` | Remaining requests |
| `X-RateLimit-Reset` | Unix timestamp when limit resets |

## Quick Links

- [Getting Started](../guides/user/getting-started.md) - Quick start guide with API examples
- [Design Documents](../design) - System architecture and design details
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns used in VeloZ
