# Engine Stdio Protocol

This document describes the stdio-based protocol used to communicate with the VeloZ C++ engine.

## Overview

The VeloZ engine runs in stdio mode (`--stdio` flag) and communicates via:

- **stdin**: Text-based commands from gateway
- **stdout**: NDJSON (newline-delimited JSON) events to gateway

This protocol is lightweight, human-readable, and machine-parsable.

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

## Related

- [HTTP API Reference](http-api.md) - REST API endpoints
- [SSE API](sse-api.md) - Server-Sent Events stream
