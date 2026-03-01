# VeloZ Performance Benchmark Report

## Overview

This document presents baseline performance metrics for VeloZ core components. The benchmarks measure throughput (operations per second) and latency (p50, p95, p99) for critical paths.

## Test Environment

- Platform: macOS (Darwin)
- Build: Debug with -O2 optimization for benchmarks
- CPU: Apple Silicon (arm64)

## Benchmark Results Summary

| Component | Throughput (ops/s) | P99 Latency (ns) | Notes |
|-----------|-------------------|------------------|-------|
| JSON Parse (small) | 1.86M | 708 | 4 fields |
| JSON Parse (medium) | 1.01M | 1,083 | Nested object |
| JSON Parse (large) | 545K | 1,958 | Arrays with 20 elements |
| JSON Build (small) | 1.06M | 1,000 | 4 fields |
| JSON Build (nested) | 1.08M | 1,000 | Nested object |
| LockFreeQueue Push | 20.2M | 42 | Single-threaded |
| LockFreeQueue Pop | 22.1M | 42 | Single-threaded |
| LockFreeQueue Push+Pop | 17.2M | 42 | Single-threaded |
| LockFreeQueue Concurrent | 13.7K | 117K | 4 threads (2 prod, 2 cons) |
| MemoryPool Allocate | 4.2M | 292 | Fixed-size pool |
| Heap Allocate | 28.4M | 42 | Standard kj::heap |
| ArenaAllocator | 2.4M | 459 | 10 allocations per iteration |
| ArenaAllocator String | 1.6M | 709 | String copy |
| EventLoop Post | 4.1K | 309K | 100 tasks per iteration |
| EventLoop Priority | 2.8K | 1.9M | Mixed priorities |

## Detailed Analysis

### JSON Performance

The yyjson-based JSON implementation provides excellent performance:

- **Parsing**: Small JSON messages (typical market data) parse at 1.86M ops/sec with sub-microsecond P99 latency
- **Building**: JSON construction is equally fast at ~1M ops/sec
- **Recommendation**: Current performance is suitable for high-frequency market data processing

### Lock-Free Queue

The Michael-Scott queue implementation shows strong single-threaded performance:

- **Single-threaded**: 20M+ ops/sec for push/pop operations
- **Concurrent**: Performance drops to ~14K ops/sec with 4 threads due to contention
- **Bottleneck**: Concurrent performance is limited by CAS contention under high load
- **Recommendation**: Consider batching for high-throughput concurrent scenarios

### Memory Management

Memory allocation patterns show interesting trade-offs:

- **Heap allocation**: Fastest at 28M ops/sec (kj::heap is highly optimized)
- **Memory pool**: 4.2M ops/sec - slower than heap but provides deterministic allocation
- **Arena allocator**: 2.4M ops/sec for bulk allocations, ideal for request-scoped memory
- **Recommendation**: Use arena allocator for per-request memory, heap for long-lived objects

### Event Loop

The KJ-based event loop shows moderate throughput:

- **Basic posting**: ~4K iterations/sec (each iteration posts 100 tasks)
- **Priority posting**: ~2.8K iterations/sec with priority overhead
- **Bottleneck**: Thread synchronization and KJ event loop overhead
- **Recommendation**: Batch events where possible, use priority sparingly

## Identified Bottlenecks

1. **Event Loop Synchronization**: Cross-thread wake-up adds latency
2. **Lock-Free Queue Contention**: CAS operations under high concurrency
3. **Memory Pool Locking**: kj::MutexGuarded adds overhead vs raw heap

## Performance Targets

For production readiness, the following targets are recommended:

| Component | Target Throughput | Target P99 Latency |
|-----------|------------------|-------------------|
| JSON Parse | > 1M ops/sec | < 2us |
| Order Processing | > 100K ops/sec | < 10us |
| Event Loop | > 10K events/sec | < 1ms |
| WebSocket Message | > 50K msgs/sec | < 100us |

## Running Benchmarks

```bash
# Run benchmarks with dev preset
./scripts/run_benchmarks.sh dev

# Run benchmarks with release preset (recommended for accurate metrics)
./scripts/run_benchmarks.sh release
```

## Files

- `libs/core/benchmarks/benchmark_framework.h` - Benchmark framework
- `libs/core/benchmarks/benchmark_core.cpp` - Core component benchmarks
- `scripts/run_benchmarks.sh` - Benchmark runner script
