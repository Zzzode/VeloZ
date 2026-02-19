# Monitoring and Metrics Guide

This guide explains how to monitor VeloZ system health, performance, and trading activity.

## Overview

VeloZ provides several monitoring capabilities:
- Real-time event streaming via SSE
- Health check endpoints
- Order and account state queries
- Audit logging
- Performance metrics

## Health Monitoring

### Health Check Endpoint

```bash
curl http://127.0.0.1:8080/health

# Response:
# {"ok": true}
```

Use this endpoint for:
- Load balancer health checks
- Container orchestration probes
- Uptime monitoring

### Configuration Check

```bash
curl http://127.0.0.1:8080/api/config

# Response:
# {
#   "market_source": "binance_rest",
#   "market_symbol": "BTCUSDT",
#   "execution_mode": "binance_testnet_spot",
#   "binance_trade_enabled": true,
#   "binance_user_stream_connected": true,
#   "auth_enabled": false
# }
```

Key fields to monitor:
- `binance_user_stream_connected`: WebSocket connection status
- `binance_trade_enabled`: Trading capability status

### Execution Ping

```bash
curl http://127.0.0.1:8080/api/execution/ping

# Response:
# {"ok": true, "result": {}}
```

This verifies connectivity to the execution venue (Binance).

## Real-Time Event Monitoring

### SSE Event Stream

Subscribe to all system events:

```bash
curl -N http://127.0.0.1:8080/api/stream
```

### Event Types

| Event | Description | Use Case |
|-------|-------------|----------|
| `market` | Price updates | Market data monitoring |
| `order_update` | Order status changes | Order tracking |
| `fill` | Trade executions | P&L tracking |
| `account` | Balance changes | Risk monitoring |
| `error` | System errors | Alerting |

### Monitoring Script Example

```python
#!/usr/bin/env python3
"""Simple SSE monitoring script."""

import json
import urllib.request
import time

def monitor_events():
    url = "http://127.0.0.1:8080/api/stream"
    req = urllib.request.Request(url)

    with urllib.request.urlopen(req) as response:
        buffer = ""
        for line in response:
            line = line.decode("utf-8")
            buffer += line

            if line == "\n" and buffer.strip():
                # Parse SSE event
                event_type = None
                data = None

                for part in buffer.strip().split("\n"):
                    if part.startswith("event:"):
                        event_type = part[6:].strip()
                    elif part.startswith("data:"):
                        data = json.loads(part[5:].strip())

                if event_type and data:
                    handle_event(event_type, data)

                buffer = ""

def handle_event(event_type, data):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")

    if event_type == "error":
        print(f"[{timestamp}] ERROR: {data.get('message')}")
    elif event_type == "fill":
        print(f"[{timestamp}] FILL: {data.get('symbol')} "
              f"{data.get('qty')} @ {data.get('price')}")
    elif event_type == "order_update":
        print(f"[{timestamp}] ORDER: {data.get('client_order_id')} "
              f"-> {data.get('status')}")

if __name__ == "__main__":
    print("Starting VeloZ monitor...")
    monitor_events()
```

## Order Monitoring

### List All Orders

```bash
curl http://127.0.0.1:8080/api/orders_state

# Response:
# {
#   "items": [
#     {
#       "client_order_id": "order-001",
#       "symbol": "BTCUSDT",
#       "side": "BUY",
#       "order_qty": 0.001,
#       "limit_price": 40000.0,
#       "status": "FILLED",
#       "executed_qty": 0.001,
#       "avg_price": 40000.0
#     }
#   ]
# }
```

### Track Specific Order

```bash
curl "http://127.0.0.1:8080/api/order_state?client_order_id=order-001"
```

### Order Status Summary Script

```bash
#!/bin/bash
# order_summary.sh - Summarize order statuses

curl -s http://127.0.0.1:8080/api/orders_state | jq '
  .items | group_by(.status) | map({
    status: .[0].status,
    count: length,
    total_qty: (map(.order_qty // 0) | add)
  })
'
```

## Account Monitoring

### Current Balances

```bash
curl http://127.0.0.1:8080/api/account

# Response:
# {
#   "ts_ns": 1704067200000000000,
#   "balances": [
#     {"asset": "USDT", "free": 9500.0, "locked": 500.0},
#     {"asset": "BTC", "free": 0.01, "locked": 0.001}
#   ]
# }
```

### Balance Monitoring Script

```python
#!/usr/bin/env python3
"""Monitor account balances."""

import json
import urllib.request
import time

def get_balances():
    url = "http://127.0.0.1:8080/api/account"
    with urllib.request.urlopen(url) as response:
        return json.loads(response.read())

def monitor_balances(interval=60):
    print("Monitoring balances...")
    prev_balances = {}

    while True:
        data = get_balances()
        current = {b["asset"]: b for b in data.get("balances", [])}

        for asset, balance in current.items():
            total = balance["free"] + balance["locked"]
            prev = prev_balances.get(asset, {})
            prev_total = prev.get("free", 0) + prev.get("locked", 0)

            if prev_total != total:
                change = total - prev_total
                print(f"{asset}: {total:.8f} ({change:+.8f})")

        prev_balances = current
        time.sleep(interval)

if __name__ == "__main__":
    monitor_balances()
```

