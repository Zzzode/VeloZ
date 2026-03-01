# Design 13: Observability Architecture

**Status**: Draft
**Author**: System Architect
**Date**: 2026-02-23
**Related Tasks**: #1, #6, #8, #20

---

## 1. Overview

This document defines the observability architecture for VeloZ production deployment. It covers metrics collection, distributed tracing, log aggregation, alerting, and performance dashboards.

### 1.1 Goals

1. **Visibility**: Complete insight into system behavior across all components
2. **Debuggability**: Ability to trace requests end-to-end and diagnose issues
3. **Alerting**: Proactive notification of anomalies before they impact trading
4. **Performance**: Validate latency targets (P50, P95, P99) and throughput limits
5. **SLO Compliance**: Track and report on service level objectives

### 1.2 Design Principles

1. **OpenTelemetry First**: Use OTEL as the unified observability standard
2. **Cloud Agnostic**: No vendor lock-in; works on AWS/GCP/Azure/on-prem
3. **Low Overhead**: Observability must not impact trading latency (< 1% overhead)
4. **Correlation**: All signals (metrics, traces, logs) must be correlatable
5. **Leverage Existing**: Build on existing MetricsRegistry and Logger infrastructure

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              VeloZ Components                                │
├─────────────────┬─────────────────┬─────────────────┬───────────────────────┤
│   C++ Engine    │  Python Gateway │    React UI     │   Background Jobs     │
│  (libs/core)    │ (apps/gateway)  │   (apps/ui)     │                       │
├─────────────────┴─────────────────┴─────────────────┴───────────────────────┤
│                         Instrumentation Layer                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │   Metrics   │  │   Traces    │  │    Logs     │  │   Health Checks     │ │
│  │ (Prometheus)│  │   (OTLP)    │  │   (JSON)    │  │   (/health, /ready) │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
└─────────┼────────────────┼────────────────┼────────────────────┼────────────┘
          │                │                │                    │
          ▼                ▼                ▼                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       OpenTelemetry Collector                                │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  Receivers: prometheus, otlp, filelog                               │    │
│  │  Processors: batch, memory_limiter, attributes, filter              │    │
│  │  Exporters: prometheus, jaeger, loki                                │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────┬────────────────┬────────────────┬────────────────────┬────────────┘
          │                │                │                    │
          ▼                ▼                ▼                    ▼
┌─────────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐
│   Prometheus    │ │   Jaeger    │ │    Loki     │ │      Alertmanager       │
│  (Metrics DB)   │ │  (Traces)   │ │   (Logs)    │ │  (Alert Routing)        │
└────────┬────────┘ └──────┬──────┘ └──────┬──────┘ └───────────┬─────────────┘
         │                 │               │                    │
         └─────────────────┴───────────────┴────────────────────┘
                                    │
                                    ▼
                          ┌─────────────────┐
                          │     Grafana     │
                          │  (Dashboards)   │
                          └─────────────────┘
```

---

## 3. Metrics Architecture

### 3.1 Existing Infrastructure

VeloZ already has a comprehensive metrics system in `libs/core/include/veloz/core/metrics.h`:

```cpp
// Existing metric types
class Counter;    // Monotonically increasing (e.g., orders_placed_total)
class Gauge;      // Can increase/decrease (e.g., active_connections)
class Histogram;  // Distribution statistics (e.g., order_latency_seconds)

// Global registry with Prometheus export
MetricsRegistry& global_metrics();
kj::String MetricsRegistry::to_prometheus() const;
```

### 3.2 Metrics Endpoint

**Engine HTTP Service** (`apps/engine/src/http_service.cpp`):

Add a `/metrics` endpoint that exposes Prometheus-format metrics:

```cpp
// Endpoint: GET /metrics
// Response: text/plain; version=0.0.4
kj::String handle_metrics() {
    return global_metrics().to_prometheus();
}
```

**Gateway** (`apps/gateway/gateway.py`):

Add a `/metrics` endpoint that combines gateway metrics with engine metrics:

```python
@app.get("/metrics")
async def metrics():
    # Combine gateway metrics + engine metrics
    gateway_metrics = generate_gateway_metrics()
    engine_metrics = await fetch_engine_metrics()
    return Response(
        content=gateway_metrics + engine_metrics,
        media_type="text/plain; version=0.0.4"
    )
