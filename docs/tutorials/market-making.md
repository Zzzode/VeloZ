# Market Making Strategy

**Time:** 35 minutes | **Level:** Intermediate

## What You'll Learn

- Understand market making fundamentals and profit mechanics
- Configure bid/ask spread and order sizing
- Deploy a market making strategy with inventory management
- Monitor quote updates and fill rates
- Implement risk controls for adverse selection
- Track P&L and performance metrics

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Binance testnet API keys (see [Binance Integration](../guides/user/binance.md))
- Understanding of bid/ask spreads and order books

---

## Step 1: Understand Market Making

Market makers provide liquidity by continuously quoting bid and ask prices. Profit comes from the spread between buy and sell prices.

Key concepts:

| Term | Description |
|------|-------------|
| **Spread** | Difference between ask and bid prices |
| **Mid Price** | Average of best bid and ask |
| **Inventory** | Current position held by the market maker |
| **Skew** | Adjusting quotes based on inventory |
| **Adverse Selection** | Risk of trading against informed traders |

Example: Quoting bid at 49,990 and ask at 50,010 creates a 20 USDT spread. Each round-trip earns 20 USDT minus fees.

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

## Step 3: Analyze Market Conditions

Before market making, analyze current market conditions to set appropriate parameters.

```bash
# Get order book depth
curl http://127.0.0.1:8080/api/market/depth?symbol=BTCUSDT&limit=5
```

**Expected Output:**
```json
{
  "symbol": "BTCUSDT",
  "bids": [
    {"price": 49995.0, "qty": 1.5},
    {"price": 49990.0, "qty": 2.3},
    {"price": 49985.0, "qty": 0.8},
    {"price": 49980.0, "qty": 1.2},
    {"price": 49975.0, "qty": 3.1}
  ],
  "asks": [
    {"price": 50005.0, "qty": 1.2},
    {"price": 50010.0, "qty": 1.8},
    {"price": 50015.0, "qty": 0.9},
    {"price": 50020.0, "qty": 2.1},
    {"price": 50025.0, "qty": 1.5}
  ],
  "spread": 10.0,
  "mid_price": 50000.0
}
```

Note the current spread (10 USDT). Set your spread wider than this to avoid immediate fills.

---

## Step 4: Configure Market Making Strategy

Create a market making strategy with conservative parameters.

```bash
# Create market making strategy
curl -X POST http://127.0.0.1:8080/api/strategy/load \
  -H "Content-Type: application/json" \
  -d '{
    "name": "btc_mm_1",
    "type": "MarketMaking",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "spread_bps": 4.0,
      "order_size": 0.001,
      "max_position": 0.01,
      "quote_levels": 3,
      "level_spacing_bps": 2.0,
      "inventory_skew": true,
      "skew_factor": 0.5
    }
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "config": {
    "spread_bps": 4.0,
    "half_spread_bps": 2.0,
    "order_size": 0.001,
    "max_position": 0.01,
    "quote_levels": 3
  }
}
```

### Parameters Explained

| Parameter | Value | Description |
|-----------|-------|-------------|
| `spread_bps` | 4.0 | Total spread in basis points (0.04%) |
| `order_size` | 0.001 | BTC per quote |
| `max_position` | 0.01 | Maximum inventory (long or short) |
| `quote_levels` | 3 | Number of quote levels on each side |
| `level_spacing_bps` | 2.0 | Spacing between quote levels |
| `inventory_skew` | true | Adjust quotes based on position |
| `skew_factor` | 0.5 | How aggressively to skew quotes |

> **Tip:** Start with wider spreads (higher `spread_bps`) and tighten as you gain confidence.

---

## Step 5: Start Market Making

Activate the market making strategy.

```bash
# Start the strategy
curl -X POST http://127.0.0.1:8080/api/strategy/btc_mm_1/start
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "status": "running",
  "initial_quotes": {
    "bid_levels": [
      {"price": 49990.0, "qty": 0.001},
      {"price": 49980.0, "qty": 0.001},
      {"price": 49970.0, "qty": 0.001}
    ],
    "ask_levels": [
      {"price": 50010.0, "qty": 0.001},
      {"price": 50020.0, "qty": 0.001},
      {"price": 50030.0, "qty": 0.001}
    ]
  }
}
```

---

## Step 6: Monitor Quote Updates

Subscribe to real-time quote and fill events.

```bash
# Subscribe to market making events
curl -N "http://127.0.0.1:8080/api/stream?filter=strategy&strategy_id=btc_mm_1"
```

