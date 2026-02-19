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

Authenticate and obtain a JWT token.

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
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer"
}
```

**Error Response (401):**
```json
{
  "error": "invalid_credentials"
}
```

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

**Success Response (201):**
```json
{
  "key_id": "vk_abc123",
  "api_key": "veloz_abc123...",
  "message": "Store this key securely. It will not be shown again."
}
```

**Error Response (403):**
```json
{
  "error": "forbidden",
  "message": "admin permission required"
}
```

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

**Error Response (404):**
```json
{
  "error": "not_found"
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

# Authentication examples (when auth is enabled)

# Login to get JWT token
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_password"}'

# Use JWT token
curl http://127.0.0.1:8080/api/config \
  -H "Authorization: Bearer <your_jwt_token>"

# Use API key
curl http://127.0.0.1:8080/api/config \
  -H "X-API-Key: veloz_abc123..."

# Create API key (requires admin)
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{"name":"my-key","permissions":["read","write"]}'

# List API keys
curl http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer <your_token>"

# Revoke API key (requires admin)
curl -X DELETE "http://127.0.0.1:8080/api/auth/keys?key_id=vk_abc123" \
  -H "Authorization: Bearer <admin_token>"
```

---

## Related

- [SSE API](sse_api.md) - Server-Sent Events stream for real-time updates
- [Engine Protocol](engine_protocol.md) - Engine stdio command format
- [Backtest API](backtest_api.md) - Backtest configuration and reporting
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns
