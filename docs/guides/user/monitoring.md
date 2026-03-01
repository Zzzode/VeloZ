# Monitoring and Observability Guide

This guide provides comprehensive documentation for monitoring VeloZ system health, trading performance, and risk metrics using the integrated observability stack.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Health Monitoring](#health-monitoring)
4. [Prometheus Metrics Reference](#prometheus-metrics-reference)
5. [Grafana Dashboards](#grafana-dashboards)
6. [Loki Log Aggregation](#loki-log-aggregation)
7. [Jaeger Distributed Tracing](#jaeger-distributed-tracing)
8. [AlertManager Configuration](#alertmanager-configuration)
9. [Alert Rules Reference](#alert-rules-reference)
10. [Audit Logging](#audit-logging)
11. [SLO Monitoring](#slo-monitoring)
12. [Deployment Configuration](#deployment-configuration)
13. [Troubleshooting](#troubleshooting)
14. [Best Practices](#best-practices)

---

## Overview

VeloZ provides a comprehensive observability stack built on industry-standard tools:

| Component | Purpose | Endpoint/Tool |
|-----------|---------|---------------|
| **Prometheus** | Metrics collection and storage | `:9090` |
| **Grafana** | Visualization and dashboards | `:3000` |
| **Loki** | Log aggregation | `:3100` |
| **Promtail** | Log shipping | Agent |
| **Jaeger** | Distributed tracing | `:16686` |
| **AlertManager** | Alert routing and notifications | `:9093` |
| **Health Checks** | System availability | `/health`, `/health/live`, `/health/ready` |
| **Metrics** | Performance and trading data | `/metrics` (Prometheus format) |
| **Audit Logs** | Security and compliance | `/api/audit/logs` |
| **SSE Stream** | Real-time events | `/api/stream` |

### Architecture

```
+------------------+     +------------------+     +------------------+
|   VeloZ Engine   |     | Python Gateway   |     |    React UI      |
|   (C++ Core)     |     | (apps/gateway)   |     |   (apps/ui)      |
+--------+---------+     +--------+---------+     +--------+---------+
         |                        |                        |
         v                        v                        v
+------------------------------------------------------------------------+
|                        Instrumentation Layer                            |
|  +-------------+  +-------------+  +-------------+  +-----------------+ |
|  |   Metrics   |  |   Traces    |  |    Logs     |  |  Health Checks  | |
|  | (Prometheus)|  |   (OTLP)    |  |   (JSON)    |  | (/health, etc.) | |
|  +------+------+  +------+------+  +------+------+  +--------+--------+ |
+---------|-----------------|-----------------|-----------------|---------+
          |                 |                 |                 |
          v                 v                 v                 v
+------------------------------------------------------------------------+
|                    OpenTelemetry Collector (Optional)                   |
+------------------------------------------------------------------------+
          |                 |                 |                 |
          v                 v                 v                 v
+-------------+     +-------------+     +-------------+     +-------------+
| Prometheus  |     |   Jaeger    |     |    Loki     |     |AlertManager |
| (Metrics)   |     |  (Traces)   |     |   (Logs)    |     |  (Alerts)   |
+------+------+     +------+------+     +------+------+     +------+------+
       |                   |                   |                   |
       +-------------------+-------------------+-------------------+
                                    |
                                    v
                          +------------------+
                          |     Grafana      |
                          |   (Dashboards)   |
                          +------------------+
```

---

## Quick Start

### Check System Health

```bash
# Basic health check
curl http://127.0.0.1:8080/health
# Response: {"ok": true}

# Liveness probe (Kubernetes)
curl http://127.0.0.1:8080/health/live
# Response: {"ok": true}

# Readiness probe with component status
curl http://127.0.0.1:8080/health/ready
# Response: {"ok": true, "components": {...}, "uptime_seconds": 3600}
```

### View Metrics

```bash
# Get Prometheus metrics
curl http://127.0.0.1:8080/metrics

# Key metrics to watch:
# veloz_orders_total - Total orders processed
# veloz_order_latency_seconds - Order processing latency
# veloz_risk_utilization_percent - Current risk utilization
```

### Start Monitoring Stack

```bash
# Start all monitoring services
docker-compose -f docker-compose.yml up -d prometheus grafana loki alertmanager jaeger

# Access dashboards:
# - Prometheus: http://localhost:9090
# - Grafana: http://localhost:3000 (admin/admin)
# - Jaeger: http://localhost:16686
# - AlertManager: http://localhost:9093
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

### Kubernetes Health Probes

```yaml
# Kubernetes deployment configuration
spec:
  template:
    spec:
      containers:
        - name: veloz-gateway
          livenessProbe:
            httpGet:
              path: /health/live
              port: 8080
            initialDelaySeconds: 10
            periodSeconds: 5
            timeoutSeconds: 3
            failureThreshold: 3

          readinessProbe:
            httpGet:
              path: /health/ready
              port: 8080
            initialDelaySeconds: 5
            periodSeconds: 3
            timeoutSeconds: 3
            failureThreshold: 3
```

### Health Monitoring Script

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

## Prometheus Metrics Reference

VeloZ exposes Prometheus-compatible metrics at `/metrics`. The metrics are organized into categories for easy reference.

### HTTP Request Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_http_requests_total` | Counter | `method`, `endpoint`, `status` | Total HTTP requests processed |
| `veloz_http_request_duration_seconds` | Histogram | `method`, `endpoint` | HTTP request duration in seconds |
| `veloz_http_request_size_bytes` | Histogram | `method`, `endpoint` | HTTP request size in bytes |
| `veloz_http_response_size_bytes` | Histogram | `method`, `endpoint` | HTTP response size in bytes |

**Histogram buckets for duration:** 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0 seconds

**Histogram buckets for size:** 100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000 bytes

### Gateway State Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_gateway_uptime_seconds` | Gauge | Gateway uptime in seconds |
| `veloz_engine_running` | Gauge | Whether the engine process is running (1=running, 0=stopped) |
| `veloz_active_connections` | Gauge | Number of active HTTP connections |
| `veloz_sse_clients` | Gauge | Number of connected SSE clients |
| `veloz_websocket_connected` | Gauge | WebSocket connection status (1=connected, 0=disconnected) |

### Order Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_orders_total` | Counter | `side`, `type` | Total orders submitted |
| `veloz_fills_total` | Counter | `side` | Total order fills |
| `veloz_cancels_total` | Counter | - | Total order cancellations |
| `veloz_active_orders` | Gauge | - | Number of active orders |
| `veloz_order_latency_seconds` | Histogram | - | Order submission to acknowledgment latency |

**Order latency histogram buckets:** 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0 seconds

### Market Data Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_market_updates_total` | Counter | `type` | Total market data updates received |
| `veloz_orderbook_updates_total` | Counter | - | Total orderbook updates |
| `veloz_trade_updates_total` | Counter | - | Total trade updates |
| `veloz_market_data_lag_ms` | Gauge | - | Market data lag in milliseconds |
| `veloz_market_processing_latency_seconds` | Histogram | - | Market data processing latency |

**Market processing latency buckets:** 0.0001, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1 seconds

### Risk Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_risk_rejections_total` | Counter | `reason` | Total orders rejected by risk checks |
| `veloz_risk_utilization_percent` | Gauge | - | Risk limit utilization percentage |
| `veloz_position_value` | Gauge | `symbol` | Position value in USD |
| `veloz_risk_var_95` | Gauge | - | 95% Value at Risk (daily) |
| `veloz_risk_var_99` | Gauge | - | 99% Value at Risk (daily) |
| `veloz_risk_max_drawdown` | Gauge | - | Maximum drawdown percentage |
| `veloz_risk_current_drawdown` | Gauge | - | Current drawdown from peak |
| `veloz_risk_sharpe_ratio` | Gauge | - | Sharpe ratio |
| `veloz_risk_sortino_ratio` | Gauge | - | Sortino ratio |
| `veloz_risk_calmar_ratio` | Gauge | - | Calmar ratio |
| `veloz_risk_win_rate` | Gauge | - | Trade win rate percentage |
| `veloz_risk_profit_factor` | Gauge | - | Profit factor |

### Position Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_position_size` | Gauge | `symbol`, `side` | Position size by symbol |
| `veloz_position_unrealized_pnl` | Gauge | `symbol` | Unrealized PnL by symbol |
| `veloz_position_realized_pnl` | Counter | `symbol` | Cumulative realized PnL |
| `veloz_position_notional` | Gauge | `symbol` | Notional value by symbol |
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

### Exchange Connectivity Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_exchange_api_latency_seconds` | Histogram | `exchange`, `endpoint` | Exchange API call latency |
| `veloz_exchange_api_errors_total` | Counter | `exchange`, `error_type` | Exchange API errors |
| `veloz_websocket_reconnects_total` | Counter | `exchange` | WebSocket reconnection attempts |

**Exchange API latency buckets:** 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0 seconds

### Error Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `veloz_errors_total` | Counter | `type`, `component` | Total errors by type and component |

### System Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_websocket_connections` | Gauge | Active WebSocket connections |
| `veloz_websocket_reconnects` | Counter | WebSocket reconnection count |
| `veloz_memory_pool_used_bytes` | Gauge | Memory pool usage |
| `veloz_memory_pool_allocated_bytes` | Gauge | Memory pool allocated |
| `veloz_event_loop_pending_tasks` | Gauge | Pending event loop tasks |
| `veloz_orderbook_depth` | Gauge | Order book depth |

### Example Prometheus Queries

```promql
# Order rate per second
rate(veloz_orders_total[1m])

# Order latency P99
histogram_quantile(0.99, rate(veloz_order_latency_seconds_bucket[5m]))

# Fill rate percentage
sum(rate(veloz_fills_total[5m])) / sum(rate(veloz_orders_total[5m])) * 100

# Error rate per minute
rate(veloz_errors_total[5m]) * 60

# API success rate
sum(rate(veloz_http_requests_total{status=~"2.."}[5m])) / sum(rate(veloz_http_requests_total[5m])) * 100

# Market data lag
veloz_market_data_lag_ms

# Risk utilization
veloz_risk_utilization_percent

# Active orders
veloz_active_orders
```

---

## Grafana Dashboards

VeloZ provides four pre-built Grafana dashboards located in `docker/grafana/provisioning/dashboards/json/`.

### 1. Trading Dashboard (`veloz-trading.json`)

**UID:** `veloz-trading`

**Sections:**
- System Overview
- Order Flow
- Market Data
- Risk & Positions
- Gateway & API

**Key Panels:**

| Panel | Metric | Description |
|-------|--------|-------------|
| System Uptime | `veloz_system_uptime` | Total system uptime |
| Event Loop Pending Tasks | `veloz_event_loop_pending_tasks` | Queue depth indicator |
| Total Orders | `veloz_orders_total` | Cumulative order count |
| Total Fills | `veloz_fills_total` | Cumulative fill count |
| Order Rate | `rate(veloz_orders_total[1m])` | Orders/sec, Fills/sec, Cancels/sec |
| Order Latency | `histogram_quantile(0.xx, rate(veloz_order_latency_bucket[5m]))` | p50, p95, p99 latency |
| Market Data Rate | `rate(veloz_market_updates_total[1m])` | Updates/sec by type |
| Market Data Latency | `veloz_market_data_lag_ms` | Current lag in ms |
| Position Value | `veloz_position_value` | Position value by symbol |
| Risk Rejections | `veloz_risk_rejections_total` | Total risk rejections |
| Risk Utilization | `veloz_risk_utilization_percent` | Current risk % |
| HTTP Request Rate | `rate(veloz_http_requests_total[1m])` | Requests/sec by endpoint |
| HTTP Request Latency | `histogram_quantile(0.xx, rate(veloz_http_request_duration_bucket[5m]))` | p50, p95, p99 |

### 2. System Health Dashboard (`veloz-system.json`)

**UID:** `veloz-system`

**Sections:**
- Service Health
- Event Loop Performance
- Exchange Connectivity
- Memory & Resources

**Key Panels:**

| Panel | Metric | Description |
|-------|--------|-------------|
| Gateway Status | `up{job="veloz-gateway"}` | UP/DOWN indicator |
| Engine Status | `veloz_engine_running` | UP/DOWN indicator |
| WebSocket Status | `veloz_websocket_connected` | CONNECTED/DISCONNECTED |
| Active Connections | `veloz_active_connections` | Current HTTP connections |
| Error Rate | `rate(veloz_errors_total[5m]) * 60` | Errors per minute |
| SSE Clients | `veloz_sse_clients` | Connected SSE clients |
| Event Loop Queue Depth | `veloz_event_loop_pending_tasks` | Pending tasks over time |
| Event Loop Task Latency | `histogram_quantile(0.xx, rate(veloz_event_loop_task_latency_bucket[5m]))` | p50, p95, p99 |
| Exchange API Latency | `histogram_quantile(0.99, rate(veloz_exchange_api_latency_bucket[5m]))` | p99 by exchange |
| Exchange Errors & Reconnects | `rate(veloz_exchange_api_errors_total[5m])` | Errors/min, Reconnects/min |
| Memory Pool Usage | `veloz_memory_pool_used_bytes`, `veloz_memory_pool_allocated_bytes` | Memory utilization |
| Data Structures | `veloz_orderbook_depth`, `veloz_active_orders` | Orderbook depth, active orders |

### 3. Logs Dashboard (`veloz-logs.json`)

**UID:** `veloz-logs`

**Sections:**
- Log Overview
- Error Logs
- Trading Logs
- All Logs

**Key Panels:**

| Panel | Query | Description |
|-------|-------|-------------|
| Log Volume by Level | `sum by (level) (count_over_time({job="veloz-gateway"} \| json \| level != "" [$__interval]))` | Stacked bar chart by level |
| Total Errors | `count_over_time({job="veloz-gateway"} \| json \| level = "ERROR" [$__range])` | Error count in range |
| Total Warnings | `count_over_time({job="veloz-gateway"} \| json \| level = "WARNING" [$__range])` | Warning count in range |
| Logs by Component | `sum by (component) (count_over_time({job="veloz-gateway"} \| json \| component != "" [$__interval]))` | Log volume by component |
| Error Logs | `{job="veloz-gateway"} \| json \| level = "ERROR"` | Live error log stream |
| Order & Trade Logs | `{job="veloz-gateway"} \| json \| event_type =~ "order.*\|trade.*"` | Trading activity logs |
| All VeloZ Logs | `{job=~"veloz.*"}` | All logs from VeloZ services |

**Template Variables:**
- `level`: Filter by log level (ERROR, WARNING, INFO, DEBUG)
- `component`: Filter by component (gateway, engine, etc.)

### 4. SLO Dashboard (`veloz-slo.json`)

**UID:** `veloz-slo`

**Sections:**
- SLO Overview
- Error Budget
- Latency SLIs
- Throughput SLIs

**Key Panels:**

| Panel | SLO Target | Query |
|-------|------------|-------|
| Gateway Availability | 99.9% | `avg_over_time(up{job="veloz-gateway"}[$__range]) * 100` |
| Order Latency SLO | 99% < 100ms | `(sum(rate(veloz_order_latency_bucket{le="0.1"}[$__range])) / sum(rate(veloz_order_latency_count[$__range]))) * 100` |
| API Success Rate | 99.9% | `sum(rate(veloz_http_requests_total{status=~"2.."}[$__range])) / sum(rate(veloz_http_requests_total[$__range])) * 100` |
| Market Data Freshness | 99% < 50ms | Data freshness percentage |
| Availability Error Budget (30d) | - | Remaining error budget percentage |
| Error Budget Burn Rate (24h) | - | Rolling burn rate for availability and errors |
| Order Latency Distribution | p99 < 100ms | p50, p90, p99 latency over time |
| API Latency Distribution | p99 < 500ms | p50, p90, p99 latency over time |
| Order Throughput | - | Orders/sec, Fills/sec |
| System Throughput | - | Market Updates/sec, API Requests/sec |

### Dashboard Import

To import dashboards into Grafana:

1. **Automatic provisioning** (recommended):
   ```bash
   # Dashboards are automatically loaded from:
   docker/grafana/provisioning/dashboards/json/
   ```

2. **Manual import**:
   - Navigate to Grafana > Dashboards > Import
   - Upload the JSON file or paste its contents
   - Select the Prometheus data source

### Dashboard Configuration

```yaml
# docker/grafana/provisioning/dashboards/dashboards.yml
apiVersion: 1

providers:
  - name: 'VeloZ Dashboards'
    orgId: 1
    folder: 'VeloZ'
    type: file
    disableDeletion: false
    editable: true
    options:
      path: /var/lib/grafana/dashboards
```

---

## Loki Log Aggregation

VeloZ uses Loki for log aggregation with Promtail for log shipping.

### Loki Configuration

**Location:** `docker/loki/loki-config.yaml`

```yaml
# Key configuration settings
auth_enabled: false

server:
  http_listen_port: 3100
  grpc_listen_port: 9096

limits_config:
  retention_period: 168h  # 7 days
  max_streams_per_user: 10000
  max_entries_limit_per_query: 5000
  ingestion_rate_mb: 4
  ingestion_burst_size_mb: 6

compactor:
  retention_enabled: true
  retention_delete_delay: 2h
```

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
  "latency_ms": 0.5,
  "trace_id": "abc123def456...",
  "span_id": "789xyz..."
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
  - job_name: veloz-gateway
    static_configs:
      - targets:
          - localhost
        labels:
          job: veloz-gateway
          __path__: /var/log/veloz/*.log
    pipeline_stages:
      - json:
          expressions:
            level: level
            component: component
            message: message
            trace_id: trace_id
            span_id: span_id
      - labels:
          level:
          component:
      - timestamp:
          source: timestamp
          format: RFC3339Nano
```

### LogQL Query Examples

```logql
# All errors
{job="veloz-gateway"} |= "ERROR"

# Errors with JSON parsing
{job="veloz-gateway"} | json | level = "ERROR"

# Order processing logs
{job="veloz-gateway"} | json | message =~ "Order.*"

# High latency operations (> 100ms)
{job="veloz-gateway"} | json | latency_ms > 100

# Logs by component
{job="veloz-gateway"} | json | component = "engine"

# Trading activity
{job="veloz-gateway"} | json | event_type =~ "order.*|trade.*|fill.*"

# Logs with specific trace ID
{job="veloz-gateway"} | json | trace_id = "abc123def456"

# Error rate over time
sum(count_over_time({job="veloz-gateway"} | json | level = "ERROR" [5m]))

# Log volume by level
sum by (level) (count_over_time({job="veloz-gateway"} | json [$__interval]))
```

### Log Retention

| Log Type | Retention | Notes |
|----------|-----------|-------|
| Application logs | 7 days | Configurable in `limits_config.retention_period` |
| Audit logs | 90+ days | Separate audit log system |
| Error logs | 30 days | Consider longer retention for debugging |

---

## Jaeger Distributed Tracing

VeloZ integrates with Jaeger for distributed tracing using OpenTelemetry.

### Tracing Configuration

**Location:** `apps/gateway/tracing.py`

```python
from apps.gateway.tracing import init_tracing, TracingConfig

# Initialize tracing
config = TracingConfig(
    service_name="veloz-gateway",
    otlp_endpoint="http://jaeger:4317",
    enabled=True,
    sample_rate=1.0,
)
init_tracing(config)
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `OTEL_ENABLED` | Enable/disable tracing | `true` |
| `OTEL_EXPORTER_OTLP_ENDPOINT` | OTLP collector endpoint | None |
| `OTEL_CONSOLE_EXPORT` | Enable console span export | `false` |
| `VELOZ_ENV` | Deployment environment | `development` |

### Jaeger Deployment

```yaml
# docker-compose.yml
services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "6831:6831/udp"  # Thrift compact
      - "14268:14268"  # HTTP collector
      - "4317:4317"    # OTLP gRPC
      - "4318:4318"    # OTLP HTTP
    environment:
      - COLLECTOR_OTLP_ENABLED=true
```

### Trace Context Propagation

VeloZ uses W3C Trace Context for propagation:

```
traceparent: 00-{trace-id}-{span-id}-{flags}
tracestate: veloz=...
```

Include trace context in requests:

```bash
curl -H "X-Request-ID: my-request-123" \
     -H "X-Correlation-ID: order-flow-456" \
     http://127.0.0.1:8080/api/order
```

### Key Trace Points

| Component | Span Name | Attributes |
|-----------|-----------|------------|
| Gateway | `http.request` | `http.method`, `http.url`, `http.status_code` |
| Gateway | `engine.command` | `command.type`, `command.symbol` |
| Engine | `order.place` | `order.symbol`, `order.side`, `order.type` |
| Engine | `order.execute` | `order.id`, `venue.name` |
| Engine | `market.process` | `event.type`, `symbol` |
| Engine | `strategy.signal` | `strategy.name`, `signal.type` |
| Engine | `risk.check` | `check.name`, `check.result` |

### Using the Tracing API

```python
from apps.gateway.tracing import get_tracing_manager, traced, add_span_attribute

# Using context manager
manager = get_tracing_manager()
with manager.span("process_order", attributes={"symbol": "BTCUSDT"}):
    # Processing logic
    add_span_attribute("order_id", "12345")

# Using decorator
@traced(name="calculate_risk", attributes={"component": "risk"})
def calculate_risk(position):
    # Risk calculation
    pass
```

### Jaeger UI

Access the Jaeger UI at `http://localhost:16686`:

1. **Search Traces**: Filter by service, operation, tags, duration
2. **Trace Timeline**: View span hierarchy and timing
3. **Span Details**: Inspect attributes, logs, and errors
4. **Compare Traces**: Compare multiple traces side-by-side
5. **Dependencies**: View service dependency graph

---

## AlertManager Configuration

AlertManager handles alert routing, grouping, and notification delivery.

### Configuration File

**Location:** `docker/alertmanager/alertmanager.yml`

### Global Settings

```yaml
global:
  resolve_timeout: 5m
  smtp_smarthost: 'smtp.example.com:587'
  smtp_from: 'alertmanager@veloz.io'
  smtp_auth_username: '${SMTP_USERNAME}'
  smtp_auth_password: '${SMTP_PASSWORD}'
  slack_api_url: '${SLACK_WEBHOOK_URL}'
  pagerduty_url: 'https://events.pagerduty.com/v2/enqueue'
  opsgenie_api_url: 'https://api.opsgenie.com/'
```

### Route Configuration

```yaml
route:
  receiver: 'default-receiver'
  group_by: ['alertname', 'service', 'severity']
  group_wait: 30s
  group_interval: 5m
  repeat_interval: 4h

  routes:
    # Critical alerts -> PagerDuty (immediate)
    - match:
        severity: critical
      receiver: 'pagerduty-critical'
      group_wait: 10s
      group_interval: 1m
      repeat_interval: 1h

    # Warning alerts -> Opsgenie
    - match:
        severity: warning
      receiver: 'opsgenie-warning'
      group_wait: 1m
      group_interval: 5m
      repeat_interval: 4h

    # Trading-specific alerts
    - match:
        service: execution
      receiver: 'trading-alerts'
      group_wait: 10s
      group_interval: 1m

    # Risk alerts
    - match:
        service: risk
      receiver: 'risk-alerts'
      group_wait: 10s
      group_interval: 1m
```

### Receivers

#### Default Receiver (Email + Slack)

```yaml
- name: 'default-receiver'
  email_configs:
    - to: 'alerts@veloz.io'
      send_resolved: true
  slack_configs:
    - channel: '#veloz-alerts'
      send_resolved: true
      title: '{{ template "slack.veloz.title" . }}'
      text: '{{ template "slack.veloz.text" . }}'
```

#### PagerDuty (Critical)

```yaml
- name: 'pagerduty-critical'
  pagerduty_configs:
    - service_key: '${PAGERDUTY_SERVICE_KEY}'
      severity: '{{ if eq .Status "firing" }}critical{{ else }}info{{ end }}'
      description: '{{ template "pagerduty.veloz.description" . }}'
      details:
        firing: '{{ template "pagerduty.veloz.firing" . }}'
        num_firing: '{{ .Alerts.Firing | len }}'
        num_resolved: '{{ .Alerts.Resolved | len }}'
```

#### Opsgenie (Warning)

```yaml
- name: 'opsgenie-warning'
  opsgenie_configs:
    - api_key: '${OPSGENIE_API_KEY}'
      message: '{{ template "opsgenie.veloz.message" . }}'
      description: '{{ template "opsgenie.veloz.description" . }}'
      priority: '{{ if eq .Status "firing" }}P3{{ else }}P5{{ end }}'
      tags: 'veloz,{{ .CommonLabels.service }},{{ .CommonLabels.severity }}'
```

#### Trading Alerts (PagerDuty + Slack)

```yaml
- name: 'trading-alerts'
  pagerduty_configs:
    - service_key: '${PAGERDUTY_TRADING_KEY}'
      severity: '{{ if eq .Status "firing" }}error{{ else }}info{{ end }}'
      description: 'Trading Alert: {{ .CommonAnnotations.summary }}'
  slack_configs:
    - channel: '#veloz-trading-alerts'
      send_resolved: true
      color: '{{ if eq .Status "firing" }}danger{{ else }}good{{ end }}'
```

#### Risk Alerts (PagerDuty + Opsgenie)

```yaml
- name: 'risk-alerts'
  pagerduty_configs:
    - service_key: '${PAGERDUTY_RISK_KEY}'
      severity: '{{ if eq .Status "firing" }}error{{ else }}info{{ end }}'
      description: 'Risk Alert: {{ .CommonAnnotations.summary }}'
  opsgenie_configs:
    - api_key: '${OPSGENIE_API_KEY}'
      message: 'Risk Alert: {{ .CommonAnnotations.summary }}'
      priority: 'P2'
      tags: 'veloz,risk,{{ .CommonLabels.severity }}'
```

### Inhibition Rules

```yaml
inhibit_rules:
  # Suppress warnings when critical is firing
  - source_match:
      severity: 'critical'
    target_match:
      severity: 'warning'
    equal: ['alertname', 'service']

  # Suppress gateway alerts when gateway is down
  - source_match:
      alertname: 'VeloZGatewayDown'
    target_match:
      service: 'gateway'
    equal: ['instance']

  # Suppress trading alerts when engine is down
  - source_match:
      alertname: 'VeloZEngineDown'
    target_match_re:
      service: 'execution|oms|risk'
```

### Notification Templates

**Location:** `docker/alertmanager/templates/veloz.tmpl`

```go
{{ define "slack.veloz.title" }}
[{{ .Status | toUpper }}{{ if eq .Status "firing" }}:{{ .Alerts.Firing | len }}{{ end }}] {{ .CommonLabels.alertname }}
{{ end }}

{{ define "slack.veloz.text" }}
{{ range .Alerts }}
*Alert:* {{ .Annotations.summary }}
*Severity:* {{ .Labels.severity }}
*Service:* {{ .Labels.service }}
*Description:* {{ .Annotations.description }}
{{ if .Labels.instance }}*Instance:* {{ .Labels.instance }}{{ end }}
{{ end }}
{{ end }}
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `SMTP_USERNAME` | SMTP authentication username |
| `SMTP_PASSWORD` | SMTP authentication password |
| `SLACK_WEBHOOK_URL` | Slack incoming webhook URL |
| `PAGERDUTY_SERVICE_KEY` | PagerDuty service integration key |
| `PAGERDUTY_TRADING_KEY` | PagerDuty key for trading alerts |
| `PAGERDUTY_RISK_KEY` | PagerDuty key for risk alerts |
| `OPSGENIE_API_KEY` | Opsgenie API key |

---

## Alert Rules Reference

VeloZ includes comprehensive alert rules in `docker/prometheus/alerts/`.

### Service Health Alerts

**File:** `docker/prometheus/alerts/veloz-alerts.yml`

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `VeloZGatewayDown` | `up{job="veloz-gateway"} == 0` | 1m | Critical | Gateway is unreachable |
| `VeloZEngineDown` | `veloz_engine_running == 0` | 30s | Critical | Engine process not running |
| `VeloZWebSocketDisconnected` | `veloz_websocket_connected == 0` | 1m | Warning | Market data WebSocket disconnected |

### Latency Alerts

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `HighOrderLatency` | `histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m])) > 0.1` | 5m | Warning | Order latency p99 > 100ms |
| `CriticalOrderLatency` | `histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m])) > 0.5` | 2m | Critical | Order latency p99 > 500ms |
| `HighMarketDataLag` | `veloz_market_data_lag_ms > 100` | 2m | Warning | Market data lag > 100ms |
| `CriticalMarketDataLag` | `veloz_market_data_lag_ms > 500` | 1m | Critical | Market data lag > 500ms |
| `HighAPILatency` | `histogram_quantile(0.99, rate(veloz_http_request_duration_bucket[5m])) > 1` | 5m | Warning | API latency p99 > 1s |

### Error Rate Alerts

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `HighErrorRate` | `rate(veloz_errors_total[5m]) > 0.1` | 5m | Warning | Error rate > 0.1/s |
| `CriticalErrorRate` | `rate(veloz_errors_total[5m]) > 1` | 2m | Critical | Error rate > 1/s |
| `ExchangeAPIErrors` | `rate(veloz_exchange_api_errors_total[5m]) > 0.1` | 5m | Warning | Exchange API errors elevated |
| `HighWebSocketReconnects` | `rate(veloz_websocket_reconnects_total[5m]) > 0.1` | 5m | Warning | Frequent WebSocket reconnections |

### Risk Alerts

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `HighRiskUtilization` | `veloz_risk_utilization_percent > 80` | 5m | Warning | Risk utilization > 80% |
| `CriticalRiskUtilization` | `veloz_risk_utilization_percent > 95` | 1m | Critical | Risk utilization > 95% |
| `RiskRejectionsSpike` | `rate(veloz_risk_rejections_total[5m]) > 0.5` | 5m | Warning | Elevated risk rejections |
| `VelozHighDrawdown` | `veloz_risk_current_drawdown > 10` | 5m | Warning | Drawdown > 10% |
| `VelozCriticalDrawdown` | `veloz_risk_current_drawdown > 20` | 1m | Critical | Drawdown > 20% |
| `VelozHighLeverage` | `veloz_exposure_leverage_ratio > 3` | 5m | Warning | Leverage > 3x |
| `VelozHighConcentration` | `veloz_concentration_largest_pct > 50` | 5m | Warning | Largest position > 50% |
| `VelozVaRBreach` | Loss exceeds VaR 95% | 5m | Critical | VaR limit breached |
| `VelozNegativeSharpe` | `veloz_risk_sharpe_ratio < 0` | 1h | Warning | Negative Sharpe ratio |
| `VelozLowWinRate` | `veloz_risk_win_rate < 30` | 30m | Warning | Win rate < 30% |

### Throughput Alerts

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `LowMarketDataRate` | `rate(veloz_market_updates_total[5m]) < 1` | 5m | Warning | Unusually low market data rate |
| `EventLoopBacklog` | `veloz_event_loop_pending_tasks > 100` | 2m | Warning | Event loop backlog > 100 tasks |
| `CriticalEventLoopBacklog` | `veloz_event_loop_pending_tasks > 500` | 1m | Critical | Event loop backlog > 500 tasks |

### Resource Alerts

| Alert | Expression | Duration | Severity | Description |
|-------|------------|----------|----------|-------------|
| `HighActiveOrders` | `veloz_active_orders > 1000` | 5m | Warning | > 1000 active orders |
| `HighSSEClients` | `veloz_sse_clients > 100` | 5m | Warning | > 100 SSE clients |
| `MemoryUsageHigh` | `veloz_memory_pool_used_bytes / veloz_memory_limit_bytes > 0.8` | 10m | Warning | Memory > 80% |

### SLO Recording Rules

**File:** `docker/prometheus/alerts/veloz-slo-recording.yml`

Pre-computed metrics for efficient SLO dashboard queries:

```yaml
# Availability SLIs
veloz:gateway:availability:ratio_rate5m
veloz:gateway:availability:ratio_rate1h
veloz:gateway:availability:ratio_rate1d
veloz:engine:availability:ratio_rate5m
veloz:websocket:availability:ratio_rate5m

# Latency SLIs
veloz:order_latency:p50_5m
veloz:order_latency:p90_5m
veloz:order_latency:p99_5m
veloz:api_latency:p50_5m
veloz:api_latency:p90_5m
veloz:api_latency:p99_5m

# Error Rate SLIs
veloz:api:error_rate:ratio_rate5m
veloz:api:success_rate:ratio_rate5m
veloz:exchange:error_rate:ratio_rate5m

# Throughput SLIs
veloz:orders:rate_5m
veloz:fills:rate_5m
veloz:market_updates:rate_5m
veloz:api_requests:rate_5m

# Error Budget
veloz:error_budget:availability:remaining_ratio
veloz:error_budget:latency:remaining_ratio
veloz:error_budget:errors:remaining_ratio
veloz:error_budget:burn_rate:1h
veloz:error_budget:burn_rate:6h

# SLO Compliance
veloz:slo:availability:compliance
veloz:slo:order_latency:compliance
veloz:slo:api_latency:compliance
veloz:slo:error_rate:compliance
veloz:slo:overall_compliance_score
```

---

## Audit Logging

VeloZ provides comprehensive audit logging for security and compliance.

### Audit Log API

```bash
# Query recent audit logs
curl "http://127.0.0.1:8080/api/audit/logs?log_type=auth&limit=100"

# Query with time range
curl "http://127.0.0.1:8080/api/audit/logs?start_time=1708689600&end_time=1708776000"

# Get audit statistics
curl "http://127.0.0.1:8080/api/audit/stats"

# Trigger manual archiving
curl -X POST "http://127.0.0.1:8080/api/audit/archive"
```

### Query Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `log_type` | string | Filter by log type (auth, order, api_key, error, access) |
| `limit` | int | Maximum number of entries (default: 100, max: 1000) |
| `offset` | int | Pagination offset |
| `start_time` | float | Start timestamp (Unix epoch) |
| `end_time` | float | End timestamp (Unix epoch) |
| `user_id` | string | Filter by user ID |
| `action` | string | Filter by action |

### Logged Events

| Log Type | Events | Retention |
|----------|--------|-----------|
| `auth` | Login, logout, token refresh, failed auth | 90 days |
| `order` | Order placement, cancellation, fills | 365 days |
| `api_key` | Key creation, revocation, rotation | 365 days |
| `error` | System errors, exceptions | 30 days |
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

### Audit Log Retention Policy

```yaml
# Recommended retention settings
auth_logs: 90 days
order_logs: 365 days
api_key_logs: 365 days
error_logs: 30 days
access_logs: 14 days
```

---

## SLO Monitoring

### Service Level Objectives

| SLO | Target | Measurement | Alert Threshold |
|-----|--------|-------------|-----------------|
| **Availability** | 99.9% | `up{job="veloz-gateway"}` | < 99.5% over 1h |
| **Order Latency P99** | < 100ms | `veloz_order_latency_seconds` | > 100ms for 5m |
| **API Latency P99** | < 500ms | `veloz_http_request_duration_seconds` | > 500ms for 5m |
| **Order Success Rate** | > 99% | `1 - (rejected/placed)` | < 98% for 5m |
| **Error Rate** | < 0.1% | `veloz_errors_total / requests` | > 1% for 5m |
| **Market Data Freshness** | 99% < 50ms | `veloz_market_data_lag_ms` | > 50ms for 5m |

### Error Budget Calculation

```promql
# 30-day availability error budget (99.9% SLO = 0.1% budget)
# Total allowed downtime: 30 days * 24 hours * 60 min * 0.001 = 43.2 minutes

# Remaining error budget percentage
100 - ((1 - avg_over_time(up{job="veloz-gateway"}[30d])) / 0.001 * 100)

# Error budget burn rate (1h vs 30d baseline)
(1 - avg_over_time(up{job="veloz-gateway"}[1h])) /
(1 - avg_over_time(up{job="veloz-gateway"}[30d]) + 0.0001)
```

### SLO Compliance Queries

```promql
# Availability SLO compliance (1 = meeting SLO)
(avg_over_time(up{job="veloz-gateway"}[30d]) >= 0.999) * 1

# Order latency SLO compliance
(histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m])) <= 0.1) * 1

# API latency SLO compliance
(histogram_quantile(0.99, rate(veloz_http_request_duration_bucket[5m])) <= 0.5) * 1

# Error rate SLO compliance
(sum(rate(veloz_http_requests_total{status=~"5.."}[5m])) /
 sum(rate(veloz_http_requests_total[5m])) <= 0.001) * 1

# Overall SLO compliance score (0-4)
veloz:slo:availability:compliance +
veloz:slo:order_latency:compliance +
veloz:slo:api_latency:compliance +
veloz:slo:error_rate:compliance
```

---

## Deployment Configuration

### Docker Compose

```yaml
# docker-compose.yml (monitoring services)
version: '3.8'

services:
  prometheus:
    image: prom/prometheus:v2.48.0
    ports:
      - "9090:9090"
    volumes:
      - ./docker/prometheus/prometheus.yml:/etc/prometheus/prometheus.yml
      - ./docker/prometheus/alerts:/etc/prometheus/alerts
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=15d'
      - '--web.enable-lifecycle'

  grafana:
    image: grafana/grafana:10.2.2
    ports:
      - "3000:3000"
    volumes:
      - ./docker/grafana/provisioning:/etc/grafana/provisioning
      - grafana-data:/var/lib/grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
      - GF_USERS_ALLOW_SIGN_UP=false

  loki:
    image: grafana/loki:2.9.2
    ports:
      - "3100:3100"
    volumes:
      - ./docker/loki/loki-config.yaml:/etc/loki/local-config.yaml
      - loki-data:/loki
    command: -config.file=/etc/loki/local-config.yaml

  promtail:
    image: grafana/promtail:2.9.2
    volumes:
      - ./docker/promtail/promtail-config.yaml:/etc/promtail/config.yml
      - /var/log/veloz:/var/log/veloz:ro
    command: -config.file=/etc/promtail/config.yml

  alertmanager:
    image: prom/alertmanager:v0.26.0
    ports:
      - "9093:9093"
    volumes:
      - ./docker/alertmanager/alertmanager.yml:/etc/alertmanager/alertmanager.yml
      - ./docker/alertmanager/templates:/etc/alertmanager/templates

  jaeger:
    image: jaegertracing/all-in-one:1.52
    ports:
      - "16686:16686"
      - "4317:4317"
      - "4318:4318"
    environment:
      - COLLECTOR_OTLP_ENABLED=true

volumes:
  prometheus-data:
  grafana-data:
  loki-data:
```

### Prometheus Configuration

```yaml
# docker/prometheus/prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - /etc/prometheus/alerts/*.yml

scrape_configs:
  - job_name: 'veloz-gateway'
    static_configs:
      - targets: ['veloz-gateway:8080']
    metrics_path: /metrics
    scrape_interval: 5s

  - job_name: 'prometheus'
    static_configs:
      - targets: ['localhost:9090']
```

### Kubernetes Deployment

```yaml
# Prometheus annotations for pod scraping
spec:
  template:
    metadata:
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "8080"
        prometheus.io/path: "/metrics"
```

---

## Troubleshooting

### Metrics Not Appearing

```bash
# Verify metrics endpoint
curl http://127.0.0.1:8080/metrics | head -20

# Check Prometheus targets
curl http://localhost:9090/api/v1/targets

# Verify scrape config
docker exec prometheus cat /etc/prometheus/prometheus.yml | grep veloz

# Check Prometheus logs
docker logs prometheus 2>&1 | tail -50
```

### Alerts Not Firing

```bash
# Check Alertmanager status
curl http://localhost:9093/api/v1/status

# View active alerts
curl http://localhost:9093/api/v1/alerts

# Check alert rules
curl http://localhost:9090/api/v1/rules

# Verify Alertmanager config
docker exec alertmanager cat /etc/alertmanager/alertmanager.yml
```

### Logs Not Aggregating

```bash
# Check Promtail status
curl http://localhost:9080/ready

# Verify log file path
ls -la /var/log/veloz/

# Check Loki ingestion
curl http://localhost:3100/ready

# Query Loki directly
curl -G -s "http://localhost:3100/loki/api/v1/query" \
  --data-urlencode 'query={job="veloz-gateway"}'
```

### Tracing Not Working

```bash
# Check Jaeger status
curl http://localhost:16686/api/services

# Verify OTLP endpoint
curl http://localhost:4318/v1/traces -X POST -H "Content-Type: application/json" -d '{}'

# Check environment variables
echo $OTEL_ENABLED
echo $OTEL_EXPORTER_OTLP_ENDPOINT
```

### Market Data Latency

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

### Common Issues

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| No metrics in Prometheus | Target not reachable | Check network, verify endpoint |
| Alerts not routing | Incorrect route config | Verify route matchers |
| Logs missing | Promtail not running | Check Promtail logs and config |
| High cardinality | Too many label values | Reduce label cardinality |
| Slow queries | Large time range | Use recording rules |
| Missing traces | OTLP not configured | Set `OTEL_EXPORTER_OTLP_ENDPOINT` |

---

## Best Practices

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
| Risk utilization | > 80% | > 95% |

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

### 6. Use Recording Rules

Pre-compute expensive queries:

```yaml
groups:
  - name: veloz-recording
    interval: 30s
    rules:
      - record: veloz:order_latency:p99_5m
        expr: histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m]))
```

### 7. Control Label Cardinality

- Good: `symbol` (finite set of traded symbols)
- Bad: `order_id` (unbounded, creates cardinality explosion)

### 8. Implement Alerting Hierarchy

```
Critical (P1) -> PagerDuty -> Immediate response
Warning (P2)  -> Opsgenie  -> 15 min response
Info (P3)     -> Slack     -> 1 hour response
```

### 9. Test Alert Routing

Regularly test that alerts reach the correct channels:

```bash
# Send test alert
curl -X POST http://localhost:9093/api/v1/alerts \
  -H "Content-Type: application/json" \
  -d '[{
    "labels": {"alertname": "TestAlert", "severity": "warning"},
    "annotations": {"summary": "Test alert", "description": "Testing alert routing"}
  }]'
```

### 10. Document Runbooks

Create runbooks for each alert:
- What the alert means
- How to investigate
- How to resolve
- Escalation path

---

## Related Documentation

- [Configuration Guide](configuration.md) - Monitoring environment variables
- [Troubleshooting Guide](troubleshooting.md) - Common issues and solutions
- [Deployment Guide](../deployment/README.md) - Production deployment
- [Security Guide](../../security/README.md) - Security best practices
- [Design: Observability Architecture](../../design/design_13_observability.md) - Technical design
