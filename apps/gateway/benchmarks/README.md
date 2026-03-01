# VeloZ Gateway Performance Benchmarks

This directory contains comprehensive performance benchmarks for the VeloZ C++ Gateway.

## Overview

The benchmark suite measures:

- **Request Latency**: P50, P90, P99, P99.9 percentiles
- **Throughput**: Maximum requests per second
- **SSE Performance**: Event broadcast latency
- **Authentication Overhead**: JWT and API key verification
- **Memory Usage**: Memory per request
- **Startup Time**: Time to initialize the gateway

## Performance Targets

| Metric | Target | Python Baseline | Improvement |
|--------|--------|-----------------|-------------|
| Latency (P50) | <100μs | ~700μs | 7x |
| Latency (P99) | <1ms | ~10ms | 10x |
| Throughput | >10K req/s | ~1K req/s | 10x |
| SSE Connections | >1000 | ~100 | 10x |
| Memory per req | <1KB | N/A | - |
| Startup Time | <100ms | ~2s | 20x |

## Building Benchmarks

Enable benchmark building with CMake:

```bash
cmake -DVELOZ_BUILD_GATEWAY_BENCHMARKS=ON --preset release
cmake --build --preset release-all -j$(nproc)
```

## Running Benchmarks

### Run benchmarks directly:

```bash
./build/release/apps/gateway_cpp/veloz_gateway_benchmarks
```

### Run using CMake target:

```bash
cmake --build --preset release --target run_gateway_benchmarks
```

### Generate full report with analysis:

```bash
cmake --build --preset release --target benchmark_report
```

The report will be generated in `build/release/apps/gateway_cpp/benchmark_report/`

## Benchmark Categories

### 1. Latency Benchmarks

- **Route Lookup**: Measures route matching performance
- **Authentication (JWT)**: Measures JWT token verification
- **Full Request**: Measures complete request processing pipeline

### 2. Throughput Benchmarks

- **Max Throughput**: Measures maximum requests per second
- **Throughput Scaling**: Tests scalability with concurrent requests

### 3. SSE Benchmarks

- **Event Broadcast**: Measures event distribution latency to subscribers

### 4. Memory & Startup

- **Memory Per Request**: Estimates memory allocation per request
- **Startup Time**: Measures gateway initialization time

## Benchmark Configuration

Edit `benchmark_config.ini` to adjust:

- Performance targets
- Iteration counts
- Concurrency levels
- Output formats

```ini
[targets]
latency_p50_us = 100
throughput_min_rps = 10000

[benchmark_config]
latency_iterations = 50000
concurrency_levels = [1, 4, 8, 16, 32]
```

## Output Format

Benchmarks output results in multiple formats:

### Text Output

```
=== Full Request Latency ===
Iterations: 50000
Total time: 42.35 ms
Latency:
  P50:     85 μs   ✓ (target: <100μs)
  P99:     850 μs  ✓ (target: <1ms)
```

### JSON Output

```json
{
  "results": {
    "full_request": {
      "stats": {
        "p50_us": 85,
        "p99_us": 850
      }
    }
  }
}
```

## Benchmark Report

Use the Python report generator for detailed analysis:

```bash
python3 benchmarks/benchmark_report.py \
  --input benchmark_output.txt \
  --output reports \
  --format all \
  --analyze
```

This generates:
- `report.txt`: Human-readable summary
- `report.json`: Machine-readable results
- Bottleneck analysis (if `--analyze` is specified)

## Continuous Integration

Benchmarks run automatically in CI on:
- Every pull request
- Merge to main branch
- Release candidates

CI will fail if any performance target is not met.

## Interpreting Results

### Target Status

- ✓ PASS: Target met or exceeded
- ✗ FAIL: Target not met

### Percentiles

- **P50 (Median)**: 50% of requests complete faster than this
- **P90**: 90% of requests complete faster than this
- **P99**: 99% of requests complete faster than this
- **P99.9**: 99.9% of requests complete faster than this

### Bottleneck Analysis

Run with `--analyze` to identify potential bottlenecks:

```bash
python3 benchmarks/benchmark_report.py --analyze
```

Example output:

```
=== Bottleneck Analysis ===

Authentication:
  Issue: JWT verification takes significant portion of request time
  Severity: Medium
  Recommendation: Consider token caching or shorter tokens
```

## Tips for Accurate Results

1. **Run in Release mode**: Use optimized builds for accurate performance data
2. **Minimize background processes**: Close unnecessary applications
3. **Multiple runs**: Run benchmarks 3-5 times and average results
4. **Warm-up**: First few iterations are warm-up and excluded from results
5. **Consistent environment**: Use same machine, OS version, and settings

## Troubleshooting

### High latency

- Check for debug builds (use Release)
- Verify KJ library is optimized
- Check for CPU throttling
- Reduce background load

### Low throughput

- Check scaling with concurrency levels
- Look for lock contention
- Verify event loop efficiency
- Check network stack settings

### Out of memory

- Reduce iteration count in `benchmark_config.ini`
- Check for memory leaks
- Verify cleanup code in handlers

## Contributing

When adding new benchmarks:

1. Follow existing patterns in `benchmark_gateway.cpp`
2. Add performance targets to `benchmark_config.ini`
3. Update CMakeLists.txt if new files are added
4. Update this README with documentation
5. Run CI to verify targets are met

## References

- [KJ Library Performance Guide](../../.claude/skills/kj-library/library_usage_guide.md)
- [C++23 Performance Optimization](https://en.cppreference.com/w/cpp/utility/feature_test)
- [HTTP Benchmarking Best Practices](https://github.com/gperftools/gperftools/wiki)
