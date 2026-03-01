# Monitoring and Observability

This document describes how to set up monitoring, metrics, and alerting for VeloZ.

## Overview

VeloZ provides comprehensive observability through:
- **Metrics**: Prometheus-compatible metrics endpoint
- **Logging**: Structured JSON logging
- **Tracing**: Request tracing with correlation IDs
- **Health Checks**: Liveness and readiness probes

## Metrics

### Built-in Metrics

VeloZ exposes metrics via the `/metrics` endpoint:

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_orders_total` | Counter | Total orders processed |
| `veloz_orders_by_status` | Counter | Orders by status (filled, cancelled, rejected) |
| `veloz_latency_ms` | Histogram | Order processing latency |
| `veloz_market_events_total` | Counter | Market events received |
| `veloz_websocket_connections` | Gauge | Active WebSocket connections |
| `veloz_websocket_reconnects` | Counter | WebSocket reconnection count |
| `veloz_memory_pool_used_bytes` | Gauge | Memory pool usage |
| `veloz_event_loop_tasks` | Gauge | Pending event loop tasks |

### Risk Metrics (P6-002)

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_risk_var_95` | Gauge | 95% Value at Risk (daily) |
| `veloz_risk_var_99` | Gauge | 99% Value at Risk (daily) |
| `veloz_risk_max_drawdown` | Gauge | Maximum drawdown percentage |
| `veloz_risk_current_drawdown` | Gauge | Current drawdown from peak |
| `veloz_risk_sharpe_ratio` | Gauge | Sharpe ratio |
| `veloz_risk_sortino_ratio` | Gauge | Sortino ratio (downside risk-adjusted) |
| `veloz_risk_calmar_ratio` | Gauge | Calmar ratio (return/max drawdown) |
| `veloz_risk_win_rate` | Gauge | Trade win rate percentage |
| `veloz_risk_profit_factor` | Gauge | Profit factor (gross profit/gross loss) |

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

