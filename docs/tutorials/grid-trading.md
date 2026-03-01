# Grid Trading Strategy

**Time:** 30 minutes | **Level:** Intermediate

## What You'll Learn

- Understand grid trading strategy fundamentals
- Configure grid parameters (price range, grid levels, order size)
- Deploy a grid trading strategy to testnet
- Monitor grid performance and active orders
- Adjust grid parameters during operation
- Handle grid rebalancing scenarios

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Binance testnet API keys (see [Binance Integration](../guides/user/binance.md))
- Understanding of limit orders and order books

---

## Step 1: Understand Grid Trading

Grid trading places buy and sell orders at regular price intervals within a defined range. When price oscillates, the strategy profits from buying low and selling high repeatedly.

Key concepts:

| Term | Description |
|------|-------------|
| **Grid Range** | Upper and lower price bounds |
| **Grid Levels** | Number of price levels within the range |
| **Grid Spacing** | Price difference between adjacent levels |
| **Order Size** | Quantity per grid order |

Example: A grid from 48,000 to 52,000 with 10 levels creates orders every 400 USDT.

---

## Step 2: Start the Gateway

Start the VeloZ gateway with testnet configuration.

```bash
# Set testnet environment
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret

# Start gateway
./scripts/run_gateway.sh dev
```

**Expected Output:**
```
Starting VeloZ Gateway...
Engine started in stdio mode
Gateway listening on http://127.0.0.1:8080
```

---

## Step 3: Check Current Market Price

Before configuring the grid, check the current market price to set appropriate bounds.

```bash
# Get current market data
curl http://127.0.0.1:8080/api/market
```

**Expected Output:**
```json
{
  "symbol": "BTCUSDT",
  "price": 50000.0,
  "bid": 49995.0,
  "ask": 50005.0,
  "timestamp": 1708704000000
}
```

Note the current price (50,000 in this example). Set your grid range around this price.

---

## Step 4: Configure Grid Parameters

Create a grid trading configuration centered around the current price.

```bash
# Create grid strategy
curl -X POST http://127.0.0.1:8080/api/strategy/load \
  -H "Content-Type: application/json" \
  -d '{
    "name": "btc_grid_1",
    "type": "GridTrading",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "lower_price": 48000.0,
      "upper_price": 52000.0,
      "grid_levels": 10,
      "order_size": 0.001,
      "mode": "arithmetic"
    }
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_grid_1",
  "grid_config": {
    "lower_price": 48000.0,
    "upper_price": 52000.0,
    "grid_levels": 10,
    "grid_spacing": 400.0,
    "total_investment": 0.01
  }
}
```

### Grid Parameters Explained

| Parameter | Value | Description |
|-----------|-------|-------------|
| `lower_price` | 48000.0 | Lowest buy order price |
| `upper_price` | 52000.0 | Highest sell order price |
| `grid_levels` | 10 | Number of grid lines |
| `order_size` | 0.001 | BTC per order |
| `mode` | arithmetic | Equal spacing between levels |

> **Tip:** Use `geometric` mode for percentage-based spacing, better for volatile assets.

---

## Step 5: Start the Grid Strategy

Activate the grid strategy to begin placing orders.

```bash
# Start the strategy
curl -X POST http://127.0.0.1:8080/api/strategy/btc_grid_1/start
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_grid_1",
  "status": "running",
  "orders_placed": 9,
  "buy_orders": 5,
  "sell_orders": 4
}
```

The strategy places buy orders below the current price and sell orders above it.

---

## Step 6: View Grid Orders

Check the orders placed by the grid strategy.

```bash
# Get grid orders
curl http://127.0.0.1:8080/api/strategy/btc_grid_1/orders
```

**Expected Output:**
```json
{
  "strategy_id": "btc_grid_1",
  "orders": [
    {
      "client_order_id": "grid-btc_grid_1-48000",
      "side": "BUY",
      "price": 48000.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-48400",
      "side": "BUY",
      "price": 48400.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-48800",
      "side": "BUY",
      "price": 48800.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-49200",
      "side": "BUY",
      "price": 49200.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-49600",
      "side": "BUY",
      "price": 49600.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-50400",
      "side": "SELL",
      "price": 50400.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-50800",
      "side": "SELL",
      "price": 50800.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-51200",
      "side": "SELL",
      "price": 51200.0,
      "qty": 0.001,
      "status": "NEW"
    },
    {
      "client_order_id": "grid-btc_grid_1-51600",
      "side": "SELL",
      "price": 51600.0,
      "qty": 0.001,
      "status": "NEW"
    }
  ]
}
```

---

## Step 7: Monitor Grid Performance

Subscribe to real-time updates to monitor grid activity.

```bash
# Subscribe to strategy events
curl -N "http://127.0.0.1:8080/api/stream?filter=strategy&strategy_id=btc_grid_1"
```

**Expected Output:**
```
event: grid_fill
data: {"strategy_id":"btc_grid_1","side":"BUY","price":49600.0,"qty":0.001,"profit":0.0}

event: grid_order_placed
data: {"strategy_id":"btc_grid_1","side":"SELL","price":50000.0,"qty":0.001}

event: grid_fill
data: {"strategy_id":"btc_grid_1","side":"SELL","price":50000.0,"qty":0.001,"profit":0.4}
```

