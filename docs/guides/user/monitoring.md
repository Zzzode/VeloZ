# Monitoring and Observability Guide

This guide explains how to monitor VeloZ system health, trading performance, and risk metrics.

## Overview

VeloZ v1.0 provides comprehensive observability through:

| Component | Purpose | Endpoint/Tool |
|-----------|---------|---------------|
| **Health Checks** | System availability | `/health`, `/health/live`, `/health/ready` |
| **Metrics** | Performance and trading data | `/metrics` (Prometheus format) |
| **Logging** | Structured JSON logs | Loki/Promtail |
| **Tracing** | Request correlation | Jaeger |
| **Audit Logs** | Security and compliance | `/api/audit/logs` |
| **SSE Stream** | Real-time events | `/api/stream` |

---

## Quick Start

### Check System Health

```bash
# Basic health check
curl http://127.0.0.1:8080/health
# Response: {"ok": true}

# Detailed health with components
curl http://127.0.0.1:8080/health/ready
# Response: {"ok": true, "components": {...}, "uptime_seconds": 3600}
```

### View Metrics

```bash
# Get Prometheus metrics
curl http://127.0.0.1:8080/metrics

# Key metrics to watch:
# veloz_orders_total - Total orders processed
# veloz_latency_ms - Order processing latency
# veloz_risk_current_drawdown - Current drawdown percentage
```

### Monitor Real-Time Events

```bash
# Subscribe to SSE stream
curl -N http://127.0.0.1:8080/api/stream

# Events include: market, order_update, fill, account, error
```

---

## Health Monitoring

### Health Check Endpoints

| Endpoint | Purpose | Use Case |
|----------|---------|----------|
| `GET /health` | Basic health | Quick availability check |
| `GET /health/live` | Liveness probe | Kubernetes liveness |
| `GET /health/ready` | Readiness probe | Kubernetes readiness |

### Health Check Response

```json
{
  "ok": true,
  "components": {
    "engine": "healthy",
    "websocket": "connected",
    "database": "healthy"
  },
  "uptime_seconds": 3600,
  "version": "1.0.0"
}
```

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

# Response: {"ok": true, "result": {}}
```

This verifies connectivity to the execution venue (Binance).

### Monitoring Script

```bash
#!/bin/bash
# health_monitor.sh - Monitor VeloZ health

ENDPOINT="http://127.0.0.1:8080/health"
INTERVAL=30

while true; do
    response=$(curl -s -o /dev/null -w "%{http_code}" $ENDPOINT)
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    if [ "$response" == "200" ]; then
        echo "[$timestamp] OK"
    else
        echo "[$timestamp] UNHEALTHY (HTTP $response)"
        # Add alerting here (email, Slack, PagerDuty)
    fi

    sleep $INTERVAL
done
```

---

## Metrics Reference

VeloZ exposes Prometheus-compatible metrics at `/metrics`.

### Trading Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_orders_total` | Counter | Total orders processed |
| `veloz_orders_by_status` | Counter | Orders by status (filled, cancelled, rejected) |
| `veloz_latency_ms` | Histogram | Order processing latency |
| `veloz_market_events_total` | Counter | Market events received |

### Risk Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_risk_var_95` | Gauge | 95% Value at Risk (daily) |
| `veloz_risk_var_99` | Gauge | 99% Value at Risk (daily) |
| `veloz_risk_max_drawdown` | Gauge | Maximum drawdown percentage |
| `veloz_risk_current_drawdown` | Gauge | Current drawdown from peak |
| `veloz_risk_sharpe_ratio` | Gauge | Sharpe ratio |
| `veloz_risk_sortino_ratio` | Gauge | Sortino ratio |
| `veloz_risk_calmar_ratio` | Gauge | Calmar ratio |
| `veloz_risk_win_rate` | Gauge | Trade win rate percentage |
| `veloz_risk_profit_factor` | Gauge | Profit factor |

### Position Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_position_size` | Gauge | symbol, side | Position size by symbol |
| `veloz_position_unrealized_pnl` | Gauge | symbol | Unrealized PnL by symbol |
| `veloz_position_realized_pnl` | Counter | symbol | Cumulative realized PnL |
| `veloz_position_notional` | Gauge | symbol | Notional value by symbol |
| `veloz_position_total_unrealized_pnl` | Gauge | - | Total unrealized PnL |
| `veloz_position_total_realized_pnl` | Counter | - | Total realized PnL |

### Exposure Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_exposure_gross` | Gauge | Sum of absolute position values |
| `veloz_exposure_net` | Gauge | Sum of signed position values |
| `veloz_exposure_long` | Gauge | Sum of long position values |
| `veloz_exposure_short` | Gauge | Sum of short position values |
| `veloz_exposure_leverage_ratio` | Gauge | Gross exposure / account equity |

