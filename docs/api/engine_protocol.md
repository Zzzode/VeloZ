# Engine Protocol Reference

This document describes the protocols used to communicate with the VeloZ C++ engine, including both stdio mode and HTTP service mode.

## Overview

The VeloZ engine supports two communication modes:

### Stdio Mode (`--stdio`)

- **stdin**: Text-based commands from gateway
- **stdout**: NDJSON (newline-delimited JSON) events to gateway
- Lightweight, human-readable, and machine-parsable
- Used by the Python gateway for process-based communication

### HTTP Service Mode (`--service`)

- REST API endpoints for engine control
- Direct HTTP communication without gateway intermediary
- Suitable for microservice deployments

---

## HTTP Service Mode Endpoints

When running in service mode, the engine exposes the following REST API endpoints:

### Engine Control

#### GET /api/health

Health check endpoint.

**Response:**
```json
{
  "ok": true
}
```

#### GET /api/status

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

#### POST /api/start

Start the engine (if not already running).

**Response:**
```json
{
  "ok": true,
  "message": "Engine started"
}
```

**Error Response:**
```json
{
  "ok": false,
  "error": "Engine already running"
}
```

#### POST /api/stop

Stop the engine gracefully.

**Response:**
```json
{
  "ok": true,
  "message": "Engine stopping"
}
```

### Strategy Management

#### GET /api/strategies

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

#### GET /api/strategies/{id}

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

#### POST /api/strategies/{id}/start

Start a specific strategy.

**Response:**
```json
{
  "ok": true,
  "message": "Strategy started"
}
```

#### POST /api/strategies/{id}/stop

Stop a specific strategy.

**Response:**
```json
{
  "ok": true,
  "message": "Strategy stopped"
}
```

### Configuration

The HTTP service mode can be configured via command-line flags:

| Flag | Default | Description |
|------|---------|-------------|
| `--port` | `8080` | HTTP server port |
| `--host` | `0.0.0.0` | HTTP server bind address |

**Example:**
```bash
./veloz_engine --service --port 9000
```

---

## Stdio Mode Protocol

## Command Format

Commands are sent as plain text, one command per line.

### ORDER Command

Place a new limit order.

**Format**:
```
ORDER <side> <symbol> <qty> <price> <client_order_id>
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `side` | string | Yes | Order side: `BUY` or `SELL` (case-insensitive) |
| `symbol` | string | Yes | Trading symbol, e.g., `BTCUSDT` |
| `qty` | number | Yes | Order quantity (must be > 0) |
| `price` | number | Yes | Limit price (must be > 0) |
| `client_order_id` | string | Yes | Client-defined order ID |

**Example**:
```
ORDER BUY BTCUSDT 0.001 50000.0 web-1234567890
ORDER sell ethusdt 0.1 3200.0 my-order-1
```

**Validation**:
- Side must be `BUY`, `SELL`, `Buy`, or `Sell`
- Quantity must be positive
- Price must be positive
- Client order ID must not be empty

### CANCEL Command

Cancel an existing order.

**Format**:
```
CANCEL <client_order_id>
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `client_order_id` | string | Yes | Client order ID to cancel |

**Example**:
```
CANCEL web-1234567890
CANCEL my-order-1
```

**Validation**:
- Client order ID must not be empty

## Event Format

Events are emitted as NDJSON (one JSON object per line).

### Market Event

Emitted when market data is available.

**Format**:
```json
{"type":"market","symbol":"BTCUSDT","ts_ns":1704067200000000000,"price":50000.0}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"market"` |
| `symbol` | string | Trading symbol |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `price` | number | Current price |

### Order Update Event

Emitted when order status changes.

**Format**:
```json
{"type":"order_update","ts_ns":1704067200000000000,"client_order_id":"web-1234567890","venue_order_id":"123456","status":"FILLED","symbol":"BTCUSDT","side":"BUY","qty":0.001,"price":50000.0}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"order_update"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `client_order_id` | string | Client order ID |
| `venue_order_id` | string? | Exchange order ID (optional) |
| `status` | string | Order status |
| `symbol` | string? | Trading symbol (optional) |
| `side` | string? | Order side: `"BUY"` or `"SELL"` (optional) |
| `qty` | number? | Order quantity (optional) |
| `price` | number? | Order price (optional) |
| `reason` | string? | Rejection/error reason (optional) |

### Fill Event

Emitted when an order is filled.

**Format**:
```json
{"type":"fill","ts_ns":1704067200000000000,"client_order_id":"web-1234567890","symbol":"BTCUSDT","qty":0.001,"price":50000.0}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"fill"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `client_order_id` | string | Client order ID |
| `symbol` | string | Trading symbol |
| `qty` | number | Fill quantity |
| `price` | number | Fill price |

### Order State Event

Emitted as a snapshot of order state.