```

### 3.3 Standard Metrics

#### 3.3.1 Engine Metrics (C++)

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `veloz_orders_placed_total` | Counter | `symbol`, `side`, `type` | Total orders placed |
| `veloz_orders_filled_total` | Counter | `symbol`, `side` | Total orders filled |
| `veloz_orders_rejected_total` | Counter | `symbol`, `reason` | Total orders rejected |
| `veloz_order_latency_seconds` | Histogram | `operation` | Order processing latency |
| `veloz_market_events_total` | Counter | `symbol`, `type` | Market events received |
| `veloz_market_latency_seconds` | Histogram | `symbol` | Market data latency |
| `veloz_orderbook_depth` | Gauge | `symbol`, `side` | Order book depth |
| `veloz_position_value` | Gauge | `symbol` | Current position value |
| `veloz_pnl_unrealized` | Gauge | `symbol` | Unrealized P&L |
| `veloz_strategy_signals_total` | Counter | `strategy`, `signal` | Strategy signals generated |
| `veloz_risk_checks_total` | Counter | `check`, `result` | Risk check results |
| `veloz_circuit_breaker_state` | Gauge | `breaker` | Circuit breaker state (0=closed, 1=open) |
| `veloz_memory_arena_bytes` | Gauge | `arena` | Memory arena usage |
| `veloz_queue_depth` | Gauge | `queue` | Lock-free queue depth |

#### 3.3.2 Gateway Metrics (Python)

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `veloz_http_requests_total` | Counter | `method`, `endpoint`, `status` | HTTP requests |
| `veloz_http_request_duration_seconds` | Histogram | `method`, `endpoint` | Request duration |
| `veloz_sse_connections_active` | Gauge | | Active SSE connections |
| `veloz_auth_attempts_total` | Counter | `result` | Authentication attempts |
| `veloz_rate_limit_hits_total` | Counter | `endpoint` | Rate limit hits |
| `veloz_engine_connection_state` | Gauge | | Engine connection (0=down, 1=up) |

#### 3.3.3 Infrastructure Metrics

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `veloz_process_cpu_seconds_total` | Counter | | CPU time consumed |
| `veloz_process_resident_memory_bytes` | Gauge | | Resident memory size |
| `veloz_process_open_fds` | Gauge | | Open file descriptors |
| `veloz_go_gc_duration_seconds` | Summary | | GC pause duration (if applicable) |

### 3.4 Metric Labels Best Practices

1. **Cardinality Control**: Limit label values to bounded sets
   - Good: `symbol` (finite set of traded symbols)
   - Bad: `order_id` (unbounded, creates cardinality explosion)

2. **Consistent Naming**: Use snake_case with `veloz_` prefix

3. **Unit Suffix**: Include unit in metric name
   - `_seconds` for durations
   - `_bytes` for sizes
   - `_total` for counters

---

## 4. Distributed Tracing

### 4.1 Trace Context Propagation

All components must propagate trace context using W3C Trace Context headers:

```
traceparent: 00-{trace-id}-{span-id}-{flags}
tracestate: veloz=...
```

### 4.2 C++ Tracing Integration

Create a lightweight tracing wrapper in `libs/core/include/veloz/core/tracing.h`:

```cpp
#pragma once

#include <kj/string.h>
#include <kj/time.h>

namespace veloz::core {

// Trace context for propagation
struct TraceContext {
    kj::String trace_id;      // 32 hex chars
    kj::String span_id;       // 16 hex chars
    kj::String parent_span_id;
    bool sampled;

    // Parse from W3C traceparent header
    static kj::Maybe<TraceContext> from_traceparent(kj::StringPtr header);

    // Generate traceparent header
    kj::String to_traceparent() const;
};

// Span for tracing operations
class Span {
public:
    Span(kj::StringPtr name, const TraceContext& parent);
    ~Span();  // Automatically ends span and exports

    void set_attribute(kj::StringPtr key, kj::StringPtr value);
    void set_attribute(kj::StringPtr key, int64_t value);
    void set_status(bool ok, kj::StringPtr message = nullptr);
    void add_event(kj::StringPtr name);

    TraceContext context() const;

private:
    struct Impl;
    kj::Own<Impl> impl_;
};

// RAII span helper
#define TRACE_SPAN(name) \
    veloz::core::Span _trace_span_##__LINE__(name, current_trace_context())

// Global trace exporter configuration
void configure_tracing(kj::StringPtr otlp_endpoint);

} // namespace veloz::core
```

### 4.3 Python Tracing Integration

Use OpenTelemetry Python SDK in the gateway:

```python
from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.instrumentation.fastapi import FastAPIInstrumentor

