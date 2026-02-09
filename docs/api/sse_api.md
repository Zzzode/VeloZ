# SSE (Server-Sent Events) API

This document describes the Server-Sent Events (SSE) stream provided by VeloZ Gateway for real-time event updates.

## Overview

The SSE stream provides a real-time feed of all engine events, including market data updates, order updates, fills, errors, and account changes. Events are pushed as they occur, ensuring minimal latency.

## Endpoint

```
GET /api/stream
```

## Connection

### Basic Connection

```bash
curl -N http://127.0.0.1:8080/api/stream
```

### Connection with Resume

To resume from a specific event after reconnection:

```bash
curl -N "http://127.0.0.1:8080/api/stream?last_id=123"
```

### Response Format

```
Content-Type: text/event-stream; charset=utf-8
Cache-Control: no-cache
Connection: keep-alive
```

## Event Format

Each SSE event follows the standard SSE format:

```
id: <event_id>
event: <event_type>
data: <json_payload>

```

The `id` is a monotonically increasing integer. Use it for:
- Ordering events
- Detecting gaps in the stream
- Resuming after disconnection (via `last_id` query parameter)

## Event Types

### 1. Market Data Event

Emitted when market data is received.

**Event Name**: `market`

**Payload**:
```json
{
  "type": "market",
  "symbol": "BTCUSDT",
  "ts_ns": 1704067200000000000,
  "price": 50000.0
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"market"` |
| `symbol` | string | Trading symbol |
| `ts_ns` | int64 | Timestamp in nanoseconds since epoch |
| `price` | number | Current price |

### 2. Order Update Event

Emitted when an order's status changes.

**Event Name**: `order_update`

**Payload**:
```json
{
  "type": "order_update",
  "ts_ns": 1704067200000000000,
  "client_order_id": "web-1234567890",
  "venue_order_id": "123456",
  "status": "FILLED",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "qty": 0.001,
  "price": 50000.0,
  "reason": ""
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"order_update"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `client_order_id` | string | Client-defined order ID |
| `venue_order_id` | string? | Exchange order ID (if available) |
| `status` | string | Order status (see Order States) |
| `symbol` | string? | Trading symbol |
| `side` | string? | Order side: `"BUY"` or `"SELL"` |
| `qty` | number? | Order quantity |
| `price` | number? | Order price |
| `reason` | string? | Rejection/error reason |

### 3. Fill Event

Emitted when an order is partially or fully filled.

**Event Name**: `fill`

**Payload**:
```json
{
  "type": "fill",
  "ts_ns": 1704067200000000000,
  "client_order_id": "web-1234567890",
  "symbol": "BTCUSDT",
  "qty": 0.001,
  "price": 50000.0
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"fill"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `client_order_id` | string | Client-defined order ID |
| `symbol` | string | Trading symbol |
| `qty` | number | Fill quantity |
| `price` | number | Fill price |

### 4. Order State Event

Emitted as a snapshot of an order's current state.

**Event Name**: `order_state`

**Payload**:
```json
{
  "type": "order_state",
  "client_order_id": "web-1234567890",
  "status": "FILLED",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "order_qty": 0.001,
  "limit_price": 50000.0,
  "venue_order_id": "123456",
  "executed_qty": 0.001,
  "avg_price": 50000.0,
  "reason": "",
  "last_ts_ns": 1704067200000000000
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"order_state"` |
| `client_order_id` | string | Client-defined order ID |
| `status` | string? | Order status |
| `symbol` | string? | Trading symbol |
| `side` | string? | Order side: `"BUY"` or `"SELL"` |
| `order_qty` | number? | Total order quantity |
| `limit_price` | number? | Limit price |
| `venue_order_id` | string? | Exchange order ID |
| `executed_qty` | number | Quantity executed so far |
| `avg_price` | number | Average execution price |
| `reason` | string? | Rejection/error reason |
| `last_ts_ns` | int64 | Last update timestamp |

### 5. Account Event

Emitted when account balance changes (Binance user stream only).

**Event Name**: `account`

**Payload**:
```json
{
  "type": "account",
  "ts_ns": 1704067200000000000,
  "balances": [
    {
      "asset": "USDT",
      "free": 9500.0,
      "locked": 500.0
    },
    {
      "asset": "BTC",
      "free": 0.01,
      "locked": 0.0
    }
  ]
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"account"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `balances` | array | Balance changes |
| `balances[].asset` | string | Asset symbol |
| `balances[].free` | number | Available balance |
| `balances[].locked` | number | Locked balance |

### 6. Error Event

Emitted when an error occurs.

**Event Name**: `error`

**Payload**:
```json
{
  "type": "error",
  "ts_ns": 1704067200000000000,
  "message": "market_source_error"
}
```

**Fields**:
| Field | Type | Description |
|--------|-------|-------------|
| `type` | string | Event type, always `"error"` |
| `ts_ns` | int64 | Timestamp in nanoseconds |
| `message` | string | Error message |

## Order States

| State | Description |
|--------|-------------|
| `ACCEPTED` | Order accepted by the venue |
| `PARTIALLY_FILLED` | Order partially filled |
| `FILLED` | Order fully filled |
| `CANCELLED` | Order cancelled |
| `REJECTED` | Order rejected |
| `EXPIRED` | Order expired |

## Keep-Alive

The server sends a keep-alive message every 10 seconds when no events occur:

```
: keep-alive

```

This maintains the SSE connection even when the system is idle.

## Reconnection Strategy

When the SSE connection drops:

1. **Save the last received event ID**
2. **Reconnect with the saved ID**:
   ```
   GET /api/stream?last_id=<saved_id>
   ```
3. **The server will send all events after that ID**

This ensures you don't miss any events during temporary disconnections.

## Client Libraries

### JavaScript/Browser

```javascript
const eventSource = new EventSource('/api/stream');

eventSource.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(`Event: ${event.event}`, data);
};

eventSource.onerror = (error) => {
  console.error('SSE error:', error);
};

// Resume from specific event ID
const eventSourceWithResume = new EventSource('/api/stream?last_id=123');
```

### Python

```python
import sseclient

def on_event(event):
    data = json.loads(event.data)
    print(f"Event: {event.event}", data)

client = sseclient.SSEClient(
    'http://127.0.0.1:8080/api/stream'
)

for event in client.events():
    on_event(event)
```

### cURL

```bash
# Simple stream
curl -N http://127.0.0.1:8080/api/stream

# Stream with resume
curl -N "http://127.0.0.1:8080/api/stream?last_id=100"

# Filter output with jq
curl -N http://127.0.0.1:8080/api/stream | \
  jq 'select(.event == "fill")'
```

## Best Practices

1. **Always save the last event ID** for reconnection
2. **Handle connection errors gracefully** and implement auto-reconnect
3. **Use separate threads/event loops** for processing SSE events
4. **Validate event data** before processing
5. **Monitor for gaps** in event IDs and request missed events

## Troubleshooting

### Connection drops frequently

- Check network stability
- Ensure your event processing doesn't block
- Verify keep-alive messages are received

### Missing events after reconnection

- Verify you're passing the correct `last_id`
- Check server logs for event retention limits

### High CPU usage

- Ensure you're processing events efficiently
- Consider using a message queue between SSE reader and event processor

## Related

- [HTTP API Reference](http-api.md) - REST API endpoints
- [Engine Protocol](engine-protocol.md) - Engine stdio command format