**Expected Output:**
```
event: quote_update
data: {"strategy_id":"btc_mm_1","mid_price":50000.0,"bid":49990.0,"ask":50010.0,"spread_bps":4.0}

event: fill
data: {"strategy_id":"btc_mm_1","side":"BUY","price":49990.0,"qty":0.001,"inventory":0.001}

event: quote_update
data: {"strategy_id":"btc_mm_1","mid_price":50000.0,"bid":49988.0,"ask":50012.0,"spread_bps":4.8,"skew":"long"}

event: fill
data: {"strategy_id":"btc_mm_1","side":"SELL","price":50012.0,"qty":0.001,"inventory":0.0,"round_trip_profit":22.0}
```

Notice how quotes skew after a fill to encourage inventory reduction.

---

## Step 7: View Current Quotes

Check the current state of your quotes.

```bash
# Get current quotes
curl http://127.0.0.1:8080/api/strategy/btc_mm_1/quotes
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "mid_price": 50000.0,
  "quotes": {
    "bids": [
      {"level": 1, "price": 49990.0, "qty": 0.001, "status": "ACTIVE"},
      {"level": 2, "price": 49980.0, "qty": 0.001, "status": "ACTIVE"},
      {"level": 3, "price": 49970.0, "qty": 0.001, "status": "ACTIVE"}
    ],
    "asks": [
      {"level": 1, "price": 50010.0, "qty": 0.001, "status": "ACTIVE"},
      {"level": 2, "price": 50020.0, "qty": 0.001, "status": "ACTIVE"},
      {"level": 3, "price": 50030.0, "qty": 0.001, "status": "ACTIVE"}
    ]
  },
  "inventory": {
    "position": 0.002,
    "avg_entry": 49985.0,
    "unrealized_pnl": 30.0
  }
}
```

---

## Step 8: Configure Risk Controls

Add risk controls to protect against adverse price movements.

```bash
# Update risk parameters
curl -X PATCH http://127.0.0.1:8080/api/strategy/btc_mm_1/parameters \
  -H "Content-Type: application/json" \
  -d '{
    "max_position": 0.005,
    "stop_loss_bps": 50.0,
    "pause_on_volatility": true,
    "volatility_threshold": 100.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "updated_parameters": {
    "max_position": 0.005,
    "stop_loss_bps": 50.0,
    "pause_on_volatility": true,
    "volatility_threshold": 100.0
  }
}
```

### Risk Parameters Explained

| Parameter | Value | Description |
|-----------|-------|-------------|
| `max_position` | 0.005 | Reduced max inventory |
| `stop_loss_bps` | 50.0 | Close position if loss exceeds 0.5% |
| `pause_on_volatility` | true | Stop quoting during high volatility |
| `volatility_threshold` | 100.0 | Pause if 1-min volatility exceeds 1% |

---

## Step 9: Monitor Performance Metrics

Track key performance indicators for your market making strategy.

```bash
# Get performance metrics
curl http://127.0.0.1:8080/api/strategy/btc_mm_1/metrics
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "uptime_seconds": 3600,
  "performance": {
    "total_trades": 48,
    "buy_fills": 25,
    "sell_fills": 23,
    "round_trips": 22,
    "gross_profit": 440.0,
    "fees_paid": 48.0,
    "net_profit": 392.0,
    "profit_per_round_trip": 17.8
  },
  "inventory": {
    "current_position": 0.002,
    "max_position_reached": 0.004,
    "avg_holding_time_seconds": 120
  },
  "quoting": {
    "quote_updates": 1200,
    "time_at_best_bid_pct": 45.0,
    "time_at_best_ask_pct": 42.0,
    "fill_rate_pct": 4.0
  },
  "risk": {
    "max_drawdown": 15.0,
    "volatility_pauses": 2,
    "adverse_selection_events": 3
  }
}
```

### Key Metrics to Monitor

| Metric | Good Range | Action if Outside |
|--------|------------|-------------------|
| `fill_rate_pct` | 2-10% | Adjust spread |
| `profit_per_round_trip` | > fees * 2 | Widen spread |
| `avg_holding_time_seconds` | < 300 | Increase skew factor |
| `time_at_best_bid_pct` | > 30% | Tighten spread |

---

## Step 10: Handle Inventory Buildup

When inventory accumulates, take action to reduce risk.

### Check Inventory Status

