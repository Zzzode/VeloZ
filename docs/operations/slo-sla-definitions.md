# VeloZ SLO/SLA Definitions

This document defines the Service Level Objectives (SLOs) and Service Level Agreements (SLAs) for the VeloZ trading platform.

## Overview

VeloZ uses a tiered SLO approach based on the criticality of different system components:

- **Tier 1 (Critical)**: Order execution, market data
- **Tier 2 (Important)**: API gateway, authentication
- **Tier 3 (Standard)**: Monitoring, logging, UI

## Service Level Objectives (SLOs)

### 1. Availability SLOs

| Service | SLO Target | Measurement Window | Error Budget (30d) |
|---------|-----------|-------------------|-------------------|
| Gateway API | 99.9% | Rolling 30 days | 43.2 minutes |
| Trading Engine | 99.95% | Rolling 30 days | 21.6 minutes |
| Market Data Feed | 99.9% | Rolling 30 days | 43.2 minutes |
| WebSocket Connections | 99.5% | Rolling 30 days | 3.6 hours |

**Measurement:**
```promql
# Gateway Availability
avg_over_time(up{job="veloz-gateway"}[30d]) * 100

# Engine Availability
avg_over_time(veloz_engine_running[30d]) * 100
```

### 2. Latency SLOs

| Operation | p50 Target | p90 Target | p99 Target | Measurement |
|-----------|-----------|-----------|-----------|-------------|
| Order Submission | < 10ms | < 50ms | < 100ms | veloz_order_latency |
| Order Acknowledgment | < 20ms | < 75ms | < 150ms | veloz_order_ack_latency |
| Market Data Processing | < 1ms | < 5ms | < 10ms | veloz_market_processing_latency |
| API Response (GET) | < 50ms | < 150ms | < 300ms | veloz_http_request_duration |
| API Response (POST) | < 100ms | < 300ms | < 500ms | veloz_http_request_duration |

**Measurement:**
```promql
# Order Latency p99
histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m]))

# API Latency p99
histogram_quantile(0.99, rate(veloz_http_request_duration_bucket[5m]))
```

### 3. Throughput SLOs

| Metric | Minimum | Target | Maximum |
|--------|---------|--------|---------|
| Orders per Second | 10 | 100 | 1000 |
| Market Updates per Second | 100 | 1000 | 10000 |
| API Requests per Second | 50 | 500 | 5000 |
| WebSocket Messages per Second | 100 | 1000 | 10000 |

### 4. Data Quality SLOs

| Metric | SLO Target | Measurement |
|--------|-----------|-------------|
| Market Data Freshness | 99% < 50ms lag | veloz_market_data_lag_ms |
| Order State Consistency | 99.99% | Reconciliation checks |
| Position Accuracy | 100% | Reconciliation checks |

### 5. Error Rate SLOs

| Error Type | SLO Target | Measurement |
|------------|-----------|-------------|
| API 5xx Errors | < 0.1% | veloz_http_requests_total{status=~"5.."} |
| Order Rejections (System) | < 0.01% | veloz_errors_total{type="order_rejection"} |
| Exchange API Errors | < 1% | veloz_exchange_api_errors_total |
| WebSocket Disconnections | < 5/hour | veloz_websocket_reconnects_total |

## Error Budget Policy

### Budget Calculation

Error budget is calculated as:
```
Error Budget = (1 - SLO Target) * Time Window
```

For 99.9% availability over 30 days:
```
Error Budget = 0.001 * 30 * 24 * 60 = 43.2 minutes
```

### Budget Consumption Alerts

| Budget Remaining | Alert Level | Action |
|-----------------|-------------|--------|
| > 50% | None | Normal operations |
| 25-50% | Warning | Review recent changes |
| 10-25% | High | Freeze non-critical deployments |
| < 10% | Critical | Incident response, freeze all deployments |

### Budget Burn Rate Alerts

| Burn Rate | Time to Exhaustion | Alert Level |
|-----------|-------------------|-------------|
| 1x | 30 days | None |
| 2x | 15 days | Warning |
| 6x | 5 days | High |
| 14.4x | 2 days | Critical |

## Service Level Agreements (SLAs)

### Production SLA Tiers

#### Tier 1: Enterprise (99.95% Availability)
- **Availability**: 99.95% uptime (21.6 minutes downtime/month)
- **Order Latency**: p99 < 100ms
- **Support Response**: 15 minutes for critical issues
- **Incident Communication**: Real-time updates
- **Compensation**: 10% credit per 0.1% below SLA

#### Tier 2: Professional (99.9% Availability)
- **Availability**: 99.9% uptime (43.2 minutes downtime/month)
- **Order Latency**: p99 < 200ms
- **Support Response**: 1 hour for critical issues
- **Incident Communication**: Hourly updates
- **Compensation**: 5% credit per 0.1% below SLA

#### Tier 3: Standard (99.5% Availability)
- **Availability**: 99.5% uptime (3.6 hours downtime/month)
- **Order Latency**: p99 < 500ms
- **Support Response**: 4 hours for critical issues
- **Incident Communication**: Daily updates
- **Compensation**: None

### Exclusions

The following are excluded from SLA calculations:
1. Scheduled maintenance windows (announced 48h in advance)
2. Force majeure events
3. Third-party exchange outages
4. Customer-caused issues
5. Beta/preview features

## Monitoring and Reporting

### Real-time Dashboards

- **SLO Dashboard**: `/grafana/d/veloz-slo`
- **Trading Dashboard**: `/grafana/d/veloz-trading`
- **System Health**: `/grafana/d/veloz-system`

### Prometheus Recording Rules

```yaml
groups:
  - name: veloz-slo-recording
    rules:
      # Availability SLI
      - record: veloz:availability:ratio_rate5m
        expr: avg_over_time(up{job="veloz-gateway"}[5m])

      # Order Latency SLI
      - record: veloz:order_latency:p99_5m
        expr: histogram_quantile(0.99, rate(veloz_order_latency_bucket[5m]))

      # Error Rate SLI
      - record: veloz:error_rate:ratio_rate5m
        expr: sum(rate(veloz_http_requests_total{status=~"5.."}[5m])) / sum(rate(veloz_http_requests_total[5m]))

      # Error Budget Remaining
      - record: veloz:error_budget:remaining_ratio
        expr: 1 - ((1 - avg_over_time(up{job="veloz-gateway"}[30d])) / 0.001)
```

### Monthly SLO Report

Generated metrics for monthly review:
- Availability percentage
- Latency percentiles (p50, p90, p99)
- Error budget consumption
- Incident count and duration
- SLA breach incidents

## Incident Response

### Severity Levels

| Severity | Impact | Response Time | Resolution Target |
|----------|--------|---------------|-------------------|
| SEV-1 | Complete outage | 5 minutes | 1 hour |
| SEV-2 | Major degradation | 15 minutes | 4 hours |
| SEV-3 | Minor degradation | 1 hour | 24 hours |
| SEV-4 | Low impact | 4 hours | 72 hours |

### Escalation Path

1. **On-call Engineer**: First responder
2. **Team Lead**: SEV-1/SEV-2 escalation
3. **Engineering Manager**: Extended outages (> 1 hour)
4. **CTO**: Major incidents (> 4 hours or data loss)

## Review and Updates

- SLOs are reviewed quarterly
- SLAs are reviewed annually
- Error budgets reset monthly
- Dashboard queries updated with schema changes
