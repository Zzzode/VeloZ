/**
 * @file load_test_framework.h
 * @brief Comprehensive load testing framework for VeloZ
 *
 * This framework provides:
 * - Realistic market data generation (1000+ symbols, 100k+ events/sec)
 * - Order placement throughput and latency testing
 * - P50/P95/P99 latency validation
 * - Sustained load testing for memory leak detection
 * - Performance report generation
 */

#pragma once

#include "veloz/common/types.h"
#include "veloz/core/metrics.h"
#include "veloz/exec/order_api.h"
#include "veloz/market/market_event.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>
#include <kj/array.h>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/string-tree.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <random>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace veloz::loadtest {

// ============================================================================
// Performance Targets (from design requirements)
// ============================================================================

struct PerformanceTargets {
  // Market data processing targets
  double market_data_p50_us{5000.0};   // 5ms P50
  double market_data_p95_us{10000.0};  // 10ms P95
  double market_data_p99_us{20000.0};  // 20ms P99
  double market_data_throughput{100000.0}; // 100k events/sec

  // Order path targets
  double order_path_p50_us{1000.0};    // 1ms P50
  double order_path_p95_us{1500.0};    // 1.5ms P95
  double order_path_p99_us{2000.0};    // 2ms P99
  double order_throughput{10000.0};    // 10k orders/sec

  // Memory targets
  size_t max_memory_growth_mb{100};    // Max 100MB growth over baseline
  double max_memory_growth_pct{10.0};  // Max 10% growth
};

// ============================================================================
// Latency Histogram with High Resolution
// ============================================================================

class LatencyHistogram {
public:
  static constexpr size_t NUM_BUCKETS = 1000;
  static constexpr double MAX_LATENCY_US = 100000.0; // 100ms max

  LatencyHistogram() : buckets_(kj::heapArray<std::atomic<uint64_t>>(NUM_BUCKETS)) {
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      buckets_[i].store(0, std::memory_order_relaxed);
    }
  }

  void record(double latency_us) {
    size_t bucket = static_cast<size_t>(latency_us / (MAX_LATENCY_US / NUM_BUCKETS));
    if (bucket >= NUM_BUCKETS) {
      bucket = NUM_BUCKETS - 1;
    }
    buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
    count_.fetch_add(1, std::memory_order_relaxed);
    sum_.fetch_add(latency_us, std::memory_order_relaxed);

    // Track min/max with CAS
    double current_min = min_.load(std::memory_order_relaxed);
    while (latency_us < current_min &&
           !min_.compare_exchange_weak(current_min, latency_us, std::memory_order_relaxed)) {
    }

    double current_max = max_.load(std::memory_order_relaxed);
    while (latency_us > current_max &&
           !max_.compare_exchange_weak(current_max, latency_us, std::memory_order_relaxed)) {
    }
  }

  [[nodiscard]] double percentile(double p) const {
    uint64_t total = count_.load(std::memory_order_relaxed);
    if (total == 0) return 0.0;

    uint64_t target = static_cast<uint64_t>(p * static_cast<double>(total));
    uint64_t cumulative = 0;

    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      cumulative += buckets_[i].load(std::memory_order_relaxed);
      if (cumulative >= target) {
        return static_cast<double>(i) * (MAX_LATENCY_US / NUM_BUCKETS);
      }
    }
    return MAX_LATENCY_US;
  }

  [[nodiscard]] double p50() const { return percentile(0.50); }
  [[nodiscard]] double p95() const { return percentile(0.95); }
  [[nodiscard]] double p99() const { return percentile(0.99); }
  [[nodiscard]] double mean() const {
    uint64_t c = count_.load(std::memory_order_relaxed);
    return c > 0 ? sum_.load(std::memory_order_relaxed) / static_cast<double>(c) : 0.0;
  }
  [[nodiscard]] double min() const { return min_.load(std::memory_order_relaxed); }
  [[nodiscard]] double max() const { return max_.load(std::memory_order_relaxed); }
  [[nodiscard]] uint64_t count() const { return count_.load(std::memory_order_relaxed); }

  void reset() {
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      buckets_[i].store(0, std::memory_order_relaxed);
    }
    count_.store(0, std::memory_order_relaxed);
    sum_.store(0.0, std::memory_order_relaxed);
    min_.store(MAX_LATENCY_US, std::memory_order_relaxed);
    max_.store(0.0, std::memory_order_relaxed);
  }

