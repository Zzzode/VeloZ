/**
 * @file benchmark_framework.h
 * @brief Lightweight performance benchmarking framework for VeloZ
 *
 * This file provides a simple benchmarking framework that integrates with
 * KJ patterns and generates performance reports. It measures:
 * - Throughput (operations per second)
 * - Latency (p50, p95, p99, max)
 * - Memory allocation patterns
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/string-tree.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::core::benchmark {

/**
 * @brief Latency statistics
 */
struct LatencyStats {
  double min_ns{0};
  double max_ns{0};
  double mean_ns{0};
  double p50_ns{0};
  double p95_ns{0};
  double p99_ns{0};
  double stddev_ns{0};
  uint64_t sample_count{0};
};

/**
 * @brief Benchmark result for a single benchmark
 */
struct BenchmarkResult {
  kj::String name;
  uint64_t iterations{0};
  double total_time_ns{0};
  double ops_per_sec{0};
  LatencyStats latency;

  [[nodiscard]] kj::String to_string() const {
    return kj::str("Benchmark: ", name, "\n", "  Iterations: ", iterations, "\n",
                   "  Total time: ", total_time_ns / 1e9, " s\n", "  Throughput: ", ops_per_sec,
                   " ops/sec\n", "  Latency:\n", "    Min:    ", latency.min_ns, " ns\n",
                   "    Mean:   ", latency.mean_ns, " ns\n", "    P50:    ", latency.p50_ns,
                   " ns\n", "    P95:    ", latency.p95_ns, " ns\n", "    P99:    ", latency.p99_ns,
                   " ns\n", "    Max:    ", latency.max_ns, " ns\n",
                   "    StdDev: ", latency.stddev_ns, " ns\n");
  }
};

/**
 * @brief Calculate latency statistics from samples
 */
inline LatencyStats calculate_latency_stats(kj::Vector<double>& samples) {
  LatencyStats stats;
  if (samples.size() == 0) {
    return stats;
  }

  stats.sample_count = samples.size();

  // Sort for percentile calculations
  std::sort(samples.begin(), samples.end());

  stats.min_ns = samples[0];
  stats.max_ns = samples[samples.size() - 1];

  // Calculate mean
  double sum = 0;
  for (double s : samples) {
    sum += s;
  }
  stats.mean_ns = sum / static_cast<double>(samples.size());

  // Calculate percentiles
  auto percentile = [&samples](double p) -> double {
    double idx = p * static_cast<double>(samples.size() - 1);
    size_t lower = static_cast<size_t>(idx);
    size_t upper = lower + 1;
    if (upper >= samples.size()) {
      return samples[samples.size() - 1];
    }
    double frac = idx - static_cast<double>(lower);
    return samples[lower] * (1.0 - frac) + samples[upper] * frac;
  };

  stats.p50_ns = percentile(0.50);
  stats.p95_ns = percentile(0.95);
  stats.p99_ns = percentile(0.99);

  // Calculate standard deviation
  double variance_sum = 0;
  for (double s : samples) {
    double diff = s - stats.mean_ns;
    variance_sum += diff * diff;
  }
  stats.stddev_ns = std::sqrt(variance_sum / static_cast<double>(samples.size()));

  return stats;
}

/**
 * @brief Benchmark runner class
 */
class Benchmark {
public:
  explicit Benchmark(kj::StringPtr name) : name_(kj::str(name)) {}

  /**
   * @brief Run benchmark with specified iterations
   * @param iterations Number of iterations to run
   * @param func Function to benchmark (takes iteration index)
   * @return Benchmark result
   */
  BenchmarkResult run(uint64_t iterations, kj::Function<void(uint64_t)> func) {
    BenchmarkResult result;
    result.name = kj::str(name_);
    result.iterations = iterations;

    kj::Vector<double> latencies;
    latencies.reserve(iterations);

    // Warmup (10% of iterations, min 10)
    uint64_t warmup_count = kj::max(iterations / 10, static_cast<uint64_t>(10));
    for (uint64_t i = 0; i < warmup_count; ++i) {
      func(i);
    }

    // Actual benchmark
    auto total_start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < iterations; ++i) {
      auto start = std::chrono::high_resolution_clock::now();
      func(i);
      auto end = std::chrono::high_resolution_clock::now();

      double elapsed_ns =
          static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
      latencies.add(elapsed_ns);
    }

    auto total_end = std::chrono::high_resolution_clock::now();

    result.total_time_ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count());
    result.ops_per_sec = static_cast<double>(iterations) / (result.total_time_ns / 1e9);
    result.latency = calculate_latency_stats(latencies);

    return result;
  }

  /**
   * @brief Run benchmark for a specified duration
   * @param duration_ms Duration to run in milliseconds
   * @param func Function to benchmark
   * @return Benchmark result
   */
  BenchmarkResult run_for_duration(uint64_t duration_ms, kj::Function<void()> func) {
    BenchmarkResult result;
    result.name = kj::str(name_);

    kj::Vector<double> latencies;

    // Warmup (100ms)
    auto warmup_end =
        std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(100);
    while (std::chrono::high_resolution_clock::now() < warmup_end) {
      func();
    }

    // Actual benchmark
    auto deadline =
        std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(duration_ms);
    auto total_start = std::chrono::high_resolution_clock::now();

    while (std::chrono::high_resolution_clock::now() < deadline) {
      auto start = std::chrono::high_resolution_clock::now();
      func();
      auto end = std::chrono::high_resolution_clock::now();

      double elapsed_ns =
          static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
      latencies.add(elapsed_ns);
    }

    auto total_end = std::chrono::high_resolution_clock::now();

    result.iterations = latencies.size();
    result.total_time_ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count());
    result.ops_per_sec = static_cast<double>(result.iterations) / (result.total_time_ns / 1e9);
    result.latency = calculate_latency_stats(latencies);

    return result;
  }

private:
  kj::String name_;
};

/**
 * @brief Benchmark suite for running multiple benchmarks
 */
class BenchmarkSuite {
public:
  explicit BenchmarkSuite(kj::StringPtr name) : name_(kj::str(name)) {}

  /**
   * @brief Add a benchmark result to the suite
   */
  void add_result(BenchmarkResult result) {
    results_.add(kj::mv(result));
  }

  /**
   * @brief Generate a report of all benchmark results
   */
  [[nodiscard]] kj::String generate_report() const {
    kj::StringTree tree = kj::strTree("================================================================================\n",
                                       "Performance Benchmark Report: ", name_, "\n",
                                       "================================================================================\n\n");

    for (const auto& result : results_) {
      tree = kj::strTree(kj::mv(tree), result.to_string(), "\n");
    }

    tree = kj::strTree(kj::mv(tree),
                       "================================================================================\n",
                       "Summary\n",
                       "================================================================================\n");

    // Summary table
    tree = kj::strTree(kj::mv(tree), "Benchmark                          | Throughput (ops/s) | P99 Latency (ns)\n",
                       "-----------------------------------|--------------------|-----------------\n");

    for (const auto& result : results_) {
      tree = kj::strTree(kj::mv(tree), result.name, " | ", result.ops_per_sec, " | ",
                         result.latency.p99_ns, "\n");
    }

    return tree.flatten();
  }

  /**
   * @brief Get all results
   */
  [[nodiscard]] const kj::Vector<BenchmarkResult>& results() const {
    return results_;
  }

private:
  kj::String name_;
  kj::Vector<BenchmarkResult> results_;
};

} // namespace veloz::core::benchmark
