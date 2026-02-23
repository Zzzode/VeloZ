# Trading User Guide

This guide covers how to use VeloZ for trading operations across multiple exchanges.

## Overview

### Trading Capabilities

VeloZ provides a complete trading solution:

| Feature | Description |
|---------|-------------|
| **Multi-Exchange** | Binance, OKX, Bybit, Coinbase |
| **Order Types** | Market, Limit, Stop-Loss, Take-Profit |
| **Smart Routing** | Automatic best-price execution |
| **Execution Algorithms** | TWAP, VWAP, POV, Iceberg |
| **Position Management** | Real-time P&L tracking |
| **Risk Controls** | Position limits, drawdown protection |

### Supported Exchanges

| Exchange | Spot | Testnet | Production |
|----------|------|---------|------------|
| Binance | Yes | Yes | Yes |
| OKX | Yes | Yes | Yes |
| Bybit | Yes | Yes | Yes |
| Coinbase | Yes | Sandbox | Yes |

### Trading Workflow

```
+----------------+     +----------------+     +----------------+
|   Configure    | --> |  Place Order   | --> |   Monitor      |
|   Exchange     |     |  (API/UI)      |     |   Execution    |
+----------------+     +----------------+     +----------------+
        |                     |                      |
        v                     v                      v
+----------------+     +----------------+     +----------------+
|   Test         |     |  Smart Router  |     |   Position     |
|   Connectivity |     |  Selects Venue |     |   Updates      |
+----------------+     +----------------+     +----------------+
```

---

## Getting Started with Trading

### Step 1: Configure Exchange Credentials

Choose your exchange and set credentials:

**Binance Testnet:**
```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
```

**OKX Testnet:**
```bash
export VELOZ_EXECUTION_MODE=okx_testnet
export VELOZ_OKX_API_KEY=your_okx_api_key
export VELOZ_OKX_API_SECRET=your_okx_api_secret
export VELOZ_OKX_PASSPHRASE=your_passphrase
```

**Bybit Testnet:**
```bash
export VELOZ_EXECUTION_MODE=bybit_testnet
export VELOZ_BYBIT_API_KEY=your_bybit_api_key
export VELOZ_BYBIT_API_SECRET=your_bybit_api_secret
```

### Step 2: Start the Gateway

```bash
./scripts/run_gateway.sh dev
```

### Step 3: Test Connectivity

```bash
# Check execution ping
curl http://127.0.0.1:8080/api/execution/ping

# Expected response:
# {"ok": true, "result": {}}
```

### Step 4: Verify Configuration

```bash
curl http://127.0.0.1:8080/api/config
```

**Response:**
```json
{
  "execution_mode": "binance_testnet_spot",
  "binance_trade_enabled": true,
  "binance_user_stream_connected": true
}
```

### Step 5: Place Your First Order

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
```

---

## Order Lifecycle

Understanding the order lifecycle is essential for trading:

```
+--------+     +----------+     +---------+     +--------+
|  New   | --> | Accepted | --> | Filled  | --> | Done   |
+--------+     +----------+     +---------+     +--------+
    |              |                |
    v              v                v
+----------+  +----------+    +-----------+
| Rejected |  | Cancelled|    | Partially |
+----------+  +----------+    |  Filled   |
                              +-----------+
```

### Order States

| State | Description | Terminal |
|-------|-------------|----------|
| `NEW` | Order created locally | No |
| `ACCEPTED` | Order accepted by exchange | No |
| `PARTIALLY_FILLED` | Partially executed | No |
| `FILLED` | Fully executed | Yes |
| `CANCELLED` | Cancelled by user | Yes |
| `REJECTED` | Rejected by exchange | Yes |
| `EXPIRED` | Order expired (time-in-force) | Yes |

---

## Order Types and Parameters

### Market Orders

Execute immediately at the best available price:

```bash
# Market buy - use high price to ensure fill
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 999999.0
  }'

# Market sell - use low price to ensure fill
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "SELL",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 0.01
  }'
```

**When to use:** Immediate execution needed, liquidity is high.

### Limit Orders

Execute at a specific price or better:

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 49500.0,
    "client_order_id": "limit-buy-001"
  }'
```

**When to use:** Price control is important, willing to wait for fill.

### Order Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `side` | string | Yes | `BUY` or `SELL` |
| `symbol` | string | Yes | Trading pair (e.g., `BTCUSDT`) |
| `qty` | number | Yes | Order quantity (base asset) |
| `price` | number | Yes | Limit price |
| `client_order_id` | string | No | Your unique order ID |