### Concentration Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_concentration_largest_pct` | Gauge | Largest position as % of total |
| `veloz_concentration_top3_pct` | Gauge | Top 3 positions as % of total |
| `veloz_concentration_hhi` | Gauge | Herfindahl-Hirschman Index (0-1) |
| `veloz_concentration_position_count` | Gauge | Number of open positions |

### System Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_websocket_connections` | Gauge | Active WebSocket connections |
| `veloz_websocket_reconnects` | Counter | WebSocket reconnection count |
| `veloz_memory_pool_used_bytes` | Gauge | Memory pool usage |
| `veloz_event_loop_tasks` | Gauge | Pending event loop tasks |

---

## Real-Time Event Monitoring

### SSE Event Stream

Subscribe to real-time events:

```bash
curl -N http://127.0.0.1:8080/api/stream
```

### Event Types

| Event | Description | Key Fields |
|-------|-------------|------------|
| `market` | Price updates | symbol, price, ts_ns |
| `order_update` | Order status changes | client_order_id, status |
| `fill` | Trade executions | client_order_id, qty, price |
| `account` | Balance changes | ts_ns |
| `error` | System errors | message, code |

### Python Monitoring Script

```python
#!/usr/bin/env python3
"""Real-time VeloZ event monitor."""

import json
import urllib.request
from datetime import datetime

def monitor_events(base_url="http://127.0.0.1:8080"):
    """Monitor SSE events from VeloZ."""
    url = f"{base_url}/api/stream"

    print(f"Connecting to {url}...")
    req = urllib.request.Request(url)

    with urllib.request.urlopen(req) as response:
        buffer = ""
        for line in response:
            line = line.decode("utf-8")
            buffer += line

            if line == "\n" and buffer.strip():
                process_event(buffer)
                buffer = ""

def process_event(buffer):
    """Process a single SSE event."""
    event_type = None
    data = None

    for part in buffer.strip().split("\n"):
        if part.startswith("event:"):
            event_type = part[6:].strip()
        elif part.startswith("data:"):
            try:
                data = json.loads(part[5:].strip())
            except json.JSONDecodeError:
                continue

    if not event_type or not data:
        return

    timestamp = datetime.now().strftime("%H:%M:%S")

    if event_type == "error":
        print(f"[{timestamp}] ERROR: {data.get('message')}")
    elif event_type == "fill":
        print(f"[{timestamp}] FILL: {data.get('symbol')} "
              f"{data.get('qty')} @ {data.get('price')}")
    elif event_type == "order_update":
        print(f"[{timestamp}] ORDER: {data.get('client_order_id')} "
              f"-> {data.get('status')}")
    elif event_type == "market":
        print(f"[{timestamp}] MARKET: {data.get('symbol')} "
              f"${data.get('price')}")

if __name__ == "__main__":
    try:
        monitor_events()
    except KeyboardInterrupt:
        print("\nMonitoring stopped.")
```

---

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

---

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

---

## Prometheus Setup

### Quick Setup with Docker

```yaml
# docker-compose.monitoring.yml
version: '3.8'

services:
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    volumes:
      - grafana-data:/var/lib/grafana

volumes:
  grafana-data:
```

### Prometheus Configuration

```yaml
# prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'veloz'
    static_configs:
      - targets: ['host.docker.internal:8080']
    metrics_path: /metrics
    scrape_interval: 5s

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - /etc/prometheus/alerts/*.yml
```

### Start Monitoring Stack

```bash
# Start Prometheus and Grafana
docker-compose -f docker-compose.monitoring.yml up -d

# Access:
# - Prometheus: http://localhost:9090
# - Grafana: http://localhost:3000 (admin/admin)
```

---

## Grafana Dashboards

VeloZ provides four pre-built Grafana dashboards.

### 1. Trading Dashboard

Key panels:
- Orders per second
- Order latency (p50, p95, p99)
- Fill rate percentage
- Order status breakdown

**Sample queries:**
```promql
# Orders per second
rate(veloz_orders_total[1m])

# P99 latency
histogram_quantile(0.99, rate(veloz_latency_ms_bucket[5m]))

# Fill rate
sum(veloz_orders_by_status{status="filled"}) / sum(veloz_orders_total) * 100
```

### 2. Risk Dashboard

Key panels:
- Value at Risk (VaR 95%, 99%)
- Current and max drawdown
- Sharpe/Sortino ratios
- Exposure (gross, net, leverage)
- Concentration (HHI, largest position %)

