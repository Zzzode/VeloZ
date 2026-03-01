# Risk Management in Practice

**Time:** 30 minutes | **Level:** Intermediate

## What You'll Learn

- Configure position limits and order size constraints
- Set up circuit breakers for loss protection
- Monitor Value at Risk (VaR) in real-time
- Implement drawdown controls
- Configure per-symbol and portfolio-level limits
- Handle risk limit breaches and recovery

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Basic understanding of risk metrics (VaR, drawdown)
- Familiarity with [Risk Management Guide](../guides/user/risk-management.md)

---

## Step 1: Understand Risk Management Layers

VeloZ implements risk management at multiple levels:

| Layer | Scope | Examples |
|-------|-------|----------|
| **Order Level** | Single order | Max order size, price deviation |
| **Position Level** | Per symbol | Max position, concentration |
| **Strategy Level** | Per strategy | Max drawdown, loss limits |
| **Portfolio Level** | All positions | Total exposure, VaR limits |

Each layer can block orders independently, providing defense in depth.

---

## Step 2: Start the Gateway

Start the VeloZ gateway with risk management enabled.

```bash
# Set environment
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_RISK_ENABLED=true

# Start gateway
./scripts/run_gateway.sh dev
```

**Expected Output:**
```
Starting VeloZ Gateway...
Risk engine initialized
Engine started in stdio mode
Gateway listening on http://127.0.0.1:8080
```

---

## Step 3: Configure Position Limits

Set maximum position sizes per symbol.

```bash
# Configure position limits
curl -X POST http://127.0.0.1:8080/api/risk/limits \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT",
    "limits": {
      "max_position_size": 0.1,
      "max_position_value": 5000.0,
      "max_order_size": 0.01,
      "max_order_value": 500.0
    }
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "symbol": "BTCUSDT",
  "limits_applied": {
    "max_position_size": 0.1,
    "max_position_value": 5000.0,
    "max_order_size": 0.01,
    "max_order_value": 500.0
  }
}
```

### Test Position Limit

```bash
# Try to place an order exceeding limit
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.05,
    "price": 50000.0
  }'
```

**Expected Output (rejected):**
```json
{
  "ok": false,
  "error": "order_size_exceeded",
  "details": {
    "requested": 0.05,
    "max_allowed": 0.01,
    "limit_type": "max_order_size"
  }
}
```

---

## Step 4: Set Up Circuit Breakers

Configure automatic trading halts when losses exceed thresholds.

```bash
# Configure circuit breakers
curl -X POST http://127.0.0.1:8080/api/risk/circuit_breakers \
  -H "Content-Type: application/json" \
  -d '{
    "breakers": [
      {
        "name": "daily_loss",
        "type": "loss_limit",
        "threshold": 100.0,
        "window": "1d",
        "action": "halt_trading"
      },
      {
        "name": "hourly_loss",
        "type": "loss_limit",
        "threshold": 50.0,
        "window": "1h",
        "action": "reduce_size"
      },
      {
        "name": "rapid_loss",
        "type": "loss_rate",
        "threshold": 20.0,
        "window": "5m",
        "action": "halt_trading"
      }
    ]
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "circuit_breakers": [
    {
      "name": "daily_loss",
      "status": "armed",
      "threshold": 100.0,
      "current_value": 0.0
    },
    {
      "name": "hourly_loss",
      "status": "armed",
      "threshold": 50.0,
      "current_value": 0.0
    },
    {
      "name": "rapid_loss",
      "status": "armed",
      "threshold": 20.0,
      "current_value": 0.0
    }
  ]
}
```

### Circuit Breaker Actions

| Action | Behavior |
|--------|----------|
| `halt_trading` | Cancel all orders, reject new orders |
| `reduce_size` | Reduce max order size by 50% |
| `close_positions` | Market sell all positions |
| `alert_only` | Send alert but continue trading |

---

## Step 5: Monitor VaR in Real-Time

Configure and monitor Value at Risk calculations.

```bash
# Configure VaR parameters
curl -X POST http://127.0.0.1:8080/api/risk/var/config \
  -H "Content-Type: application/json" \
  -d '{
    "model": "historical",
    "confidence_level": 0.95,
    "time_horizon_days": 1,
    "lookback_days": 30,
    "var_limit": 500.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "var_config": {
    "model": "historical",
    "confidence_level": 0.95,
    "time_horizon_days": 1,
    "lookback_days": 30,
    "var_limit": 500.0
  }
}
```

### Check Current VaR

```bash
# Get current VaR
curl http://127.0.0.1:8080/api/risk/var
```

**Expected Output:**
```json
{
  "portfolio_var": {
    "value": 125.50,
    "confidence": 0.95,
    "time_horizon": "1d",
    "limit": 500.0,
    "utilization_pct": 25.1
  },
  "component_var": [
    {
      "symbol": "BTCUSDT",
      "position_value": 2500.0,
      "var": 100.0,
      "var_contribution_pct": 79.7
    },
    {
      "symbol": "ETHUSDT",
      "position_value": 500.0,
      "var": 25.5,
      "var_contribution_pct": 20.3
    }
  ],
  "last_updated": 1708704000000
}
```

---

## Step 6: Configure Drawdown Controls

Set maximum drawdown limits to protect capital.

```bash
# Configure drawdown limits
curl -X POST http://127.0.0.1:8080/api/risk/drawdown \
  -H "Content-Type: application/json" \
  -d '{
    "max_drawdown_pct": 10.0,
    "trailing_drawdown_pct": 5.0,
    "drawdown_action": "halt_trading",
    "recovery_threshold_pct": 2.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "drawdown_config": {
    "max_drawdown_pct": 10.0,
    "trailing_drawdown_pct": 5.0,
    "drawdown_action": "halt_trading",
    "recovery_threshold_pct": 2.0
  }
}
```

### Monitor Drawdown Status

```bash
# Get drawdown status
curl http://127.0.0.1:8080/api/risk/drawdown/status
```

**Expected Output:**
```json
{
  "drawdown": {
    "current_pct": 2.5,
    "peak_value": 10000.0,
    "current_value": 9750.0,
    "max_allowed_pct": 10.0,
    "remaining_pct": 7.5
  },
  "trailing": {
    "high_water_mark": 9900.0,
    "current_drawdown_pct": 1.5,
    "max_trailing_pct": 5.0
  },
  "status": "normal"
}
```

---

## Step 7: Set Portfolio-Level Limits

Configure limits that apply across all positions.

```bash
# Configure portfolio limits
curl -X POST http://127.0.0.1:8080/api/risk/portfolio_limits \
  -H "Content-Type: application/json" \
  -d '{
    "max_total_exposure": 10000.0,
    "max_gross_exposure": 15000.0,
    "max_concentration_pct": 50.0,
    "max_correlation_exposure": 0.8,
    "margin_utilization_limit": 0.7
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "portfolio_limits": {
    "max_total_exposure": 10000.0,
    "max_gross_exposure": 15000.0,
    "max_concentration_pct": 50.0,
    "max_correlation_exposure": 0.8,
    "margin_utilization_limit": 0.7
  }
}
```

### Check Portfolio Risk Status

```bash
# Get portfolio risk summary
curl http://127.0.0.1:8080/api/risk/portfolio
```

**Expected Output:**
```json
{
  "exposure": {
    "net_exposure": 3000.0,
    "gross_exposure": 3500.0,
    "long_exposure": 3250.0,
    "short_exposure": 250.0
  },
  "concentration": {
    "largest_position": {
      "symbol": "BTCUSDT",
      "pct_of_portfolio": 45.0
    },
    "top_3_concentration_pct": 85.0
  },
  "limits_status": {
    "total_exposure": {"value": 3000.0, "limit": 10000.0, "pct": 30.0},
    "gross_exposure": {"value": 3500.0, "limit": 15000.0, "pct": 23.3},
    "concentration": {"value": 45.0, "limit": 50.0, "pct": 90.0}
  },
  "overall_status": "normal"
}
```

---

## Step 8: Subscribe to Risk Events

Monitor risk events in real-time.

```bash
# Subscribe to risk events
curl -N "http://127.0.0.1:8080/api/stream?filter=risk"
```

**Expected Output:**
```
event: risk_update
data: {"type":"var_update","portfolio_var":125.50,"limit":500.0,"pct":25.1}

event: risk_warning
data: {"type":"concentration_warning","symbol":"BTCUSDT","pct":48.0,"limit":50.0}

event: circuit_breaker_triggered
data: {"name":"hourly_loss","threshold":50.0,"current":52.3,"action":"reduce_size"}

event: risk_update
data: {"type":"drawdown_update","current_pct":3.2,"max_pct":10.0}
```

---

## Step 9: Handle Risk Limit Breaches

When a limit is breached, the system takes automatic action. Review and recover.

### Check Breach Status

```bash
# Get active breaches
curl http://127.0.0.1:8080/api/risk/breaches
```

**Expected Output:**
```json
{
  "active_breaches": [
    {
      "type": "circuit_breaker",
      "name": "hourly_loss",
      "triggered_at": 1708704000000,
      "threshold": 50.0,
      "actual_value": 52.3,
      "action_taken": "reduce_size",
      "auto_reset_at": 1708707600000
    }
  ],
  "trading_status": "restricted",
  "restrictions": {
    "max_order_size_multiplier": 0.5,
    "new_positions_allowed": true
  }
}
```