### Client Order ID

Use `client_order_id` for:
- **Idempotency**: Same ID = same order (prevents duplicates)
- **Tracking**: Reference orders in your system
- **Reconciliation**: Match fills to your orders

```bash
# Generate unique ID
CLIENT_ORDER_ID="order-$(date +%s)-$(openssl rand -hex 4)"

curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d "{
    \"side\": \"BUY\",
    \"symbol\": \"BTCUSDT\",
    \"qty\": 0.001,
    \"price\": 50000.0,
    \"client_order_id\": \"$CLIENT_ORDER_ID\"
  }"
```

---

## Order Management

### Place an Order

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0,
    "client_order_id": "my-order-001"
  }'
```

**Response:**
```json
{
  "ok": true,
  "client_order_id": "my-order-001",
  "venue_order_id": "123456789"
}
```

### Cancel an Order

```bash
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{
    "client_order_id": "my-order-001",
    "symbol": "BTCUSDT"
  }'
```

**Response:**
```json
{
  "ok": true,
  "client_order_id": "my-order-001"
}
```

### Modify an Order (Cancel-Replace)

VeloZ uses the cancel-replace pattern for order modification:

```bash
# Step 1: Cancel the existing order
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{"client_order_id": "my-order-001", "symbol": "BTCUSDT"}'

# Step 2: Place a new order with updated parameters
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.002,
    "price": 49800.0,
    "client_order_id": "my-order-002"
  }'
```

### Query Order Status

```bash
# Single order
curl "http://127.0.0.1:8080/api/order_state?client_order_id=my-order-001"

# All orders
curl http://127.0.0.1:8080/api/orders_state
```

**Single Order Response:**
```json
{
  "client_order_id": "my-order-001",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "order_qty": 0.001,
  "limit_price": 50000.0,
  "venue_order_id": "123456789",
  "status": "FILLED",
  "executed_qty": 0.001,
  "avg_price": 49998.5,
  "last_ts_ns": 1708689600000000000
}
```

### List All Orders

```bash
curl http://127.0.0.1:8080/api/orders_state
```

**Response:**
```json
{
  "items": [
    {
      "client_order_id": "my-order-001",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "status": "FILLED",
      "executed_qty": 0.001,
      "avg_price": 49998.5
    },
    {
      "client_order_id": "my-order-002",
      "symbol": "ETHUSDT",
      "side": "SELL",
      "status": "ACCEPTED",
      "executed_qty": 0.0
    }
  ]
}
```

---

## Position Management

### View All Positions

```bash
curl http://127.0.0.1:8080/api/positions
```

**Response:**
```json
{
  "positions": [
    {
      "symbol": "BTCUSDT",
      "venue": "binance",
      "side": "LONG",
      "qty": 0.5,
      "avg_entry_price": 49500.0,
      "current_price": 50000.0,
      "unrealized_pnl": 250.0,
      "realized_pnl": 1200.0,
      "last_update_ts_ns": 1708689600000000000
    }
  ]
}
```

### View Position by Symbol

```bash
curl http://127.0.0.1:8080/api/positions/BTCUSDT
```

### Position Fields

| Field | Description |
|-------|-------------|
| `symbol` | Trading pair |
| `venue` | Exchange name |
| `side` | `LONG` or `SHORT` |
| `qty` | Position size |
| `avg_entry_price` | Average entry price |
| `current_price` | Current market price |
| `unrealized_pnl` | Unrealized profit/loss |
| `realized_pnl` | Realized profit/loss |

### Calculate P&L

**Unrealized P&L:**
```
unrealized_pnl = (current_price - avg_entry_price) * qty
```

**For LONG positions:**
- Positive when current_price > avg_entry_price
- Negative when current_price < avg_entry_price

**For SHORT positions:**
- Positive when current_price < avg_entry_price
- Negative when current_price > avg_entry_price

### Close a Position

To close a position, place an opposite order:

```bash
# Close LONG position (sell)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "SELL",
    "symbol": "BTCUSDT",
    "qty": 0.5,
    "price": 50000.0,
    "client_order_id": "close-btc-001"
  }'
