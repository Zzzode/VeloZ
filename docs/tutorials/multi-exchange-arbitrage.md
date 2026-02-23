# Multi-Exchange Arbitrage

**Time:** 25 minutes | **Level:** Intermediate

## What You'll Learn

- Configure multiple exchange connections
- Monitor price differences across venues
- Execute arbitrage trades using smart order routing
- Manage cross-exchange positions
- Handle reconciliation across venues

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- API keys for at least two exchanges (testnet recommended)
- Understanding of arbitrage concepts

---

## Step 1: Configure Multiple Exchanges

Set up connections to multiple exchanges. This example uses Binance and OKX testnets.

```bash
# Binance testnet configuration
export VELOZ_BINANCE_API_KEY=your_binance_testnet_key
export VELOZ_BINANCE_API_SECRET=your_binance_testnet_secret
export VELOZ_BINANCE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision

# OKX testnet configuration
export VELOZ_OKX_API_KEY=your_okx_testnet_key
export VELOZ_OKX_API_SECRET=your_okx_testnet_secret
export VELOZ_OKX_PASSPHRASE=your_okx_passphrase
export VELOZ_OKX_BASE_URL=https://www.okx.com

# Enable multi-exchange mode
export VELOZ_MULTI_EXCHANGE=true
export VELOZ_EXCHANGES=binance,okx
```

> **Warning:** Never use production API keys for testing. Always use testnet credentials.

---

## Step 2: Start the Gateway

Start the gateway with multi-exchange configuration.

```bash
# Start gateway with multi-exchange support
./scripts/run_gateway.sh dev
```

**Expected Output:**
```
Starting VeloZ Gateway...
Connecting to exchanges: binance, okx
Binance: connected
OKX: connected
Gateway listening on http://127.0.0.1:8080
```

---

## Step 3: Verify Exchange Connections

Check that all configured exchanges are connected and operational.

```bash
# Check exchange status
curl http://127.0.0.1:8080/api/exchanges
```

**Expected Output:**
```json
{
  "exchanges": [
    {
      "name": "binance",
      "status": "connected",
      "latency_ms": 45,
      "last_heartbeat": 1708704000000
    },
    {
      "name": "okx",
      "status": "connected",
      "latency_ms": 62,
      "last_heartbeat": 1708704000000
    }
  ]
}
```

---

## Step 4: Monitor Cross-Exchange Prices

Fetch prices from all connected exchanges to identify arbitrage opportunities.

```bash
# Get prices from all exchanges
curl http://127.0.0.1:8080/api/market/all?symbol=BTCUSDT
```

**Expected Output:**
```json
{
  "symbol": "BTCUSDT",
  "prices": [
    {
      "exchange": "binance",
      "bid": 50000.0,
      "ask": 50010.0,
      "timestamp": 1708704000000
    },
    {
      "exchange": "okx",
      "bid": 50015.0,
      "ask": 50025.0,
      "timestamp": 1708704000000
    }
  ],
  "spread": {
    "best_bid": {"exchange": "okx", "price": 50015.0},
    "best_ask": {"exchange": "binance", "price": 50010.0},
    "arbitrage_opportunity": true,
    "potential_profit_bps": 1.0
  }
}
```

In this example, you could buy on Binance at 50,010 and sell on OKX at 50,015 for a 5 USDT profit per BTC (before fees).

---

## Step 5: Subscribe to Price Updates

Monitor real-time price updates across all exchanges.

```bash
# Subscribe to multi-exchange price stream
curl -N "http://127.0.0.1:8080/api/stream?filter=market&exchanges=binance,okx"
```

**Expected Output:**
```
event: market_update
data: {"exchange":"binance","symbol":"BTCUSDT","bid":50000.0,"ask":50010.0}

event: market_update
data: {"exchange":"okx","symbol":"BTCUSDT","bid":50015.0,"ask":50025.0}

event: arbitrage_signal
data: {"buy_exchange":"binance","sell_exchange":"okx","spread_bps":1.0,"profitable":true}
```

---

## Step 6: Execute an Arbitrage Trade

When an arbitrage opportunity is detected, execute simultaneous buy and sell orders.

### Buy on the Lower-Priced Exchange

```bash
# Buy on Binance (lower ask price)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "exchange": "binance",
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.01,
    "price": 50010.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "exchange": "binance",
  "client_order_id": "arb-buy-1708704000000",
  "venue_order_id": "bin-12345678"
}
```

### Sell on the Higher-Priced Exchange

