# Performance Tuning

**Time:** 35 minutes | **Level:** Intermediate

## What You'll Learn

- Identify performance bottlenecks using built-in metrics
- Optimize network latency for exchange connections
- Tune memory allocation and object pooling
- Configure CPU affinity and thread pinning
- Measure and reduce order-to-fill latency
- Apply kernel and system-level optimizations

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Linux server access (for kernel tuning)
- Basic understanding of latency concepts

---

## Step 1: Establish Baseline Metrics

Before optimizing, measure current performance to establish a baseline.

```bash
# Get current performance metrics
curl http://127.0.0.1:8080/api/metrics/latency
```

**Expected Output:**
```json
{
  "latency": {
    "order_to_ack": {
      "p50_us": 1500,
      "p95_us": 3200,
      "p99_us": 5800,
      "max_us": 12000
    },
    "order_to_fill": {
      "p50_us": 45000,
      "p95_us": 85000,
      "p99_us": 150000,
      "max_us": 250000
    },
    "market_data": {
      "exchange_to_engine_p50_us": 800,
      "exchange_to_engine_p95_us": 1500,
      "processing_p50_us": 50,
      "processing_p99_us": 200
    }
  },
  "throughput": {
    "orders_per_second": 45,
    "market_updates_per_second": 500
  }
}
```

Record these values for comparison after optimization.

---

## Step 2: Identify Bottlenecks

Use the profiling endpoint to identify where time is spent.

```bash
# Get detailed timing breakdown
curl http://127.0.0.1:8080/api/metrics/profile
```

**Expected Output:**
```json
{
  "order_path": {
    "api_receive_us": 50,
    "validation_us": 100,
    "risk_check_us": 200,
    "serialization_us": 150,
    "network_send_us": 800,
    "exchange_processing_us": 1200,
    "network_receive_us": 600,
    "total_us": 3100
  },
  "market_data_path": {
    "websocket_receive_us": 100,
    "deserialization_us": 80,
    "order_book_update_us": 50,
    "strategy_notify_us": 120,
    "total_us": 350
  },
  "bottlenecks": [
    {
      "component": "network_send",
      "pct_of_total": 25.8,
      "recommendation": "Check network configuration"
    },
    {
      "component": "risk_check",
      "pct_of_total": 6.5,
      "recommendation": "Consider async risk checks"
    }
  ]
}
```

Focus optimization efforts on the largest contributors.

---

## Step 3: Optimize Network Configuration

Reduce network latency with TCP tuning.

### Check Current Network Settings

```bash
# View current TCP settings
sysctl net.ipv4.tcp_nodelay
sysctl net.core.rmem_max
sysctl net.core.wmem_max
```

### Apply Network Optimizations

```bash
# Enable TCP_NODELAY (disable Nagle's algorithm)
sudo sysctl -w net.ipv4.tcp_nodelay=1

# Increase socket buffer sizes
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# Enable TCP timestamps for better RTT estimation
sudo sysctl -w net.ipv4.tcp_timestamps=1

# Reduce TCP connection timeout
sudo sysctl -w net.ipv4.tcp_fin_timeout=15
```

### Configure VeloZ Network Settings

```bash
# Set VeloZ network options
export VELOZ_TCP_NODELAY=true
export VELOZ_SOCKET_RECV_BUFFER=16777216
export VELOZ_SOCKET_SEND_BUFFER=16777216
export VELOZ_KEEPALIVE_INTERVAL=10

# Restart gateway
./scripts/run_gateway.sh dev
```

---

## Step 4: Tune Memory Allocation

Configure memory pools to reduce allocation overhead.

```bash
# Configure memory pool
curl -X POST http://127.0.0.1:8080/api/config/memory \
  -H "Content-Type: application/json" \
  -d '{
    "order_pool_size": 10000,
    "market_event_pool_size": 50000,
    "string_pool_size_mb": 64,
    "preallocate": true
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "memory_config": {
    "order_pool_size": 10000,
    "market_event_pool_size": 50000,
    "string_pool_size_mb": 64,
    "preallocate": true
  },
  "memory_usage": {
    "order_pool_mb": 8.5,
    "market_event_pool_mb": 24.0,
    "string_pool_mb": 64.0,
    "total_preallocated_mb": 96.5
  }
}
```

### Check Memory Pool Efficiency

```bash
# Get memory pool statistics
curl http://127.0.0.1:8080/api/metrics/memory
```

