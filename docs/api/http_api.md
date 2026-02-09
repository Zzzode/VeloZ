# HTTP API Reference

This document describes the HTTP API endpoints available in VeloZ Gateway.

## Base URL

```
http://127.0.0.1:8080
```

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
Access-Control-Allow-Methods: GET,POST,OPTIONS
Access-Control-Allow-Headers: Content-Type
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
| 404 | `not_found` | Order not found |
| 500 | `execution_unavailable` | Execution router unavailable |

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
```
