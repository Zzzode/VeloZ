# Binance Integration Guide

This guide explains how to connect VeloZ to Binance for live market data and trading.

## Overview

VeloZ supports Binance integration for:
- Real-time market data via REST API and WebSocket
- Order execution on Binance Spot testnet
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
export VELOZ_BINANCE_API_KEY=your_api_key_here
export VELOZ_BINANCE_API_SECRET=your_api_secret_here

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

## Related

- [HTTP API Reference](../api/http_api.md) - Complete API documentation
- [SSE API](../api/sse_api.md) - Real-time event stream
- [Configuration Guide](configuration.md) - All configuration options
- [Getting Started](getting-started.md) - Quick start guide
