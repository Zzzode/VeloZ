# VeloZ Failure Modes Documentation

This document describes known failure modes, their impact, and expected system behavior.

## Failure Categories

### 1. Network Failures

#### 1.1 Network Partition (Gateway <-> Engine)

**Scenario:** Gateway loses connectivity to the C++ engine process.

**Impact:**
- Order placement fails
- Position updates stop
- Market data may become stale

**Expected Behavior:**
- Gateway returns 503 Service Unavailable
- Health check fails
- Automatic reconnection attempts
- Orders in-flight may be in unknown state

**Recovery:**
- Engine process restart
- Gateway reconnects automatically
- Manual order reconciliation may be needed

**Mitigation:**
- Heartbeat monitoring
- Automatic engine restart
- Order state persistence

---

#### 1.2 Network Latency (> 500ms)

**Scenario:** High latency between gateway and exchange APIs.

**Impact:**
- Order execution delays
- Stale market data
- Potential for slippage

**Expected Behavior:**
- Orders still execute but with delay
- Timeouts may occur for long operations
- Rate limiting may be affected

**Recovery:**
- Automatic when latency resolves
- No manual intervention needed

**Mitigation:**
- Timeout configuration
- Circuit breaker for exchange calls
- Local caching of market data

---

#### 1.3 DNS Resolution Failure

**Scenario:** DNS lookup for exchange APIs fails.

**Impact:**
- Cannot reach exchange APIs
- All trading operations fail

**Expected Behavior:**
- Connection errors logged
- Graceful degradation to cached data
- Health check shows degraded

**Recovery:**
- Automatic when DNS resolves
- May need container restart if DNS cache is stale

**Mitigation:**
- DNS caching
- Multiple DNS servers
- IP-based fallback (not recommended for production)

---

### 2. Container/Process Failures

#### 2.1 Gateway Container Crash

**Scenario:** Gateway container terminates unexpectedly.

**Impact:**
- All API requests fail
- SSE connections drop
- In-flight orders may be orphaned

**Expected Behavior:**
- Kubernetes/Docker restarts container
- Health checks detect failure
- Alerts triggered

**Recovery:**
- Automatic restart (< 30s typical)
- State restored from persistence
- Clients reconnect

**Mitigation:**
- Multiple replicas (HA mode)
- Persistent order state
- Client retry logic

---

#### 2.2 Engine Process Crash

**Scenario:** C++ engine process crashes.

**Impact:**
- Order processing stops
- Position calculations unavailable
- Risk checks fail

**Expected Behavior:**
- Gateway detects engine failure
- Returns 503 for trading operations
- Attempts automatic restart

**Recovery:**
- Engine restart (< 10s typical)
- State restored from last checkpoint
- Order reconciliation with exchange

**Mitigation:**
- Engine watchdog
- State persistence
- Graceful degradation in gateway

---

#### 2.3 OOM (Out of Memory) Kill

**Scenario:** Container killed due to memory limit.

**Impact:**
- Same as container crash
- May indicate memory leak

**Expected Behavior:**
- Container restart
- Memory usage logged before kill

**Recovery:**
- Automatic restart
- Investigate memory usage patterns

**Mitigation:**
- Appropriate memory limits
- Memory monitoring/alerting
- Regular profiling

---

### 3. Resource Exhaustion

#### 3.1 CPU Saturation

**Scenario:** CPU usage at 100% sustained.

**Impact:**
- Increased latency
- Timeouts possible
- Degraded throughput

**Expected Behavior:**
- Requests still processed but slower
- Health checks may timeout
- Autoscaling triggered (if configured)

**Recovery:**
- Automatic when load decreases
- May need horizontal scaling

**Mitigation:**
- CPU limits and requests
- Horizontal pod autoscaling
- Load shedding

---

#### 3.2 Memory Pressure

**Scenario:** Memory usage approaching limits.