# Initialize tracing
trace.set_tracer_provider(TracerProvider())
tracer = trace.get_tracer(__name__)

# Configure OTLP exporter
otlp_exporter = OTLPSpanExporter(endpoint="otel-collector:4317")
trace.get_tracer_provider().add_span_processor(BatchSpanProcessor(otlp_exporter))

# Auto-instrument FastAPI
FastAPIInstrumentor.instrument_app(app)
```

### 4.4 Key Trace Points

| Component | Span Name | Attributes |
|-----------|-----------|------------|
| Gateway | `http.request` | `http.method`, `http.url`, `http.status_code` |
| Gateway | `engine.command` | `command.type`, `command.symbol` |
| Engine | `order.place` | `order.symbol`, `order.side`, `order.type` |
| Engine | `order.execute` | `order.id`, `venue.name` |
| Engine | `market.process` | `event.type`, `symbol` |
| Engine | `strategy.signal` | `strategy.name`, `signal.type` |
| Engine | `risk.check` | `check.name`, `check.result` |

### 4.5 Sampling Strategy

For production, use adaptive sampling to balance visibility and overhead:

```yaml
# otel-collector-config.yaml
processors:
  probabilistic_sampler:
    sampling_percentage: 10  # Sample 10% of traces

  tail_sampling:
    decision_wait: 10s
    policies:
      - name: errors
        type: status_code
        status_code: {status_codes: [ERROR]}
      - name: slow-requests
        type: latency
        latency: {threshold_ms: 100}
      - name: random
        type: probabilistic
        probabilistic: {sampling_percentage: 5}
```

---

## 5. Log Aggregation

### 5.1 Existing Infrastructure

VeloZ has a comprehensive logging system in `libs/core/include/veloz/core/logger.h`:

```cpp
// Existing formatters
class TextFormatter;  // Human-readable
class JsonFormatter;  // Structured JSON (for aggregation)

