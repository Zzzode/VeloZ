# Latency Optimization Guide

This document describes the latency optimization techniques and time synchronization mechanisms implemented in VeloZ.

## Time Synchronization

### Overview

VeloZ implements a comprehensive time synchronization system to ensure accurate timestamps across all components. This is critical for:

- Order timing and sequencing
- Latency measurement accuracy
- Exchange time correlation
- Audit trail integrity

### Components

#### TimeSyncManager (`libs/core/include/veloz/core/time_sync.h`)

The central time synchronization manager provides:

1. **NTP Synchronization**: Periodic synchronization with NTP servers
2. **Exchange Time Calibration**: Per-exchange offset calibration
3. **Clock Drift Monitoring**: Continuous drift detection and compensation
4. **High-Resolution Timestamps**: Sub-microsecond precision using TSC (x86) or steady_clock

#### Configuration

```cpp
TimeSyncConfig config;
config.ntp_servers.add(kj::str("time.google.com"));
config.ntp_poll_interval_ms = 60000;  // 1 minute
config.max_offset_ns = 1000000;       // 1ms max offset alert
config.max_drift_ppm = 100.0;         // 100 ppm max drift

auto& time_sync = global_time_sync();
init_time_sync(config);
```

### Exchange Time Calibration

Each exchange may have clock skew relative to local time. VeloZ calibrates this automatically:

```cpp
// When receiving exchange server time
time_sync.calibrate_exchange(
    "binance",
    exchange_time_ns,
    local_time_ns,
    round_trip_ns
);

// Convert local time to exchange time
int64_t exchange_ts = time_sync.to_exchange_time("binance", local_time_ns);

// Convert exchange time to local time
int64_t local_ts = time_sync.from_exchange_time("binance", exchange_time_ns);
```

## Latency Profiling

### LatencyProfiler

Use `LatencyProfiler` to measure code path latencies:

```cpp
LatencyProfiler profiler("order_processing");

// Validation phase
validate_order(order);
profiler.checkpoint("validation");

// Risk check phase
check_risk(order);
profiler.checkpoint("risk_check");

// Execution phase
execute_order(order);
profiler.checkpoint("execution");

auto profile = profiler.finish();
KJ_LOG(INFO, profile.to_string());
```

Output:
```
Profile: order_processing (total: 1234567 ns)
  validation: 234567 ns (19%)
  risk_check: 456789 ns (37%)
  execution: 543211 ns (44%)
```

### ScopedLatency

For simple latency measurement with automatic recording:

```cpp
{
    ScopedLatency latency("database_query", [](int64_t ns) {
        histogram_observe("db_latency_ns", ns);
    });

    // ... database query code ...
}  // Callback invoked automatically with duration
```

## Critical Path Optimization

### Market Data Path

Target: P99 < 5ms for market data processing

Optimizations:
1. **Zero-copy parsing**: Use `kj::ArrayPtr` views instead of copying
2. **Pre-allocated buffers**: Use memory pools for event objects
3. **Lock-free queues**: Use `LockFreeQueue` for inter-thread communication
4. **Batch processing**: Process events in batches to amortize overhead

### Order Path

Target: P99 < 2ms for order placement

Optimizations:
1. **Pre-computed signatures**: Cache HMAC keys for exchange authentication
2. **Connection pooling**: Maintain persistent connections to exchanges
3. **Async I/O**: Use KJ async for non-blocking network operations
4. **Priority queues**: Use priority event loop for critical orders

## Memory Optimization

### Arena Allocator

Use `ArenaAllocator` for request-scoped allocations:

```cpp
void process_market_event(const MarketEvent& event) {
    ArenaAllocator arena(4096);  // 4KB arena

    // All allocations freed when arena goes out of scope
    auto& parsed = arena.allocate<ParsedEvent>();
    auto symbol = arena.copyString(event.symbol.value);

    // Process event...
}
```

### Memory Pool

Use `FixedSizeMemoryPool` for frequently allocated objects:

```cpp
// Create pool for order objects
FixedSizeMemoryPool<Order, 1024> order_pool(16);  // 16 blocks of 1024

// Allocate from pool (O(1) allocation)
auto order = order_pool.create();
order->symbol = "BTCUSDT";

// Automatically returned to pool when kj::Own goes out of scope
```

## Network Optimization

### Connection Management

1. **Keep-alive connections**: Maintain persistent WebSocket connections
2. **Connection pre-warming**: Establish connections before trading starts
3. **Failover pools**: Multiple connections per exchange for redundancy

### Message Optimization

1. **Binary protocols**: Use binary WebSocket frames where supported
2. **Compression**: Enable compression for REST API calls
3. **Batching**: Batch multiple orders in single requests where supported

## Monitoring

### Latency Metrics

VeloZ exposes latency metrics via Prometheus:

```
# Market data latency
veloz_market_data_latency_ns{venue="binance",quantile="0.99"} 2345678

# Order placement latency
veloz_order_latency_ns{venue="binance",quantile="0.99"} 1234567

# Time sync offset
veloz_time_sync_offset_ns{source="ntp"} 12345
```

### Latency Alerts

Configure alerts for latency SLO violations:

```yaml
# Prometheus alerting rule
- alert: HighOrderLatency
  expr: histogram_quantile(0.99, veloz_order_latency_ns) > 2000000
  for: 5m
  labels:
    severity: warning
  annotations:
    summary: "Order latency P99 exceeds 2ms"
```

## Performance Targets

| Component | P50 Target | P95 Target | P99 Target |
|-----------|------------|------------|------------|
| Market Data Processing | 1ms | 3ms | 5ms |
| Order Placement | 500us | 1ms | 2ms |
| Risk Check | 100us | 200us | 500us |
| Time Sync Offset | - | - | < 1ms |

## Benchmarking

Run the load test suite to validate performance:

```bash
# Quick validation (30 seconds)
./scripts/run_load_tests.sh release quick

# Full test (5 minutes per test)
./scripts/run_load_tests.sh release full

# Sustained test for memory leak detection
./scripts/run_load_tests.sh release sustained --hours 24
```

## Files

- `libs/core/include/veloz/core/time_sync.h` - Time synchronization interface
- `libs/core/src/time_sync.cpp` - Time synchronization implementation
- `libs/exec/include/veloz/exec/latency_tracker.h` - Per-venue latency tracking
- `tests/load/load_test_framework.h` - Load testing framework
- `docs/performance/benchmark_report.md` - Benchmark results
