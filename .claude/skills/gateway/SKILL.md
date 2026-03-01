---
name: gateway
description: Guides the C++ gateway's HTTP/SSE API, authentication, middleware, and engine bridge. Invoke when modifying apps/gateway or API behavior.
---

# Gateway (C++)

## Purpose

Use this skill for changes in apps/gateway or API behavior.

## Architecture

The gateway is a C++ HTTP service built with KJ async I/O:

- **HTTP Server**: KJ HttpService with Router for request dispatch
- **Middleware Chain**: Metrics → CORS → Auth → Rate Limit → Audit
- **Handlers**: Health, Auth, Order, Market, Account, Config, SSE, Metrics
- **Engine Bridge**: Direct in-process integration (no subprocess)
- **Event Broadcaster**: SSE streaming to multiple clients

## Key Components

| Component | Location | Purpose |
|-----------|----------|---------|
| GatewayServer | `src/gateway_server.h` | HTTP service entry point |
| Router | `src/router.h` | Route matching and middleware chain |
| EngineBridge | `src/bridge/engine_bridge.h` | Engine integration |
| EventBroadcaster | `src/bridge/event_broadcaster.h` | SSE event distribution |
| AuthManager | `src/auth/auth_manager.h` | JWT + API Key authentication |
| AuditLogger | `src/audit/audit_logger.h` | Structured audit logging |

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Simple health check |
| `/api/health` | GET | Detailed health status |
| `/api/auth/login` | POST | JWT token creation |
| `/api/auth/refresh` | POST | Refresh access token |
| `/api/auth/keys` | GET/POST/DELETE | API key management |
| `/api/orders` | GET/POST | Order management |
| `/api/orders/{id}` | GET/DELETE | Single order operations |
| `/api/cancel` | POST | Bulk cancel |
| `/api/market` | GET | Market data |
| `/api/account` | GET | Account information |
| `/api/config` | GET/POST | Configuration |
| `/api/stream` | GET | SSE event stream |
| `/metrics` | GET | Prometheus metrics |

## Environment Variables

### Server Configuration
- `VELOZ_HOST` (default: 0.0.0.0)
- `VELOZ_PORT` (default: 8080)
- `VELOZ_STATIC_DIR` (default: ./apps/ui)

### Authentication
- `VELOZ_AUTH_ENABLED` (default: true)
- `VELOZ_JWT_SECRET` (required in production)
- `VELOZ_JWT_ACCESS_EXPIRY` (default: 3600)
- `VELOZ_JWT_REFRESH_EXPIRY` (default: 604800)
- `VELOZ_ADMIN_PASSWORD` (required for admin login)

### Rate Limiting
- `VELOZ_RATE_LIMIT_CAPACITY` (default: 100)
- `VELOZ_RATE_LIMIT_REFILL` (default: 10.0)

### CORS
- `VELOZ_CORS_ORIGIN` (default: *)

### Audit Logging
- `VELOZ_AUDIT_LOG_DIR` (default: /var/log/veloz/audit)

## Testing

Tests use KJ Test framework:

```bash
# Run unit tests
./build/dev/apps/gateway/veloz_gateway_tests

# Run integration tests
./build/dev/apps/gateway/veloz_gateway_integration_tests

# Run with CTest
ctest --preset dev -R gateway
```

## Build

```bash
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

Executable: `build/dev/apps/gateway/veloz_gateway`