# HTTP API Reference

This document describes the HTTP API endpoints available in VeloZ Gateway.

## Base URL

```
http://127.0.0.1:8080
```

---

## Authentication

The gateway supports optional authentication via JWT tokens or API keys. Authentication is disabled by default and can be enabled via the `VELOZ_AUTH_ENABLED` environment variable.

### Authentication Methods

1. **JWT Bearer Token** - Obtained via login endpoint
2. **API Key** - Created via API key management endpoints

### Headers

```
Authorization: Bearer <jwt_token>
```

or

```
X-API-Key: <api_key>
```

### Public Endpoints

The following endpoints do not require authentication:
- `GET /health`
- `GET /api/stream`

### POST /api/auth/login

Authenticate and obtain JWT tokens (access token and refresh token).

**Request Body (JSON):**
```json
{
  "user_id": "admin",
  "password": "your_password"
}
```

**Success Response:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Fields:**
| Field | Type | Description |
|-------|------|-------------|
| `access_token` | string | Short-lived JWT token for API access (default: 1 hour) |
| `refresh_token` | string | Long-lived token for obtaining new access tokens (default: 7 days) |
| `token_type` | string | Always "Bearer" |
| `expires_in` | int | Access token expiry in seconds |

**Error Response (401):**
```json
{
  "error": "invalid_credentials"
}
```

### POST /api/auth/refresh

Refresh an expired access token using a refresh token.

