# Troubleshooting Common Issues

**Time:** 25 minutes | **Level:** Intermediate

## What You'll Learn

- Diagnose connection issues with exchanges
- Debug order rejection and execution problems
- Resolve performance degradation issues
- Fix market data synchronization problems
- Troubleshoot strategy behavior issues
- Use diagnostic tools and logs effectively

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Access to system logs
- Basic familiarity with [Troubleshooting Guide](../guides/user/troubleshooting.md)

---

## Step 1: Check System Health

Always start troubleshooting by checking overall system health.

```bash
# Get comprehensive health status
curl http://127.0.0.1:8080/api/health/detailed
```

**Expected Output (healthy):**
```json
{
  "status": "healthy",
  "components": {
    "engine": {"status": "running", "uptime_seconds": 3600},
    "gateway": {"status": "running", "uptime_seconds": 3605},
    "exchange_binance": {"status": "connected", "latency_ms": 45},
    "risk_engine": {"status": "active", "breaches": 0},
    "market_data": {"status": "streaming", "last_update_ms": 50}
  },
  "version": "1.0.0"
}
```

**Expected Output (unhealthy):**
```json
{
  "status": "degraded",
  "components": {
    "engine": {"status": "running", "uptime_seconds": 3600},
    "gateway": {"status": "running", "uptime_seconds": 3605},
    "exchange_binance": {"status": "disconnected", "error": "connection_timeout"},
    "risk_engine": {"status": "active", "breaches": 1},
    "market_data": {"status": "stale", "last_update_ms": 5000}
  },
  "issues": [
    "Exchange connection lost",
    "Market data stale (>1s)"
  ]
}
```

---

## Step 2: Debug Connection Issues

### Symptom: Exchange Connection Failed

```bash
# Check exchange connection status
curl http://127.0.0.1:8080/api/exchanges/binance/status
```

**Expected Output (connection error):**
```json
{
  "exchange": "binance",
  "status": "disconnected",
  "error": {
    "type": "connection_timeout",
    "message": "Failed to connect to api.binance.com:443",
    "last_attempt": 1708704000000,
    "retry_count": 5
  },
  "diagnostics": {
    "dns_resolved": true,
    "tcp_connected": false,
    "tls_handshake": false,
    "authenticated": false
  }
}
```

### Solution: Test Network Connectivity

```bash
# Test DNS resolution
curl http://127.0.0.1:8080/api/diagnostics/dns?host=api.binance.com
```

**Expected Output:**
```json
{
  "host": "api.binance.com",
  "resolved": true,
  "addresses": ["52.84.150.23", "52.84.150.45"],
  "resolution_time_ms": 15
}
```

```bash
# Test TCP connectivity
curl http://127.0.0.1:8080/api/diagnostics/tcp?host=api.binance.com&port=443
```

**Expected Output:**
```json
{
  "host": "api.binance.com",
  "port": 443,
  "connected": true,
  "latency_ms": 45
}
```

### Force Reconnection

```bash
# Force reconnect to exchange
curl -X POST http://127.0.0.1:8080/api/exchanges/binance/reconnect
```

**Expected Output:**
```json
{
  "ok": true,
  "exchange": "binance",
  "status": "connecting",
  "message": "Reconnection initiated"
}
```

---

## Step 3: Debug Order Rejections

### Symptom: Orders Being Rejected

```bash
# Get recent order rejections
curl http://127.0.0.1:8080/api/orders/rejected?limit=5
```

**Expected Output:**
```json
{
  "rejected_orders": [
    {
      "client_order_id": "web-1708704000000",
      "timestamp": 1708704000000,
      "reason": "insufficient_balance",
      "details": {
        "required": 500.0,
        "available": 250.0,
        "asset": "USDT"
      }
    },
    {
      "client_order_id": "web-1708704001000",
      "timestamp": 1708704001000,
      "reason": "price_deviation",
      "details": {
        "order_price": 45000.0,
        "market_price": 50000.0,
        "deviation_pct": 10.0,
        "max_allowed_pct": 5.0
      }
    },
    {
      "client_order_id": "web-1708704002000",
      "timestamp": 1708704002000,
      "reason": "position_limit_exceeded",
      "details": {
        "current_position": 0.09,
        "order_qty": 0.02,
        "max_position": 0.1
      }
    }
  ]
}
```

### Common Rejection Reasons and Solutions

| Reason | Cause | Solution |
|--------|-------|----------|
| `insufficient_balance` | Not enough funds | Check balance, reduce order size |
| `price_deviation` | Price too far from market | Use market order or adjust price |
| `position_limit_exceeded` | Would exceed max position | Reduce qty or increase limit |
| `order_size_exceeded` | Order too large | Split into smaller orders |
| `rate_limit_exceeded` | Too many orders | Reduce order frequency |
| `symbol_not_trading` | Market closed | Wait for market open |

### Check Account Balance

```bash
# Get account balances
curl http://127.0.0.1:8080/api/account/balances
```

**Expected Output:**
```json
{
  "balances": [
    {"asset": "USDT", "free": 250.0, "locked": 100.0},
    {"asset": "BTC", "free": 0.05, "locked": 0.01}
  ]
}
```