```bash
# Sell on OKX (higher bid price)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "exchange": "okx",
    "side": "SELL",
    "symbol": "BTCUSDT",
    "qty": 0.01,
    "price": 50015.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "exchange": "okx",
  "client_order_id": "arb-sell-1708704000000",
  "venue_order_id": "okx-87654321"
}
```

---

## Step 7: Use Smart Order Routing

Let VeloZ automatically route orders to the best exchange.

```bash
# Place order with smart routing
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "routing": "best_price",
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.01
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "routed_to": "binance",
  "reason": "best_ask_price",
  "client_order_id": "smart-1708704000000",
  "venue_order_id": "bin-12345679",
  "price": 50010.0
}
```

### Routing Options

| Routing Mode | Description |
|--------------|-------------|
| `best_price` | Route to exchange with best price |
| `lowest_fee` | Route to exchange with lowest fees |
| `fastest` | Route to exchange with lowest latency |
| `split` | Split order across multiple exchanges |

---

## Step 8: View Cross-Exchange Positions

Monitor your positions across all connected exchanges.

```bash
# Get positions from all exchanges
curl http://127.0.0.1:8080/api/positions/all
```

**Expected Output:**
```json
{
  "positions": [
    {
      "exchange": "binance",
      "symbol": "BTCUSDT",
      "size": 0.01,
      "entry_price": 50010.0,
      "unrealized_pnl": 0.0
    },
    {
      "exchange": "okx",
      "symbol": "BTCUSDT",
      "size": -0.01,
      "entry_price": 50015.0,
      "unrealized_pnl": 0.0
    }
  ],
  "aggregate": {
    "symbol": "BTCUSDT",
    "net_size": 0.0,
    "total_value": 0.0,
    "realized_pnl": 0.5
  }
}
```

The aggregate view shows your net position across all exchanges. In this arbitrage example, the net position is zero (long on Binance, short on OKX).

---

## Step 9: Configure Per-Exchange Risk Limits

Set different risk limits for each exchange.

```bash
# Set Binance risk limits
curl -X POST http://127.0.0.1:8080/api/risk/limits \
  -H "Content-Type: application/json" \
  -d '{
    "exchange": "binance",
    "max_position_size": 1.0,
    "max_order_size": 0.1,
    "max_daily_volume": 10.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "exchange": "binance",
  "limits_applied": {
    "max_position_size": 1.0,
    "max_order_size": 0.1,
    "max_daily_volume": 10.0
  }
}
```

---

## Step 10: Trigger Reconciliation

Ensure local order state matches exchange state across all venues.

```bash
# Reconcile all exchanges
curl -X POST http://127.0.0.1:8080/api/reconcile
```

**Expected Output:**
```json
{
  "ok": true,
  "results": [
    {
      "exchange": "binance",
      "orders_synced": 5,
      "positions_synced": 1,
      "discrepancies": 0
    },
    {
      "exchange": "okx",
      "orders_synced": 3,
      "positions_synced": 1,
      "discrepancies": 0
    }
  ]
}
```

> **Tip:** Reconciliation runs automatically every 30 seconds. Use manual reconciliation after network issues.

---

## Summary

**What you accomplished:**
- Configured connections to multiple exchanges
- Monitored cross-exchange price differences
- Executed arbitrage trades across venues
- Used smart order routing for optimal execution
- Managed positions across multiple exchanges
- Configured per-exchange risk limits

## Troubleshooting

### Issue: Exchange connection failed
**Symptom:** Exchange status shows "disconnected"
**Solution:** Verify API credentials and network connectivity:
```bash
# Test Binance connectivity
curl https://testnet.binance.vision/api/v3/ping
```

### Issue: Order rejected on one exchange
**Symptom:** Buy succeeds but sell fails
**Solution:** Check balance on the selling exchange. Ensure you have sufficient assets to sell.

### Issue: Price data stale
**Symptom:** Timestamps are old, arbitrage signals unreliable
**Solution:** Check WebSocket connections and exchange rate limits.

### Issue: Reconciliation shows discrepancies
**Symptom:** Local state differs from exchange state
**Solution:** Review discrepancy details and manually sync if needed:
```bash
curl http://127.0.0.1:8080/api/reconcile/discrepancies
```

## Next Steps

- [Risk Management Guide](../guides/user/risk-management.md) - Add VaR and circuit breakers to your strategy
- [Monitoring Guide](../guides/user/monitoring.md) - Set up Prometheus, Grafana for multi-exchange observability
- [Trading Guide](../guides/user/trading-guide.md) - Learn about smart order routing and execution algorithms
- [Production Deployment](./production-deployment.md) - Deploy your arbitrage system to production
