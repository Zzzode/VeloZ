# C++ Gateway Deployment and Rollout Strategy

**Version**: 1.0
**Last Updated**: 2026-02-27
**Author**: Gateway Migration Team

## Table of Contents

1. [Overview](#overview)
2. [Deployment Phases](#deployment-phases)
3. [Traffic Migration Plan](#traffic-migration-plan)
4. [Dual-Stack Operation](#dual-stack-operation)
5. [Rollback Procedures](#rollback-procedures)
6. [Monitoring and Alerting](#monitoring-and-alerting)
7. [Feature Flags](#feature-flags)
8. [Data Migration](#data-migration)
9. [Load Balancer Configuration](#load-balancer-configuration)
10. [Health Checks and Circuit Breakers](#health-checks-and-circuit-breakers)
11. [Success Criteria](#success-criteria)
12. [Deployment Scripts](#deployment-scripts)
13. [Rollback Decision Tree](#rollback-decision-tree)

---

## Overview

This document describes the comprehensive rollout strategy for migrating from Python Gateway to C++ Gateway. The strategy ensures zero-downtime migration with automated rollback capability.

### Key Objectives

- **Zero downtime**: No service interruption during migration
- **Safe rollback**: Automated rollback capability within 30 seconds
- **Gradual migration**: Percentage-based traffic splitting
- **Backward compatibility**: Maintain API compatibility during transition
- **Monitoring**: Comprehensive observability throughout migration

### Migration Timeline

- **Total duration**: 2 weeks
- **Phase 1 (Canary)**: Days 1-3
- **Phase 2 (Staging)**: Days 4-7
- **Phase 3 (Production)**: Days 8-11
- **Phase 4 (Full Rollout)**: Days 12-13
- **Phase 5 (Decommission)**: Day 14

---

## Deployment Phases

### Phase 1: Canary Deployment (5% Traffic)

**Duration**: Days 1-3 (72 hours)

**Objectives**:
- Validate C++ Gateway stability in production environment
- Monitor error rates, latency, and resource usage
- Test automated rollback triggers
- Verify SSE streaming for 5% of clients

**Prerequisites**:
- C++ Gateway built and tested in staging
- Load balancer configured for traffic splitting
- Monitoring dashboards deployed
- Rollback scripts tested

**Steps**:
1. Deploy C++ Gateway to production (port 8081)
2. Configure load balancer to route 5% traffic to C++ Gateway
3. Enable automated health checks
4. Set up rollback triggers (error rate > 1%, P99 latency > 5ms)
5. Monitor metrics continuously for 72 hours
6. Collect performance data for comparison

**Success Criteria**:
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 30%
- Memory stable (no leaks)
- No authentication failures
- All health checks passing

**Exit Criteria**:
- All success criteria met for 24 consecutive hours
- No automated rollbacks triggered
- Engineering team approves moving to next phase

### Phase 2: Staging Deployment (25% Traffic)

**Duration**: Days 4-7 (96 hours)

**Objectives**:
- Validate C++ Gateway under increased load
- Test order submission and cancellation flows
- Verify SSE streaming for 25% of clients
- Monitor for any edge cases

**Prerequisites**:
- Phase 1 completed successfully
- No issues identified in canary

**Steps**:
1. Increase C++ Gateway traffic to 25%
2. Monitor order flow consistency
3. Verify SSE connection stability
4. Test Binance integration (if applicable)
5. Collect detailed metrics

**Success Criteria**:
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 40%
- Memory usage stable
- Order processing consistent with Python Gateway
- SSE connections stable

**Exit Criteria**:
- All success criteria met for 24 consecutive hours
- No order discrepancies
- No SSE connection drops

### Phase 3: Production Rollout (50% Traffic)

**Duration**: Days 8-11 (96 hours)

**Objectives**:
- Validate C++ Gateway at production scale
- Test full load scenarios
- Verify disaster recovery procedures

**Prerequisites**:
- Phase 2 completed successfully
- No issues identified at 25% traffic

**Steps**:
1. Increase C++ Gateway traffic to 50%
2. Conduct load testing (10k req/s)
3. Test failover scenarios
4. Verify audit logging consistency
5. Test all API endpoints

**Success Criteria**:
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 50%
- Memory usage < 200MB
- All API endpoints functional
- Audit logs consistent

**Exit Criteria**:
- All success criteria met for 24 consecutive hours
- Load testing successful
- Failover tests passed

### Phase 4: Full Rollout (100% Traffic)

**Duration**: Days 12-13 (48 hours)

**Objectives**:
- Complete migration to C++ Gateway
- Validate full production load
- Prepare for Python Gateway decommission

**Prerequisites**:
- Phase 3 completed successfully
- All tests passed

**Steps**:
1. Increase C++ Gateway traffic to 100%
2. Monitor all metrics for 24 hours
3. Verify no degradation
4. Validate all client connections
5. Prepare Python Gateway for shutdown

**Success Criteria**:
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 50%
- Memory usage stable
- All clients functioning normally

**Exit Criteria**:
- All success criteria met for 24 consecutive hours
- No issues reported by users
- Engineering team approves decommission

### Phase 5: Python Gateway Decommission

**Duration**: Day 14 (24 hours)

**Objectives**:
- Safely shut down Python Gateway
- Clean up infrastructure
- Document migration completion

**Steps**:
1. Verify Python Gateway has zero connections
2. Stop Python Gateway process
3. Remove Python Gateway from load balancer
4. Archive Python Gateway logs
5. Update documentation
6. Remove Python Gateway code (optional)

**Success Criteria**:
- Python Gateway successfully stopped
- No service interruption
- All clients using C++ Gateway
- Documentation updated

---

## Traffic Migration Plan

### Load Balancer Configuration

**Load Balancer**: nginx

**Backend Configuration**:
```nginx
upstream veloz_gateway {
    # Python Gateway (fallback)
    server 127.0.0.1:8080 weight=95 max_fails=3 fail_timeout=30s;

    # C++ Gateway (canary)
    server 127.0.0.1:8081 weight=5 max_fails=3 fail_timeout=30s;

    keepalive 100;
}
```

**Traffic Splitting Strategy**:

| Phase | C++ Gateway Weight | Python Gateway Weight | Duration |
|-------|-------------------|----------------------|----------|
| Phase 1 | 5% | 95% | 72 hours |
| Phase 2 | 25% | 75% | 96 hours |
| Phase 3 | 50% | 50% | 96 hours |
| Phase 4 | 100% | 0% | 48 hours |
| Phase 5 | 100% | 0% (removed) | 24 hours |

### Traffic Splitting Implementation

**Script**: `scripts/adjust_traffic_split.sh`

```bash
#!/usr/bin/env bash
CPP_WEIGHT=${1:-5}
PY_WEIGHT=$((100 - CPP_WEIGHT))

echo "Adjusting traffic split: C++ ${CPP_WEIGHT}%, Python ${PY_WEIGHT}%"

# Update nginx configuration
cat > /etc/nginx/conf.d/veloz-gateway.conf <<EOF
upstream veloz_gateway {
    # Python Gateway
    server 127.0.0.1:8080 weight=${PY_WEIGHT} max_fails=3 fail_timeout=30s;

    # C++ Gateway
    server 127.0.0.1:8081 weight=${CPP_WEIGHT} max_fails=3 fail_timeout=30s;

    keepalive 100;
}

server {
    listen 80;
    server_name veloz.example.com;

    location / {
        proxy_pass http://veloz_gateway;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;

        # SSE streaming
        proxy_buffering off;
        proxy_cache off;
    }

    location /metrics {
        proxy_pass http://veloz_gateway;
        proxy_http_version 1.1;
    }
}
EOF

# Reload nginx
nginx -t && nginx -s reload
```

---

## Dual-Stack Operation

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        nginx LB                              │
│                    (Traffic Splitter)                        │
└───────────────────┬─────────────────┬─────────────────────────┘
                    │                 │
                    │ 5%-100%         │ 0%-95%
                    ▼                 ▼
┌───────────────────────────────┐ ┌───────────────────────────────┐
│      C++ Gateway              │ │      Python Gateway          │
│      Port 8081                │ │      Port 8080                │
│                               │ │                               │
│  • HTTP Server (KJ)          │ │  • HTTP Server (FastAPI)     │
│  • Engine Integration         │ │  • Engine Integration        │
│  • SSE Streaming             │ │  • SSE Streaming            │
│  • Metrics (Prometheus)       │ │  • Metrics (Prometheus)       │
│  • Health Checks             │ │  • Health Checks             │
└───────────────────────────────┘ └───────────────────────────────┘
```

### Data Consistency

**No Shared State**: Both gateways are stateless except for:
- Engine state (same engine process)
- In-memory caches (identical behavior)
- SSE connections (independent)

**Audit Logging**: Both gateways write to the same audit log directory.

### Session Management

**SSE Connections**: Each gateway manages its own SSE connections.
- Clients reconnect automatically on failover
- No session state lost (events rebroadcast)

**Authentication**: Both gateways use the same JWT secret and API keys.

---

## Rollback Procedures

### Automated Rollback

**Trigger Conditions**:
- Error rate > 1% for 5 minutes
- P99 latency > 5ms for 5 minutes
- Memory leak detected (>50MB increase in 1 hour)
- Authentication failures > 2x baseline
- Health check failure

**Rollback Script**: `scripts/rollback_gateway.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Emergency Rollback to Python Gateway ==="

# 1. Immediately route 100% traffic to Python
echo "Routing 100% traffic to Python Gateway..."
./scripts/adjust_traffic_split.sh 0

# 2. Check Python Gateway health
echo "Checking Python Gateway health..."
if ./scripts/health_check_gateway.sh http://127.0.0.1:8080; then
    echo "Python Gateway is healthy"
else
    echo "WARNING: Python Gateway health check failed"
    exit 1
fi

# 3. Verify traffic routing
echo "Waiting 10 seconds for traffic to stabilize..."
sleep 10

# 4. Monitor error rate
echo "Monitoring error rate..."
./scripts/monitor_error_rate.sh --timeout 60

# 5. Alert engineering team
echo "Rollback complete. Alerting engineering team..."
./scripts/alert_rollback.sh "Automatic rollback triggered"

echo "=== Rollback Complete ==="
```

### Manual Rollback

**Steps**:
1. Stop C++ Gateway (if needed):
   ```bash
   pkill -f veloz_gateway_cpp
   ```

2. Route 100% traffic to Python:
   ```bash
   ./scripts/adjust_traffic_split.sh 0
   ```

3. Verify Python Gateway:
   ```bash
   curl http://127.0.0.1:8080/health
   ```

4. Monitor metrics for 10 minutes

5. Document rollback reason

### Rollback Validation

**Post-Rollback Checks**:
- Error rate returns to baseline
- Latency returns to baseline
- All API endpoints functional
- SSE connections stable

---

## Monitoring and Alerting

### Key Metrics

**Latency Metrics**:
- `veloz_gateway_request_duration_seconds` (histogram)
- P50: < 150μs
- P99: < 1ms
- P99.9: < 5ms

**Error Metrics**:
- `veloz_gateway_requests_total{status="5xx"}` (counter)
- Error rate: < 0.1%

**Resource Metrics**:
- `process_cpu_seconds_total` (counter)
- `process_resident_memory_bytes` (gauge)
- CPU: < 50%
- Memory: < 200MB

**Connection Metrics**:
- `veloz_gateway_sse_connections` (gauge)
- Active SSE connections

### Prometheus Alerts

**Alerting Rules**: `config/prometheus/alerts.yml`

```yaml
groups:
  - name: veloz_gateway
    interval: 30s
    rules:
      # High error rate
      - alert: GatewayHighErrorRate
        expr: |
          (
            sum(rate(veloz_gateway_requests_total{status=~"5.."}[5m]))
            /
            sum(rate(veloz_gateway_requests_total[5m]))
          ) > 0.01
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate on C++ Gateway"
          description: "Error rate is {{ $value | humanizePercentage }} for 5 minutes"

      # High P99 latency
      - alert: GatewayHighP99Latency
        expr: |
          histogram_quantile(0.99,
            sum(rate(veloz_gateway_request_duration_seconds_bucket[5m])) by (le, instance)
          ) > 0.005
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High P99 latency on C++ Gateway"
          description: "P99 latency is {{ $value }}s for 5 minutes"

      # Memory leak
      - alert: GatewayMemoryLeak
        expr: |
          (
            process_resident_memory_bytes{job="veloz_gateway_cpp"}
            -
            process_resident_memory_bytes{job="veloz_gateway_cpp"} offset 1h
          ) > 52428800
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "Possible memory leak in C++ Gateway"
          description: "Memory increased by >50MB in 1 hour"

      # Authentication failures
      - alert: GatewayAuthFailures
        expr: |
          (
            sum(rate(veloz_gateway_auth_failures_total[5m]))
            /
            sum(rate(veloz_gateway_auth_attempts_total[5m]))
          ) > 0.1
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High authentication failure rate"
          description: "Auth failure rate is {{ $value | humanizePercentage }}"

      # Health check failure
      - alert: GatewayHealthCheckFailed
        expr: up{job="veloz_gateway_cpp"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "C++ Gateway health check failed"
          description: "Gateway is down for >1 minute"
```

### Grafana Dashboard

**Dashboard**: `config/grafana/dashboards/gateway-migration.json`

**Panels**:
1. Traffic Split (percentage)
2. Request Rate (requests/second)
3. Error Rate (percentage)
4. Latency (P50, P99, P99.9)
5. CPU Usage
6. Memory Usage
7. SSE Connections
8. Order Processing Rate
9. Health Check Status

---

## Feature Flags

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `VELOZ_GATEWAY_USE_CPP` | Enable C++ Gateway (0=Python, 1=C++) | 0 |
| `VELOZ_GATEWAY_CPP_PORT` | C++ Gateway port | 8081 |
| `VELOZ_GATEWAY_PY_PORT` | Python Gateway port | 8080 |
| `VELOZ_GATEWAY_FEATURE_BLOB_STREAMING` | Enable blob streaming | 0 |
| `VELOZ_GATEWAY_FEATURE_RATE_LIMITING` | Enable rate limiting | 1 |

### Feature Flag Implementation

**Load Balancer Based**:
```bash
# Enable C++ Gateway for specific percentage
export VELOZ_GATEWAY_CPP_PERCENTAGE=5
```

**Header Based**:
```nginx
map $http_x_gateway_version $backend {
    default "python";
    "cpp" "cpp";
}

upstream python_gateway { server 127.0.0.1:8080; }
upstream cpp_gateway { server 127.0.0.1:8081; }

server {
    location / {
        proxy_pass http://${backend}_gateway;
    }
}
```

---

## Data Migration

### Audit Logs

**No Migration Required**: Both gateways write to the same audit log directory.

**Log Format**: NDJSON (compatible)

### API Keys

**No Migration Required**: Both gateways use the same keychain file (`veloz_keychain.json`).

### JWT Tokens

**No Migration Required**: Both gateways use the same JWT secret.

**Token Validation**: Compatible token format and signature.

---

## Load Balancer Configuration

### nginx Configuration

**File**: `/etc/nginx/conf.d/veloz-gateway.conf`

```nginx
upstream veloz_gateway {
    # Python Gateway (fallback)
    server 127.0.0.1:8080 weight=95 max_fails=3 fail_timeout=30s;

    # C++ Gateway (canary)
    server 127.0.0.1:8081 weight=5 max_fails=3 fail_timeout=30s;

    keepalive 100;
}

server {
    listen 80;
    server_name veloz.example.com;

    # Health check endpoint
    location /health {
        proxy_pass http://veloz_gateway/health;
        access_log off;
    }

    # Metrics endpoint
    location /metrics {
        proxy_pass http://veloz_gateway/metrics;
        proxy_http_version 1.1;
    }

    # API endpoints
    location /api/ {
        proxy_pass http://veloz_gateway;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeouts
        proxy_connect_timeout 5s;
        proxy_send_timeout 30s;
        proxy_read_timeout 30s;
    }

    # SSE streaming
    location /api/stream {
        proxy_pass http://veloz_gateway/api/stream;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;

        # Disable buffering for SSE
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 3600s;
    }
}
```

### HAProxy Configuration (Alternative)

**File**: `/etc/haproxy/haproxy.cfg`

```haproxy
frontend veloz_gateway_front
    bind *:80
    mode http
    timeout client 30s

    # Health check
    use_backend health_check if { path -i /health }

    # SSE streaming
    use_backend sse_backend if { path -i /api/stream }

    # API endpoints
    default_backend veloz_gateway_back

backend health_check
    mode http
    timeout connect 5s
    timeout server 5s
    server python_gateway 127.0.0.1:8080 check
    server cpp_gateway 127.0.0.1:8081 check backup

backend sse_backend
    mode http
    timeout server 3600s
    timeout tunnel 3600s
    balance leastconn
    server python_gateway 127.0.0.1:8080 weight 95 check
    server cpp_gateway 127.0.0.1:8081 weight 5 check

backend veloz_gateway_back
    mode http
    timeout connect 5s
    timeout server 30s
    balance roundrobin
    server python_gateway 127.0.0.1:8080 weight 95 check maxconn 1000
    server cpp_gateway 127.0.0.1:8081 weight 5 check maxconn 1000
```

---

## Health Checks and Circuit Breakers

### Health Check Endpoint

**Endpoint**: `GET /health`

**Response**:
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "gateway": "cpp",
  "timestamp": "2026-02-27T00:00:00Z",
  "checks": {
    "engine": "healthy",
    "database": "healthy",
    "auth": "healthy"
  }
}
```

**Health Check Script**: `scripts/health_check_gateway.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

GATEWAY_URL=${1:-"http://127.0.0.1:8081"}
TIMEOUT=5

echo "Checking gateway health at ${GATEWAY_URL}..."

# Perform health check
RESPONSE=$(curl -s -f --max-time ${TIMEOUT} "${GATEWAY_URL}/health") || {
    echo "ERROR: Health check failed"
    exit 1
}

# Parse response
STATUS=$(echo "${RESPONSE}" | jq -r '.status')

if [[ "${STATUS}" == "healthy" ]]; then
    echo "Gateway is healthy"
    echo "${RESPONSE}" | jq .
    exit 0
else
    echo "ERROR: Gateway status is ${STATUS}"
    echo "${RESPONSE}" | jq .
    exit 1
fi
```

### Circuit Breaker Configuration

**Implementation**: nginx max_fails and fail_timeout

```nginx
server 127.0.0.1:8081 max_fails=3 fail_timeout=30s;
```

**Behavior**:
- 3 consecutive failures → mark as down
- Wait 30 seconds → retry connection
- Successful connection → mark as up

---

## Success Criteria

### Phase-Wise Success Criteria

#### Phase 1 (Canary - 5%)
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 30%
- Memory stable
- No authentication failures
- All health checks passing

#### Phase 2 (Staging - 25%)
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 40%
- Order processing consistent
- SSE connections stable

#### Phase 3 (Production - 50%)
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 50%
- All API endpoints functional
- Audit logs consistent

#### Phase 4 (Full Rollout - 100%)
- Error rate < 0.1%
- P50 latency < 150μs
- P99 latency < 1ms
- CPU usage < 50%
- All clients functioning
- No user issues

#### Phase 5 (Decommission)
- Python Gateway stopped
- No service interruption
- Documentation updated

### Overall Success Criteria

**Performance**:
- Latency P50 < 150μs (7x improvement over Python)
- Latency P99 < 1ms
- Throughput > 10,000 req/s (10x improvement)

**Reliability**:
- Error rate < 0.1%
- Uptime > 99.9%

**Resources**:
- CPU < 50%
- Memory < 200MB

---

## Deployment Scripts

### deploy_gateway_canary.sh

**Purpose**: Deploy C++ Gateway to canary environment

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Deploying C++ Gateway to Canary ==="

# Parse arguments
PRESET=${1:-"dev"}
DRY_RUN=${2:-0}

export VELOZ_PRESET="${PRESET}"

# Check if this is a dry run
if [[ "${DRY_RUN}" -eq 1 ]]; then
    echo "=== Dry run mode ==="
    echo "Preset: ${PRESET}"
    echo "Would deploy:"
    echo "  1. Build C++ Gateway"
    echo "  2. Deploy to port 8081"
    echo "  3. Run health checks"
    echo "  4. Configure 5% traffic split"
    exit 0
fi

# Step 1: Build C++ Gateway
echo "=== Step 1/5: Building C++ Gateway ==="
BUILD_DIR="build/${PRESET}"

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "Configuring CMake..."
    cmake --preset "${PRESET}"
fi

echo "Building Gateway..."
cmake --build --preset "${PRESET}-gateway" -j$(nproc)

# Step 2: Stop existing C++ Gateway (if running)
echo "=== Step 2/5: Stopping existing C++ Gateway ==="
if pgrep -f "veloz_gateway_cpp"; then
    echo "Stopping existing C++ Gateway..."
    pkill -f "veloz_gateway_cpp"
    sleep 2
fi

# Step 3: Deploy new C++ Gateway
echo "=== Step 3/5: Deploying C++ Gateway ==="
GATEWAY_BIN="${BUILD_DIR}/apps/gateway_cpp/veloz_gateway_cpp"

if [[ ! -x "${GATEWAY_BIN}" ]]; then
    echo "ERROR: Gateway binary not found: ${GATEWAY_BIN}"
    exit 1
fi

# Start gateway in background
echo "Starting C++ Gateway on port 8081..."
nohup "${GATEWAY_BIN}" --port 8081 > /var/log/veloz/gateway_cpp.log 2>&1 &
GATEWAY_PID=$!

echo "Gateway started with PID: ${GATEWAY_PID}"

# Step 4: Run health checks
echo "=== Step 4/5: Running health checks ==="
sleep 5  # Wait for gateway to start

MAX_RETRIES=10
RETRY_DELAY=5

for ((i=1; i<=MAX_RETRIES; i++)); do
    if ./scripts/health_check_gateway.sh http://127.0.0.1:8081; then
        echo "Health check passed!"
        break
    fi

    if [[ ${i} -eq ${MAX_RETRIES} ]]; then
        echo "ERROR: Health check failed after ${MAX_RETRIES} attempts"
        echo "Rolling back..."
        kill ${GATEWAY_PID}
        exit 1
    fi

    echo "Health check failed, retrying in ${RETRY_DELAY}s (attempt ${i}/${MAX_RETRIES})..."
    sleep ${RETRY_DELAY}
done

# Step 5: Configure traffic split
echo "=== Step 5/5: Configuring traffic split ==="
./scripts/adjust_traffic_split.sh 5

echo ""
echo "=== Canary Deployment Complete ==="
echo "C++ Gateway deployed successfully"
echo "Gateway PID: ${GATEWAY_PID}"
echo "Traffic split: 5% C++, 95% Python"
echo ""
echo "Monitor at:"
echo "  - Health: http://127.0.0.1:8081/health"
echo "  - Metrics: http://127.0.0.1:8081/metrics"
echo ""
```

### rollback_gateway.sh

**Purpose**: Rollback to Python Gateway

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Rollback to Python Gateway ==="

# Step 1: Route 100% traffic to Python
echo "=== Step 1/4: Routing traffic to Python Gateway ==="
./scripts/adjust_traffic_split.sh 0

# Step 2: Check Python Gateway health
echo "=== Step 2/4: Checking Python Gateway health ==="
if ./scripts/health_check_gateway.sh http://127.0.0.1:8080; then
    echo "Python Gateway is healthy"
else
    echo "WARNING: Python Gateway health check failed"
    echo "Attempting to start Python Gateway..."

    # Try to start Python Gateway
    if ! pgrep -f "gateway.py"; then
        echo "Starting Python Gateway..."
        nohup python3 apps/gateway/gateway.py > /var/log/veloz/gateway_py.log 2>&1 &
        sleep 5

        if ./scripts/health_check_gateway.sh http://127.0.0.1:8080; then
            echo "Python Gateway started successfully"
        else
            echo "ERROR: Failed to start Python Gateway"
            exit 1
        fi
    fi
fi

# Step 3: Stop C++ Gateway (optional)
echo "=== Step 3/4: Stopping C++ Gateway ==="
if pgrep -f "veloz_gateway_cpp"; then
    echo "Stopping C++ Gateway..."
    pkill -f "veloz_gateway_cpp" || true
    sleep 2
else
    echo "C++ Gateway not running"
fi

# Step 4: Verify traffic
echo "=== Step 4/4: Verifying traffic ==="
sleep 10

echo "Rollback complete!"
echo ""
echo "Traffic routing: 100% Python Gateway"
echo "Python Gateway: http://127.0.0.1:8080"
echo ""
```

### health_check_gateway.sh

**Purpose**: Health check for Gateway

```bash
#!/usr/bin/env bash
set -euo pipefail

GATEWAY_URL=${1:-"http://127.0.0.1:8081"}
TIMEOUT=${2:-5}
VERBOSE=${3:-0}

# Check if jq is available
if ! command -v jq >/dev/null 2>&1; then
    echo "WARNING: jq not found, skipping JSON parsing"
    JQ_AVAILABLE=0
else
    JQ_AVAILABLE=1
fi

# Perform health check
RESPONSE=$(curl -s -f --max-time "${TIMEOUT}" "${GATEWAY_URL}/health" 2>&1) || {
    echo "ERROR: Health check failed"
    echo "URL: ${GATEWAY_URL}/health"
    echo "Error: ${RESPONSE}"
    exit 1
}

if [[ ${VERBOSE} -eq 1 ]]; then
    echo "Health check response:"
    echo "${RESPONSE}"
fi

if [[ ${JQ_AVAILABLE} -eq 1 ]]; then
    # Parse response
    STATUS=$(echo "${RESPONSE}" | jq -r '.status')
    GATEWAY_TYPE=$(echo "${RESPONSE}" | jq -r '.gateway')
    TIMESTAMP=$(echo "${RESPONSE}" | jq -r '.timestamp')

    if [[ "${STATUS}" == "healthy" ]]; then
        if [[ ${VERBOSE} -eq 1 ]]; then
            echo "Gateway: ${GATEWAY_TYPE}"
            echo "Timestamp: ${TIMESTAMP}"
        fi
        echo "Gateway is healthy"
        exit 0
    else
        echo "ERROR: Gateway status is ${STATUS}"
        exit 1
    fi
else
    # Simple check if response looks OK
    if echo "${RESPONSE}" | grep -q '"status":"healthy"'; then
        echo "Gateway is healthy"
        exit 0
    else
        echo "WARNING: Could not verify health status (jq not available)"
        exit 0
    fi
fi
```

---

## Rollback Decision Tree

```
                          Start Rollback Assessment
                                   │
                                   │ Is error rate > 1%?
                                   │
                    ┌──────────────┴──────────────┐
                    │ Yes                         │ No
                    ▼                             │
           Check P99 latency                     │
                    │                             │
                    │ Is P99 > 5ms?               │ Is memory leak detected?
                    │                             │
         ┌──────────┴──────────┐                  │
         │ Yes                │ No                │
         ▼                    ▼                   ▼
  Check auth failures  Check memory leak   Check authentication
         │                    │                   │
         │ Is auth > 2x baseline?            │ Is auth > 2x baseline?
         │                    │                   │
  ┌──────┴──────┐     ┌───────┴───────┐   ┌─────┴─────┐
  │ Yes        │ No  │ Yes           │ No │ Yes      │ No
  ▼            ▼     ▼               ▼     ▼          ▼
Immediate     Check Health Check
Rollback      Check Failed
              ┌────┴────┐
              │ Yes     │ No
              ▼         ▼
    Immediate  Check Authentication
    Rollback  Failures
              ┌────┴────┐
              │ Yes     │ No
              ▼         ▼
    Immediate  No Rollback Needed
    Rollback
```

### Rollback Decision Matrix

| Condition | Threshold | Duration | Action |
|-----------|-----------|----------|--------|
| Error rate | > 1% | 5 minutes | Immediate rollback |
| P99 latency | > 5ms | 5 minutes | Immediate rollback |
| P99 latency | > 1ms | 30 minutes | Alert team, monitor |
| Memory leak | > 50MB/hour | 10 minutes | Alert team, prepare rollback |
| Auth failures | > 2x baseline | 5 minutes | Immediate rollback |
| Health check | Failed | 1 minute | Immediate rollback |
| CPU usage | > 80% | 30 minutes | Alert team, monitor |

---

## Appendix A: Emergency Contact

**On-Call Engineering**: oncall@veloz.example.com
**Engineering Team**: engineering@veloz.example.com

---

## Appendix B: Migration Checklist

### Pre-Migration Checklist
- [ ] C++ Gateway built and tested
- [ ] Load balancer configured
- [ ] Monitoring dashboards deployed
- [ ] Alert rules configured
- [ ] Rollback scripts tested
- [ ] Team notified

### Phase 1 Checklist
- [ ] Deploy C++ Gateway to canary
- [ ] Configure 5% traffic split
- [ ] Verify health checks
- [ ] Monitor for 72 hours
- [ ] Collect performance data

### Phase 2 Checklist
- [ ] Increase traffic to 25%
- [ ] Monitor for 96 hours
- [ ] Verify order flow
- [ ] Test SSE stability

### Phase 3 Checklist
- [ ] Increase traffic to 50%
- [ ] Conduct load testing
- [ ] Test failover scenarios
- [ ] Monitor for 96 hours

### Phase 4 Checklist
- [ ] Increase traffic to 100%
- [ ] Monitor for 24 hours
- [ ] Verify all endpoints
- [ ] Prepare for decommission

### Phase 5 Checklist
- [ ] Stop Python Gateway
- [ ] Remove from load balancer
- [ ] Archive logs
- [ ] Update documentation

---

**Document History**:
- 2026-02-27: Initial version (v1.0)