private:
  kj::Array<std::atomic<uint64_t>> buckets_;
  std::atomic<uint64_t> count_{0};
  std::atomic<double> sum_{0.0};
  std::atomic<double> min_{MAX_LATENCY_US};
  std::atomic<double> max_{0.0};
};

// ============================================================================
// Market Data Generator Config
// ============================================================================

struct MarketDataGeneratorConfig {
  size_t num_symbols = 1000;
  double base_price = 50000.0;
  double price_volatility = 0.001; // 0.1% per tick
  double trade_probability = 0.3;
  double book_update_probability = 0.7;
  size_t book_depth = 20;
};

// ============================================================================
// Market Data Generator
// ============================================================================

class MarketDataGenerator {
public:
  using Config = MarketDataGeneratorConfig;

  explicit MarketDataGenerator(Config config = {})
      : config_(config), rng_(std::random_device{}()),
        price_dist_(0.0, config.price_volatility),
        uniform_dist_(0.0, 1.0),
        qty_dist_(0.001, 10.0) {
    // Initialize symbols
    symbols_.reserve(config_.num_symbols);
    prices_.reserve(config_.num_symbols);

    for (size_t i = 0; i < config_.num_symbols; ++i) {
      symbols_.add(kj::str("SYM", i, "USDT"));
      prices_.add(config_.base_price * (0.5 + uniform_dist_(rng_)));
    }
  }

  /**
   * @brief Generate a random market event
   */
  market::MarketEvent generate() {
    size_t symbol_idx = static_cast<size_t>(uniform_dist_(rng_) * static_cast<double>(config_.num_symbols));
    if (symbol_idx >= config_.num_symbols) {
      symbol_idx = config_.num_symbols - 1;
    }

    // Update price with random walk
    double price_change = prices_[symbol_idx] * price_dist_(rng_);
    prices_[symbol_idx] += price_change;
    if (prices_[symbol_idx] < 0.01) {
      prices_[symbol_idx] = 0.01;
    }

    market::MarketEvent event;
    event.venue = common::Venue::Binance;
    event.market = common::MarketKind::Spot;
    event.symbol = common::SymbolId(symbols_[symbol_idx]);

    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    event.ts_exchange_ns = ns - 1000000; // 1ms ago
    event.ts_recv_ns = ns - 500000;      // 0.5ms ago
    event.ts_pub_ns = ns;

    if (uniform_dist_(rng_) < config_.trade_probability) {
      event.type = market::MarketEventType::Trade;
      market::TradeData trade;
      trade.price = prices_[symbol_idx];
      trade.qty = qty_dist_(rng_);
      trade.is_buyer_maker = uniform_dist_(rng_) < 0.5;
      trade.trade_id = trade_id_++;
      event.data = trade;
    } else {
      event.type = market::MarketEventType::BookTop;
      market::BookData book;
      double spread = prices_[symbol_idx] * 0.0001; // 0.01% spread
      book.bids.add(market::BookLevel{prices_[symbol_idx] - spread / 2, qty_dist_(rng_)});
      book.asks.add(market::BookLevel{prices_[symbol_idx] + spread / 2, qty_dist_(rng_)});
      book.sequence = sequence_++;
      book.is_snapshot = false;
      event.data = kj::mv(book);
    }

    return event;
  }

  /**
   * @brief Generate a batch of events
   */
  kj::Vector<market::MarketEvent> generate_batch(size_t count) {
    kj::Vector<market::MarketEvent> events;
    events.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      events.add(generate());
    }
    return events;
  }

  [[nodiscard]] size_t num_symbols() const { return config_.num_symbols; }