**Expected Output:**
```json
{
  "pools": {
    "order_pool": {
      "capacity": 10000,
      "in_use": 45,
      "peak_usage": 230,
      "allocations": 15000,
      "pool_hits_pct": 99.8,
      "heap_fallbacks": 30
    },
    "market_event_pool": {
      "capacity": 50000,
      "in_use": 120,
      "peak_usage": 5000,
      "allocations": 500000,
      "pool_hits_pct": 100.0,
      "heap_fallbacks": 0
    }
  },
  "heap": {
    "allocated_mb": 128.5,
    "resident_mb": 156.0,
    "fragmentation_pct": 2.3
  }
}
```

Target `pool_hits_pct` > 99% to minimize heap allocations.

---

## Step 5: Configure CPU Affinity

Pin critical threads to specific CPU cores for consistent performance.

```bash
# Check current CPU topology
lscpu | grep -E "^CPU\(s\)|Thread|Core|Socket"
```

**Expected Output:**
```
CPU(s):              8
Thread(s) per core:  2
Core(s) per socket:  4
Socket(s):           1
```

### Configure Thread Pinning

```bash
# Set CPU affinity for VeloZ
export VELOZ_CPU_AFFINITY=true
export VELOZ_NETWORK_THREAD_CORES=0,1
export VELOZ_ENGINE_THREAD_CORES=2,3
export VELOZ_STRATEGY_THREAD_CORES=4,5

# Restart gateway
./scripts/run_gateway.sh dev
```

### Verify Thread Assignment

```bash
# Check thread CPU affinity
curl http://127.0.0.1:8080/api/metrics/threads
```

**Expected Output:**
```json
{
  "threads": [
    {
      "name": "network-io",
      "tid": 12345,
      "cpu_affinity": [0, 1],
      "cpu_time_ms": 45000,
      "context_switches": 12000
    },
    {
      "name": "engine-main",
      "tid": 12346,
      "cpu_affinity": [2, 3],
      "cpu_time_ms": 120000,
      "context_switches": 5000
    },
    {
      "name": "strategy-worker",
      "tid": 12347,
      "cpu_affinity": [4, 5],
      "cpu_time_ms": 30000,
      "context_switches": 8000
    }
  ]
}
```

> **Tip:** Lower context switches indicate better thread isolation.

---

## Step 6: Optimize Order Processing

Reduce order processing latency with configuration tuning.

```bash
# Configure order processing options
curl -X POST http://127.0.0.1:8080/api/config/order_processing \
  -H "Content-Type: application/json" \
  -d '{
    "batch_size": 1,
    "async_risk_check": false,
    "skip_redundant_validation": true,
    "use_binary_protocol": true
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "order_processing": {
    "batch_size": 1,
    "async_risk_check": false,
    "skip_redundant_validation": true,
    "use_binary_protocol": true
  }
}
```

### Processing Options Explained

| Option | Value | Impact |
|--------|-------|--------|
| `batch_size` | 1 | Process orders immediately (no batching) |
| `async_risk_check` | false | Synchronous for lowest latency |
| `skip_redundant_validation` | true | Skip re-validation of known-good orders |
| `use_binary_protocol` | true | Use binary instead of JSON for exchange |

---

## Step 7: Tune Market Data Processing

Optimize market data handling for faster strategy updates.

```bash
# Configure market data processing
curl -X POST http://127.0.0.1:8080/api/config/market_data \
  -H "Content-Type: application/json" \
  -d '{
    "update_throttle_us": 0,
    "order_book_depth": 5,
    "conflate_updates": false,
    "use_incremental_updates": true
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "market_data_config": {
    "update_throttle_us": 0,
    "order_book_depth": 5,
    "conflate_updates": false,
    "use_incremental_updates": true
  }
}
```

### Market Data Options

| Option | Low Latency | High Throughput |
|--------|-------------|-----------------|
| `update_throttle_us` | 0 | 1000 |
| `order_book_depth` | 5 | 20 |
| `conflate_updates` | false | true |
| `use_incremental_updates` | true | true |

---

## Step 8: Apply Kernel Optimizations

For production systems, apply kernel-level tuning.

### Disable CPU Frequency Scaling