### Position Metrics (P6-001)

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_position_size` | Gauge | Position size by symbol (labels: symbol, side) |
| `veloz_position_unrealized_pnl` | Gauge | Unrealized PnL by symbol |
| `veloz_position_realized_pnl` | Counter | Cumulative realized PnL by symbol |
| `veloz_position_notional` | Gauge | Notional value by symbol |
| `veloz_position_avg_price` | Gauge | Average entry price by symbol |
| `veloz_position_total_unrealized_pnl` | Gauge | Total unrealized PnL across all positions |
| `veloz_position_total_realized_pnl` | Counter | Total realized PnL across all positions |
| `veloz_position_total_notional` | Gauge | Total notional value across all positions |

### Prometheus Configuration

```yaml
# prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'veloz'
    static_configs:
      - targets: ['gateway:8080']
    metrics_path: /metrics
    scrape_interval: 5s

  - job_name: 'veloz-engine'
    static_configs:
      - targets: ['engine:9090']
    scrape_interval: 5s

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - /etc/prometheus/alerts/*.yml
```

### Grafana Dashboards

#### Trading Dashboard

```json
{
  "dashboard": {
    "title": "VeloZ Trading",
    "panels": [
      {
        "title": "Orders per Second",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(veloz_orders_total[1m])",
            "legendFormat": "Orders/s"
          }
        ]
      },
      {
        "title": "Order Latency (p99)",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.99, rate(veloz_latency_ms_bucket[5m]))",
            "legendFormat": "p99 Latency"
          }
        ]
      },
      {
        "title": "Fill Rate",
        "type": "stat",
        "targets": [
          {
            "expr": "sum(veloz_orders_by_status{status='filled'}) / sum(veloz_orders_total) * 100",
            "legendFormat": "Fill Rate %"
          }
        ]
      }
    ]
  }
}
```

#### Risk Dashboard

```json
{
  "dashboard": {
    "title": "VeloZ Risk",
    "panels": [
      {
        "title": "Value at Risk",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_risk_var_95",
            "legendFormat": "VaR 95%"
          },
          {
            "expr": "veloz_risk_var_99",
            "legendFormat": "VaR 99%"
          }
        ]
      },
      {
        "title": "Drawdown",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_risk_current_drawdown",
            "legendFormat": "Current Drawdown"
          },
          {
            "expr": "veloz_risk_max_drawdown",
            "legendFormat": "Max Drawdown"
          }
        ]
      },
      {
        "title": "Risk-Adjusted Returns",
        "type": "stat",
        "targets": [
          {
            "expr": "veloz_risk_sharpe_ratio",
            "legendFormat": "Sharpe"
          },
          {
            "expr": "veloz_risk_sortino_ratio",
            "legendFormat": "Sortino"
          }
        ]
      },
      {
        "title": "Exposure",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_exposure_gross",
            "legendFormat": "Gross"
          },
          {
            "expr": "veloz_exposure_net",
            "legendFormat": "Net"
          },
          {
            "expr": "veloz_exposure_leverage_ratio",
            "legendFormat": "Leverage"
          }
        ]
      },
      {
        "title": "Concentration",
        "type": "gauge",
        "targets": [
          {
            "expr": "veloz_concentration_hhi",
            "legendFormat": "HHI"
          }
        ]
      },
      {
        "title": "Position Count",
        "type": "stat",
        "targets": [
          {
            "expr": "veloz_concentration_position_count",
            "legendFormat": "Positions"
          }
        ]
      }
    ]
  }
}
```

#### Position Dashboard

```json
{
  "dashboard": {
    "title": "VeloZ Positions",
    "panels": [
      {
        "title": "Position Sizes by Symbol",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_position_size",
            "legendFormat": "{{symbol}}"
          }
        ]
      },
      {
        "title": "Unrealized PnL by Symbol",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_position_unrealized_pnl",
            "legendFormat": "{{symbol}}"
          }
        ]
      },
      {
        "title": "Total PnL",
        "type": "stat",
        "targets": [
          {
            "expr": "veloz_position_total_unrealized_pnl",
            "legendFormat": "Unrealized"
          },
          {
            "expr": "veloz_position_total_realized_pnl",
            "legendFormat": "Realized"
          }
        ]
      },
      {
        "title": "Total Notional",
        "type": "stat",
        "targets": [
          {
            "expr": "veloz_position_total_notional",
            "legendFormat": "Notional Value"
          }
        ]
      }
    ]
  }
}
```

#### System Dashboard

```json
{
  "dashboard": {
    "title": "VeloZ System",
    "panels": [
      {
        "title": "Memory Usage",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_memory_pool_used_bytes",
            "legendFormat": "Memory Used"
          }
        ]
      },
      {
        "title": "WebSocket Connections",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_websocket_connections",
            "legendFormat": "Active Connections"
          }
        ]
      },
      {
        "title": "Event Loop Queue",
        "type": "graph",
        "targets": [
          {
            "expr": "veloz_event_loop_tasks",
            "legendFormat": "Pending Tasks"
          }
        ]
      }
    ]
  }
}
```

## Logging

### Log Format

VeloZ uses structured JSON logging:

```json
{
  "timestamp": "2026-02-14T10:30:00.123Z",
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

### Log Aggregation

#### Loki Configuration

```yaml
# loki-config.yml
auth_enabled: false

server:
  http_listen_port: 3100

ingester:
  lifecycler:
    ring:
      kvstore:
        store: inmemory
      replication_factor: 1

schema_config:
  configs:
    - from: 2020-01-01
      store: boltdb-shipper
      object_store: filesystem
      schema: v11
      index:
        prefix: index_
        period: 24h

storage_config:
  boltdb_shipper:
    active_index_directory: /loki/index
    cache_location: /loki/cache
  filesystem:
    directory: /loki/chunks
```

#### Promtail Configuration

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

## Alerting

### Alert Rules

```yaml
# alerts/veloz.yml
groups:
  - name: veloz
    rules:
      # High error rate
      - alert: VelozHighErrorRate
        expr: rate(veloz_orders_by_status{status="rejected"}[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High order rejection rate"
          description: "Order rejection rate is {{ $value | humanizePercentage }}"

      # High latency
      - alert: VelozHighLatency
        expr: histogram_quantile(0.99, rate(veloz_latency_ms_bucket[5m])) > 100
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High order latency"
          description: "P99 latency is {{ $value }}ms"

      # WebSocket disconnected
      - alert: VelozWebSocketDisconnected
        expr: veloz_websocket_connections == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "WebSocket disconnected"
          description: "No active WebSocket connections"

      # Memory pressure
      - alert: VelozMemoryPressure
        expr: veloz_memory_pool_used_bytes / veloz_memory_pool_total_bytes > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage"
          description: "Memory pool is {{ $value | humanizePercentage }} full"

      # Gateway down
      - alert: VelozGatewayDown
        expr: up{job="veloz"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "VeloZ gateway is down"
          description: "Gateway instance {{ $labels.instance }} is unreachable"

  - name: veloz-risk
    rules:
      # High drawdown alert
      - alert: VelozHighDrawdown
        expr: veloz_risk_current_drawdown > 0.10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High drawdown detected"
          description: "Current drawdown is {{ $value | humanizePercentage }}"

      # Critical drawdown alert
      - alert: VelozCriticalDrawdown
        expr: veloz_risk_current_drawdown > 0.20
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Critical drawdown - consider halting trading"
          description: "Current drawdown is {{ $value | humanizePercentage }}"

      # High leverage alert
      - alert: VelozHighLeverage
        expr: veloz_exposure_leverage_ratio > 3.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High leverage detected"
          description: "Leverage ratio is {{ $value }}"

      # High concentration alert
      - alert: VelozHighConcentration
        expr: veloz_concentration_largest_pct > 0.50
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High position concentration"
          description: "Largest position is {{ $value | humanizePercentage }} of portfolio"

      # Low win rate alert
      - alert: VelozLowWinRate
        expr: veloz_risk_win_rate < 0.30 and veloz_orders_total > 100
        for: 30m
        labels:
          severity: warning
        annotations:
          summary: "Low win rate detected"
          description: "Win rate is {{ $value | humanizePercentage }}"

      # Negative Sharpe ratio alert
      - alert: VelozNegativeSharpe
        expr: veloz_risk_sharpe_ratio < 0
        for: 1h
        labels:
          severity: warning
        annotations:
          summary: "Negative Sharpe ratio"
          description: "Sharpe ratio is {{ $value }}"

      # VaR breach alert
      - alert: VelozVaRBreach
        expr: veloz_position_total_unrealized_pnl < -veloz_risk_var_95
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "VaR 95% breach"
          description: "Unrealized loss exceeds 95% VaR threshold"
```

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
      - to: 'ops@yourdomain.com'

  - name: 'slack'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/xxx'
        channel: '#veloz-alerts'
        title: '{{ .GroupLabels.alertname }}'
        text: '{{ .CommonAnnotations.description }}'

  - name: 'pagerduty'
    pagerduty_configs:
      - service_key: 'your-pagerduty-key'
```

## Health Checks

### Endpoints

| Endpoint | Description | Response |
|----------|-------------|----------|
| `GET /health` | Basic health check | `{"ok": true}` |
| `GET /health/live` | Liveness probe | `200 OK` or `503` |
| `GET /health/ready` | Readiness probe | `200 OK` or `503` |

### Health Check Response

```json
{
  "ok": true,
  "components": {
    "engine": "healthy",
    "database": "healthy",
    "cache": "healthy",
    "websocket": "connected"
  },
  "uptime_seconds": 3600,
  "version": "0.2.0"
}
```

### Kubernetes Probes

```yaml
livenessProbe:
  httpGet:
    path: /health/live
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 5
  failureThreshold: 3

readinessProbe:
  httpGet:
    path: /health/ready
    port: 8080
  initialDelaySeconds: 5
  periodSeconds: 3
  failureThreshold: 3
```

## Tracing

### Request Tracing

VeloZ supports distributed tracing with correlation IDs:

```
X-Request-ID: abc-123-def-456
X-Correlation-ID: order-flow-789
```

### Jaeger Integration

```yaml
# docker-compose.yml
services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "6831:6831/udp"  # Thrift
    environment:
      - COLLECTOR_ZIPKIN_HOST_PORT=:9411
```

## Runbooks

### High Latency

1. Check event loop queue: `veloz_event_loop_tasks`
2. Check WebSocket reconnects: `veloz_websocket_reconnects`
3. Check memory usage: `veloz_memory_pool_used_bytes`
4. Review recent logs for errors
5. Consider scaling horizontally

### WebSocket Disconnection

1. Check network connectivity
2. Verify Binance API status
3. Check rate limiting status
4. Review reconnection logs
5. Manually trigger reconnection if needed

### Memory Pressure

1. Check memory pool usage
2. Review recent allocation patterns
3. Check for memory leaks with ASan
4. Consider increasing pool size
5. Restart if necessary

### High Drawdown

1. Check current positions: `veloz_position_size`
2. Review recent trades for losses
3. Check market conditions for adverse moves
4. Consider reducing position sizes
5. If critical (>20%), consider halting new orders
6. Review risk limits configuration

### High Leverage

1. Check gross exposure: `veloz_exposure_gross`
2. Review position sizes across symbols
3. Reduce positions to bring leverage within limits
4. Check if margin requirements are being met
5. Review risk rule engine thresholds

### High Concentration

1. Check concentration metrics: `veloz_concentration_largest_pct`
2. Identify the concentrated position
3. Consider diversifying by reducing largest position
4. Review position sizing rules
5. Check if concentration limits are configured correctly

### VaR Breach

1. Immediately review all open positions
2. Check market conditions for extreme moves
3. Consider reducing exposure
4. Review VaR calculation parameters
5. Notify risk management team
6. Document the breach for compliance

### Negative Sharpe Ratio

1. Review recent trade performance
2. Check strategy parameters
3. Analyze market regime changes
4. Consider pausing underperforming strategies
5. Review risk-adjusted return targets

## Related Documents

- [Production Architecture](production_architecture.md)
- [Troubleshooting](troubleshooting.md)
- [CI/CD Pipeline](ci_cd.md)
- [User Monitoring Guide](../user/monitoring.md) - User-facing monitoring guide
- [Risk Management Guide](../user/risk-management.md) - Risk metrics and alerts
- [Glossary](../user/glossary.md) - Technical term definitions