## Audit Logging

When authentication is enabled, VeloZ logs all authenticated actions:

### Log Format

```
2024-01-01 12:00:00 [AUDIT] {"action": "place_order", "user": "admin", "auth_method": "jwt", "ip": "127.0.0.1", "client_order_id": "order-001", "symbol": "BTCUSDT", "side": "BUY", "qty": 0.001, "price": 40000.0}
```

### Logged Actions

| Action | Description |
|--------|-------------|
| `place_order` | Order placed |
| `cancel_order` | Order cancelled |
| `list_orders` | Orders listed |
| `list_orders_state` | Order states listed |
| `get_order_state` | Single order state queried |
| `get_account` | Account balance queried |
| `create_api_key` | API key created |
| `revoke_api_key` | API key revoked |
| `list_api_keys` | API keys listed |

### Enable Audit Logging

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
./scripts/run_gateway.sh dev
```

## Performance Metrics

### Market Data Latency

Monitor market data freshness by checking timestamps:

```bash
#!/bin/bash
# Check market data latency

while true; do
    response=$(curl -s http://127.0.0.1:8080/api/market)
    ts_ns=$(echo $response | jq '.ts_ns')
    now_ns=$(date +%s%N)
    latency_ms=$(( (now_ns - ts_ns) / 1000000 ))
    echo "Market data latency: ${latency_ms}ms"
    sleep 1
done
```

### Request Timing

```bash
# Time API requests
time curl -s http://127.0.0.1:8080/api/market > /dev/null
```

## Alerting

### Error Event Alerting

```python
#!/usr/bin/env python3
"""Alert on error events."""

import json
import urllib.request
import smtplib
from email.message import EmailMessage

def send_alert(message):
    # Configure your alerting method
    print(f"ALERT: {message}")
    # Example: send email, Slack webhook, PagerDuty, etc.

def monitor_errors():
    url = "http://127.0.0.1:8080/api/stream"
    req = urllib.request.Request(url)

    with urllib.request.urlopen(req) as response:
        buffer = ""
        for line in response:
            line = line.decode("utf-8")
            buffer += line

            if line == "\n" and buffer.strip():
                if "event: error" in buffer:
                    for part in buffer.strip().split("\n"):
                        if part.startswith("data:"):
                            data = json.loads(part[5:].strip())
                            send_alert(data.get("message", "Unknown error"))
                buffer = ""

if __name__ == "__main__":
    monitor_errors()
```

### Health Check Alerting

```bash
#!/bin/bash
# health_check.sh - Alert if gateway is unhealthy

ENDPOINT="http://127.0.0.1:8080/health"
ALERT_CMD="echo 'VeloZ gateway is down!'"

while true; do
    response=$(curl -s -o /dev/null -w "%{http_code}" $ENDPOINT)
    if [ "$response" != "200" ]; then
        eval $ALERT_CMD
    fi
    sleep 30
done
```

## Dashboard Integration

### Prometheus Metrics (Future)

VeloZ is planned to expose Prometheus-compatible metrics:

```
# HELP veloz_orders_total Total orders placed
# TYPE veloz_orders_total counter
veloz_orders_total{status="filled"} 150
veloz_orders_total{status="cancelled"} 20
veloz_orders_total{status="rejected"} 5

# HELP veloz_market_latency_ms Market data latency in milliseconds
# TYPE veloz_market_latency_ms gauge
veloz_market_latency_ms 50
```

### Grafana Dashboard (Future)

A Grafana dashboard template will be provided for:
- Order throughput
- Fill rate
- P&L tracking
- Market data latency
- Error rates

## Best Practices

### 1. Set Up Health Checks

Configure your orchestration system to check `/health` regularly:

```yaml
# Kubernetes example
livenessProbe:
  httpGet:
    path: /health
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 30
```

### 2. Monitor Error Events

Always have an error event monitor running to catch issues early.

### 3. Track Order States

Regularly reconcile order states with your expected positions.

### 4. Log Retention

Keep audit logs for compliance and debugging:
- Minimum 30 days for trading logs
- 90 days recommended for full audit trail

### 5. Alert Thresholds

Set up alerts for:
- Health check failures
- Error event rate > 1/minute
- Market data latency > 1 second
- WebSocket disconnections

## Related

- [HTTP API Reference](../api/http_api.md) - Complete API documentation
- [SSE API](../api/sse_api.md) - Event stream details
- [Configuration Guide](configuration.md) - All configuration options
- [Binance Integration](binance.md) - Binance-specific monitoring