**Impact:**
- GC pauses (Python)
- Potential OOM kill
- Degraded performance

**Expected Behavior:**
- Increased latency
- Memory warnings in logs
- May trigger OOM kill

**Recovery:**
- Restart if OOM killed
- Automatic GC if not killed

**Mitigation:**
- Memory monitoring
- Appropriate limits
- Memory-efficient code

---

#### 3.3 Disk Full

**Scenario:** Log volume fills up.

**Impact:**
- Cannot write logs
- May affect state persistence
- Container may become unhealthy

**Expected Behavior:**
- Log rotation should prevent
- Errors if rotation fails

**Recovery:**
- Log cleanup
- Increase volume size

**Mitigation:**
- Log rotation
- Volume monitoring
- Retention policies

---

### 4. External Service Failures

#### 4.1 Exchange API Unavailable

**Scenario:** Binance API returns errors or is unreachable.

**Impact:**
- Cannot place/cancel orders
- Market data unavailable
- Position sync fails

**Expected Behavior:**
- Graceful degradation
- Cached data used where possible
- Clear error messages to clients

**Recovery:**
- Automatic when exchange recovers
- May need order reconciliation

**Mitigation:**
- Circuit breaker
- Retry with backoff
- Fallback to cached data
- Multi-exchange support

---

#### 4.2 Exchange Rate Limiting

**Scenario:** Hit exchange API rate limits.

**Impact:**
- Requests rejected (429)
- Temporary trading pause

**Expected Behavior:**
- Backoff and retry
- Rate limit headers respected
- Queue requests if possible

**Recovery:**
- Automatic after cooldown
- Typically 1-5 minutes

**Mitigation:**
- Request rate limiting
- Request prioritization
- Efficient API usage

---

#### 4.3 Database Connection Loss

**Scenario:** Cannot connect to PostgreSQL.

**Impact:**
- Order persistence fails
- Audit logging fails
- State recovery impaired

**Expected Behavior:**
- In-memory operation continues
- Errors logged
- Reconnection attempts

**Recovery:**
- Automatic reconnection
- State sync when restored

**Mitigation:**
- Connection pooling
- Retry logic
- In-memory fallback

---

### 5. Security Failures

#### 5.1 Vault Unavailable

**Scenario:** Cannot reach HashiCorp Vault.

**Impact:**
- Cannot rotate secrets
- New instances cannot start

**Expected Behavior:**
- Use cached credentials
- Fallback to environment variables
- Warnings logged

**Recovery:**
- Automatic when Vault recovers
- May need manual secret injection

**Mitigation:**
- Credential caching
- Environment variable fallback
- Multiple Vault instances

---

## Failure Response Matrix

| Failure | Detection Time | Recovery Time | Manual Action |
|---------|---------------|---------------|---------------|
| Network Partition | < 30s | < 60s | None |
| Gateway Crash | < 10s | < 30s | None |
| Engine Crash | < 5s | < 10s | None |
| OOM Kill | Immediate | < 30s | Investigate |
| Exchange Down | < 30s | Variable | Monitor |
| Database Down | < 30s | < 60s | None |
| Vault Down | < 60s | Variable | May need secrets |

## Monitoring Alerts

Each failure mode should trigger appropriate alerts:

| Failure | Alert Severity | Alert Channel |
|---------|---------------|---------------|
| Container Crash | Critical | PagerDuty |
| Network Partition | High | Slack + PagerDuty |
| Exchange Down | High | Slack |
| High Latency | Warning | Slack |
| Resource Pressure | Warning | Slack |
| Rate Limiting | Info | Slack |

## Testing Schedule

| Experiment | Frequency | Environment |
|------------|-----------|-------------|
| Pod Kill | Weekly | Staging |
| Network Latency | Weekly | Staging |
| CPU Stress | Monthly | Staging |
| Exchange Failure | Monthly | Staging |
| Full DR Test | Quarterly | Production |