---

## Step 4: Debug Performance Issues

### Symptom: High Latency

```bash
# Get latency breakdown
curl http://127.0.0.1:8080/api/diagnostics/latency
```

**Expected Output:**
```json
{
  "latency_breakdown": {
    "api_processing_us": 500,
    "validation_us": 100,
    "risk_check_us": 2500,
    "serialization_us": 150,
    "network_us": 1200,
    "total_us": 4450
  },
  "anomalies": [
    {
      "component": "risk_check",
      "expected_us": 200,
      "actual_us": 2500,
      "severity": "high"
    }
  ]
}
```

### Solution: Identify Slow Component

```bash
# Get detailed risk check timing
curl http://127.0.0.1:8080/api/diagnostics/risk_check
```

**Expected Output:**
```json
{
  "risk_check_breakdown": {
    "position_limit_check_us": 50,
    "var_calculation_us": 2300,
    "circuit_breaker_check_us": 50,
    "total_us": 2400
  },
  "issue": "var_calculation_slow",
  "recommendation": "Reduce VaR lookback period or use parametric model"
}
```

### Apply Fix

```bash
# Optimize VaR calculation
curl -X PATCH http://127.0.0.1:8080/api/risk/var/config \
  -H "Content-Type: application/json" \
  -d '{
    "model": "parametric",
    "lookback_days": 10
  }'
```

---

## Step 5: Debug Market Data Issues

### Symptom: Stale or Missing Market Data

```bash
# Check market data status
curl http://127.0.0.1:8080/api/market/status
```

**Expected Output (stale data):**
```json
{
  "status": "stale",
  "symbols": {
    "BTCUSDT": {
      "last_update": 1708703995000,
      "age_ms": 5000,
      "status": "stale"
    },
    "ETHUSDT": {
      "last_update": 1708704000000,
      "age_ms": 50,
      "status": "live"
    }
  },
  "websocket": {
    "status": "connected",
    "subscriptions": ["btcusdt@trade", "ethusdt@trade"],
    "messages_per_second": 45
  }
}
```

### Solution: Resubscribe to Market Data

```bash
# Resubscribe to symbol
curl -X POST http://127.0.0.1:8080/api/market/resubscribe \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT"
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "symbol": "BTCUSDT",
  "action": "resubscribed",
  "streams": ["btcusdt@trade", "btcusdt@depth"]
}
```

### Check WebSocket Health

```bash
# Get WebSocket diagnostics
curl http://127.0.0.1:8080/api/diagnostics/websocket
```

**Expected Output:**
```json
{
  "connections": [
    {
      "endpoint": "wss://stream.binance.com:9443",
      "status": "connected",
      "uptime_seconds": 3600,
      "messages_received": 180000,
      "last_message_ms": 50,
      "ping_latency_ms": 45
    }
  ],
  "health": "good"
}
```

---

## Step 6: Debug Strategy Issues

### Symptom: Strategy Not Generating Signals

```bash
# Get strategy diagnostics
curl http://127.0.0.1:8080/api/strategy/btc_mm_1/diagnostics
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "status": "running",
  "diagnostics": {
    "events_received": 5000,
    "events_processed": 5000,
    "signals_generated": 0,
    "last_event_ms": 50,
    "processing_errors": 0
  },
  "state": {
    "indicators_ready": false,
    "warmup_progress_pct": 45.0,
    "warmup_events_needed": 100,
    "warmup_events_received": 45
  },
  "issue": "strategy_warming_up",
  "recommendation": "Wait for warmup to complete"
}
```

### Solution: Check Strategy State

```bash
# Get detailed strategy state
curl http://127.0.0.1:8080/api/strategy/btc_mm_1/state
```

**Expected Output:**
```json
{
  "strategy_id": "btc_mm_1",
  "internal_state": {
    "position": 0.0,
    "last_signal_time": null,
    "indicators": {
      "short_sma": 49950.0,
      "long_sma": 50000.0,
      "ready": true
    }
  },
  "conditions": {
    "market_data_available": true,
    "risk_limits_ok": true,
    "position_within_limits": true,
    "can_generate_signals": true
  }
}
```

### Force Strategy Reset

```bash
# Reset strategy state
curl -X POST http://127.0.0.1:8080/api/strategy/btc_mm_1/reset
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "btc_mm_1",
  "action": "reset",
  "state": "restarting"
}
```

---

## Step 7: Check Logs for Errors

### Get Recent Error Logs

```bash
# Get error logs
curl "http://127.0.0.1:8080/api/logs?level=error&limit=10"
```

**Expected Output:**
```json
{
  "logs": [
    {
      "timestamp": "2026-02-23T10:00:00Z",
      "level": "error",
      "component": "exchange_adapter",
      "message": "Order rejected by exchange",
      "details": {
        "order_id": "web-1708704000000",
        "exchange_error": "INSUFFICIENT_BALANCE"
      }
    },
    {
      "timestamp": "2026-02-23T09:55:00Z",
      "level": "error",
      "component": "websocket",
      "message": "Connection lost",
      "details": {
        "endpoint": "wss://stream.binance.com:9443",
        "reason": "ping_timeout"
      }
    }
  ]
}
```