**Sample queries:**
```promql
# Current drawdown
veloz_risk_current_drawdown

# Leverage ratio
veloz_exposure_leverage_ratio

# Concentration index
veloz_concentration_hhi
```

### 3. Position Dashboard

Key panels:
- Position sizes by symbol
- Unrealized PnL by symbol
- Total PnL (unrealized + realized)
- Total notional value

**Sample queries:**
```promql
# Position size by symbol
veloz_position_size

# Total unrealized PnL
veloz_position_total_unrealized_pnl
```

### 4. System Dashboard

Key panels:
- Memory usage
- WebSocket connections
- Event loop queue depth
- WebSocket reconnections

**Sample queries:**
```promql
# Memory usage
veloz_memory_pool_used_bytes

# WebSocket status
veloz_websocket_connections
```

---

## Alerting

### Alert Rules

VeloZ includes pre-configured alert rules for critical conditions.

#### System Alerts

| Alert | Condition | Severity |
|-------|-----------|----------|
| VelozEngineDown | `up{job="veloz"} == 0` for 1m | Critical |
| VelozHighLatency | P99 latency > 100ms for 5m | Warning |
| VelozHighErrorRate | Error rate > 0.1/s for 5m | Warning |
| VelozMemoryPressure | Memory > 90% for 5m | Warning |
| VelozWebSocketDisconnected | Connections == 0 for 1m | Critical |

#### Risk Alerts

| Alert | Condition | Severity |
|-------|-----------|----------|
| VelozHighDrawdown | Drawdown > 10% for 5m | Warning |
| VelozCriticalDrawdown | Drawdown > 20% for 1m | Critical |
| VelozHighLeverage | Leverage > 3x for 5m | Warning |
| VelozHighConcentration | Largest position > 50% for 5m | Warning |
| VelozVaRBreach | Loss exceeds VaR 95% for 5m | Critical |
| VelozNegativeSharpe | Sharpe < 0 for 1h | Warning |
| VelozLowWinRate | Win rate < 30% (after 100 trades) for 30m | Warning |

### Alertmanager Configuration

```yaml
# alertmanager.yml
global:
  resolve_timeout: 5m

route:
  group_by: ['alertname', 'severity']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 1h
  receiver: 'default'
  routes:
    - match:
        severity: critical
      receiver: 'pagerduty'
    - match:
        severity: warning
      receiver: 'slack'

receivers:
  - name: 'default'
    email_configs:
      - to: 'ops@example.com'

  - name: 'slack'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/YOUR/WEBHOOK/URL'
        channel: '#veloz-alerts'
        title: '{{ .GroupLabels.alertname }}'
        text: '{{ .CommonAnnotations.description }}'

  - name: 'pagerduty'
    pagerduty_configs:
      - service_key: 'YOUR_PAGERDUTY_KEY'
```

---

## Log Aggregation

### Structured Log Format

VeloZ uses structured JSON logging:

```json
{
  "timestamp": "2026-02-23T10:30:00.123Z",
  "level": "INFO",
  "component": "engine",
  "message": "Order processed",
  "order_id": "web-1234567890",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "latency_ms": 0.5
}
```

### Log Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| `ERROR` | Error conditions | Failures, exceptions |
| `WARN` | Warning conditions | Degraded performance, retries |
| `INFO` | Informational | Normal operations |
| `DEBUG` | Debug information | Development, troubleshooting |
| `TRACE` | Detailed tracing | Performance analysis |

### Loki Setup

```yaml
# docker-compose.loki.yml
version: '3.8'

services:
  loki:
    image: grafana/loki:latest
    ports:
      - "3100:3100"
    volumes:
      - ./loki-config.yml:/etc/loki/local-config.yaml
    command: -config.file=/etc/loki/local-config.yaml

  promtail:
    image: grafana/promtail:latest
    volumes:
      - ./promtail-config.yml:/etc/promtail/config.yml
      - /var/log/veloz:/var/log/veloz:ro
    command: -config.file=/etc/promtail/config.yml
```

### Promtail Configuration

```yaml
# promtail-config.yml
server:
  http_listen_port: 9080

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://loki:3100/loki/api/v1/push

scrape_configs:
  - job_name: veloz
    static_configs:
      - targets:
          - localhost
        labels:
          job: veloz
          __path__: /var/log/veloz/*.log
    pipeline_stages:
      - json:
          expressions:
            level: level
            component: component
            message: message
      - labels:
          level:
          component:
```

### Log Queries in Grafana

```logql
# All errors
{job="veloz"} |= "ERROR"

# Order processing logs
{job="veloz", component="engine"} | json | message =~ "Order.*"

# High latency operations
{job="veloz"} | json | latency_ms > 100
```