When a buy order fills, the strategy places a sell order one level above. When a sell order fills, it places a buy order one level below.

---

## Step 8: Check Grid Statistics

View cumulative grid performance statistics.

```bash
# Get grid statistics
curl http://127.0.0.1:8080/api/strategy/btc_grid_1/stats
```

**Expected Output:**
```json
{
  "strategy_id": "btc_grid_1",
  "status": "running",
  "uptime_seconds": 3600,
  "statistics": {
    "total_trades": 24,
    "buy_fills": 12,
    "sell_fills": 12,
    "grid_profit": 4.8,
    "unrealized_pnl": 0.2,
    "total_pnl": 5.0,
    "avg_profit_per_trade": 0.4,
    "current_position": 0.002
  },
  "grid_state": {
    "active_buy_orders": 4,
    "active_sell_orders": 5,
    "filled_levels": 3
  }
}
```

---

## Step 9: Adjust Grid Parameters

Modify grid parameters without stopping the strategy.

```bash
# Update grid parameters
curl -X PATCH http://127.0.0.1:8080/api/strategy/btc_grid_1/parameters \
  -H "Content-Type: application/json" \
  -d '{
    "order_size": 0.002
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_grid_1",
  "updated_parameters": {
    "order_size": 0.002
  },
  "note": "New orders will use updated size"
}
```

> **Note:** Parameter changes apply to new orders only. Existing orders retain their original parameters.

---

## Step 10: Handle Grid Rebalancing

When price moves outside the grid range, rebalance the grid.

### Check if Rebalancing is Needed

```bash
# Get grid status
curl http://127.0.0.1:8080/api/strategy/btc_grid_1/state
```

**Expected Output (price outside range):**
```json
{
  "strategy_id": "btc_grid_1",
  "status": "running",
  "warning": "price_outside_range",
  "current_price": 53000.0,
  "grid_range": {
    "lower": 48000.0,
    "upper": 52000.0
  },
  "recommendation": "rebalance_grid"
}
```

### Rebalance the Grid

```bash
# Rebalance grid around current price
curl -X POST http://127.0.0.1:8080/api/strategy/btc_grid_1/rebalance \
  -H "Content-Type: application/json" \
  -d '{
    "center_price": 53000.0,
    "range_percent": 5.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_grid_1",
  "old_range": {
    "lower": 48000.0,
    "upper": 52000.0
  },
  "new_range": {
    "lower": 50350.0,
    "upper": 55650.0
  },
  "orders_cancelled": 9,
  "orders_placed": 9
}
```

---

## Step 11: Stop the Grid Strategy

When finished, stop the strategy and cancel all orders.

```bash
# Stop strategy
curl -X POST http://127.0.0.1:8080/api/strategy/btc_grid_1/stop
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_grid_1",
  "status": "stopped",
  "orders_cancelled": 9,
  "final_pnl": 5.0,
  "position_remaining": 0.002
}
```

### Close Remaining Position (Optional)

```bash
# Close remaining position
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "SELL",
    "symbol": "BTCUSDT",
    "qty": 0.002,
    "type": "MARKET"
  }'
```

---

## Summary

**What you accomplished:**
- Configured a grid trading strategy with appropriate parameters
- Deployed the grid to testnet and monitored order placement
- Tracked grid fills and profit accumulation
- Adjusted parameters during operation
- Rebalanced the grid when price moved outside range
- Stopped the strategy and handled remaining positions

## Troubleshooting

### Issue: Grid orders not filling
**Symptom:** Orders remain in NEW status for extended periods
**Solution:** Check if the grid range is appropriate for current volatility:
```bash
# Check 24h price range
curl http://127.0.0.1:8080/api/market/stats?symbol=BTCUSDT
```
Widen the grid range or reduce grid levels for more spacing.

### Issue: Insufficient balance for grid orders
**Symptom:** Error "insufficient_balance" when starting grid
**Solution:** Calculate required capital:
- Buy side: `(grid_levels / 2) * order_size * avg_buy_price`
- Sell side: `(grid_levels / 2) * order_size` in base asset

### Issue: Grid profit lower than expected
**Symptom:** Low profit despite many fills
**Solution:** Check trading fees. Grid profit must exceed:
```
min_spacing = 2 * fee_rate * price
```
For 0.1% fees at 50,000 USDT: min_spacing = 100 USDT

### Issue: Price moved outside grid range
**Symptom:** No orders filling, warning about price outside range
**Solution:** Rebalance the grid (Step 10) or wait for price to return to range.

## Next Steps

- [Market Making Strategy](./market-making.md) - Learn market making techniques
- [Risk Management in Practice](./risk-management-practice.md) - Add risk controls to your grid
- [Trading Guide](../guides/user/trading-guide.md) - Advanced trading concepts
- [Monitoring Guide](../guides/user/monitoring.md) - Set up performance dashboards
- [Glossary](../guides/user/glossary.md) - Definitions of trading and technical terms