```bash
# Get inventory details
curl http://127.0.0.1:8080/api/strategy/btc_mm_1/inventory
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "inventory": {
    "position": 0.008,
    "direction": "long",
    "avg_entry": 49950.0,
    "current_price": 49900.0,
    "unrealized_pnl": -0.4,
    "pct_of_max": 80.0
  },
  "recommendation": "increase_skew"
}
```

### Manually Reduce Inventory

```bash
# Reduce inventory with aggressive quote
curl -X POST http://127.0.0.1:8080/api/strategy/btc_mm_1/reduce_inventory \
  -H "Content-Type: application/json" \
  -d '{
    "target_position": 0.002,
    "aggression": "moderate"
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "action": "inventory_reduction",
  "method": "aggressive_ask",
  "target_position": 0.002,
  "current_position": 0.008,
  "qty_to_sell": 0.006
}
```

---

## Step 11: Analyze Trade History

Review individual trades for strategy refinement.

```bash
# Get recent trades
curl "http://127.0.0.1:8080/api/strategy/btc_mm_1/trades?limit=10"
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "trades": [
    {
      "timestamp": 1708704000000,
      "side": "BUY",
      "price": 49990.0,
      "qty": 0.001,
      "fee": 0.05,
      "inventory_after": 0.003
    },
    {
      "timestamp": 1708704005000,
      "side": "SELL",
      "price": 50010.0,
      "qty": 0.001,
      "fee": 0.05,
      "inventory_after": 0.002,
      "round_trip_profit": 19.9
    },
    {
      "timestamp": 1708704010000,
      "side": "BUY",
      "price": 49985.0,
      "qty": 0.001,
      "fee": 0.05,
      "inventory_after": 0.003
    }
  ]
}
```

---

## Step 12: Stop Market Making

When finished, stop the strategy gracefully.

```bash
# Stop strategy
curl -X POST http://127.0.0.1:8080/api/strategy/btc_mm_1/stop
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "status": "stopped",
  "final_state": {
    "quotes_cancelled": 6,
    "position": 0.002,
    "unrealized_pnl": 10.0,
    "realized_pnl": 392.0,
    "total_pnl": 402.0
  }
}
```

### Close Remaining Position (Optional)

```bash
# Market sell remaining inventory
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
- Understood market making profit mechanics
- Configured spread, sizing, and inventory parameters
- Deployed a market making strategy with multiple quote levels
- Monitored quote updates and fill events in real-time
- Implemented risk controls for volatility and adverse selection
- Tracked performance metrics and analyzed trade history
- Managed inventory buildup and closed positions

## Troubleshooting

### Issue: Quotes not getting filled
**Symptom:** Zero fills despite active quotes
**Solution:** Check if spread is too wide:
```bash
# Compare your spread to market spread
curl http://127.0.0.1:8080/api/market/depth?symbol=BTCUSDT&limit=1
```
Reduce `spread_bps` to be closer to market spread.

### Issue: Inventory accumulating on one side
**Symptom:** Position keeps growing in one direction
**Solution:** Increase `skew_factor` or reduce `max_position`:
```bash
curl -X PATCH http://127.0.0.1:8080/api/strategy/btc_mm_1/parameters \
  -H "Content-Type: application/json" \
  -d '{"skew_factor": 0.8}'
```

### Issue: Losing money despite round trips
**Symptom:** Negative P&L with completed round trips
**Solution:** Your spread is too tight relative to fees. Calculate minimum profitable spread:
```
min_spread_bps = 2 * fee_rate_bps
```
For 0.1% maker fee: min_spread = 2 bps (you need at least 2 bps spread to break even).

### Issue: Strategy pausing frequently
**Symptom:** "volatility_pause" events in stream
**Solution:** Increase `volatility_threshold` or disable `pause_on_volatility` for testing:
```bash
curl -X PATCH http://127.0.0.1:8080/api/strategy/btc_mm_1/parameters \
  -H "Content-Type: application/json" \
  -d '{"volatility_threshold": 200.0}'
```

## Next Steps

- [Grid Trading Strategy](./grid-trading.md) - Alternative passive strategy
- [Risk Management in Practice](./risk-management-practice.md) - Advanced risk controls
- [Performance Tuning](./performance-tuning-practice.md) - Optimize latency for market making
- [Trading Guide](../guides/user/trading-guide.md) - Advanced execution concepts
- [Glossary](../guides/user/glossary.md) - Definitions of trading and technical terms