```

### Position Limits

Configure position limits via risk management:

| Limit Type | Description |
|------------|-------------|
| Max Position Size | Maximum quantity per symbol |
| Max Notional | Maximum USD value per position |
| Max Positions | Maximum number of open positions |
| Concentration Limit | Maximum % of portfolio in one position |

---

## Multi-Exchange Trading

### Smart Order Routing

VeloZ automatically routes orders to the best exchange based on:

| Factor | Weight | Description |
|--------|--------|-------------|
| **Price** | 35% | Best bid/ask price |
| **Fees** | 20% | Maker/taker fees |
| **Latency** | 15% | Historical execution speed |
| **Liquidity** | 20% | Available depth |
| **Reliability** | 10% | Fill rate history |

### How Smart Routing Works

```
+----------------+     +------------------+     +----------------+
|  Place Order   | --> | Score All Venues | --> | Route to Best  |
+----------------+     +------------------+     +----------------+
                              |
                              v
                    +--------------------+
                    | Binance: 0.85      |
                    | OKX:     0.82      |
                    | Bybit:   0.78      |
                    +--------------------+
```

### Exchange Selection Strategies

**1. Best Price:**
```bash
# Default - routes to best price
export VELOZ_ROUTING_STRATEGY=best_price
```

**2. Lowest Fee:**
```bash
# Prioritize lowest fees
export VELOZ_ROUTING_STRATEGY=lowest_fee
```

**3. Fastest Execution:**
```bash
# Prioritize speed
export VELOZ_ROUTING_STRATEGY=fastest
```

**4. Specific Exchange:**
```bash
# Force specific exchange
export VELOZ_EXECUTION_MODE=binance_spot
```

### Cross-Exchange Arbitrage

VeloZ can detect and execute arbitrage opportunities:

```
Exchange A: BTC/USDT = $50,000 (bid)
Exchange B: BTC/USDT = $50,100 (ask)

Opportunity: Buy on A, Sell on B
Profit: $100 - fees
```

**Note:** Arbitrage requires:
- Accounts on multiple exchanges
- Pre-positioned capital
- Low latency connectivity

### Latency Considerations

| Exchange | Typical Latency | Notes |
|----------|-----------------|-------|
| Binance | 10-50ms | Fastest, most liquid |
| OKX | 20-80ms | Good liquidity |
| Bybit | 30-100ms | Derivatives focus |
| Coinbase | 50-150ms | US-regulated |

**Tips for low latency:**
- Use exchange closest to your server
- Enable WebSocket for real-time updates
- Minimize order modifications

---

## Execution Algorithms

VeloZ provides sophisticated execution algorithms for large orders.

### TWAP (Time-Weighted Average Price)

Splits order evenly over time to minimize market impact.

**Use case:** Execute large order over a period without moving the market.

**Parameters:**

| Parameter | Default | Description |
|-----------|---------|-------------|
| `duration` | 60s | Total execution time |
| `slice_interval` | 5s | Time between slices |
| `randomization` | 10% | Random timing variation |
| `limit_offset_bps` | 5 | Basis points from mid |

**Example:**
```
Order: Buy 10 BTC over 1 hour
TWAP splits into: 12 slices of ~0.83 BTC every 5 minutes
```

**API (when available):**
```bash
curl -X POST http://127.0.0.1:8080/api/algo/twap \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT",
    "side": "BUY",
    "quantity": 10.0,
    "duration_seconds": 3600,
    "slice_interval_seconds": 300
  }'
```

### VWAP (Volume-Weighted Average Price)

Executes in proportion to historical volume patterns.

**Use case:** Match market volume profile for minimal impact.

**Parameters:**

| Parameter | Default | Description |
|-----------|---------|-------------|
| `duration` | 60s | Total execution time |
| `participation_rate` | 10% | Max % of market volume |
| `volume_profile` | [] | Historical volume distribution |

**Example:**
```
If market trades 100 BTC/hour and participation_rate = 10%:
Maximum execution: 10 BTC/hour
Slices sized proportionally to volume
```

### POV (Percentage of Volume)

Executes as a percentage of real-time market volume.

**Use case:** Participate in market without leading or lagging.

**Parameters:**

| Parameter | Default | Description |
|-----------|---------|-------------|
| `target_pct` | 10% | Target % of volume |
| `max_pct` | 20% | Maximum % of volume |
| `min_order_size` | 0.001 | Minimum slice size |

### Iceberg Orders

Shows only a portion of the total order.

**Use case:** Hide large order size from market.

**Parameters:**

| Parameter | Default | Description |
|-----------|---------|-------------|
| `total_quantity` | - | Full order size |
| `display_quantity` | - | Visible portion |
| `variance` | 10% | Random variation |

**Example:**
```
Total: 100 BTC
Display: 1 BTC (visible)
Hidden: 99 BTC (replenishes as display fills)
```

### Algorithm States

| State | Description |
|-------|-------------|
| `Pending` | Algorithm created, not started |
| `Running` | Actively executing |
| `Paused` | Temporarily stopped |
| `Completed` | Successfully finished |
| `Cancelled` | Manually cancelled |
| `Failed` | Error occurred |

---

## Best Practices

### 1. Risk Management Integration

Always set risk limits before trading:

```bash
# Set position limits
export VELOZ_MAX_POSITION_SIZE=10.0
export VELOZ_MAX_NOTIONAL=500000.0
export VELOZ_MAX_DRAWDOWN=0.10

