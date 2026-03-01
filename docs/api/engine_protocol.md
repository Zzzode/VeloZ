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

Place a new order.

**Format**:
```
ORDER <side> <symbol> <qty> <price> <client_order_id> [type] [tif]
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `side` | string | Yes | Order side: `BUY` or `SELL` (case-insensitive, aliases: `B`, `S`) |
| `symbol` | string | Yes | Trading symbol, e.g., `BTCUSDT` |
| `qty` | number | Yes | Order quantity (must be > 0) |
| `price` | number | Yes | Limit price (must be > 0 for limit orders; 0 for market orders) |
| `client_order_id` | string | Yes | Client-defined order ID |
| `type` | string | No | Order type: `LIMIT` or `MARKET` (aliases: `L`, `M`). Default: `LIMIT` |
| `tif` | string | No | Time-in-force: `GTC`, `IOC`, `FOK`, `GTX` (aliases: `G`, `I`, `F`, `G`). Default: `GTC` |

**Examples**:
```
ORDER BUY BTCUSDT 0.001 50000.0 web-1234567890
ORDER sell ethusdt 0.1 3200.0 my-order-1
ORDER BUY BTCUSDT 0.001 50000.0 my-market-order market
ORDER SELL BTCUSDT 0.001 0 my-market-order market ioc
```

**Validation**:
- Side must be `BUY`, `SELL`, `Buy`, `Sell`, `B`, or `S`
- Quantity must be positive
- Price must be positive for limit orders; can be 0 for market orders
- Client order ID must not be empty
- Type must be `LIMIT`, `MARKET`, `L`, or `M` (if provided)
- TIF must be `GTC`, `IOC`, `FOK`, `GTX`, `G`, `I`, `F`, or `G` (if provided)

**Time-in-Force Values**:
| TIF | Description |
|-----|-------------|
| `GTC` (Good-Til-Canceled) | Order remains active until filled or canceled |
| `IOC` (Immediate-Or-Cancel) | Fill immediately, cancel remaining |
| `FOK` (Fill-Or-Kill) | Fill entirely or cancel |
| `GTX` (Good-Til-Crossing) | Maker only; no immediate fill |

### BUY Command (Shorthand)

Place a buy order without the ORDER keyword.

**Format**:
```
BUY <symbol> <qty> <price> <client_order_id> [type] [tif]
```

**Example**:
```
BUY BTCUSDT 0.001 50000.0 buy-order-1 limit gtc
```

### SELL Command (Shorthand)

Place a sell order without the ORDER keyword.

**Format**:
```
SELL <symbol> <qty> <price> <client_order_id> [type] [tif]
```

**Example**:
```
SELL BTCUSDT 0.001 50000.0 sell-order-1
```

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

### QUERY Command

Query engine for information.

**Format**:
```
QUERY <type> [params]
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `type` | string | Yes | Query type |
| `params` | string | No | Additional parameters (space-separated) |

**Example**:
```
QUERY status
QUERY strategies
```

### STRATEGY Command

Manage trading strategies.

**Formats**:
```
STRATEGY LOAD <type> <name> [params...]
STRATEGY START <strategy_id>
STRATEGY STOP <strategy_id>
STRATEGY PAUSE <strategy_id>
STRATEGY RESUME <strategy_id>
STRATEGY UNLOAD <strategy_id>
STRATEGY LIST
STRATEGY STATUS [strategy_id]
STRATEGY PARAMS <strategy_id> <key>=<value>...
STRATEGY METRICS [strategy_id]
```

**Parameters**:
| Subcommand | Parameters | Description |
|------------|-------------|-------------|
| `LOAD` | `type`, `name`, `params...` | Load a strategy |
| `START` | `strategy_id` | Start a strategy |
| `STOP` | `strategy_id` | Stop a strategy |
| `PAUSE` | `strategy_id` | Pause a strategy |
| `RESUME` | `strategy_id` | Resume a paused strategy |
| `UNLOAD` | `strategy_id` | Unload a strategy |
| `LIST` | none | List all strategies |
| `STATUS` | `[strategy_id]` | Get strategy status |
| `PARAMS` | `strategy_id`, `key>=<value>...` | Update strategy parameters |
| `METRICS` | `[strategy_id]` | Get strategy metrics |