### Search Logs by Component

```bash
# Search logs for specific component
curl "http://127.0.0.1:8080/api/logs?component=risk_engine&limit=20"
```

---

## Step 8: Run Diagnostic Tests

### Run Full System Diagnostics

```bash
# Run comprehensive diagnostics
curl -X POST http://127.0.0.1:8080/api/diagnostics/run
```

**Expected Output:**
```json
{
  "diagnostics": {
    "network": {
      "status": "pass",
      "tests": [
        {"name": "dns_resolution", "status": "pass", "latency_ms": 15},
        {"name": "exchange_connectivity", "status": "pass", "latency_ms": 45},
        {"name": "websocket_health", "status": "pass"}
      ]
    },
    "engine": {
      "status": "pass",
      "tests": [
        {"name": "order_processing", "status": "pass", "latency_us": 500},
        {"name": "market_data_processing", "status": "pass", "latency_us": 50},
        {"name": "risk_checks", "status": "pass", "latency_us": 200}
      ]
    },
    "memory": {
      "status": "pass",
      "tests": [
        {"name": "heap_usage", "status": "pass", "usage_pct": 45},
        {"name": "pool_efficiency", "status": "pass", "hit_rate_pct": 99.8}
      ]
    },
    "storage": {
      "status": "warning",
      "tests": [
        {"name": "wal_write", "status": "pass", "latency_us": 500},
        {"name": "disk_space", "status": "warning", "free_pct": 15}
      ]
    }
  },
  "overall_status": "warning",
  "recommendations": [
    "Disk space below 20% - consider cleanup"
  ]
}
```

---

## Step 9: Common Issues Quick Reference

### Connection Issues

| Symptom | Check | Fix |
|---------|-------|-----|
| "Connection refused" | Gateway running? | Start gateway |
| "DNS resolution failed" | Network config | Check DNS settings |
| "TLS handshake failed" | Certificate | Update CA certs |
| "Authentication failed" | API keys | Verify credentials |

### Order Issues

| Symptom | Check | Fix |
|---------|-------|-----|
| "Insufficient balance" | Account balance | Deposit funds |
| "Position limit exceeded" | Risk limits | Increase limit or reduce position |
| "Rate limit exceeded" | Order frequency | Add delays between orders |
| "Invalid symbol" | Symbol format | Use correct format (e.g., BTCUSDT) |

### Performance Issues

| Symptom | Check | Fix |
|---------|-------|-----|
| High latency | Latency breakdown | Optimize slow component |
| Low throughput | CPU/memory | Increase resources |
| Latency spikes | GC/memory | Tune memory pools |
| Stale data | WebSocket | Reconnect/resubscribe |

---

## Step 10: Escalation Procedures

When self-service troubleshooting fails:

### Collect Diagnostic Bundle

```bash
# Generate diagnostic bundle
curl -X POST http://127.0.0.1:8080/api/diagnostics/bundle \
  -o veloz_diagnostics.tar.gz
```

**Expected Output:**
```json
{
  "ok": true,
  "bundle_path": "/tmp/veloz_diagnostics_20260223_100000.tar.gz",
  "contents": [
    "system_info.json",
    "config.json",
    "recent_logs.txt",
    "metrics_snapshot.json",
    "thread_dump.txt"
  ]
}
```

### Information to Include in Support Request

1. Diagnostic bundle
2. Steps to reproduce
3. Expected vs actual behavior
4. Timestamp of issue occurrence
5. Any recent configuration changes

---

## Summary

**What you accomplished:**
- Checked system health and identified issues
- Debugged exchange connection problems
- Diagnosed order rejection causes
- Identified and fixed performance bottlenecks
- Resolved market data synchronization issues
- Troubleshot strategy behavior problems
- Used diagnostic tools and logs effectively
- Learned escalation procedures

## Troubleshooting

### Issue: Diagnostic endpoints not responding
**Symptom:** Timeout when calling diagnostic APIs
**Solution:** Check if gateway is overloaded:
```bash
curl http://127.0.0.1:8080/api/health
```
If unresponsive, restart gateway.

### Issue: Logs not showing expected errors
**Symptom:** No errors in logs despite issues
**Solution:** Check log level configuration:
```bash
curl http://127.0.0.1:8080/api/config/logging
```
Ensure log level is set to "debug" for troubleshooting.

### Issue: Cannot generate diagnostic bundle
**Symptom:** Bundle generation fails
**Solution:** Check disk space and permissions:
```bash
df -h /tmp
ls -la /tmp
```

## Next Steps

- [Risk Management in Practice](./risk-management-practice.md) - Debug risk-related issues
- [Performance Tuning](./performance-tuning-practice.md) - Optimize after fixing issues
- [Troubleshooting Guide](../guides/user/troubleshooting.md) - Complete troubleshooting reference
- [Operations Runbook](../guides/deployment/operations_runbook.md) - Production troubleshooting procedures
- [Glossary](../guides/user/glossary.md) - Definitions of technical terms