private:
  Config config_;
  std::mt19937_64 rng_;
  std::normal_distribution<double> price_dist_;
  std::uniform_real_distribution<double> uniform_dist_;
  std::uniform_real_distribution<double> qty_dist_;
  kj::Vector<kj::String> symbols_;
  kj::Vector<double> prices_;
  std::atomic<int64_t> trade_id_{1};
  std::atomic<int64_t> sequence_{1};
};

// ============================================================================
// Order Generator Config
// ============================================================================

struct OrderGeneratorConfig {
  size_t num_symbols = 100;
  double base_price = 50000.0;
  double price_range = 0.01; // +/- 1% from base
  double min_qty = 0.001;
  double max_qty = 1.0;
};

// ============================================================================
// Order Generator
// ============================================================================

class OrderGenerator {
public:
  using Config = OrderGeneratorConfig;

  explicit OrderGenerator(Config config = {})
      : config_(config), rng_(std::random_device{}()),
        price_dist_(1.0 - config.price_range, 1.0 + config.price_range),
        qty_dist_(config.min_qty, config.max_qty),
        uniform_dist_(0.0, 1.0) {
    // Initialize symbols
    symbols_.reserve(config_.num_symbols);
    for (size_t i = 0; i < config_.num_symbols; ++i) {
      symbols_.add(kj::str("SYM", i, "USDT"));
    }
  }

  /**
   * @brief Generate a random order request
   */
  exec::PlaceOrderRequest generate() {
    size_t symbol_idx = static_cast<size_t>(uniform_dist_(rng_) * static_cast<double>(config_.num_symbols));
    if (symbol_idx >= config_.num_symbols) {
      symbol_idx = config_.num_symbols - 1;
    }

    exec::PlaceOrderRequest request;
    request.client_order_id = kj::str("LOAD_", order_id_++);
    request.symbol = common::SymbolId(symbols_[symbol_idx]);
    request.side = uniform_dist_(rng_) < 0.5 ? exec::OrderSide::Buy : exec::OrderSide::Sell;
    request.type = exec::OrderType::Limit;
    request.tif = exec::TimeInForce::GTC;
    request.qty = qty_dist_(rng_);
    request.price = config_.base_price * price_dist_(rng_);

    return request;
  }

  /**
   * @brief Generate a batch of orders
   */
  kj::Vector<exec::PlaceOrderRequest> generate_batch(size_t count) {
    kj::Vector<exec::PlaceOrderRequest> orders;
    orders.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      orders.add(generate());
    }
    return orders;
  }

private:
  Config config_;
  std::mt19937_64 rng_;
  std::uniform_real_distribution<double> price_dist_;
  std::uniform_real_distribution<double> qty_dist_;
  std::uniform_real_distribution<double> uniform_dist_;
  kj::Vector<kj::String> symbols_;
  std::atomic<uint64_t> order_id_{1};
};

// ============================================================================
// Load Test Result
// ============================================================================

struct LoadTestResult {
  kj::String test_name;
  bool passed{false};

  // Throughput metrics
  double events_per_sec{0.0};
  double orders_per_sec{0.0};
  uint64_t total_events{0};
  uint64_t total_orders{0};

  // Latency metrics (microseconds)
  double latency_p50_us{0.0};
  double latency_p95_us{0.0};
  double latency_p99_us{0.0};
  double latency_mean_us{0.0};
  double latency_min_us{0.0};
  double latency_max_us{0.0};

  // Memory metrics
  size_t memory_start_mb{0};
  size_t memory_end_mb{0};
  double memory_growth_pct{0.0};

  // Error metrics
  uint64_t errors{0};
  double error_rate{0.0};

  // Duration
  double duration_sec{0.0};

