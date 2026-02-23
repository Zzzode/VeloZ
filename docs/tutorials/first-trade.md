# Your First Trade

**Time:** 15 minutes | **Level:** Beginner

## What You'll Learn

- Start the VeloZ gateway and verify system health
- Place a limit order via the REST API
- Monitor order status using Server-Sent Events
- Check your position after the order fills
- Cancel an open order

## Prerequisites

- VeloZ built successfully (see [Getting Started](../guides/user/getting-started.md))
- Terminal access with `curl` installed
- Basic understanding of limit orders

---

## Step 1: Start the Gateway

Start the VeloZ gateway in simulation mode. This allows you to practice trading without connecting to a real exchange.

```bash
# Start gateway in development mode (simulation)
./scripts/run_gateway.sh dev
```

**Expected Output:**
```
Starting VeloZ Gateway...
Engine started in stdio mode
Gateway listening on http://127.0.0.1:8080
```

Keep this terminal window open. The gateway must remain running throughout this tutorial.

> **Tip:** Open a new terminal window for the remaining commands.

---

## Step 2: Verify System Health

Before placing orders, verify that all system components are operational.

```bash
# Check system health
curl http://127.0.0.1:8080/api/health
```

**Expected Output:**
```json
{
  "status": "healthy",
  "engine": "running",
  "uptime_seconds": 5,
  "version": "1.0.0"
}
```

If the status is not "healthy", check the gateway terminal for error messages.

---

## Step 3: Check Market Data

View the current market data to determine an appropriate order price.

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

Note the current price. You will use this to set your limit order price.

---

## Step 4: Place Your First Order

Place a limit buy order for BTCUSDT. In simulation mode, this order will be processed by the simulated matching engine.

```bash
# Place a limit buy order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "client_order_id": "web-1708704000000",
  "venue_order_id": "sim-12345678"
}
```

Save the `client_order_id` value. You will use it to track and manage your order.

> **Tip:** You can specify your own `client_order_id` by adding it to the request body for easier tracking.

---

## Step 5: Monitor Order Status

Open a new terminal and subscribe to the event stream to receive real-time order updates.

```bash
# Subscribe to order events
curl -N http://127.0.0.1:8080/api/stream
```

**Expected Output:**
```
event: order_update
data: {"type":"order_update","client_order_id":"web-1708704000000","status":"NEW","filled_qty":0}

event: order_update
data: {"type":"order_update","client_order_id":"web-1708704000000","status":"FILLED","filled_qty":0.001}
```

The order progresses through these states:
- `NEW`: Order accepted by the matching engine
- `PARTIALLY_FILLED`: Some quantity filled (for larger orders)
- `FILLED`: Order completely filled
- `CANCELED`: Order canceled before filling

Press `Ctrl+C` to stop the event stream when done.

---

## Step 6: Check Order State

Query the current state of your order using the client order ID.

```bash
# Get order state
curl "http://127.0.0.1:8080/api/order_state?client_order_id=web-1708704000000"
```

**Expected Output:**
```json
{
  "client_order_id": "web-1708704000000",
  "venue_order_id": "sim-12345678",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "price": 50000.0,
  "qty": 0.001,
  "filled_qty": 0.001,
  "status": "FILLED",
  "created_at": 1708704000000,
  "updated_at": 1708704001000
}
```

The `status: FILLED` confirms your order has been completely executed.

---

## Step 7: View Your Position

After your order fills, check your position to see your holdings.

```bash
# Get all positions
curl http://127.0.0.1:8080/api/positions
```

**Expected Output:**
```json
{
  "positions": [
    {
      "symbol": "BTCUSDT",
      "size": 0.001,
      "entry_price": 50000.0,
      "current_price": 50000.0,
      "unrealized_pnl": 0.0,
      "realized_pnl": 0.0
    }
  ]
}
```

You now hold 0.001 BTC at an entry price of 50,000 USDT.

---

## Step 8: Place and Cancel an Order

Practice canceling an order before it fills. Place an order at a price unlikely to fill immediately.

```bash
# Place an order below market price (unlikely to fill)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 45000.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "client_order_id": "web-1708704100000",
  "venue_order_id": "sim-12345679"
}
```

Now cancel this order:

```bash
# Cancel the order
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{
    "client_order_id": "web-1708704100000",
    "symbol": "BTCUSDT"
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "client_order_id": "web-1708704100000",
  "status": "CANCELED"
}
```

---

## Step 9: View All Orders

Review all orders you have placed during this session.

```bash
# Get all order states
curl http://127.0.0.1:8080/api/orders_state
```

**Expected Output:**
```json
{
  "orders": [
    {
      "client_order_id": "web-1708704000000",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "status": "FILLED",
      "qty": 0.001,
      "filled_qty": 0.001
    },
    {
      "client_order_id": "web-1708704100000",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "status": "CANCELED",
      "qty": 0.001,
      "filled_qty": 0.0
    }
  ]
}
```

---

## Summary

**What you accomplished:**
- Started the VeloZ gateway in simulation mode
- Verified system health before trading
- Placed a limit buy order and received a fill
- Monitored order status via Server-Sent Events
- Viewed your position after the fill
- Canceled an open order

## Troubleshooting

### Issue: Gateway fails to start
**Symptom:** Error message "Address already in use"
**Solution:** Another process is using port 8080. Find and stop it:
```bash
lsof -i :8080
kill <PID>
```

### Issue: Order rejected with "insufficient_balance"
**Symptom:** API returns `{"ok": false, "error": "insufficient_balance"}`
**Solution:** In simulation mode, the default balance may be insufficient. Restart the gateway or reduce order size.

### Issue: curl command not found
**Symptom:** Terminal shows "command not found: curl"
**Solution:** Install curl:
- macOS: `brew install curl`
- Ubuntu: `sudo apt install curl`

### Issue: Connection refused
**Symptom:** `curl: (7) Failed to connect to 127.0.0.1 port 8080`
**Solution:** Ensure the gateway is running in another terminal window.

## Next Steps

- [Risk Management Guide](../guides/user/risk-management.md) - Set up VaR, circuit breakers, and risk controls
- [Multi-Exchange Arbitrage](./multi-exchange-arbitrage.md) - Trade across multiple exchanges
- [Binance Integration](../guides/user/binance.md) - Connect to Binance testnet for realistic simulation
- [Trading Guide](../guides/user/trading-guide.md) - Learn advanced trading patterns and execution algorithms