# Enable circuit breakers
export VELOZ_CIRCUIT_BREAKER_ENABLED=true
```

### 2. Order Sizing

**Position sizing formula:**
```
position_size = (account_equity * risk_per_trade) / (entry_price - stop_loss)
```

**Example:**
```
Account: $100,000
Risk per trade: 1% ($1,000)
Entry: $50,000
Stop loss: $49,000

Position size = $1,000 / $1,000 = 1 BTC
```

### 3. Slippage Management

**Estimate slippage:**
```
expected_slippage = order_size / available_liquidity * price_impact_factor
```

**Reduce slippage:**
- Use limit orders instead of market orders
- Split large orders using TWAP/VWAP
- Trade during high-liquidity periods
- Avoid trading during news events

### 4. Error Handling

**Common errors and solutions:**

| Error | Cause | Solution |
|-------|-------|----------|
| `insufficient_balance` | Not enough funds | Check account balance |
| `min_notional` | Order too small | Increase order size |
| `price_filter` | Price out of range | Adjust price |
| `lot_size` | Invalid quantity | Round to lot size |
| `rate_limit` | Too many requests | Implement backoff |

**Retry strategy:**
```python
def place_order_with_retry(order, max_retries=3):
    for attempt in range(max_retries):
        try:
            return api.place_order(order)
        except RateLimitError:
            time.sleep(2 ** attempt)
        except InsufficientBalanceError:
            raise  # Don't retry
        except Exception as e:
            if attempt == max_retries - 1:
                raise
            time.sleep(1)
```

### 5. Order Monitoring

**Monitor orders in real-time:**
```bash
# Subscribe to SSE stream
curl -N http://127.0.0.1:8080/api/stream | grep order_update
```

**Key events to watch:**
- `order_update`: Status changes
- `fill`: Execution fills
- `error`: Order errors

### 6. Reconciliation

Enable automatic reconciliation to sync with exchange:

```bash
export VELOZ_RECONCILIATION_ENABLED=true
export VELOZ_RECONCILIATION_INTERVAL=30
export VELOZ_AUTO_CANCEL_ORPHANED=true
```

**Reconciliation checks:**
- Order states match exchange
- Position sizes match exchange
- Balance matches exchange

---

## Trading Checklist

### Before Trading

- [ ] Configure exchange credentials
- [ ] Test connectivity (`/api/execution/ping`)
- [ ] Verify configuration (`/api/config`)
- [ ] Check account balance (`/api/account`)
- [ ] Set risk limits
- [ ] Enable monitoring

### During Trading

- [ ] Monitor SSE stream for updates
- [ ] Track order states
- [ ] Watch position P&L
- [ ] Check risk metrics
- [ ] Review execution quality

### After Trading

- [ ] Verify all orders completed
- [ ] Check final positions
- [ ] Review P&L
- [ ] Analyze execution quality
- [ ] Archive audit logs

---

## Troubleshooting

### Order Not Filling

1. **Check price**: Is your limit price competitive?
2. **Check liquidity**: Is there enough volume?
3. **Check status**: Is the order still active?

```bash
curl "http://127.0.0.1:8080/api/order_state?client_order_id=my-order"
```

### Order Rejected

1. **Check error message**: What's the rejection reason?
2. **Check balance**: Do you have enough funds?
3. **Check limits**: Are you within exchange limits?

### Position Mismatch

1. **Run reconciliation**: Force sync with exchange
2. **Check fills**: Were all fills processed?
3. **Check WebSocket**: Is user stream connected?

```bash
curl http://127.0.0.1:8080/api/config | jq '.binance_user_stream_connected'
```

---

## Related Documentation

- [API Usage Guide](api-usage-guide.md) - API examples
- [Configuration Guide](configuration.md) - Exchange setup
- [Risk Management Guide](risk-management-guide.md) - Risk controls
- [Monitoring Guide](monitoring.md) - Metrics and alerts