  [[nodiscard]] kj::String to_string() const {
    return kj::str(
        "================================================================================\n",
        "Load Test: ", test_name, "\n",
        "================================================================================\n",
        "Status: ", passed ? "PASSED" : "FAILED", "\n\n",
        "Throughput:\n",
        "  Events/sec:  ", events_per_sec, "\n",
        "  Orders/sec:  ", orders_per_sec, "\n",
        "  Total Events: ", total_events, "\n",
        "  Total Orders: ", total_orders, "\n\n",
        "Latency (microseconds):\n",
        "  P50:  ", latency_p50_us, "\n",
        "  P95:  ", latency_p95_us, "\n",
        "  P99:  ", latency_p99_us, "\n",
        "  Mean: ", latency_mean_us, "\n",
        "  Min:  ", latency_min_us, "\n",
        "  Max:  ", latency_max_us, "\n\n",
        "Memory:\n",
        "  Start:  ", memory_start_mb, " MB\n",
        "  End:    ", memory_end_mb, " MB\n",
        "  Growth: ", memory_growth_pct, "%\n\n",
        "Errors:\n",
        "  Count: ", errors, "\n",
        "  Rate:  ", error_rate * 100.0, "%\n\n",
        "Duration: ", duration_sec, " seconds\n"
    );
  }

  [[nodiscard]] kj::String to_json() const {
    return kj::str(
        "{",
        "\"test_name\":\"", test_name, "\",",
        "\"passed\":", passed ? "true" : "false", ",",
        "\"events_per_sec\":", events_per_sec, ",",
        "\"orders_per_sec\":", orders_per_sec, ",",
        "\"total_events\":", total_events, ",",
        "\"total_orders\":", total_orders, ",",
        "\"latency_p50_us\":", latency_p50_us, ",",
        "\"latency_p95_us\":", latency_p95_us, ",",
        "\"latency_p99_us\":", latency_p99_us, ",",
        "\"latency_mean_us\":", latency_mean_us, ",",
        "\"latency_min_us\":", latency_min_us, ",",
        "\"latency_max_us\":", latency_max_us, ",",
        "\"memory_start_mb\":", memory_start_mb, ",",
        "\"memory_end_mb\":", memory_end_mb, ",",
        "\"memory_growth_pct\":", memory_growth_pct, ",",
        "\"errors\":", errors, ",",
        "\"error_rate\":", error_rate, ",",
        "\"duration_sec\":", duration_sec,
        "}"
    );
  }
};

// ============================================================================
// Memory Tracker
// ============================================================================

class MemoryTracker {
public:
  MemoryTracker() : baseline_mb_(get_current_memory_mb()) {}

  [[nodiscard]] size_t get_current_memory_mb() const {
#if defined(__APPLE__)
    // macOS: use task_info
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
      return info.resident_size / (1024 * 1024);
    }
#elif defined(__linux__)
    // Linux: read /proc/self/statm
    std::ifstream statm("/proc/self/statm");
    if (statm) {
      size_t size, resident;
      statm >> size >> resident;
      return (resident * sysconf(_SC_PAGESIZE)) / (1024 * 1024);
    }
#endif
    return 0;
  }

  [[nodiscard]] size_t baseline_mb() const { return baseline_mb_; }
  [[nodiscard]] size_t current_mb() const { return get_current_memory_mb(); }
  [[nodiscard]] int64_t growth_mb() const {
    return static_cast<int64_t>(current_mb()) - static_cast<int64_t>(baseline_mb_);
  }
  [[nodiscard]] double growth_pct() const {
    if (baseline_mb_ == 0) return 0.0;
    return 100.0 * static_cast<double>(growth_mb()) / static_cast<double>(baseline_mb_);
  }

  void reset_baseline() { baseline_mb_ = get_current_memory_mb(); }

private:
  size_t baseline_mb_;
};

// ============================================================================
// Load Test Runner Config
// ============================================================================

struct LoadTestRunnerConfig {
  // Test duration
  uint64_t duration_sec = 60;

  // Target rates
  double target_events_per_sec = 100000.0;
  double target_orders_per_sec = 1000.0;

  // Generator configs
  MarketDataGenerator::Config market_config;
  OrderGenerator::Config order_config;

  // Performance targets
  PerformanceTargets targets;

  // Reporting
  uint64_t report_interval_sec = 10;
};

// ============================================================================
// Load Test Runner
// ============================================================================