**Format**:
```json
{"type":"order_state","client_order_id":"web-1234567890","status":"FILLED","symbol":"BTCUSDT","side":"BUY","order_qty":0.001,"limit_price":50000.0,"venue_order_id":"123456","executed_qty":0.001,"avg_price":50000.0,"reason":"","last_ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"order_state"` |
| `client_order_id` | string | Client order ID |
| `status` | string? | Order status (optional) |
| `symbol` | string? | Trading symbol (optional) |
| `side` | string? | Order side (optional) |
| `order_qty` | number? | Total order quantity (optional) |
| `limit_price` | number? | Limit price (optional) |
| `venue_order_id` | string? | Exchange order ID (optional) |
| `executed_qty` | number | Executed quantity |
| `avg_price` | number | Average execution price |
| `reason` | string? | Rejection reason (optional) |
| `last_ts_ns` | int64? | Last update timestamp (optional) |

### Account Event

Emitted when account balance changes.

**Format**:
```json
{"type":"account","ts_ns":1704067200000000000,"balances":[{"asset":"USDT","free":9500.0,"locked":500.0},{"asset":"BTC","free":0.01,"locked":0.0}]}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"account"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `balances` | array | Balance entries |
| `balances[].asset` | string | Asset symbol |
| `balances[].free` | number | Available balance |
| `balances[].locked` | number | Locked balance |

### Error Event

Emitted when an error occurs.

**Format**:
```json
{"type":"error","ts_ns":1704067200000000000,"message":"market_source_error"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"error"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `message` | string | Error message |

### Trade Event

Emitted when a trade occurs on the market.

**Format**:
```json
{"type":"trade","symbol":"BTCUSDT","venue":"binance","price":50000.0,"qty":0.5,"is_buyer_maker":true,"trade_id":123456789,"ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"trade"` |
| `symbol` | string | Trading symbol |
| `venue` | string | Exchange venue: `"binance"`, `"okx"`, `"bybit"` |
| `price` | number | Trade price |
| `qty` | number | Trade quantity |
| `is_buyer_maker` | boolean | True if buyer is the maker |
| `trade_id` | int64 | Exchange trade ID |
| `ts_ns` | int64 | Timestamp in nanoseconds |

### Book Top Event

Emitted when best bid/ask changes.

**Format**:
```json
{"type":"book_top","symbol":"BTCUSDT","venue":"binance","bid_price":49999.0,"bid_qty":1.5,"ask_price":50001.0,"ask_qty":2.0,"ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"book_top"` |
| `symbol` | string | Trading symbol |
| `venue` | string | Exchange venue |
| `bid_price` | number | Best bid price |
| `bid_qty` | number | Best bid quantity |
| `ask_price` | number | Best ask price |
| `ask_qty` | number | Best ask quantity |
| `ts_ns` | int64 | Timestamp in nanoseconds |

### Book Delta Event

Emitted when order book levels change.

**Format**:
```json
{"type":"book_delta","symbol":"BTCUSDT","venue":"binance","sequence":12345,"bids":[[49999.0,1.5],[49998.0,2.0]],"asks":[[50001.0,2.0],[50002.0,1.0]],"ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"book_delta"` |
| `symbol` | string | Trading symbol |
| `venue` | string | Exchange venue |
| `sequence` | int64 | Sequence number for ordering |
| `bids` | array | Bid levels as `[price, qty]` pairs |
| `asks` | array | Ask levels as `[price, qty]` pairs |
| `ts_ns` | int64 | Timestamp in nanoseconds |

### Kline Event

Emitted for candlestick/OHLCV data.

**Format**:
```json
{"type":"kline","symbol":"BTCUSDT","venue":"binance","open":49500.0,"high":50500.0,"low":49000.0,"close":50000.0,"volume":1234.5,"start_time":1704067200000,"close_time":1704070800000,"ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"kline"` |
| `symbol` | string | Trading symbol |
| `venue` | string | Exchange venue |
| `open` | number | Open price |
| `high` | number | High price |
| `low` | number | Low price |
| `close` | number | Close price |
| `volume` | number | Trading volume |
| `start_time` | int64 | Candle start time (milliseconds) |
| `close_time` | int64 | Candle close time (milliseconds) |
| `ts_ns` | int64 | Event timestamp in nanoseconds |

### Subscription Status Event

Emitted when subscription state changes.

**Format**:
```json
{"type":"subscription_status","symbol":"BTCUSDT","event_type":"trade","status":"subscribed","ts_ns":1704067200000000000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"subscription_status"` |
| `symbol` | string | Trading symbol |
| `event_type` | string | Subscribed event type: `"trade"`, `"book"`, `"kline"` |
| `status` | string | Status: `"subscribed"`, `"unsubscribed"`, `"error"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |

## Order States

| State | Description |
|--------|-------------|
| `ACCEPTED` | Order accepted by venue |
| `PARTIALLY_FILLED` | Order partially filled |
| `FILLED` | Order fully filled |
| `CANCELLED` | Order cancelled |
| `REJECTED` | Order rejected |
| `EXPIRED` | Order expired |

## JSON Escaping

String values in events are JSON-escaped. Special characters:
- `\` → `\\`
- `"` → `\"`
- `\n` → `\\n`
- `\r` → `\\r`
- `\t` → `\\t`

## Example Session

```
# stdin -> engine
ORDER BUY BTCUSDT 0.001 50000.0 test-order-1

# stdout <- engine
{"type":"order_update","ts_ns":1704067200000000000,"client_order_id":"test-order-1","status":"ACCEPTED","symbol":"BTCUSDT","side":"BUY","qty":0.001,"price":50000.0}

# stdout <- engine (later)
{"type":"fill","ts_ns":1704067201000000000,"client_order_id":"test-order-1","symbol":"BTCUSDT","qty":0.001,"price":50000.0}

# stdout <- engine
{"type":"order_update","ts_ns":1704067201000000000,"client_order_id":"test-order-1","status":"FILLED","symbol":"BTCUSDT","side":"BUY","qty":0.001,"price":50000.0}

# stdin -> engine
CANCEL test-order-1

# stdout <- engine
{"type":"order_update","ts_ns":1704067202000000000,"client_order_id":"test-order-1","status":"CANCELLED"}
```

## Implementation Notes

### Gateway to Engine

The gateway (`apps/gateway/gateway.py`) communicates with the engine:

```python
# Write command to engine stdin
cmd = f"ORDER {side} {symbol} {qty} {price} {client_order_id}\n"
engine_proc.stdin.write(cmd)
engine_proc.stdin.flush()

# Read events from engine stdout
for line in engine_proc.stdout:
    line = line.strip()
    if not line:
        continue
    event = json.loads(line)
    # Process event...
```

### Idempotency

The engine maintains order state internally. Sending the same command multiple times:
- **ORDER**: Creates separate orders (use unique client_order_id for idempotency)
- **CANCEL**: No error if order already cancelled

## Event Type Summary

| Event Type | Description | Source |
|------------|-------------|--------|
| `market` | Simple market price update | Market data |
| `trade` | Individual trade execution | Market data |
| `book_top` | Best bid/ask update | Market data |
| `book_delta` | Order book level changes | Market data |
| `kline` | Candlestick/OHLCV data | Market data |
| `order_update` | Order status change | Order management |
| `fill` | Trade execution fill | Order management |
| `order_state` | Order state snapshot | Order management |
| `account` | Account balance update | Account management |
| `subscription_status` | Subscription state change | System |
| `error` | Error notification | System |

## Configuration API Format

Strategy configuration is passed as JSON with the following structure:

```json
{
  "strategy_id": "my_strategy_1",
  "strategy_type": "MomentumStrategy",
  "symbol": "BTCUSDT",
  "venue": "binance",
  "parameters": {
    "roc_period": 14,
    "rsi_period": 14,
    "rsi_overbought": 70.0,
    "rsi_oversold": 30.0,
    "position_size": 0.01,
    "use_rsi_filter": true
  },
  "risk_limits": {
    "max_position_size": 1.0,
    "max_daily_loss": 1000.0,
    "max_drawdown": 0.1
  }
}
```

**Common Configuration Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `strategy_id` | string | Yes | Unique strategy identifier |
| `strategy_type` | string | Yes | Strategy class name |
| `symbol` | string | Yes | Trading symbol |
| `venue` | string | No | Exchange venue (default: binance) |
| `parameters` | object | No | Strategy-specific parameters |
| `risk_limits` | object | No | Risk management limits |

**Available Strategy Types:**

| Type | Description |
|------|-------------|
| `TrendFollowingStrategy` | MA crossover trend following |
| `MeanReversionStrategy` | Mean reversion with Bollinger Bands |
| `MomentumStrategy` | ROC and RSI momentum |
| `MarketMakingStrategy` | Bid/ask spread market making |
| `GridStrategy` | Grid trading strategy |

## Running Modes

### Stdio Mode

```bash
# Run engine in stdio mode (for gateway integration)
./veloz_engine --stdio

# With configuration file
./veloz_engine --stdio --config /path/to/config.json
```

### Service Mode

```bash
# Run engine in HTTP service mode
./veloz_engine --service

# With custom port
./veloz_engine --service --port 9000

# With configuration
./veloz_engine --service --config /path/to/config.json
```

## Related

- [HTTP API Reference](http_api.md) - Gateway REST API endpoints
- [SSE API](sse_api.md) - Server-Sent Events stream
- [Backtest API](backtest_api.md) - Backtest configuration and reporting