### Manual Reset (After Review)

```bash
# Reset circuit breaker after review
curl -X POST http://127.0.0.1:8080/api/risk/breaches/reset \
  -H "Content-Type: application/json" \
  -d '{
    "breach_name": "hourly_loss",
    "reason": "Market conditions normalized, manual review completed"
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "breach_name": "hourly_loss",
  "status": "reset",
  "trading_status": "normal"
}
```

> **Warning:** Only reset breaches after understanding why they triggered.

---

## Step 10: Configure Strategy-Level Risk

Apply risk limits to specific strategies.

```bash
# Configure strategy risk limits
curl -X POST http://127.0.0.1:8080/api/strategy/btc_mm_1/risk \
  -H "Content-Type: application/json" \
  -d '{
    "max_position": 0.05,
    "max_daily_loss": 50.0,
    "max_drawdown_pct": 5.0,
    "stop_loss_pct": 2.0,
    "take_profit_pct": 5.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "risk_limits": {
    "max_position": 0.05,
    "max_daily_loss": 50.0,
    "max_drawdown_pct": 5.0,
    "stop_loss_pct": 2.0,
    "take_profit_pct": 5.0
  }
}
```

---

## Step 11: View Risk Dashboard

Get a comprehensive risk overview.

```bash
# Get risk dashboard
curl http://127.0.0.1:8080/api/risk/dashboard
```

**Expected Output:**
```json
{
  "timestamp": 1708704000000,
  "overall_status": "normal",
  "summary": {
    "portfolio_value": 10000.0,
    "unrealized_pnl": 150.0,
    "realized_pnl_today": 75.0,
    "var_95": 125.5,
    "current_drawdown_pct": 2.5
  },
  "limits": {
    "position_limits": {"breaches": 0, "warnings": 1},
    "circuit_breakers": {"triggered": 0, "armed": 3},
    "var_limit": {"utilization_pct": 25.1},
    "drawdown": {"current_pct": 2.5, "max_pct": 10.0}
  },
  "positions": [
    {
      "symbol": "BTCUSDT",
      "size": 0.05,
      "value": 2500.0,
      "pnl": 100.0,
      "risk_score": "low"
    },
    {
      "symbol": "ETHUSDT",
      "size": 0.5,
      "value": 500.0,
      "pnl": 50.0,
      "risk_score": "low"
    }
  ],
  "recent_events": [
    {
      "timestamp": 1708703900000,
      "type": "warning",
      "message": "Concentration approaching limit for BTCUSDT"
    }
  ]
}
```

---

## Summary

**What you accomplished:**
- Configured position and order size limits
- Set up circuit breakers with automatic actions
- Monitored VaR in real-time with component breakdown
- Implemented drawdown controls with recovery thresholds
- Configured portfolio-level exposure limits
- Subscribed to risk events for real-time monitoring
- Handled risk limit breaches and recovery procedures

## Troubleshooting

### Issue: Orders rejected with "position_limit_exceeded"
**Symptom:** Valid orders being rejected
**Solution:** Check current position and limits:
```bash
curl http://127.0.0.1:8080/api/risk/limits/BTCUSDT
```
Increase limits or reduce existing position.

### Issue: Circuit breaker triggered unexpectedly
**Symptom:** Trading halted without apparent reason
**Solution:** Check breach details:
```bash
curl http://127.0.0.1:8080/api/risk/breaches
```
Review the threshold and actual value that triggered the breach.

### Issue: VaR calculation seems incorrect
**Symptom:** VaR values don't match expectations
**Solution:** Verify VaR configuration and data:
```bash
curl http://127.0.0.1:8080/api/risk/var/debug
```
Check lookback period and price data availability.

### Issue: Cannot reset circuit breaker
**Symptom:** Reset request fails
**Solution:** Check if auto-reset time has passed:
```bash
curl http://127.0.0.1:8080/api/risk/breaches
```
Some breakers require waiting for the window to expire.

## Next Steps

- [Performance Tuning](./performance-tuning-practice.md) - Optimize risk calculation latency
- [Troubleshooting Common Issues](./troubleshooting-practice.md) - Debug risk-related problems
- [Risk Management Guide](../guides/user/risk-management.md) - Complete risk configuration reference
- [Monitoring Guide](../guides/user/monitoring.md) - Set up risk dashboards in Grafana
- [Glossary](../guides/user/glossary.md) - Definitions of risk management terms