class LoadTestRunner {
public:
  using EventHandler = kj::Function<void(const market::MarketEvent&)>;
  using OrderHandler = kj::Function<void(const exec::PlaceOrderRequest&)>;
  using Config = LoadTestRunnerConfig;

  explicit LoadTestRunner(Config config = {})
      : config_(config),
        market_gen_(config.market_config),
        order_gen_(config.order_config) {}

  /**
   * @brief Run market data throughput test
   */
  LoadTestResult run_market_data_test(EventHandler handler) {
    LoadTestResult result;
    result.test_name = kj::str("Market Data Throughput (", config_.target_events_per_sec, " events/sec)");

    MemoryTracker memory;
    result.memory_start_mb = memory.baseline_mb();

    LatencyHistogram latency;
    std::atomic<uint64_t> event_count{0};
    std::atomic<uint64_t> error_count{0};
    std::atomic<bool> running{true};

    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::seconds(config_.duration_sec);

    // Calculate events per batch and sleep time
    constexpr size_t BATCH_SIZE = 1000;
    double batches_per_sec = config_.target_events_per_sec / BATCH_SIZE;
    auto sleep_time = std::chrono::microseconds(
        static_cast<int64_t>(1000000.0 / batches_per_sec));

    // Progress reporter thread
    kj::Own<kj::Thread> reporter = kj::heap<kj::Thread>([this, &event_count, &latency, &running]() {
      while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.report_interval_sec));
        if (!running.load(std::memory_order_relaxed)) break;
        KJ_LOG(INFO, "Progress: ", event_count.load(), " events, ",
               "P99: ", latency.p99(), " us");
      }
    });

    // Main event generation loop
    while (std::chrono::high_resolution_clock::now() < deadline) {
      auto batch = market_gen_.generate_batch(BATCH_SIZE);
      auto batch_start = std::chrono::high_resolution_clock::now();

      for (auto& event : batch) {
        try {
          auto op_start = std::chrono::high_resolution_clock::now();
          handler(event);
          auto op_end = std::chrono::high_resolution_clock::now();

          double latency_us = static_cast<double>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0;
          latency.record(latency_us);
          event_count.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
          error_count.fetch_add(1, std::memory_order_relaxed);
        }
      }

      // Rate limiting
      auto batch_end = std::chrono::high_resolution_clock::now();
      auto batch_duration = batch_end - batch_start;
      if (batch_duration < sleep_time) {
        std::this_thread::sleep_for(sleep_time - batch_duration);
      }
    }

    running.store(false, std::memory_order_relaxed);
    reporter = nullptr; // Join reporter thread

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_sec = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.0;

    // Populate results
    result.total_events = event_count.load();
    result.events_per_sec = static_cast<double>(result.total_events) / result.duration_sec;
    result.latency_p50_us = latency.p50();
    result.latency_p95_us = latency.p95();
    result.latency_p99_us = latency.p99();
    result.latency_mean_us = latency.mean();
    result.latency_min_us = latency.min();
    result.latency_max_us = latency.max();
    result.errors = error_count.load();
    result.error_rate = result.total_events > 0 ?
        static_cast<double>(result.errors) / static_cast<double>(result.total_events) : 0.0;
    result.memory_end_mb = memory.current_mb();
    result.memory_growth_pct = memory.growth_pct();

    // Validate against targets
    result.passed = (result.latency_p99_us <= config_.targets.market_data_p99_us) &&
                    (result.events_per_sec >= config_.targets.market_data_throughput * 0.9) &&
                    (result.memory_growth_pct <= config_.targets.max_memory_growth_pct);

    return result;
  }

  /**
   * @brief Run order throughput test
   */
  LoadTestResult run_order_test(OrderHandler handler) {
    LoadTestResult result;
    result.test_name = kj::str("Order Throughput (", config_.target_orders_per_sec, " orders/sec)");

    MemoryTracker memory;
    result.memory_start_mb = memory.baseline_mb();

    LatencyHistogram latency;
    std::atomic<uint64_t> order_count{0};
    std::atomic<uint64_t> error_count{0};
    std::atomic<bool> running{true};

    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::seconds(config_.duration_sec);

    // Calculate orders per batch and sleep time
    constexpr size_t BATCH_SIZE = 100;
    double batches_per_sec = config_.target_orders_per_sec / BATCH_SIZE;
    auto sleep_time = std::chrono::microseconds(
        static_cast<int64_t>(1000000.0 / batches_per_sec));

    // Progress reporter thread
    kj::Own<kj::Thread> reporter = kj::heap<kj::Thread>([this, &order_count, &latency, &running]() {
      while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.report_interval_sec));
        if (!running.load(std::memory_order_relaxed)) break;
        KJ_LOG(INFO, "Progress: ", order_count.load(), " orders, ",
               "P99: ", latency.p99(), " us");
      }
    });

    // Main order generation loop
    while (std::chrono::high_resolution_clock::now() < deadline) {
      auto batch = order_gen_.generate_batch(BATCH_SIZE);
      auto batch_start = std::chrono::high_resolution_clock::now();

      for (auto& order : batch) {
        try {
          auto op_start = std::chrono::high_resolution_clock::now();
          handler(order);
          auto op_end = std::chrono::high_resolution_clock::now();

          double latency_us = static_cast<double>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0;
          latency.record(latency_us);
          order_count.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
          error_count.fetch_add(1, std::memory_order_relaxed);
        }
      }

      // Rate limiting
      auto batch_end = std::chrono::high_resolution_clock::now();
      auto batch_duration = batch_end - batch_start;
      if (batch_duration < sleep_time) {
        std::this_thread::sleep_for(sleep_time - batch_duration);
      }
    }

    running.store(false, std::memory_order_relaxed);
    reporter = nullptr; // Join reporter thread

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_sec = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.0;

    // Populate results
    result.total_orders = order_count.load();
    result.orders_per_sec = static_cast<double>(result.total_orders) / result.duration_sec;
    result.latency_p50_us = latency.p50();
    result.latency_p95_us = latency.p95();
    result.latency_p99_us = latency.p99();
    result.latency_mean_us = latency.mean();
    result.latency_min_us = latency.min();
    result.latency_max_us = latency.max();
    result.errors = error_count.load();
    result.error_rate = result.total_orders > 0 ?
        static_cast<double>(result.errors) / static_cast<double>(result.total_orders) : 0.0;
    result.memory_end_mb = memory.current_mb();
    result.memory_growth_pct = memory.growth_pct();

    // Validate against targets
    result.passed = (result.latency_p99_us <= config_.targets.order_path_p99_us) &&
                    (result.orders_per_sec >= config_.targets.order_throughput * 0.9) &&
                    (result.memory_growth_pct <= config_.targets.max_memory_growth_pct);

    return result;
  }

  /**
   * @brief Run sustained load test for memory leak detection
   */
  LoadTestResult run_sustained_test(EventHandler event_handler, OrderHandler order_handler,
                                     uint64_t duration_hours = 1) {
    LoadTestResult result;
    result.test_name = kj::str("Sustained Load Test (", duration_hours, " hours)");

    MemoryTracker memory;
    result.memory_start_mb = memory.baseline_mb();

    LatencyHistogram event_latency;
    LatencyHistogram order_latency;
    std::atomic<uint64_t> event_count{0};
    std::atomic<uint64_t> order_count{0};
    std::atomic<uint64_t> error_count{0};
    std::atomic<bool> running{true};

    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::hours(duration_hours);

    // Memory sampling thread
    kj::Vector<size_t> memory_samples;
    kj::Own<kj::Thread> memory_sampler = kj::heap<kj::Thread>([&memory, &memory_samples, &running]() {
      while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        if (!running.load(std::memory_order_relaxed)) break;
        memory_samples.add(memory.current_mb());
      }
    });

    // Progress reporter thread
    kj::Own<kj::Thread> reporter = kj::heap<kj::Thread>(
        [this, &event_count, &order_count, &memory, &running]() {
      while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        if (!running.load(std::memory_order_relaxed)) break;
        KJ_LOG(INFO, "Sustained test progress: ",
               event_count.load(), " events, ",
               order_count.load(), " orders, ",
               "Memory: ", memory.current_mb(), " MB (",
               memory.growth_pct(), "% growth)");
      }
    });

    // Reduced rate for sustained test (50% of target)
    double sustained_event_rate = config_.target_events_per_sec * 0.5;
    double sustained_order_rate = config_.target_orders_per_sec * 0.5;

    constexpr size_t EVENT_BATCH_SIZE = 500;
    constexpr size_t ORDER_BATCH_SIZE = 50;

    auto event_sleep = std::chrono::microseconds(
        static_cast<int64_t>(1000000.0 * EVENT_BATCH_SIZE / sustained_event_rate));
    auto order_sleep = std::chrono::microseconds(
        static_cast<int64_t>(1000000.0 * ORDER_BATCH_SIZE / sustained_order_rate));

    // Event generation thread
    kj::Own<kj::Thread> event_thread = kj::heap<kj::Thread>(
        [this, &event_handler, &event_latency, &event_count, &error_count, &running, &deadline, event_sleep]() {
      while (running.load(std::memory_order_relaxed) &&
             std::chrono::high_resolution_clock::now() < deadline) {
        auto batch = market_gen_.generate_batch(EVENT_BATCH_SIZE);
        auto batch_start = std::chrono::high_resolution_clock::now();

        for (auto& event : batch) {
          if (!running.load(std::memory_order_relaxed)) break;
          try {
            auto op_start = std::chrono::high_resolution_clock::now();
            event_handler(event);
            auto op_end = std::chrono::high_resolution_clock::now();

            double latency_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0;
            event_latency.record(latency_us);
            event_count.fetch_add(1, std::memory_order_relaxed);
          } catch (...) {
            error_count.fetch_add(1, std::memory_order_relaxed);
          }
        }

        auto batch_end = std::chrono::high_resolution_clock::now();
        auto batch_duration = batch_end - batch_start;
        if (batch_duration < event_sleep) {
          std::this_thread::sleep_for(event_sleep - batch_duration);
        }
      }
    });

    // Order generation thread
    kj::Own<kj::Thread> order_thread = kj::heap<kj::Thread>(
        [this, &order_handler, &order_latency, &order_count, &error_count, &running, &deadline, order_sleep]() {
      while (running.load(std::memory_order_relaxed) &&
             std::chrono::high_resolution_clock::now() < deadline) {
        auto batch = order_gen_.generate_batch(ORDER_BATCH_SIZE);
        auto batch_start = std::chrono::high_resolution_clock::now();

        for (auto& order : batch) {
          if (!running.load(std::memory_order_relaxed)) break;
          try {
            auto op_start = std::chrono::high_resolution_clock::now();
            order_handler(order);
            auto op_end = std::chrono::high_resolution_clock::now();

            double latency_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()) / 1000.0;
            order_latency.record(latency_us);
            order_count.fetch_add(1, std::memory_order_relaxed);
          } catch (...) {
            error_count.fetch_add(1, std::memory_order_relaxed);
          }
        }

        auto batch_end = std::chrono::high_resolution_clock::now();
        auto batch_duration = batch_end - batch_start;
        if (batch_duration < order_sleep) {
          std::this_thread::sleep_for(order_sleep - batch_duration);
        }
      }
    });

    // Wait for deadline
    std::this_thread::sleep_until(deadline);
    running.store(false, std::memory_order_relaxed);

    // Join threads
    event_thread = nullptr;
    order_thread = nullptr;
    memory_sampler = nullptr;
    reporter = nullptr;

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_sec = static_cast<double>(
        std::chrono::duration_cast<std::chrono::seconds>(end - start).count());

    // Populate results
    result.total_events = event_count.load();
    result.total_orders = order_count.load();
    result.events_per_sec = static_cast<double>(result.total_events) / result.duration_sec;
    result.orders_per_sec = static_cast<double>(result.total_orders) / result.duration_sec;
    result.latency_p50_us = (event_latency.p50() + order_latency.p50()) / 2.0;
    result.latency_p95_us = kj::max(event_latency.p95(), order_latency.p95());
    result.latency_p99_us = kj::max(event_latency.p99(), order_latency.p99());
    result.latency_mean_us = (event_latency.mean() + order_latency.mean()) / 2.0;
    result.latency_min_us = kj::min(event_latency.min(), order_latency.min());
    result.latency_max_us = kj::max(event_latency.max(), order_latency.max());
    result.errors = error_count.load();
    result.error_rate = (result.total_events + result.total_orders) > 0 ?
        static_cast<double>(result.errors) / static_cast<double>(result.total_events + result.total_orders) : 0.0;
    result.memory_end_mb = memory.current_mb();
    result.memory_growth_pct = memory.growth_pct();

    // Check for memory leak: analyze trend in memory samples
    bool memory_leak_detected = false;
    if (memory_samples.size() >= 10) {
      // Simple linear regression to detect upward trend
      double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
      size_t n = memory_samples.size();
      for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(memory_samples[i]);
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
      }
      double slope = (static_cast<double>(n) * sum_xy - sum_x * sum_y) /
                     (static_cast<double>(n) * sum_xx - sum_x * sum_x);

      // If slope indicates > 1MB/hour growth, flag as potential leak
      double growth_per_hour = slope * 60.0; // samples are per minute
      memory_leak_detected = growth_per_hour > 1.0;
    }

    // Validate against targets
    result.passed = !memory_leak_detected &&
                    (result.memory_growth_pct <= config_.targets.max_memory_growth_pct) &&
                    (result.error_rate < 0.001); // < 0.1% error rate

    return result;
  }