```bash
# Set CPU governor to performance mode
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  echo performance | sudo tee $cpu
done

# Verify
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

### Disable Transparent Huge Pages

```bash
# Disable THP (can cause latency spikes)
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag
```

### Set Process Priority

```bash
# Run VeloZ with real-time priority
sudo chrt -f 50 ./build/release/apps/engine/veloz_engine --port 8080
```

> **Warning:** Real-time priority requires careful testing. A bug can lock up the system.

---

## Step 9: Measure Improvement

After applying optimizations, measure the new baseline.

```bash
# Get updated latency metrics
curl http://127.0.0.1:8080/api/metrics/latency
```

**Expected Output (after optimization):**
```json
{
  "latency": {
    "order_to_ack": {
      "p50_us": 800,
      "p95_us": 1500,
      "p99_us": 2800,
      "max_us": 5000
    },
    "order_to_fill": {
      "p50_us": 35000,
      "p95_us": 65000,
      "p99_us": 120000,
      "max_us": 180000
    },
    "market_data": {
      "exchange_to_engine_p50_us": 500,
      "exchange_to_engine_p95_us": 900,
      "processing_p50_us": 30,
      "processing_p99_us": 100
    }
  },
  "throughput": {
    "orders_per_second": 85,
    "market_updates_per_second": 800
  }
}
```

### Compare Results

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Order-to-ack p50 | 1500 us | 800 us | 47% |
| Order-to-ack p99 | 5800 us | 2800 us | 52% |
| Market data p50 | 800 us | 500 us | 38% |
| Orders/second | 45 | 85 | 89% |

---

## Step 10: Set Up Continuous Monitoring

Configure alerts for performance degradation.

```bash
# Configure latency alerts
curl -X POST http://127.0.0.1:8080/api/alerts/latency \
  -H "Content-Type: application/json" \
  -d '{
    "alerts": [
      {
        "name": "order_latency_high",
        "metric": "order_to_ack_p99_us",
        "threshold": 5000,
        "duration_seconds": 60,
        "action": "alert"
      },
      {
        "name": "market_data_stale",
        "metric": "market_data_age_ms",
        "threshold": 100,
        "duration_seconds": 10,
        "action": "alert"
      }
    ]
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "alerts_configured": 2
}
```

### Subscribe to Performance Events

```bash
# Monitor performance in real-time
curl -N "http://127.0.0.1:8080/api/stream?filter=performance"
```

**Expected Output:**
```
event: latency_update
data: {"order_to_ack_p50_us":820,"order_to_ack_p99_us":2750}

event: throughput_update
data: {"orders_per_second":82,"market_updates_per_second":790}

event: performance_alert
data: {"name":"order_latency_high","current":5200,"threshold":5000}
```

---

## Summary

**What you accomplished:**
- Established baseline performance metrics
- Identified bottlenecks using profiling data
- Optimized network configuration for lower latency
- Tuned memory pools to reduce allocation overhead
- Configured CPU affinity for consistent performance
- Applied order and market data processing optimizations
- Implemented kernel-level tuning for production
- Set up continuous performance monitoring

## Troubleshooting

### Issue: Latency spikes after optimization
**Symptom:** Occasional high latency despite good averages
**Solution:** Check for garbage collection or memory pressure:
```bash
curl http://127.0.0.1:8080/api/metrics/memory
```
Increase pool sizes if `heap_fallbacks` > 0.

### Issue: CPU affinity not taking effect
**Symptom:** Threads not pinned to specified cores
**Solution:** Verify permissions and core availability:
```bash
taskset -cp $(pgrep veloz_engine)
```
Ensure specified cores exist and are not isolated.

### Issue: Network latency unchanged
**Symptom:** No improvement after TCP tuning
**Solution:** Verify settings are applied:
```bash
sysctl net.ipv4.tcp_nodelay
```
Check if VeloZ is using the tuned sockets.

### Issue: Throughput decreased after optimization
**Symptom:** Lower orders/second despite lower latency
**Solution:** Check if batch_size=1 is causing overhead:
```bash
curl -X POST http://127.0.0.1:8080/api/config/order_processing \
  -H "Content-Type: application/json" \
  -d '{"batch_size": 10}'
```
Balance latency vs throughput based on your use case.

## Next Steps

- [Troubleshooting Common Issues](./troubleshooting-practice.md) - Debug performance problems
- [Production Deployment](./production-deployment.md) - Deploy optimized configuration
- [Monitoring Guide](../guides/user/monitoring.md) - Set up performance dashboards
- [Latency Optimization Guide](../performance/latency_optimization.md) - Advanced optimization techniques
- [Glossary](../guides/user/glossary.md) - Definitions of technical terms
