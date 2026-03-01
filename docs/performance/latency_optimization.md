# Performance Tuning Guide

This comprehensive guide covers all aspects of performance optimization in VeloZ, including latency, memory, CPU, network, and caching strategies.

## Table of Contents

1. [Overview](#overview)
2. [Time Synchronization](#time-synchronization)
3. [Latency Optimization](#latency-optimization)
4. [Memory Optimization](#memory-optimization)
5. [CPU Optimization](#cpu-optimization)
6. [Network Optimization](#network-optimization)
7. [Database Optimization](#database-optimization)
8. [Caching Strategies](#caching-strategies)
9. [Performance Monitoring](#performance-monitoring)
10. [Benchmarking](#benchmarking)
11. [Profiling](#profiling)
12. [Performance Targets](#performance-targets)

---

## Overview

VeloZ is designed for low-latency trading operations. This guide provides techniques and configurations to optimize performance across all system components.

### Key Performance Principles

| Principle | Description |
|-----------|-------------|
| **Minimize allocations** | Use memory pools and arenas for hot paths |
| **Avoid locks** | Use lock-free data structures where possible |
| **Batch operations** | Amortize overhead across multiple operations |
| **Pre-compute** | Cache computed values (signatures, hashes) |
| **Zero-copy** | Use views (`kj::ArrayPtr`, `kj::StringPtr`) instead of copies |

### Quick Configuration Reference

```bash
# High-performance configuration
export VELOZ_WORKER_THREADS=8
export VELOZ_EVENT_LOOP_THREADS=4
export VELOZ_MEMORY_POOL_SIZE=1073741824  # 1GB
export VELOZ_ORDER_RATE_LIMIT=100
export VELOZ_LOG_LEVEL=WARN
export VELOZ_WAL_SYNC_MODE=async
```

---

## Time Synchronization

Accurate time synchronization is critical for order timing, latency measurement, and exchange time correlation.

### TimeSyncManager

The central time synchronization manager (`libs/core/include/veloz/core/time_sync.h`) provides:

1. **NTP Synchronization**: Periodic synchronization with NTP servers
2. **Exchange Time Calibration**: Per-exchange offset calibration
3. **Clock Drift Monitoring**: Continuous drift detection and compensation
4. **High-Resolution Timestamps**: Sub-microsecond precision using TSC (x86) or steady_clock

### Configuration

```cpp
TimeSyncConfig config;
config.ntp_servers.add(kj::str("time.google.com"));
config.ntp_servers.add(kj::str("time.cloudflare.com"));
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

### Time Source Priority

| Source | Accuracy | Use Case |
|--------|----------|----------|
| PTP | < 1us | Co-located servers |
| GPS | < 100us | Dedicated trading infrastructure |
| NTP | < 10ms | Standard deployments |
| System | Variable | Fallback only |

---

## Latency Optimization

### Latency Profiling

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

### Per-Venue Latency Tracking

The `LatencyTracker` (`libs/exec/include/veloz/exec/latency_tracker.h`) monitors round-trip times per exchange:

```cpp
LatencyTracker tracker;

// Record latency measurement
tracker.record_latency(Venue::Binance, latency, timestamp);

// Get statistics
KJ_IF_SOME(stats, tracker.get_stats(Venue::Binance)) {
    KJ_LOG(INFO, "Binance P99:", stats.p99);
}

// Get venues sorted by latency (fastest first)
auto venues = tracker.get_venues_by_latency();
```

### Critical Path Optimization

#### Market Data Path

Target: P99 < 5ms for market data processing

| Optimization | Technique | Impact |
|--------------|-----------|--------|
| Zero-copy parsing | Use `kj::ArrayPtr` views | 30-50% faster |
| Pre-allocated buffers | Memory pools for events | 20-40% faster |
| Lock-free queues | `LockFreeQueue` for IPC | 50-80% faster |
| Batch processing | Process events in batches | 40-60% faster |

```cpp
// Zero-copy parsing example
void process_market_data(kj::ArrayPtr<const kj::byte> data) {
    // Parse directly from buffer without copying
    auto price = parse_price(data.slice(0, 8));
    auto qty = parse_qty(data.slice(8, 16));
    // ...
}
```

#### Order Path

Target: P99 < 2ms for order placement

| Optimization | Technique | Impact |
|--------------|-----------|--------|
| Pre-computed signatures | Cache HMAC keys | 60-80% faster |
| Connection pooling | Persistent connections | 40-60% faster |
| Async I/O | KJ async operations | 30-50% faster |
| Priority queues | Priority event loop | Ensures SLA |

```cpp
// Pre-computed signature example
class SignatureCache {
    kj::HashMap<kj::String, kj::Array<kj::byte>> cached_keys_;

public:
    kj::ArrayPtr<const kj::byte> get_signing_key(kj::StringPtr api_secret) {
        return cached_keys_.findOrCreate(api_secret, [&]() {
            return compute_hmac_key(api_secret);
        });
    }
};
```

---

## Memory Optimization

### Memory Pool Architecture

VeloZ provides multiple memory management strategies optimized for different use cases.

### FixedSizeMemoryPool

Use for frequently allocated objects of the same type:

```cpp
// Create pool for order objects
FixedSizeMemoryPool<Order, 1024> order_pool(16);  // 16 blocks of 1024

// Allocate from pool (O(1) allocation)
auto order = order_pool.create();
order->symbol = "BTCUSDT";

// Automatically returned to pool when kj::Own goes out of scope
```

**Configuration:**

| Parameter | Description | Recommendation |
|-----------|-------------|----------------|
| `BlockSize` | Objects per block | 64-1024 |
| `initial_blocks` | Pre-allocated blocks | Based on expected load |
| `max_blocks` | Maximum blocks (0=unlimited) | Set for memory limits |

### ArenaAllocator

Use for request-scoped allocations where all objects have the same lifetime:

```cpp
void process_market_event(const MarketEvent& event) {
    ArenaAllocator arena(4096);  // 4KB arena

    // All allocations freed when arena goes out of scope
    auto& parsed = arena.allocate<ParsedEvent>();
    auto symbol = arena.copyString(event.symbol.value);

    // Process event...
}
```

**Benefits:**
- O(1) allocation (bump pointer)
- O(1) deallocation (single free)
- Excellent cache locality
- No fragmentation

### ResettableArenaPool

For batch processing patterns:

```cpp
ResettableArenaPool pool(8192);  // 8KB chunks

// Process batch
for (const auto& event : events) {
    auto& data = pool.allocate<EventData>(event);
    process(data);
}

// Reset for next batch (all memory freed)
pool.reset();
```

### Lock-Free Node Pool

For concurrent allocation without locks:

```cpp
LockFreeNodePool<MarketEvent> pool;

// Thread-safe allocation
auto* node = pool.allocate();
node->construct(symbol, price, qty);

// Thread-safe deallocation
pool.deallocate(node);
```

### Memory Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_MEMORY_POOL_SIZE` | 512MB | Total memory pool size |

**Workload-Based Sizing:**

| Workload | Memory Pool | Arena Size |
|----------|-------------|------------|
| Light (< 10 orders/s) | 256MB | 4KB |
| Medium (10-100 orders/s) | 512MB | 8KB |
| Heavy (> 100 orders/s) | 1GB+ | 16KB |

### Memory Monitoring

```cpp
// Get global memory statistics
auto& stats = global_memory_stats();
KJ_LOG(INFO, "Allocated:", stats.total_allocated());
KJ_LOG(INFO, "Peak:", stats.peak_allocated());

// Monitor specific allocation sites
MemoryMonitor& monitor = global_memory_monitor();
monitor.track_allocation("order_pool", sizeof(Order), 1);

// Generate report
auto report = monitor.generate_report();
KJ_LOG(INFO, report);
```

---

## CPU Optimization

### Thread Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WORKER_THREADS` | 4 | Worker thread count |
| `VELOZ_EVENT_LOOP_THREADS` | 2 | Event loop thread count |

**Recommendations:**

| CPU Cores | Worker Threads | Event Loop Threads |
|-----------|----------------|-------------------|
| 2-4 | 2 | 1 |
| 4-8 | 4 | 2 |
| 8-16 | 8 | 4 |
| 16+ | 12-16 | 4-8 |

### Lock-Free Data Structures

VeloZ uses lock-free structures to minimize contention:

#### LockFreeQueue (MPMC)

Based on Michael-Scott algorithm with tagged pointers:

```cpp
LockFreeQueue<MarketEvent> queue;

// Producer thread
queue.push(MarketEvent{symbol, price, qty});

// Consumer thread
KJ_IF_SOME(event, queue.pop()) {
    process(event);
}
```

**Features:**
- Cache-line aligned head/tail (prevents false sharing)
- Tagged pointers (solves ABA problem)
- Node pooling (reduces allocations)

#### HierarchicalTimerWheel

O(1) timer scheduling and firing:

```cpp
HierarchicalTimerWheel wheel;

// Schedule timer (O(1))
auto id = wheel.schedule(100, []() {
    // Timer callback
});

// Advance time and fire expired timers
size_t fired = wheel.tick();
```

**Timer Wheel Levels:**

| Level | Resolution | Range |
|-------|------------|-------|
| 0 | 1ms | 256ms |
| 1 | 256ms | ~65s |
| 2 | ~65s | ~4.6 hours |
| 3 | ~4.6 hours | ~49 days |

### OptimizedEventLoop

The optimized event loop combines lock-free queues with hierarchical timer wheels:

```cpp
OptimizedEventLoop loop;

// Post task (lock-free)
loop.post([]() { process_order(); });

// Post with priority
loop.post([]() { critical_task(); }, EventPriority::High);

// Post delayed task (O(1) scheduling)
loop.post_delayed([]() { cleanup(); }, 1000ms);

// Run event loop
loop.run();
```

### CPU Affinity

For latency-critical deployments, pin threads to specific cores:

```bash
# Pin VeloZ to cores 0-3
taskset -c 0-3 ./veloz_engine

# Or use isolcpus kernel parameter
# Add to /etc/default/grub: GRUB_CMDLINE_LINUX="isolcpus=0-3"
```

---

## Network Optimization

### Connection Management

1. **Keep-alive connections**: Maintain persistent WebSocket connections
2. **Connection pre-warming**: Establish connections before trading starts
3. **Failover pools**: Multiple connections per exchange for redundancy

```cpp
// Connection pool configuration
ConnectionPoolConfig config;
config.min_connections = 2;
config.max_connections = 10;
config.idle_timeout_ms = 30000;
config.connect_timeout_ms = 5000;
```

### Message Optimization

| Technique | Description | Impact |
|-----------|-------------|--------|
| Binary protocols | Use binary WebSocket frames | 20-40% smaller |
| Compression | Enable for REST API calls | 50-70% smaller |
| Batching | Batch multiple orders | Reduced overhead |

### WebSocket Configuration

```cpp
WebSocketConfig ws_config;
ws_config.ping_interval_ms = 30000;
ws_config.pong_timeout_ms = 10000;
ws_config.reconnect_delay_ms = 1000;
ws_config.max_reconnect_attempts = 5;
ws_config.message_buffer_size = 65536;
```

### TCP Tuning

For Linux systems, optimize TCP settings:

```bash
# /etc/sysctl.conf
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
net.ipv4.tcp_nodelay = 1
net.ipv4.tcp_low_latency = 1
```

### Network Latency Monitoring

```cpp
// Monitor network latency per venue
LatencyTracker tracker;

// Check venue health
bool healthy = tracker.is_healthy(
    Venue::Binance,
    10 * kj::MILLISECONDS,  // max acceptable latency
    60 * kj::SECONDS        // max staleness
);

// Get venues sorted by latency
auto venues = tracker.get_venues_by_latency();
```

---

## Database Optimization

### WAL Configuration

VeloZ uses Write-Ahead Logging for durability:

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WAL_ENABLED` | true | Enable WAL |
| `VELOZ_WAL_SYNC_MODE` | fsync | Sync mode |
| `VELOZ_WAL_MAX_SIZE_MB` | 100 | Max file size |
| `VELOZ_WAL_CHECKPOINT_INTERVAL` | 60 | Checkpoint interval (seconds) |

**Sync Modes:**

| Mode | Durability | Performance | Use Case |
|------|------------|-------------|----------|
| `fsync` | High | Lower | Production |
| `async` | Lower | Higher | Development, backtesting |

### Query Optimization

1. **Index frequently queried fields**: order_id, symbol, timestamp
2. **Use prepared statements**: Avoid query parsing overhead
3. **Batch inserts**: Group multiple inserts into transactions
4. **Connection pooling**: Reuse database connections

```cpp
// Batch insert example
void batch_insert_orders(const kj::Vector<Order>& orders) {
    auto txn = db.begin_transaction();
    for (const auto& order : orders) {
        txn.insert(order);
    }
    txn.commit();  // Single commit for all inserts
}
```

### Database Connection Pool

```bash
export VELOZ_CONNECTION_POOL_SIZE=10
export VELOZ_REQUEST_TIMEOUT_MS=30000
```

---

## Caching Strategies

### Signature Cache

Pre-compute and cache HMAC signatures:

```cpp
class SignatureCache {
    kj::HashMap<kj::String, SignedRequest> cache_;
    kj::MutexGuarded<CacheState> guarded_;

public:
    kj::StringPtr get_signature(kj::StringPtr payload, kj::StringPtr secret) {
        auto lock = guarded_.lockExclusive();
        return lock->cache.findOrCreate(payload, [&]() {
            return compute_hmac_sha256(payload, secret);
        });
    }
};
```

### Market Data Cache

Cache recent market data for quick access:

```cpp
class MarketDataCache {
    static constexpr size_t MAX_ENTRIES = 10000;
    kj::HashMap<kj::String, MarketData> cache_;

public:
    void update(kj::StringPtr symbol, const MarketData& data) {
        cache_.upsert(symbol, data);
        if (cache_.size() > MAX_ENTRIES) {
            evict_oldest();
        }
    }

    kj::Maybe<const MarketData&> get(kj::StringPtr symbol) const {
        return cache_.find(symbol);
    }
};
```

### Order Book Cache

Maintain local order book snapshots:

```cpp
class OrderBookCache {
    struct CachedBook {
        kj::Vector<PriceLevel> bids;
        kj::Vector<PriceLevel> asks;
        uint64_t last_update_id;
    };

    kj::HashMap<kj::String, CachedBook> books_;

public:
    void apply_delta(kj::StringPtr symbol, const OrderBookDelta& delta) {
        auto& book = books_.findOrCreate(symbol, []() {
            return CachedBook{};
        });
        // Apply incremental update
        apply_update(book, delta);
    }
};
```

### Cache Invalidation

| Strategy | Description | Use Case |
|----------|-------------|----------|
| TTL | Time-based expiration | Market data |
| LRU | Least recently used | General caching |
| Event-driven | Invalidate on updates | Order state |

---

## Performance Monitoring

### Prometheus Metrics

VeloZ exposes latency metrics via Prometheus:

```
# Market data latency
veloz_market_data_latency_ns{venue="binance",quantile="0.99"} 2345678

# Order placement latency
veloz_order_latency_ns{venue="binance",quantile="0.99"} 1234567

# Time sync offset
veloz_time_sync_offset_ns{source="ntp"} 12345

# Memory pool usage
veloz_memory_pool_used_bytes 536870912

# Event loop queue depth
veloz_event_loop_tasks 42
```

### Latency Alerts

Configure alerts for latency SLO violations:

```yaml
# Prometheus alerting rule
groups:
  - name: veloz_latency
    rules:
      - alert: HighOrderLatency
        expr: histogram_quantile(0.99, veloz_order_latency_ns) > 2000000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Order latency P99 exceeds 2ms"

      - alert: HighMarketDataLatency
        expr: histogram_quantile(0.99, veloz_market_data_latency_ns) > 5000000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Market data latency P99 exceeds 5ms"
```

### Real-Time Monitoring Dashboard

Key metrics to display:

| Panel | Metric | Target |
|-------|--------|--------|
| Order Latency P99 | `veloz_order_latency_ns{quantile="0.99"}` | < 2ms |
| Market Data Latency P99 | `veloz_market_data_latency_ns{quantile="0.99"}` | < 5ms |
| Memory Usage | `veloz_memory_pool_used_bytes` | < 80% of pool |
| Event Queue Depth | `veloz_event_loop_tasks` | < 1000 |
| Time Sync Offset | `veloz_time_sync_offset_ns` | < 1ms |

---

## Benchmarking

### Load Test Suite

Run the load test suite to validate performance:

```bash
# Quick validation (30 seconds)
./scripts/run_load_tests.sh release quick

# Full test (5 minutes per test)
./scripts/run_load_tests.sh release full

# Sustained test for memory leak detection
./scripts/run_load_tests.sh release sustained --hours 24

# Stress test (maximum throughput)
./scripts/run_load_tests.sh release stress

# Gateway API load test
./scripts/run_load_tests.sh dev gateway --gateway-url http://localhost:8080
```

### Test Modes

| Mode | Duration | Purpose |
|------|----------|---------|
| `quick` | 30 seconds | Quick validation |
| `full` | 5 minutes | Comprehensive testing |
| `sustained` | 1-24 hours | Memory leak detection |
| `stress` | Variable | Maximum throughput |
| `gateway` | Variable | API endpoint testing |

### Benchmark Metrics

```cpp
// Example benchmark output
struct BenchmarkResults {
    uint64_t total_operations;
    double operations_per_second;
    double avg_latency_ns;
    double p50_latency_ns;
    double p95_latency_ns;
    double p99_latency_ns;
    double max_latency_ns;
    size_t memory_used_bytes;
};
```

### Micro-Benchmarks

```cpp
// Memory pool benchmark
void benchmark_memory_pool() {
    FixedSizeMemoryPool<Order, 1024> pool(16);
    Timer timer;

    for (int i = 0; i < 1000000; ++i) {
        auto obj = pool.create();
        // Use object
    }

    auto elapsed = timer.elapsed_ns();
    KJ_LOG(INFO, "Allocations/sec:", 1000000.0 / (elapsed.count() / 1e9));
}

// Lock-free queue benchmark
void benchmark_lockfree_queue() {
    LockFreeQueue<int> queue;
    Timer timer;

    for (int i = 0; i < 1000000; ++i) {
        queue.push(i);
        queue.pop();
    }

    auto elapsed = timer.elapsed_ns();
    KJ_LOG(INFO, "Operations/sec:", 2000000.0 / (elapsed.count() / 1e9));
}
```

---

## Profiling

### CPU Profiling

#### Using perf (Linux)

```bash
# Record CPU profile
perf record -g ./veloz_engine

# Generate report
perf report

# Flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

#### Using Instruments (macOS)

```bash
# CPU profiling
xcrun xctrace record --template "Time Profiler" --launch ./veloz_engine

# Memory profiling
xcrun xctrace record --template "Allocations" --launch ./veloz_engine
```

### Memory Profiling

#### Using AddressSanitizer

```bash
# Build with ASan
cmake --preset asan
cmake --build --preset asan-all

# Run with ASan
./build/asan/apps/engine/veloz_engine
```

#### Using Valgrind

```bash
# Memory leak detection
valgrind --leak-check=full ./veloz_engine

# Cache profiling
valgrind --tool=cachegrind ./veloz_engine
```

### Latency Profiling

```cpp
// Instrument critical paths
void process_order(const Order& order) {
    LatencyProfiler profiler("process_order");

    validate(order);
    profiler.checkpoint("validate");

    check_risk(order);
    profiler.checkpoint("risk_check");

    execute(order);
    profiler.checkpoint("execute");

    auto profile = profiler.finish();

    // Log if exceeds threshold
    if (profile.total_ns > 1000000) {  // > 1ms
        KJ_LOG(WARNING, "Slow order processing:", profile.to_string());
    }
}
```

### Continuous Profiling

For production environments, use continuous profiling:

```bash
# Enable profiling metrics
export VELOZ_PROFILING_ENABLED=true
export VELOZ_PROFILING_SAMPLE_RATE=100  # 1% sampling
```

---

## Performance Targets

### Latency Targets

| Component | P50 Target | P95 Target | P99 Target |
|-----------|------------|------------|------------|
| Market Data Processing | 1ms | 3ms | 5ms |
| Order Placement | 500us | 1ms | 2ms |
| Risk Check | 100us | 200us | 500us |
| Time Sync Offset | - | - | < 1ms |

### Throughput Targets

| Component | Target |
|-----------|--------|
| Market events/second | 10,000+ |
| Orders/second | 100+ |
| WebSocket messages/second | 1,000+ |

### Resource Targets

| Resource | Target |
|----------|--------|
| Memory usage | < 80% of pool |
| CPU usage | < 70% average |
| Network bandwidth | < 50% capacity |
| Event queue depth | < 1000 |

---

## Related Documentation

- [Configuration Guide](../guides/user/configuration.md) - Performance environment variables
- [Monitoring Guide](../guides/user/monitoring.md) - Metrics and observability
- [Troubleshooting Guide](../guides/user/troubleshooting.md) - Performance issues
- [Architecture Overview](../design/README.md) - System design

---

## Files Reference

| File | Description |
|------|-------------|
| `libs/core/include/veloz/core/time_sync.h` | Time synchronization interface |
| `libs/core/include/veloz/core/memory_pool.h` | Memory pool implementations |
| `libs/core/include/veloz/core/memory.h` | Arena allocators and utilities |
| `libs/core/include/veloz/core/lockfree_queue.h` | Lock-free MPMC queue |
| `libs/core/include/veloz/core/timer_wheel.h` | Hierarchical timer wheel |
| `libs/core/include/veloz/core/optimized_event_loop.h` | Optimized event loop |
| `libs/core/include/veloz/core/metrics.h` | Performance metrics |
| `libs/exec/include/veloz/exec/latency_tracker.h` | Per-venue latency tracking |
| `scripts/run_load_tests.sh` | Load testing script |