**Request Body (JSON):**
```json
{
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Success Response:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Error Response (400):**
```json
{
  "error": "bad_params",
  "message": "refresh_token is required"
}
```

**Error Response (401):**
```json
{
  "error": "invalid_refresh_token"
}
```

**Notes:**
- Refresh tokens are valid for 7 days by default
- Revoked refresh tokens will be rejected
- Refresh tokens are automatically cleaned up after expiry

### POST /api/auth/logout

Logout and revoke the refresh token to prevent further use.

**Request Body (JSON):**
```json
{
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Success Response:**
```json
{
  "ok": true,
  "message": "Logged out successfully"
}
```

**Error Response (400):**
```json
{
  "error": "bad_params",
  "message": "refresh_token is required"
}
```

**Notes:**
- Logout revokes the refresh token, preventing it from being used to obtain new access tokens
- Existing access tokens remain valid until they expire
- For complete logout, clients should discard both access and refresh tokens

### GET /api/auth/keys

List API keys. Admin users see all keys, others see only their own.

**Response:**
```json
{
  "keys": [
    {
      "key_id": "vk_abc123",
      "user_id": "admin",
      "name": "my-key",
      "created_at": 1704067200,
      "last_used_at": 1704067300,
      "revoked": false,
      "permissions": ["read", "write"]
    }
  ]
}
```

### POST /api/auth/keys

Create a new API key. Requires admin permission.

**Request Body (JSON):**
```json
{
  "name": "my-api-key",
  "permissions": ["read", "write"]
}
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | Yes | Human-readable name for the API key |
| `permissions` | array | No | List of permissions (default: `["read", "write"]`) |

**Available Permissions:**
| Permission | Description |
|------------|-------------|
| `read` | Read-only access to all GET endpoints |
| `write` | Write access to POST/DELETE endpoints |
| `admin` | Full administrative access including key management |

**Success Response (201):**
```json
{
  "key_id": "vk_abc123",
  "api_key": "veloz_abc123...",
  "message": "Store this key securely. It will not be shown again."
}
```

**Error Response (400):**
```json
{
  "error": "bad_params",
  "message": "permissions must be a list"
}
```

**Error Response (403):**
```json
{
  "error": "forbidden",
  "message": "admin permission required"
}
```

**Notes:**
- The raw API key is only returned once during creation
- API keys are stored as hashes for security
- Requires `admin` permission in the authenticated user's token or API key

### DELETE /api/auth/keys

Revoke an API key. Requires admin permission.

**Query Parameters:**
- `key_id` (required) - The API key ID to revoke

**Success Response:**
```json
{
  "ok": true,
  "key_id": "vk_abc123"
}
```

**Error Response (403):**
```json
{
  "error": "forbidden",
  "message": "admin permission required"
}
```

**Error Response (404):**
```json
{
  "error": "not_found"
}
```

**Notes:**
- Requires `admin` permission
- Revoked keys are immediately invalidated
- Audit logs record all key revocations

### POST /api/auth/roles

Update a user's roles. Requires admin permission.

**Request Body (JSON):**
```json
{
  "user_id": "trader1",
  "roles": ["viewer", "trader"]
}
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `user_id` | string | Yes | User ID to update |
| `roles` | array | Yes | List of roles to assign |

**Success Response:**
```json
{
  "ok": true,
  "user_id": "trader1",
  "roles": ["viewer", "trader"]
}
```

**Error Response (403):**
```json
{
  "error": "forbidden",
  "message": "admin permission required"
}
```

### DELETE /api/auth/roles

Remove a user's roles. Requires admin permission.

**Query Parameters:**
- `user_id` (required) - The user ID
- `roles` (optional) - Specific roles to remove (comma-separated). If not provided, all roles are removed.

**Success Response:**
```json
{
  "ok": true,
  "user_id": "trader1",
  "removed_roles": ["viewer", "trader"]
}
```

---

## Exchange Keys Management

Manage exchange API keys for connecting to trading venues.

### GET /api/exchange-keys

List all configured exchange API keys.

**Response:**
```json
{
  "keys": [
    {
      "exchange": "binance",
      "name": "my-binance-key",
      "created_at": 1704067200,
      "has_secret": true,
      "permissions": ["spot", "futures"]
    }
  ]
}
```

### POST /api/exchange-keys

Add or update an exchange API key.

**Request Body (JSON):**
```json
{
  "exchange": "binance",
  "api_key": "your_api_key",
  "api_secret": "your_api_secret",
  "name": "my-binance-key",
  "permissions": ["spot", "futures"]
}
```

**Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `exchange` | string | Yes | Exchange name: `binance`, `okx`, `bybit` |
| `api_key` | string | Yes | API key from the exchange |
| `api_secret` | string | Yes | API secret from the exchange |
| `name` | string | No | Human-readable name |
| `permissions` | array | No | Trading permissions |

**Success Response:**
```json
{
  "ok": true,
  "exchange": "binance",
  "name": "my-binance-key"
}
```

### DELETE /api/exchange-keys

Delete an exchange API key.

**Query Parameters:**
- `exchange` (required) - The exchange name

**Success Response:**
```json
{
  "ok": true,
  "exchange": "binance"
}
```

### POST /api/exchange-keys/test

Test exchange API key connectivity.

**Request Body (JSON):**
```json
{
  "exchange": "binance"
}
```

**Success Response:**
```json
{
  "ok": true,
  "exchange": "binance",
  "status": "connected",
  "server_time": 1704067200000
}
```

**Error Response:**
```json
{
  "ok": false,
  "exchange": "binance",
  "error": "invalid_api_key"
}
```

---

## Settings Management

Manage gateway settings and configuration.

### GET /api/settings

Get current gateway settings.

**Response:**
```json
{
  "settings": {
    "market_source": "sim",
    "market_symbol": "BTCUSDT",
    "execution_mode": "sim_engine",
    "auth_enabled": false,
    "audit_enabled": true
  }
}
```

### POST /api/settings

Update gateway settings.

**Request Body (JSON):**
```json
{
  "market_source": "binance_rest",
  "market_symbol": "ETHUSDT",
  "execution_mode": "binance_testnet_spot"
}
```

**Success Response:**
```json
{
  "ok": true,
  "settings": {
    "market_source": "binance_rest",
    "market_symbol": "ETHUSDT",
    "execution_mode": "binance_testnet_spot"
  }
}
```

### GET /api/settings/export

Export current settings as a downloadable JSON file.

**Response:**
```json
{
  "version": "1.0",
  "exported_at": 1704067200,
  "settings": {
    "market_source": "sim",
    "market_symbol": "BTCUSDT",
    "execution_mode": "sim_engine"
  },
  "exchange_keys": [
    {
      "exchange": "binance",
      "name": "my-binance-key"
    }
  ]
}
```

### POST /api/settings/import

Import settings from a JSON file.

**Request Body (JSON):**
```json
{
  "version": "1.0",
  "settings": {
    "market_source": "sim",
    "market_symbol": "BTCUSDT"
  },
  "import_exchange_keys": false
}
```

**Success Response:**
```json
{
  "ok": true,
  "imported": {
    "settings": true,
    "exchange_keys": false
  }
}
```

---

## Permission-Based Access Control

When authentication is enabled, all endpoints (except public ones) require proper permissions.

### Permission Model

VeloZ uses a role-based permission system with granular permissions.

| Permission | Access Level | Description |
|------------|--------------|-------------|
| `read:market` | Read-only | Access to market data endpoints |
| `read:orders` | Read-only | Access to order endpoints |
| `read:account` | Read-only | Access to account endpoints |
| `read:config` | Read-only | Access to configuration endpoints |
| `write:orders` | Write | Access to place orders |
| `write:cancel` | Write | Access to cancel orders |
| `admin:keys` | Admin | API key management |
| `admin:users` | Admin | User and role management |
| `admin:config` | Admin | System configuration |

**Built-in Roles:**
- **viewer**: read:market, read:config
- **trader**: read:market, read:orders, read:account, read:config, write:orders, write:cancel
- **admin**: All permissions

**Legacy Permissions:**
For backward compatibility, the following legacy permissions are expanded:
- `read` → read:market, read:orders, read:account, read:config
- `write` → write:orders, write:cancel
- `admin` → admin:keys, admin:users, admin:config

### Permission Enforcement

**Public Endpoints** (no authentication required):
- `GET /health`
- `GET /api/stream` (SSE)

**Read Permission Required:**
- `GET /api/config`
- `GET /api/market`
- `GET /api/orders`
- `GET /api/orders_state`
- `GET /api/order_state`
- `GET /api/account`
- `GET /api/execution/ping`
- `GET /api/auth/keys` (users see only their own keys)
- `GET /api/settings`
- `GET /api/settings/export`
- `GET /api/exchange-keys`

**Write Permission Required:**
- `POST /api/order`
- `POST /api/cancel`
- `POST /api/settings`

**Admin Permission Required:**
- `POST /api/auth/keys` (create API keys)
- `DELETE /api/auth/keys` (revoke API keys)
- `GET /api/auth/keys` (view all keys)
- `POST /api/auth/roles` (update user roles)
- `DELETE /api/auth/roles` (delete user roles)
- `GET /api/audit/logs`
- `GET /api/audit/stats`
- `POST /api/audit/archive`
- `POST /api/settings/import`
- `POST /api/exchange-keys`
- `DELETE /api/exchange-keys`
- `POST /api/exchange-keys/test`

### Permission Checking

Permissions are checked in the following order:

1. **JWT Token**: Permissions embedded in the token payload
2. **API Key**: Permissions associated with the API key

**Example JWT Payload:**
```json
{
  "sub": "admin",
  "iat": 1704067200,
  "exp": 1704070800,
  "type": "access",
  "permissions": ["read", "write", "admin"]
}
```

**Example API Key Permissions:**
```json
{
  "key_id": "vk_abc123",
  "permissions": ["read", "write"]
}
```

### Permission Errors

**403 Forbidden** - Insufficient permissions:
```json
{
  "error": "forbidden",
  "message": "admin permission required"
}
```

**401 Unauthorized** - Authentication required:
```json
{
  "error": "authentication_required"
}
```

---

## Rate Limiting

The gateway implements token bucket rate limiting. Rate limit headers are included in all responses when authentication is enabled.

### Rate Limit Headers

| Header | Description |
|--------|-------------|
| `X-RateLimit-Limit` | Maximum requests per window |
| `X-RateLimit-Remaining` | Remaining requests in current window |
| `X-RateLimit-Reset` | Unix timestamp when the limit resets |
| `Retry-After` | Seconds to wait before retrying (only on 429) |

### Rate Limit Response (429)

```json
{
  "error": "rate_limit_exceeded"
}
```

---

## Health Check

### GET /health

Check if the gateway is running.

**Response:**
```json
{
  "ok": true
}
```

---

## Engine Service Mode Endpoints

When the engine runs in service mode (`--service`), these endpoints are available directly on the engine (default port 8080). These are separate from the gateway endpoints.

### GET /api/status

Get engine status and lifecycle state.

**Response:**
```json
{
  "state": "Running",
  "uptime_ms": 12345,
  "strategies_loaded": 3,
  "strategies_running": 2
}
```

**Lifecycle States:**
| State | Description |
|-------|-------------|
| `Starting` | Engine is starting up |
| `Running` | Engine is running normally |
| `Stopping` | Engine is shutting down gracefully |
| `Stopped` | Engine has stopped |

### POST /api/start

Start the engine.

**Response:**
```json
{
  "ok": true,
  "message": "Engine started"
}
```

### POST /api/stop

Stop the engine gracefully.

**Response:**
```json
{
  "ok": true,
  "message": "Engine stopping"
}
```

### GET /api/strategies

List all loaded strategies.

**Response:**
```json
{
  "strategies": [
    {
      "id": "ma_crossover_1",
      "name": "MA Crossover",
      "type": "TrendFollowing",
      "state": "Running",
      "pnl": 1250.50,
      "trade_count": 42
    }
  ]
}
```

### GET /api/strategies/{id}

Get detailed state of a specific strategy.

**Response:**
```json
{
  "id": "ma_crossover_1",
  "name": "MA Crossover",
  "type": "TrendFollowing",
  "state": "Running",
  "is_running": true,
  "pnl": 1250.50,
  "max_drawdown": 150.25,
  "trade_count": 42,
  "win_count": 28,
  "lose_count": 14,
  "win_rate": 0.667,
  "profit_factor": 2.1
}
```

**Error Response (404):**
```json
{
  "error": "Strategy not found"
}
```

### POST /api/strategies/{id}/start

Start a specific strategy.

**Response:**
```json
{
  "ok": true,
  "message": "Strategy started"
}
```

### POST /api/strategies/{id}/stop

Stop a specific strategy.

**Response:**
```json
{
  "ok": true,
  "message": "Strategy stopped"
}
```

---

## Configuration

### GET /api/config

Get the current configuration of the gateway.

**Response:**
```json
{
  "market_source": "sim|binance_rest",
  "market_symbol": "BTCUSDT",
  "binance_base_url": "https://api.binance.com",
  "execution_mode": "sim_engine|binance_testnet_spot",
  "binance_trade_enabled": false,
  "binance_trade_base_url": "https://testnet.binance.vision",
  "binance_user_stream_connected": false
}
```

---

## Market Data

### GET /api/market

Get the latest market data.

**Response:**
```json
{
  "symbol": "BTCUSDT",
  "price": 50000.0,
  "ts_ns": 1704067200000000000
}
```

---

## Orders

### GET /api/orders

Get recent order events (engine events, not full state).

**Response:**
```json
{
  "items": [
    {
      "type": "order_update",
      "ts_ns": 1704067200000000000,
      "client_order_id": "web-1234567890",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "qty": 0.001,
      "price": 50000.0,
      "status": "FILLED"
    }
  ]
}
```

### GET /api/orders_state

Get the full state of all orders (aggregated state from OrderStore).

**Response:**
```json
{
  "items": [
    {
      "client_order_id": "web-1234567890",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "order_qty": 0.001,
      "limit_price": 50000.0,
      "venue_order_id": "123456",
      "status": "FILLED",
      "executed_qty": 0.001,
      "avg_price": 50000.0,
      "last_ts_ns": 1704067200000000000
    }
  ]
}
```

### GET /api/order_state

Get the state of a specific order by `client_order_id`.

**Query Parameters:**
- `client_order_id` (required) - The client order ID

**Response:**
```json
{
  "client_order_id": "web-1234567890",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "order_qty": 0.001,
  "limit_price": 50000.0,
  "venue_order_id": "123456",
  "status": "FILLED",
  "executed_qty": 0.001,
  "avg_price": 50000.0,
  "last_ts_ns": 1704067200000000000
}
```

**Error Response (404):**
```json
{
  "error": "not_found"
}
```

### POST /api/order

Place a new order.

**Request Body (JSON):**
```json
{
  "side": "BUY|SELL",
  "symbol": "BTCUSDT",
  "qty": 0.001,
  "price": 50000.0,
  "client_order_id": "web-1234567890"
}
```

**Parameters:**
- `side` (required) - Order side: "BUY" or "SELL"
- `symbol` (required) - Trading symbol, e.g., "BTCUSDT"
- `qty` (required) - Order quantity (must be > 0)
- `price` (required) - Limit price (must be > 0)
- `client_order_id` (optional) - Client-defined order ID. If not provided, a default is generated.

**Success Response:**
```json
{
  "ok": true,
  "client_order_id": "web-1234567890",
  "venue_order_id": "123456"
}
```

**Error Response (400):**
```json
{
  "error": "bad_params"
}
```

**Error Response (Binance not configured):**
```json
{
  "error": "binance_not_configured"
}
```

### POST /api/cancel

Cancel an existing order.

**Request Body (JSON):**
```json
{
  "client_order_id": "web-1234567890",
  "symbol": "BTCUSDT"
}
```

**Parameters:**
- `client_order_id` (required) - The client order ID to cancel
- `symbol` (optional) - Trading symbol (required for Binance execution mode)

**Success Response:**
```json
{
  "ok": true,
  "client_order_id": "web-1234567890"
}
```

**Error Response (400):**
```json
{
  "error": "bad_params"
}
```

---

## Account

### GET /api/account

Get the account balance state.

**Response:**
```json
{
  "ts_ns": 1704067200000000000,
  "balances": [
    {
      "asset": "USDT",
      "free": 10000.0,
      "locked": 0.0
    },
    {
      "asset": "BTC",
      "free": 0.5,
      "locked": 0.0
    }
  ]
}
```

---

## Execution

### GET /api/execution/ping

Ping the execution router to check connectivity.

**Response:**
```json
{
  "ok": true,
  "result": {}
}
```

**Response (Binance ping):**
```json
{
  "ok": true,
  "result": {}
}
```

**Error Response:**
```json
{
  "ok": false,
  "error": "binance_not_configured"
}
```

---

## Server-Sent Events (SSE)

### GET /api/stream

Subscribe to real-time event stream via Server-Sent Events.

**Query Parameters:**
- `last_id` (optional) - Resume from event ID (used for reconnection)

**Response:** `text/event-stream`

**Event Types:**

1. **market** - Market data update
   ```
   id: 1
   event: market
   data: {"symbol":"BTCUSDT","price":50000.0,"ts_ns":1704067200000000000}
   ```

2. **order_update** - Order state update
   ```
   id: 2
   event: order_update
   data: {"client_order_id":"web-1234567890","status":"FILLED","symbol":"BTCUSDT"}
   ```

3. **fill** - Trade execution fill
   ```
   id: 3
   event: fill
   data: {"client_order_id":"web-1234567890","symbol":"BTCUSDT","qty":0.001,"price":50000.0}
   ```

4. **error** - Error event
   ```
   id: 4
   event: error
   data: {"ts_ns":1704067200000000000,"message":"market_source_error"}
   ```

5. **account** - Account balance update
   ```
   id: 5
   event: account
   data: {"ts_ns":1704067200000000000}
   ```

**Keep-Alive:** The server sends a `: keep-alive` message every 10 seconds if no events occur.

**Reconnection:** Use the `last_id` query parameter to resume from the last received event.

---

## CORS Support

All endpoints support CORS with the following headers:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET,POST,DELETE,OPTIONS
Access-Control-Allow-Headers: Content-Type,Authorization,X-API-Key
```

---

## Environment Variables

The gateway behavior can be configured via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset (dev/release/asan) |
| `VELOZ_MARKET_SOURCE` | `sim` | Market data source: `sim` or `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol |
| `VELOZ_BINANCE_BASE_URL` | `https://api.binance.com` | Binance REST API base URL (for market data) |
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode: `sim_engine` or `binance_testnet_spot` |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://testnet.binance.vision` | Binance trade API base URL |
| `VELOZ_BINANCE_API_KEY` | (empty) | Binance API key |
| `VELOZ_BINANCE_API_SECRET` | (empty) | Binance API secret |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://testnet.binance.vision/ws` | Binance WebSocket base URL |
| `VELOZ_AUTH_ENABLED` | `false` | Enable authentication (`true`/`false`) |
| `VELOZ_JWT_SECRET` | (auto-generated) | JWT signing secret |
| `VELOZ_TOKEN_EXPIRY` | `3600` | JWT token expiry in seconds |
| `VELOZ_ADMIN_PASSWORD` | (empty) | Admin password for login |
| `VELOZ_RATE_LIMIT_CAPACITY` | `100` | Rate limit bucket capacity |
| `VELOZ_RATE_LIMIT_REFILL` | `10.0` | Rate limit refill rate (tokens/second) |
| `VELOZ_AUDIT_LOG_ENABLED` | `true` | Enable audit logging (when auth is enabled) |
| `VELOZ_AUDIT_LOG_FILE` | (stderr) | Audit log file path (default: stderr) |
| `VELOZ_AUDIT_LOG_RETENTION_DAYS` | `90` | Audit log retention period in days |

---

## Audit Logging

When authentication is enabled, the gateway automatically logs security-relevant events to the audit log.

### Audit Log Format

Audit logs use a structured format with timestamp and event details:

```
2026-02-21 10:30:45 [AUDIT] Login success: user=admin, ip=127.0.0.1
2026-02-21 10:31:12 [AUDIT] API key created: key_id=vk_abc123, user=admin, name=my-key
2026-02-21 10:32:05 [AUDIT] Token refresh success: ip=127.0.0.1
2026-02-21 10:35:20 [AUDIT] API key revoked: key_id=vk_abc123
2026-02-21 10:40:15 [AUDIT] Login failed: user=admin, ip=192.168.1.100
```

### Audited Events

| Event | Description |
|-------|-------------|
| Login success | Successful authentication via `/api/auth/login` |
| Login failed | Failed authentication attempt |
| Token refresh success | Successful token refresh via `/api/auth/refresh` |
| Token refresh failed | Failed token refresh attempt |
| Logout | User logout via `/api/auth/logout` |
| API key created | New API key created |
| API key revoked | API key revoked |
| Permission denied | Access denied due to insufficient permissions |

### Audit Log Retention

Audit logs are automatically managed based on retention policies per log type:

| Log Type | Default Retention | Archive Before Delete |
|----------|-------------------|----------------------|
| `auth` | 90 days | Yes |
| `order` | 365 days | Yes |
| `api_key` | 365 days | Yes |
| `error` | 30 days | Yes |
| `access` | 14 days | No |
| `default` | 30 days | Yes |

**Retention behavior:**
- **Automatic cleanup**: Logs beyond retention period are archived (if enabled) then deleted
- **Archive compression**: Old logs are compressed with gzip before deletion
- **Archive retention**: Archives are kept for 2x the retention period
- **Configurable**: Set default via `VELOZ_AUDIT_LOG_RETENTION_DAYS`

### Audit Log Location

By default, audit logs are written to stderr and can be redirected:

```bash
# Write audit logs to file
VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log ./scripts/run_gateway.sh dev

# Write to stderr (default)
./scripts/run_gateway.sh dev 2>> audit.log
```

### Audit Log Storage

Audit logs are stored in NDJSON (newline-delimited JSON) format for easy parsing:

```json
{"timestamp":1708512645.123,"datetime":"2026-02-21T10:30:45","log_type":"auth","action":"login_success","user_id":"admin","ip_address":"127.0.0.1","details":{},"request_id":"abc123"}
```

**Storage structure:**
- **Log directory**: `/var/log/veloz/audit/` (configurable)
- **Archive directory**: `/var/log/veloz/audit/archive/`
- **File naming**: `{log_type}_{YYYY-MM-DD}.ndjson`
- **Archive naming**: `{log_type}_{YYYYMMDD_HHMMSS}.ndjson.gz`

**Entry fields:**
| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | float | Unix timestamp |
| `datetime` | string | ISO 8601 formatted timestamp |
| `log_type` | string | Log category (auth, order, api_key, error, access) |
| `action` | string | Action performed |
| `user_id` | string | User who performed the action |
| `ip_address` | string | Client IP address |
| `details` | object | Additional action-specific details |
| `request_id` | string | Request correlation ID (optional) |

### Audit Query API

#### GET /api/audit/logs

Query audit logs with filtering and pagination. Requires `admin:config` permission.

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | (all) | Filter by log type (auth, order, api_key, error, access) |
| `user_id` | string | (all) | Filter by user ID |
| `action` | string | (all) | Filter by action |
| `start_time` | int | (none) | Start timestamp (Unix seconds) |
| `end_time` | int | (none) | End timestamp (Unix seconds) |
| `limit` | int | 100 | Maximum results (1-1000) |
| `offset` | int | 0 | Skip first N results |

**Success Response:**
```json
{
  "logs": [
    {
      "timestamp": 1708512645.123,
      "datetime": "2026-02-21T10:30:45",
      "log_type": "auth",
      "action": "login_success",
      "user_id": "admin",
      "ip_address": "127.0.0.1",
      "details": {},
      "request_id": "abc123"
    }
  ],
  "total": 150,
  "limit": 100,
  "offset": 0,
  "has_more": true
}
```

**Error Response (401):**
```json
{
  "error": "authentication_required"
}
```

**Error Response (403):**
```json
{
  "error": "forbidden",
  "message": "admin:config permission required"
}
```

#### GET /api/audit/stats

Get audit log statistics and retention status. Requires `admin:config` permission.

**Success Response:**
```json
{
  "status": "active",
  "policies": {
    "auth": {"retention_days": 90, "archive_before_delete": true, "max_file_size_mb": 100},
    "order": {"retention_days": 365, "archive_before_delete": true, "max_file_size_mb": 100}
  },
  "log_files": {
    "auth": {"count": 5, "size_bytes": 1048576},
    "order": {"count": 10, "size_bytes": 5242880}
  },
  "archive_files": {
    "auth": {"count": 2, "size_bytes": 204800}
  },
  "total_size_bytes": 6291456,
  "archive_size_bytes": 204800
}
```

#### POST /api/audit/archive

Manually trigger audit log archival and cleanup. Requires `admin:config` permission.

**Success Response:**
```json
{
  "ok": true,
  "stats": {
    "archived_files": 3,
    "deleted_files": 1,
    "bytes_freed": 1048576,
    "errors": []
  }
}
```

**Error Response (500):**
```json
{
  "error": "not_initialized",
  "message": "Audit retention manager not initialized"
}
```

### Compliance

Audit logs support compliance requirements by providing:

- **Immutable records** of security events
- **Timestamp precision** for forensic analysis
- **User attribution** for all authenticated actions
- **IP address tracking** for access patterns
- **Retention policies** for regulatory compliance

---

## Order States

| State | Description |
|-------|-------------|
| `ACCEPTED` | Order accepted by venue |
| `PARTIALLY_FILLED` | Order partially filled |
| `FILLED` | Order fully filled |
| `CANCELLED` | Order cancelled |
| `REJECTED` | Order rejected |
| `EXPIRED` | Order expired |

---

## Error Codes

| HTTP Code | Error | Description |
|-----------|-------|-------------|
| 400 | `bad_params` | Invalid request parameters |
| 400 | `bad_json` | Invalid JSON in request body |
| 400 | `binance_not_configured` | Binance API credentials not configured |
| 401 | `authentication_required` | Authentication required |
| 401 | `invalid_token` | Invalid JWT token |
| 401 | `invalid_api_key` | Invalid API key |
| 401 | `invalid_credentials` | Invalid login credentials |
| 403 | `forbidden` | Permission denied |
| 404 | `not_found` | Order not found |
| 429 | `rate_limit_exceeded` | Rate limit exceeded |
| 500 | `execution_unavailable` | Execution router unavailable |
| 500 | `auth_not_configured` | Authentication not configured |

---

## KJ Type Patterns in C++ Libraries

The VeloZ C++ libraries use KJ types from Cap'n Proto. When integrating with the gateway or writing C++ code, use these patterns:

### String Types

| KJ Type | Usage |
|---------|-------|
| `kj::String` | Owned string (heap allocated) |
| `kj::StringPtr` | Non-owning string view |
| `"text"_kj` | String literal suffix |

### Container Types

| KJ Type | Equivalent | Usage |
|---------|------------|-------|
| `kj::Vector<T>` | `std::vector<T>` | Dynamic array |
| `kj::Array<T>` | - | Fixed-size array |
| `kj::TreeMap<K,V>` | `std::map<K,V>` | Ordered map |

### Memory Management

| KJ Type | Equivalent | Usage |
|---------|------------|-------|
| `kj::Own<T>` | `std::unique_ptr<T>` | Unique ownership |
| `kj::Rc<T>` | `std::shared_ptr<T>` | Reference counted (single-threaded) |
| `kj::Arc<T>` | `std::shared_ptr<T>` | Reference counted (thread-safe) |

### Example

```cpp
// Create owned string
kj::String name = kj::str("BTCUSDT");

// Use string view
void process(kj::StringPtr symbol);
process(name);

// Create owned pointer
kj::Own<MyClass> obj = kj::heap<MyClass>(args...);

// Reference counted
kj::Rc<IStrategy> strategy = StrategyFactory::create("ma_crossover"_kj);
```

For comprehensive KJ documentation, see [KJ Library Usage Guide](../kj/library_usage_guide.md).

---

## Example cURL Commands

```bash
# Health check
curl http://127.0.0.1:8080/health

# Get configuration
curl http://127.0.0.1:8080/api/config

# Get market data
curl http://127.0.0.1:8080/api/market

# Place an order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{"side":"BUY","symbol":"BTCUSDT","qty":0.001,"price":50000.0}'

# Cancel an order
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{"client_order_id":"web-1234567890"}'

# Get order state
curl "http://127.0.0.1:8080/api/order_state?client_order_id=web-1234567890"

# Get account state
curl http://127.0.0.1:8080/api/account

# Listen to SSE events
curl -N http://127.0.0.1:8080/api/stream

# Engine service mode endpoints (when engine runs with --service)

# Get engine status
curl http://127.0.0.1:8080/api/status

# Start/stop engine
curl -X POST http://127.0.0.1:8080/api/start
curl -X POST http://127.0.0.1:8080/api/stop

# List strategies
curl http://127.0.0.1:8080/api/strategies

# Get specific strategy
curl http://127.0.0.1:8080/api/strategies/ma_crossover_1

# Start/stop strategy
curl -X POST http://127.0.0.1:8080/api/strategies/ma_crossover_1/start
curl -X POST http://127.0.0.1:8080/api/strategies/ma_crossover_1/stop

# Get positions
curl http://127.0.0.1:8080/api/positions
curl http://127.0.0.1:8080/api/positions/BTCUSDT

# WebSocket connection (using websocat)
websocat ws://127.0.0.1:8080/ws/market
websocat ws://127.0.0.1:8080/ws/orders

# Authentication examples (when auth is enabled)

# Login to get JWT tokens (access + refresh)
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_password"}'

# Response includes both access_token and refresh_token
# {
#   "access_token": "eyJ...",
#   "refresh_token": "eyJ...",
#   "token_type": "Bearer",
#   "expires_in": 3600
# }

# Use JWT access token
curl http://127.0.0.1:8080/api/config \
  -H "Authorization: Bearer <your_access_token>"

# Refresh expired access token
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"<your_refresh_token>"}'

# Logout (revoke refresh token)
curl -X POST http://127.0.0.1:8080/api/auth/logout \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"<your_refresh_token>"}'

# Use API key
curl http://127.0.0.1:8080/api/config \
  -H "X-API-Key: veloz_abc123..."

# Create API key (requires admin permission)
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{"name":"my-key","permissions":["read","write"]}'

# Create admin API key
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{"name":"admin-key","permissions":["read","write","admin"]}'

# List API keys
curl http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer <your_token>"

# Revoke API key (requires admin permission)
curl -X DELETE "http://127.0.0.1:8080/api/auth/keys?key_id=vk_abc123" \
  -H "Authorization: Bearer <admin_token>"

# Audit log examples (requires admin:config permission)

# Query audit logs
curl "http://127.0.0.1:8080/api/audit/logs?type=auth&limit=50" \
  -H "Authorization: Bearer <admin_token>"

# Query audit logs with filters
curl "http://127.0.0.1:8080/api/audit/logs?type=auth&user_id=admin&action=login_success&limit=100" \
  -H "Authorization: Bearer <admin_token>"

# Get audit log statistics
curl http://127.0.0.1:8080/api/audit/stats \
  -H "Authorization: Bearer <admin_token>"

# Trigger manual archive/cleanup
curl -X POST http://127.0.0.1:8080/api/audit/archive \
  -H "Authorization: Bearer <admin_token>"
```

---

## Related

- [SSE API](sse_api.md) - Server-Sent Events stream for real-time updates
- [Engine Protocol](engine_protocol.md) - Engine stdio command format
- [Backtest API](backtest_api.md) - Backtest configuration and reporting
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns
