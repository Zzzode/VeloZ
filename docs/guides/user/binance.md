# Binance Integration Guide

This guide explains how to connect VeloZ to Binance for live market data and trading.

## Overview

VeloZ supports comprehensive Binance integration for:
- **Real-time market data** via REST API and WebSocket (depth, trades, klines)
- **Production trading** with full order lifecycle management (P3-001)
- **Depth data processing** with order book reconstruction (P2-001)
- **Historical data download** for backtesting (P5-003)
- **Kline aggregation** from tick data (P2-003)
- Account balance monitoring
- Order status tracking

## Prerequisites

1. **Binance Account**: Create an account at [binance.com](https://www.binance.com)
2. **Testnet Access**: For testing, use [testnet.binance.vision](https://testnet.binance.vision)
3. **API Keys**: Generate API keys from your Binance account settings

## Getting API Keys

### Production Keys (Live Trading)

1. Log in to [binance.com](https://www.binance.com)
2. Go to **API Management** in account settings
3. Click **Create API**
4. Enable **Spot Trading** permission
5. Save your API Key and Secret Key securely

### Testnet Keys (Recommended for Testing)

1. Go to [testnet.binance.vision](https://testnet.binance.vision)
2. Log in with GitHub
3. Click **Generate HMAC_SHA256 Key**
4. Save your API Key and Secret Key

## Configuration

### Environment Variables

Set these environment variables before starting the gateway:

```bash
# Market Data Source
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com

# Execution Mode (testnet)
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=<your-api-key>
export VELOZ_BINANCE_API_SECRET=<your-api-secret>

# WebSocket (optional, for user data stream)
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws
```

### Configuration Summary

| Variable | Testnet Value | Production Value |
|----------|---------------|------------------|
| `VELOZ_BINANCE_BASE_URL` | `https://testnet.binance.vision` | `https://api.binance.com` |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://testnet.binance.vision` | `https://api.binance.com` |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://testnet.binance.vision/ws` | `wss://stream.binance.com:9443/ws` |

## Starting with Binance

### 1. Start Gateway with Binance Market Data

```bash
# Using testnet
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://testnet.binance.vision

./scripts/run_gateway.sh dev
```

### 2. Verify Connection

```bash
# Check configuration
curl http://127.0.0.1:8080/api/config

# Expected response:
# {
#   "market_source": "binance_rest",
#   "market_symbol": "BTCUSDT",
#   "binance_base_url": "https://testnet.binance.vision",
#   ...
# }

# Check market data
curl http://127.0.0.1:8080/api/market

# Expected response:
# {
#   "symbol": "BTCUSDT",
#   "price": 42000.50,
#   "ts_ns": 1704067200000000000
# }
```

### 3. Enable Trading (Testnet)

```bash
# Set execution mode and credentials
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret

./scripts/run_gateway.sh dev
```

### 4. Verify Trading Connection

```bash
# Ping execution router
curl http://127.0.0.1:8080/api/execution/ping

# Expected response:
# {
#   "ok": true,
#   "result": {}
# }
```

## Placing Orders

### Place a Limit Order

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 40000.0,
    "client_order_id": "my-order-001"
  }'

# Response:
# {
#   "ok": true,
#   "client_order_id": "my-order-001",
#   "venue_order_id": "123456789"
# }
```

### Cancel an Order

```bash
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{
    "client_order_id": "my-order-001",
    "symbol": "BTCUSDT"
  }'

# Response:
# {
#   "ok": true,
#   "client_order_id": "my-order-001"
# }
```

### Check Order Status

```bash
curl "http://127.0.0.1:8080/api/order_state?client_order_id=my-order-001"

# Response:
# {
#   "client_order_id": "my-order-001",
#   "symbol": "BTCUSDT",
#   "side": "BUY",
#   "order_qty": 0.001,
#   "limit_price": 40000.0,
#   "venue_order_id": "123456789",
#   "status": "ACCEPTED",
#   "executed_qty": 0.0,
#   "avg_price": null,
#   "last_ts_ns": 1704067200000000000
# }
```

## Real-Time Updates

### Subscribe to SSE Stream

```bash
curl -N http://127.0.0.1:8080/api/stream
```

### Event Types

**Market Data**
```
event: market
data: {"type":"market","symbol":"BTCUSDT","price":42000.50,"ts_ns":1704067200000000000}
```

**Order Updates**
```
event: order_update
data: {"type":"order_update","client_order_id":"my-order-001","status":"FILLED","symbol":"BTCUSDT"}
```

**Fills**
```
event: fill
data: {"type":"fill","client_order_id":"my-order-001","symbol":"BTCUSDT","qty":0.001,"price":42000.50}
```

**Account Updates**
```
event: account
data: {"type":"account","ts_ns":1704067200000000000}
```

## Account Information

### Get Account Balance

```bash
curl http://127.0.0.1:8080/api/account

# Response:
# {
#   "ts_ns": 1704067200000000000,
#   "balances": [
#     {"asset": "USDT", "free": 10000.0, "locked": 0.0},
#     {"asset": "BTC", "free": 0.5, "locked": 0.001}
#   ]
# }
```

## Order States

| State | Description |
|-------|-------------|
| `ACCEPTED` | Order accepted by Binance |
| `PARTIALLY_FILLED` | Order partially filled |
| `FILLED` | Order fully filled |
| `CANCELLED` | Order cancelled |
| `REJECTED` | Order rejected by Binance |
| `EXPIRED` | Order expired |

## WebSocket User Stream

VeloZ automatically connects to Binance's User Data Stream for real-time order and account updates when:
- `VELOZ_EXECUTION_MODE=binance_testnet_spot`
- Valid API credentials are configured

The user stream provides:
- Instant order status updates
- Real-time fill notifications
- Account balance changes

Check connection status:
```bash
curl http://127.0.0.1:8080/api/config | jq '.binance_user_stream_connected'
```

## Rate Limits

Binance enforces rate limits. VeloZ handles these automatically:

| Endpoint Type | Limit |
|---------------|-------|
| REST API | 1200 requests/minute |
| Order placement | 10 orders/second |
| WebSocket connections | 5 per IP |

## Troubleshooting

### "binance_not_configured" Error

Ensure all required environment variables are set:
```bash
echo $VELOZ_BINANCE_API_KEY
echo $VELOZ_BINANCE_API_SECRET
echo $VELOZ_EXECUTION_MODE
```

### Order Rejected

Common reasons:
- Insufficient balance
- Invalid symbol
- Price outside allowed range
- Quantity below minimum

Check the error response for details.

### WebSocket Disconnects

The gateway automatically reconnects with exponential backoff. Check logs for:
```
[AUDIT] binance_ws_error
```

### Market Data Not Updating

1. Verify market source is set:
   ```bash
   curl http://127.0.0.1:8080/api/config | jq '.market_source'
   ```

2. Check for errors in SSE stream:
   ```bash
   curl -N http://127.0.0.1:8080/api/stream | grep error
   ```

## Security Best Practices

1. **Never commit API keys** to version control
2. **Use testnet** for development and testing
3. **Restrict API permissions** to only what's needed
4. **Enable IP whitelist** on Binance for production
5. **Rotate keys regularly**
6. **Monitor for unauthorized activity**

## Example: Complete Setup Script

```bash
#!/bin/bash
# setup_binance_testnet.sh

# Testnet configuration
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://testnet.binance.vision
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws

# Load API keys from secure location
source ~/.veloz/binance_testnet_keys.sh

# Start gateway
./scripts/run_gateway.sh dev
```

## Production Trading Features (P3-001)

VeloZ supports full production trading on Binance with:

### Order Types

| Order Type | Description | Parameters |
|------------|-------------|------------|
| `LIMIT` | Limit order at specified price | `price`, `qty` |
| `MARKET` | Market order at best price | `qty` |
| `STOP_LOSS` | Stop loss trigger | `stop_price`, `qty` |
| `STOP_LOSS_LIMIT` | Stop loss with limit | `stop_price`, `price`, `qty` |
| `TAKE_PROFIT` | Take profit trigger | `stop_price`, `qty` |
| `TAKE_PROFIT_LIMIT` | Take profit with limit | `stop_price`, `price`, `qty` |

### HMAC Authentication

All trading requests are signed using HMAC-SHA256:
- Timestamp included in all requests
- Signature computed over query string
- Automatic recvWindow handling

### Rate Limiting

VeloZ implements automatic rate limiting:
- Request weight tracking per endpoint
- Automatic backoff when approaching limits
- Order rate limiting (10 orders/second)

## Depth Data Processing (P2-001)

### Order Book Subscription

Subscribe to real-time order book updates:

```bash
# Subscribe via HTTP API
curl -X POST http://127.0.0.1:8080/api/subscribe \
  -H "Content-Type: application/json" \
  -d '{"symbol": "BTCUSDT", "event_type": "depth"}'
```

### SSE Depth Events

**Book Top (best bid/ask):**
```
event: book_top
data: {"type":"book_top","symbol":"BTCUSDT","venue":"Binance","bid_price":42000.50,"bid_qty":1.5,"ask_price":42001.00,"ask_qty":2.0,"ts_ns":1704067200000000000}
```

**Book Delta (incremental updates):**
```
event: book_delta
data: {"type":"book_delta","symbol":"BTCUSDT","venue":"Binance","sequence":12345,"bids":[[42000.50,1.5],[41999.00,3.0]],"asks":[[42001.00,2.0]],"ts_ns":1704067200000000000}
```

### Order Book Features

- Sequence-based reconstruction with gap detection
- Best bid/ask tracking
- Spread calculation
- Price level aggregation

## Historical Data Download (P5-003)

### Download Kline Data

Download historical kline data for backtesting:

```cpp
// C++ API
BinanceDataSource dataSource;
dataSource.download_data(
    "BTCUSDT",           // symbol
    1609459200000,       // start_time (ms)
    1640995200000,       // end_time (ms)
    "kline",             // data_type
    "1h",                // time_frame
    "/data/btcusdt_1h.csv"  // output_path
);
```

### Supported Time Frames

| Time Frame | Description |
|------------|-------------|
| `1m` | 1 minute |
| `5m` | 5 minutes |
| `15m` | 15 minutes |
| `1h` | 1 hour |
| `4h` | 4 hours |
| `1d` | 1 day |

### Download Progress Tracking

```cpp
dataSource.download_data_with_progress(
    "BTCUSDT", start, end, "kline", "1h", "/data/output.csv",
    [](double progress, int64_t downloaded, int64_t total) {
        std::cout << "Progress: " << (progress * 100) << "%" << std::endl;
    }
);
```

## Kline Aggregation (P2-003)

### Real-Time Kline Building

VeloZ aggregates tick data into klines in real-time:

```cpp
KlineAggregator aggregator;
aggregator.set_period("1m");
aggregator.on_tick(tick);  // Process each tick

// Get completed kline
KJ_IF_SOME(kline, aggregator.get_kline("BTCUSDT", "1m")) {
    // Use kline data: open, high, low, close, volume
}
```

### SSE Kline Events

```
event: kline
data: {"type":"kline","symbol":"BTCUSDT","period":"1m","open":42000.0,"high":42100.0,"low":41950.0,"close":42050.0,"volume":150.5,"ts_ns":1704067200000000000}
```

### Multi-Timeframe Support

Subscribe to multiple timeframes simultaneously:

```bash
curl -X POST http://127.0.0.1:8080/api/subscribe \
  -d '{"symbol": "BTCUSDT", "event_type": "kline", "interval": "1m"}'

curl -X POST http://127.0.0.1:8080/api/subscribe \
  -d '{"symbol": "BTCUSDT", "event_type": "kline", "interval": "1h"}'
```

## Connection Troubleshooting

### Common Connection Issues

#### WebSocket Connection Failed

**Symptoms:** No market data updates, connection timeout

**Solutions:**
1. Check network connectivity to Binance endpoints
2. Verify firewall allows WebSocket connections (port 443)
3. Check TLS certificate validity
4. Try testnet endpoint first

```bash
# Test WebSocket connectivity
wscat -c wss://stream.binance.com:9443/ws/btcusdt@trade
```

#### Authentication Errors

**Symptoms:** `{"code":-2015,"msg":"Invalid API-key, IP, or permissions for action"}`

**Solutions:**
1. Verify API key and secret are correct
2. Check IP whitelist settings on Binance
3. Ensure Spot Trading permission is enabled
4. Check timestamp synchronization (within 5 seconds)

```bash
# Check system time sync
timedatectl status
```

#### Rate Limit Exceeded

**Symptoms:** `{"code":-1015,"msg":"Too many requests"}`

**Solutions:**
1. VeloZ handles rate limiting automatically
2. Check for multiple instances running
3. Reduce request frequency in custom code
4. Wait for rate limit window to reset (1 minute)

#### Order Rejected

**Symptoms:** Order placement fails with error code

**Common Error Codes:**

| Code | Message | Solution |
|------|---------|----------|
| -1013 | Filter failure: MIN_NOTIONAL | Increase order value (qty * price) |
| -1021 | Timestamp outside recvWindow | Sync system clock |
| -2010 | Insufficient balance | Check account balance |
| -2011 | Unknown order | Order already filled/cancelled |

#### Reconnection Issues

VeloZ implements automatic reconnection with exponential backoff:

```
Attempt 1: Wait 100ms
Attempt 2: Wait 200ms
Attempt 3: Wait 400ms
...
Maximum: Wait 30 seconds
```

Check reconnection status:
```bash
curl http://127.0.0.1:8080/api/config | jq '.websocket_reconnect_count'
```

### Debug Logging

Enable verbose logging for troubleshooting:

```bash
export VELOZ_LOG_LEVEL=DEBUG
./scripts/run_gateway.sh dev
```

Look for these log patterns:
```
[DEBUG] binance_ws: Connecting to wss://stream.binance.com:9443/ws
[DEBUG] binance_ws: WebSocket handshake complete
[DEBUG] binance_ws: Subscribed to btcusdt@depth
[WARN]  binance_ws: Connection lost, reconnecting...
[ERROR] binance_ws: Authentication failed: invalid signature
```

## Related

- [HTTP API Reference](../../api/http_api.md) - Complete API documentation
- [SSE API](../../api/sse_api.md) - Real-time event stream
- [Configuration Guide](configuration.md) - All configuration options
- [Getting Started](getting-started.md) - Quick start guide
- [Trading Guide](trading-guide.md) - Order types and execution algorithms
- [Glossary](glossary.md) - Exchange and trading term definitions