// Existing outputs
class ConsoleOutput;  // stdout/stderr
class FileOutput;     // File with rotation
class MultiOutput;    // Multiple destinations
```

### 5.2 Structured Logging Format

All logs must use JSON format with standard fields for aggregation:

```json
{
  "timestamp": "2026-02-23T10:15:30.123456Z",
  "level": "INFO",
  "message": "Order placed successfully",
  "service": "veloz-engine",
  "version": "1.0.0",
  "trace_id": "abc123...",
  "span_id": "def456...",
  "file": "order_router.cpp",
  "line": 142,
  "function": "place_order",
  "attributes": {
    "order_id": "ord-123",
    "symbol": "BTCUSDT",
    "side": "BUY",
    "quantity": 0.1
  }
}
```

### 5.3 Log Levels and Usage

| Level | Usage | Example |
|-------|-------|---------|
| `TRACE` | Detailed debugging, high-frequency events | Order book updates |
| `DEBUG` | Development debugging | Function entry/exit |
| `INFO` | Normal operations | Order placed, strategy started |
| `WARN` | Potential issues | Rate limit approaching, reconnecting |
| `ERROR` | Errors that don't stop operation | Order rejected, API error |
| `CRITICAL` | Errors requiring immediate attention | Exchange disconnected, risk breach |

### 5.4 Log Collection with Loki

Configure OpenTelemetry Collector to collect logs and forward to Loki:

```yaml
# otel-collector-config.yaml
receivers:
  filelog:
    include:
      - /var/log/veloz/*.log
    operators:
      - type: json_parser
        timestamp:
          parse_from: attributes.timestamp
          layout: '%Y-%m-%dT%H:%M:%S.%fZ'

exporters:
  loki:
    endpoint: http://loki:3100/loki/api/v1/push
    labels:
      attributes:
        service: ""
        level: ""
```

### 5.5 Log Correlation

All logs must include trace context for correlation:

```cpp
// C++ logging with trace context
void log_with_trace(LogLevel level, kj::StringPtr message, const TraceContext& ctx) {
    auto entry = LogEntry{
        .level = level,
        .message = message,
        .trace_id = ctx.trace_id,
        .span_id = ctx.span_id
    };
    global_logger().log(entry);
}
```

```python
# Python logging with trace context
import logging
from opentelemetry import trace

class TraceContextFilter(logging.Filter):
    def filter(self, record):
        span = trace.get_current_span()
        if span:
            ctx = span.get_span_context()
            record.trace_id = format(ctx.trace_id, '032x')
            record.span_id = format(ctx.span_id, '016x')
        return True
```

---

## 6. Alerting Architecture

### 6.1 Alert Categories

| Category | Severity | Response Time | Examples |
|----------|----------|---------------|----------|
| **Critical** | P1 | < 5 min | Exchange disconnected, risk limit breach |
| **High** | P2 | < 15 min | Order failures > 10%, latency P99 > 100ms |
| **Medium** | P3 | < 1 hour | Memory usage > 80%, disk space low |
| **Low** | P4 | < 24 hours | Deprecation warnings, minor anomalies |

### 6.2 Alertmanager Configuration

```yaml
# alertmanager.yaml
global:
  resolve_timeout: 5m

route:
  group_by: ['alertname', 'severity']
  group_wait: 10s
  group_interval: 5m
  repeat_interval: 4h
  receiver: 'default'
  routes:
    - match:
        severity: critical
      receiver: 'pagerduty-critical'
      continue: true
    - match:
        severity: high
      receiver: 'slack-high'

receivers:
  - name: 'default'
    slack_configs:
      - channel: '#veloz-alerts'
        send_resolved: true

  - name: 'pagerduty-critical'
    pagerduty_configs:
      - service_key: '${PAGERDUTY_SERVICE_KEY}'
        severity: critical

  - name: 'slack-high'
    slack_configs:
      - channel: '#veloz-alerts-high'
        send_resolved: true
```

### 6.3 Alert Rules

```yaml
# prometheus-rules.yaml
groups:
  - name: veloz-trading
    rules:
      # Critical: Exchange connection lost
      - alert: ExchangeDisconnected
        expr: veloz_engine_connection_state == 0
        for: 30s
        labels:
          severity: critical
        annotations:
          summary: "Exchange connection lost"
          description: "VeloZ engine lost connection to exchange"

      # Critical: Risk limit breach
      - alert: RiskLimitBreach
        expr: veloz_circuit_breaker_state == 1
        for: 0s
        labels:
          severity: critical
        annotations:
          summary: "Circuit breaker triggered"
          description: "Risk circuit breaker {{ $labels.breaker }} is open"

      # High: Order latency degradation
      - alert: OrderLatencyHigh
        expr: histogram_quantile(0.99, veloz_order_latency_seconds_bucket) > 0.1
        for: 5m
        labels:
          severity: high
        annotations:
          summary: "Order latency P99 > 100ms"
          description: "Order processing latency is {{ $value }}s"

      # High: Order rejection rate
      - alert: OrderRejectionRateHigh
        expr: |
          rate(veloz_orders_rejected_total[5m]) /
          rate(veloz_orders_placed_total[5m]) > 0.1
        for: 5m
        labels:
          severity: high
        annotations:
          summary: "Order rejection rate > 10%"
          description: "{{ $value | humanizePercentage }} of orders rejected"

      # Medium: Memory usage high
      - alert: MemoryUsageHigh
        expr: veloz_process_resident_memory_bytes / veloz_memory_limit_bytes > 0.8
        for: 10m
        labels:
          severity: medium
        annotations:
          summary: "Memory usage > 80%"
          description: "Memory usage is {{ $value | humanizePercentage }}"

      # Medium: Market data latency
      - alert: MarketDataLatencyHigh
        expr: histogram_quantile(0.99, veloz_market_latency_seconds_bucket) > 0.05
        for: 5m
        labels:
          severity: medium
        annotations:
          summary: "Market data latency P99 > 50ms"
          description: "Market data latency is {{ $value }}s"

  - name: veloz-infrastructure
    rules:
      # High: Service down
      - alert: ServiceDown
        expr: up{job="veloz"} == 0
        for: 1m
        labels:
          severity: high
        annotations:
          summary: "VeloZ service is down"
          description: "{{ $labels.instance }} is not responding"

      # Medium: Disk space low
      - alert: DiskSpaceLow
        expr: node_filesystem_avail_bytes / node_filesystem_size_bytes < 0.2
        for: 15m
        labels:
          severity: medium
        annotations:
          summary: "Disk space < 20%"
          description: "{{ $labels.mountpoint }} has {{ $value | humanizePercentage }} free"
```

---

## 7. Dashboards and SLOs

### 7.1 Dashboard Hierarchy

```
Grafana Dashboards
├── Executive Overview
│   ├── Trading P&L
│   ├── System Health Score
│   └── SLO Compliance
├── Trading Operations
│   ├── Order Flow
│   ├── Position Summary
│   ├── Strategy Performance
│   └── Risk Metrics
├── System Performance
│   ├── Latency (P50, P95, P99)
│   ├── Throughput
│   ├── Error Rates
│   └── Resource Usage
├── Infrastructure
│   ├── Kubernetes Cluster
│   ├── Node Health
│   └── Network Traffic
└── Debugging
    ├── Trace Explorer
    ├── Log Search
    └── Error Analysis
```

### 7.2 Service Level Objectives (SLOs)

| SLO | Target | Measurement | Alert Threshold |
|-----|--------|-------------|-----------------|
| **Availability** | 99.9% | `up{job="veloz"}` | < 99.5% over 1h |
| **Order Latency P99** | < 5ms | `veloz_order_latency_seconds` | > 10ms for 5m |
| **Market Data Latency P99** | < 10ms | `veloz_market_latency_seconds` | > 20ms for 5m |
| **Order Success Rate** | > 99% | `1 - (rejected/placed)` | < 98% for 5m |
| **Error Rate** | < 0.1% | `veloz_errors_total / requests` | > 1% for 5m |

### 7.3 SLO Dashboard Panels

```yaml
# Grafana dashboard JSON (excerpt)
panels:
  - title: "Availability SLO"
    type: gauge
    targets:
      - expr: avg_over_time(up{job="veloz"}[30d]) * 100
    thresholds:
      - value: 99.9
        color: green
      - value: 99.5
        color: yellow
      - value: 0
        color: red

  - title: "Order Latency SLO"
    type: timeseries
    targets:
      - expr: histogram_quantile(0.99, rate(veloz_order_latency_seconds_bucket[5m]))
        legendFormat: "P99"
      - expr: histogram_quantile(0.95, rate(veloz_order_latency_seconds_bucket[5m]))
        legendFormat: "P95"
      - expr: histogram_quantile(0.50, rate(veloz_order_latency_seconds_bucket[5m]))
        legendFormat: "P50"
    thresholds:
      - value: 0.005
        color: green
      - value: 0.010
        color: yellow
```

---

## 8. Deployment Architecture

### 8.1 Kubernetes Deployment

```yaml
# infra/k8s/observability/namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: veloz-observability
  labels:
    name: veloz-observability
---
# infra/k8s/observability/otel-collector.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: otel-collector
  namespace: veloz-observability
spec:
  replicas: 2
  selector:
    matchLabels:
      app: otel-collector
  template:
    metadata:
      labels:
        app: otel-collector
    spec:
      containers:
        - name: otel-collector
          image: otel/opentelemetry-collector-contrib:0.92.0
          args:
            - --config=/etc/otel/config.yaml
          ports:
            - containerPort: 4317  # OTLP gRPC
            - containerPort: 4318  # OTLP HTTP
            - containerPort: 8888  # Metrics
            - containerPort: 8889  # Prometheus exporter
          volumeMounts:
            - name: config
              mountPath: /etc/otel
          resources:
            requests:
              memory: "256Mi"
              cpu: "200m"
            limits:
              memory: "512Mi"
              cpu: "500m"
      volumes:
        - name: config
          configMap:
            name: otel-collector-config
---
apiVersion: v1
kind: Service
metadata:
  name: otel-collector
  namespace: veloz-observability
spec:
  type: ClusterIP
  ports:
    - name: otlp-grpc
      port: 4317
      targetPort: 4317
    - name: otlp-http
      port: 4318
      targetPort: 4318
    - name: prometheus
      port: 8889
      targetPort: 8889
  selector:
    app: otel-collector
```

### 8.2 Docker Compose (Development)

```yaml
# docker-compose.observability.yaml
version: '3.8'

services:
  otel-collector:
    image: otel/opentelemetry-collector-contrib:0.92.0
    command: ["--config=/etc/otel/config.yaml"]
    volumes:
      - ./config/otel-collector.yaml:/etc/otel/config.yaml
    ports:
      - "4317:4317"   # OTLP gRPC
      - "4318:4318"   # OTLP HTTP
      - "8889:8889"   # Prometheus metrics
    depends_on:
      - prometheus
      - jaeger
      - loki

  prometheus:
    image: prom/prometheus:v2.48.0
    volumes:
      - ./config/prometheus.yaml:/etc/prometheus/prometheus.yml
      - ./config/prometheus-rules.yaml:/etc/prometheus/rules.yml
      - prometheus-data:/prometheus
    ports:
      - "9090:9090"
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=15d'

  jaeger:
    image: jaegertracing/all-in-one:1.52
    ports:
      - "16686:16686"  # UI
      - "14250:14250"  # gRPC
    environment:
      - COLLECTOR_OTLP_ENABLED=true

  loki:
    image: grafana/loki:2.9.2
    ports:
      - "3100:3100"
    volumes:
      - ./config/loki.yaml:/etc/loki/local-config.yaml
      - loki-data:/loki

  grafana:
    image: grafana/grafana:10.2.2
    ports:
      - "3000:3000"
    volumes:
      - ./config/grafana/provisioning:/etc/grafana/provisioning
      - ./config/grafana/dashboards:/var/lib/grafana/dashboards
      - grafana-data:/var/lib/grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
      - GF_USERS_ALLOW_SIGN_UP=false

  alertmanager:
    image: prom/alertmanager:v0.26.0
    ports:
      - "9093:9093"
    volumes:
      - ./config/alertmanager.yaml:/etc/alertmanager/alertmanager.yml

volumes:
  prometheus-data:
  loki-data:
  grafana-data:
```

### 8.3 OpenTelemetry Collector Configuration

```yaml
# config/otel-collector.yaml
receivers:
  otlp:
    protocols:
      grpc:
        endpoint: 0.0.0.0:4317
      http:
        endpoint: 0.0.0.0:4318

  prometheus:
    config:
      scrape_configs:
        - job_name: 'veloz-engine'
          scrape_interval: 15s
          static_configs:
            - targets: ['veloz-engine:8080']
        - job_name: 'veloz-gateway'
          scrape_interval: 15s
          static_configs:
            - targets: ['veloz-gateway:8000']

  filelog:
    include:
      - /var/log/veloz/*.log
    operators:
      - type: json_parser
        timestamp:
          parse_from: attributes.timestamp
          layout: '%Y-%m-%dT%H:%M:%S.%fZ'

processors:
  batch:
    timeout: 1s
    send_batch_size: 1024

  memory_limiter:
    check_interval: 1s
    limit_mib: 400
    spike_limit_mib: 100

  attributes:
    actions:
      - key: environment
        value: ${ENVIRONMENT}
        action: insert

  filter:
    logs:
      exclude:
        match_type: strict
        bodies:
          - "health check"

exporters:
  prometheus:
    endpoint: 0.0.0.0:8889
    namespace: veloz

  jaeger:
    endpoint: jaeger:14250
    tls:
      insecure: true

  loki:
    endpoint: http://loki:3100/loki/api/v1/push
    labels:
      attributes:
        service: ""
        level: ""

  logging:
    loglevel: warn

service:
  pipelines:
    metrics:
      receivers: [otlp, prometheus]
      processors: [memory_limiter, batch, attributes]
      exporters: [prometheus]

    traces:
      receivers: [otlp]
      processors: [memory_limiter, batch, attributes]
      exporters: [jaeger]

    logs:
      receivers: [otlp, filelog]
      processors: [memory_limiter, batch, attributes, filter]
      exporters: [loki]
```

---

## 9. Implementation Plan

### 9.1 Phase 1: Foundation (Task #1)

1. Add `/metrics` endpoint to engine and gateway
2. Deploy Prometheus and configure scraping
3. Deploy Grafana with basic dashboards
4. Configure basic alerting rules

**Deliverables**:
- `/metrics` endpoints operational
- Prometheus scraping all components
- Basic Grafana dashboard
- Critical alerts configured

### 9.2 Phase 2: Log Aggregation (Task #6)

1. Configure JsonFormatter for all logs
2. Deploy Loki
3. Configure OTEL Collector log pipeline
4. Create log search dashboards in Grafana

**Deliverables**:
- All logs in JSON format
- Loki receiving logs
- Log search in Grafana
- Log-based alerts

### 9.3 Phase 3: Alerting (Task #8)

1. Deploy Alertmanager
2. Configure alert routing (Slack, PagerDuty)
3. Create comprehensive alert rules
4. Document runbooks for each alert

**Deliverables**:
- Alertmanager operational
- PagerDuty/Slack integration
- Alert runbooks
- On-call rotation setup

### 9.4 Phase 4: Dashboards & SLOs (Task #20)

1. Create executive dashboard
2. Create trading operations dashboard
3. Define and implement SLOs
4. Create SLO compliance reports

**Deliverables**:
- Complete dashboard hierarchy
- SLO definitions documented
- SLO tracking dashboards
- Monthly SLO reports

---

## 10. Integration Points

### 10.1 Engine Integration

```cpp
// apps/engine/src/main.cpp
#include "veloz/core/metrics.h"
#include "veloz/core/tracing.h"

int main(int argc, char* argv[]) {
    // Initialize metrics
    auto& metrics = global_metrics();
    metrics.register_counter("veloz_orders_placed_total", "Total orders placed");
    metrics.register_histogram("veloz_order_latency_seconds", "Order latency");

    // Initialize tracing
    configure_tracing("otel-collector:4317");

    // ... rest of initialization
}
```

### 10.2 Gateway Integration

```python
# apps/gateway/gateway.py
from prometheus_client import Counter, Histogram, generate_latest
from opentelemetry import trace
from opentelemetry.instrumentation.fastapi import FastAPIInstrumentor

# Metrics
http_requests = Counter('veloz_http_requests_total', 'HTTP requests', ['method', 'endpoint', 'status'])
http_duration = Histogram('veloz_http_request_duration_seconds', 'Request duration', ['method', 'endpoint'])

# Tracing
FastAPIInstrumentor.instrument_app(app)

@app.get("/metrics")
async def metrics():
    return Response(content=generate_latest(), media_type="text/plain")
```

### 10.3 Kubernetes Integration

Update the existing deployment to include observability annotations:

```yaml
# infra/k8s/deployment.yaml (additions)
spec:
  template:
    metadata:
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "8080"
        prometheus.io/path: "/metrics"
```

---

## 11. Security Considerations

1. **Metrics Authentication**: Use service mesh or network policies to restrict `/metrics` access
2. **Log Sanitization**: Never log sensitive data (API keys, passwords, PII)
3. **Trace Sampling**: Sample traces to avoid exposing all request details
4. **Dashboard Access**: RBAC for Grafana dashboards based on user roles
5. **Alert Channels**: Secure webhook URLs and API keys in secrets

---

## 12. Performance Impact

| Component | Expected Overhead | Mitigation |
|-----------|-------------------|------------|
| Metrics collection | < 0.1% CPU | Atomic counters, no locks on hot path |
| Trace instrumentation | < 0.5% CPU | Sampling, async export |
| Log formatting | < 0.2% CPU | Buffered writes, async flush |
| OTEL Collector | 200-500MB RAM | Memory limiter, batch processing |

---

## 13. References

- [OpenTelemetry Documentation](https://opentelemetry.io/docs/)
- [Prometheus Best Practices](https://prometheus.io/docs/practices/)
- [Grafana Dashboards](https://grafana.com/docs/grafana/latest/dashboards/)
- [Loki Documentation](https://grafana.com/docs/loki/latest/)
- [Alertmanager Configuration](https://prometheus.io/docs/alerting/latest/configuration/)

---

## Appendix A: Metric Naming Conventions

```
veloz_<subsystem>_<metric>_<unit>

Examples:
- veloz_orders_placed_total
- veloz_order_latency_seconds
- veloz_market_events_received_total
- veloz_position_value_usd
- veloz_memory_arena_bytes
```

## Appendix B: Log Field Reference

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `timestamp` | string | Yes | ISO 8601 timestamp |
| `level` | string | Yes | Log level (TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL) |
| `message` | string | Yes | Log message |
| `service` | string | Yes | Service name |
| `version` | string | Yes | Service version |
| `trace_id` | string | No | W3C trace ID (32 hex chars) |
| `span_id` | string | No | W3C span ID (16 hex chars) |
| `file` | string | No | Source file |
| `line` | int | No | Source line number |
| `function` | string | No | Function name |
| `attributes` | object | No | Additional structured data |