private:
  Config config_;
  MarketDataGenerator market_gen_;
  OrderGenerator order_gen_;
};

// ============================================================================
// Load Test Suite
// ============================================================================

class LoadTestSuite {
public:
  explicit LoadTestSuite(kj::StringPtr name) : name_(kj::str(name)) {}

  void add_result(LoadTestResult result) {
    results_.add(kj::mv(result));
  }

  [[nodiscard]] bool all_passed() const {
    for (const auto& result : results_) {
      if (!result.passed) return false;
    }
    return true;
  }

  [[nodiscard]] kj::String generate_report() const {
    kj::StringTree tree = kj::strTree(
        "\n",
        "################################################################################\n",
        "#                         LOAD TEST REPORT                                     #\n",
        "################################################################################\n",
        "\n",
        "Suite: ", name_, "\n",
        "Status: ", all_passed() ? "ALL TESTS PASSED" : "SOME TESTS FAILED", "\n",
        "\n"
    );

    for (const auto& result : results_) {
      tree = kj::strTree(kj::mv(tree), result.to_string(), "\n");
    }

    // Summary table
    tree = kj::strTree(kj::mv(tree),
        "################################################################################\n",
        "#                              SUMMARY                                         #\n",
        "################################################################################\n",
        "\n",
        "Test Name                                    | Status | Events/s | Orders/s | P99 (us)\n",
        "--------------------------------------------|--------|----------|----------|----------\n"
    );

    for (const auto& result : results_) {
      tree = kj::strTree(kj::mv(tree),
          result.test_name, " | ",
          result.passed ? "PASS" : "FAIL", " | ",
          result.events_per_sec, " | ",
          result.orders_per_sec, " | ",
          result.latency_p99_us, "\n"
      );
    }

    return tree.flatten();
  }

  [[nodiscard]] kj::String to_json() const {
    kj::StringTree tree = kj::strTree("{\"suite\":\"", name_, "\",\"results\":[");
    bool first = true;
    for (const auto& result : results_) {
      if (!first) tree = kj::strTree(kj::mv(tree), ",");
      tree = kj::strTree(kj::mv(tree), result.to_json());
      first = false;
    }
    tree = kj::strTree(kj::mv(tree), "]}");
    return tree.flatten();
  }

private:
  kj::String name_;
  kj::Vector<LoadTestResult> results_;
};

} // namespace veloz::loadtest