**Examples**:
```
STRATEGY LOAD TrendFollowing ma_cross_1 fast_period=10 slow_period=20
STRATEGY START ma_cross_1
STRATEGY STOP ma_cross_1
STRATEGY LIST
STRATEGY STATUS ma_cross_1
STRATEGY METRICS
```

### SUBSCRIBE Command

Subscribe to market data events.

**Format**:
```
SUBSCRIBE <venue> <symbol> <event_type>
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `venue` | string | Yes | Exchange venue: `binance`, `okx`, `bybit` |
| `symbol` | string | Yes | Trading symbol |
| `event_type` | string | Yes | Event type: `trade`, `booktop`, `bookdelta`, `kline` |

**Example**:
```
SUBSCRIBE binance BTCUSDT trade
SUBSCRIBE binance BTCUSDT booktop
```

### UNSUBSCRIBE Command

Unsubscribe from market data events.

**Format**:
```
UNSUBSCRIBE <venue> <symbol> <event_type>
```

**Parameters**:
| Parameter | Type | Required | Description |
|-----------|-------|-----------|-------------|
| `venue` | string | Yes | Exchange venue |
| `symbol` | string | Yes | Trading symbol |
| `event_type` | string | Yes | Event type to unsubscribe |

**Example**:
```
UNSUBSCRIBE binance BTCUSDT trade
```

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

### Order Received Event

Emitted when an ORDER command is received.

**Format**:
```json
{"type":"order_received","command_id":1,"client_order_id":"web-1234567890","symbol":"BTCUSDT","side":"buy","order_type":"limit","quantity":0.001,"price":50000.0}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"order_received"` |
| `command_id` | int | Command sequence number |
| `client_order_id` | string | Client order ID |
| `symbol` | string | Trading symbol |
| `side` | string | Order side: `"buy"` or `"sell"` |
| `order_type` | string | Order type: `"limit"` or `"market"` |
| `quantity` | number | Order quantity |
| `price` | number | Order price |

### Cancel Received Event

Emitted when a CANCEL command is received.

**Format**:
```json
{"type":"cancel_received","command_id":2,"client_order_id":"web-1234567890"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"cancel_received"` |
| `command_id` | int | Command sequence number |
| `client_order_id` | string | Client order ID to cancel |

### Query Received Event

Emitted when a QUERY command is received.

**Format**:
```json
{"type":"query_received","command_id":3,"query_type":"status","params":""}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"query_received"` |
| `command_id` | int | Command sequence number |
| `query_type` | string | Query type |
| `params` | string | Query parameters |

### Strategy Command Received Event

Emitted when a STRATEGY command is received.

**Format**:
```json
{"type":"strategy_command_received","command_id":4,"subcommand":1}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_command_received"` |
| `command_id` | int | Command sequence number |
| `subcommand` | int | Strategy subcommand ID |

### Strategy Loaded Event

Emitted when a strategy is loaded.

**Format**:
```json
{"type":"strategy_loaded","strategy_id":"ma_cross_1","name":"MA Crossover","strategy_type":"TrendFollowing"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_loaded"` |
| `strategy_id` | string | Strategy ID |
| `name` | string | Strategy name |
| `strategy_type` | string | Strategy type |

### Strategy Started Event

Emitted when a strategy starts execution.

**Format**:
```json
{"type":"strategy_started","strategy_id":"ma_cross_1"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_started"` |
| `strategy_id` | string | Strategy ID |

### Strategy Stopped Event

Emitted when a strategy stops.

**Format**:
```json
{"type":"strategy_stopped","strategy_id":"ma_cross_1"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_stopped"` |
| `strategy_id` | string | Strategy ID |

### Strategy Paused Event

Emitted when a strategy is paused.

**Format**:
```json
{"type":"strategy_paused","strategy_id":"ma_cross_1"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_paused"` |
| `strategy_id` | string | Strategy ID |

### Strategy Resumed Event

Emitted when a paused strategy is resumed.

**Format**:
```json
{"type":"strategy_resumed","strategy_id":"ma_cross_1"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_resumed"` |
| `strategy_id` | string | Strategy ID |

### Strategy Unloaded Event

Emitted when a strategy is unloaded.

**Format**:
```json
{"type":"strategy_unloaded","strategy_id":"ma_cross_1"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_unloaded"` |
| `strategy_id` | string | Strategy ID |

### Strategy List Event

Emitted when listing all strategies.

**Format**:
```json
{"type":"strategy_list","count":3,"strategies":["ma_cross_1","rsi_1","grid_1"]}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_list"` |
| `count` | int | Number of strategies |
| `strategies` | array | Array of strategy IDs |

### Strategy Status Event

Emitted when querying a specific strategy's status.

**Format**:
```json
{"type":"strategy_status","strategy_id":"ma_cross_1","name":"MA Crossover","is_running":true,"pnl":1250.50,"trade_count":42,"win_rate":0.667}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_status"` |
| `strategy_id` | string | Strategy ID |
| `name` | string | Strategy name |
| `is_running` | boolean | Whether strategy is running |
| `pnl` | number | Profit/loss |
| `trade_count` | int | Number of trades |
| `win_rate` | number | Win rate (0.0-1.0) |

### Strategy Status All Event

Emitted when querying all strategies' status.

**Format**:
```json
{"type":"strategy_status_all","count":3,"strategies":[{"strategy_id":"ma_cross_1","name":"MA Crossover","is_running":true,"pnl":1250.50,"trade_count":42,"win_rate":0.667}]}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_status_all"` |
| `count` | int | Number of strategies |
| `strategies` | array | Array of strategy status objects |

### Strategy Params Updated Event

Emitted when strategy parameters are updated.

**Format**:
```json
{"type":"strategy_params_updated","strategy_id":"ma_cross_1","param_count":2}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_params_updated"` |
| `strategy_id` | string | Strategy ID |
| `param_count` | int | Number of parameters updated |

### Strategy Metrics Event

Emitted when querying strategy metrics.

**Format**:
```json
{"type":"strategy_metrics","strategy_id":"ma_cross_1","events_processed":1000,"signals_generated":50,"avg_execution_time_us":125.5,"signals_per_second":0.5,"errors":0}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_metrics"` |
| `strategy_id` | string | Strategy ID |
| `events_processed` | int | Number of events processed |
| `signals_generated` | int | Number of signals generated |
| `avg_execution_time_us` | number | Average execution time in microseconds |
| `signals_per_second` | number | Signals per second |
| `errors` | int | Error count |

### Strategy Metrics Summary Event

Emitted when querying aggregated strategy metrics.

**Format**:
```json
{"type":"strategy_metrics_summary","summary":"Total strategies: 3, Running: 2, Total trades: 150"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"strategy_metrics_summary"` |
| `summary` | string | Summary text |

### Engine Started Event

Emitted when the engine starts.

**Format**:
```json
{"type":"engine_started","version":"1.0.0"}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"engine_started"` |
| `version` | string | Engine version |

### Engine Stopped Event

Emitted when the engine shuts down.

**Format**:
```json
{"type":"engine_stopped","commands_processed":1000}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type: `"engine_stopped"` |
| `commands_processed` | int | Number of commands processed |

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
| `order_received` | ORDER command received | Order management |
| `cancel_received` | CANCEL command received | Order management |
| `query_received` | QUERY command received | System |
| `strategy_command_received` | STRATEGY command received | Strategy management |
| `order_update` | Order status change | Order management |
| `fill` | Trade execution fill | Order management |
| `order_state` | Order state snapshot | Order management |
| `strategy_loaded` | Strategy loaded | Strategy management |
| `strategy_started` | Strategy started | Strategy management |
| `strategy_stopped` | Strategy stopped | Strategy management |
| `strategy_paused` | Strategy paused | Strategy management |
| `strategy_resumed` | Strategy resumed | Strategy management |
| `strategy_unloaded` | Strategy unloaded | Strategy management |
| `strategy_list` | List of strategies | Strategy management |
| `strategy_status` | Single strategy status | Strategy management |
| `strategy_status_all` | All strategies status | Strategy management |
| `strategy_params_updated` | Strategy parameters updated | Strategy management |
| `strategy_metrics` | Strategy metrics | Strategy management |
| `strategy_metrics_summary` | Strategy metrics summary | Strategy management |
| `account` | Account balance update | Account management |
| `subscription_status` | Subscription state change | System |
| `error` | Error notification | System |
| `engine_started` | Engine startup | System |
| `engine_stopped` | Engine shutdown | System |

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