---

## Audit Logging

### Overview

VeloZ provides comprehensive audit logging for security and compliance.

### Audit Log API

```bash
# Query recent audit logs
curl "http://127.0.0.1:8080/api/audit/logs?log_type=auth&limit=100"

# Get audit statistics
curl "http://127.0.0.1:8080/api/audit/stats"

# Trigger manual archiving
curl -X POST "http://127.0.0.1:8080/api/audit/archive"
```

### Logged Events

| Log Type | Events | Retention |
|----------|--------|-----------|
| `auth` | Login, logout, token refresh | 90 days |
| `order` | Order placement, cancellation, fills | 365 days |
| `api_key` | Key creation, revocation | 365 days |
| `error` | System errors | 30 days |
| `access` | General API access | 14 days |

### Audit Log Format

```json
{
  "timestamp": 1708689600.123,
  "datetime": "2026-02-23T10:00:00.123Z",
  "log_type": "auth",
  "action": "login_success",
  "user_id": "admin",
  "ip_address": "192.168.1.100",
  "details": {
    "auth_method": "jwt",
    "user_agent": "curl/7.68.0"
  },
  "request_id": "req-abc123"
}
```

### Enable Audit Logging

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_AUDIT_LOG_ENABLED=true
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit/audit.log
./scripts/run_gateway.sh dev
```

---

## Distributed Tracing

### Jaeger Setup

```yaml
# docker-compose.jaeger.yml
version: '3.8'

services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "6831:6831/udp"  # Thrift compact
      - "14268:14268"  # HTTP collector
    environment:
      - COLLECTOR_ZIPKIN_HOST_PORT=:9411
```

### Request Correlation

VeloZ supports request tracing via headers:

```bash
# Include correlation ID in requests
curl -H "X-Request-ID: my-request-123" \
     -H "X-Correlation-ID: order-flow-456" \
     http://127.0.0.1:8080/api/order
```

### Trace Propagation

All downstream calls include trace context:
- `X-Request-ID`: Unique request identifier
- `X-Correlation-ID`: Business flow correlation

---

## Monitoring Best Practices

### 1. Set Up Health Checks

Configure load balancers and orchestration to check `/health`:

```yaml
# Kubernetes example
livenessProbe:
  httpGet:
    path: /health/live
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 5

readinessProbe:
  httpGet:
    path: /health/ready
    port: 8080
  initialDelaySeconds: 5
  periodSeconds: 3
```

### 2. Monitor Key Metrics

Essential metrics to watch:

| Metric | Warning Threshold | Critical Threshold |
|--------|-------------------|-------------------|
| Order latency (p99) | > 50ms | > 100ms |
| Error rate | > 0.05/s | > 0.1/s |
| Drawdown | > 10% | > 20% |
| Leverage | > 2x | > 3x |
| WebSocket connections | < 1 | 0 |

### 3. Configure Alerting

Set up alerts for:
- System availability (engine down, WebSocket disconnected)
- Performance degradation (high latency, error rate)
- Risk thresholds (drawdown, leverage, VaR breach)
- Resource usage (memory, CPU)

### 4. Retain Audit Logs

For compliance:
- Keep auth logs for 90+ days
- Keep order logs for 365+ days
- Archive before deletion
- Regular backup verification

### 5. Review Dashboards Daily

Check daily:
- Trading performance (fill rate, latency)
- Risk metrics (drawdown, exposure)
- System health (memory, connections)
- Error logs

---

## Troubleshooting

### Metrics Not Appearing

```bash
# Verify metrics endpoint
curl http://127.0.0.1:8080/metrics | head -20

# Check Prometheus targets
curl http://localhost:9090/api/v1/targets

# Verify scrape config
cat prometheus.yml | grep veloz
```

### Alerts Not Firing

```bash
# Check Alertmanager status
curl http://localhost:9093/api/v1/status

# View active alerts
curl http://localhost:9093/api/v1/alerts

# Test alert rule
curl http://localhost:9090/api/v1/rules
```

### Logs Not Aggregating

```bash
# Check Promtail status
curl http://localhost:9080/ready

# Verify log file path
ls -la /var/log/veloz/

# Check Loki ingestion
curl http://localhost:3100/ready
```

### Market Data Latency

Monitor market data freshness:

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

---

## Related Documentation

- [Configuration Guide](configuration.md) - Monitoring environment variables
- [Troubleshooting Guide](troubleshooting.md) - Common issues and solutions
- [Deployment Monitoring](../deployment/monitoring.md) - Production monitoring setup
- [API Reference](../../api/http_api.md) - Complete API documentation
