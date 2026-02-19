#pragma once

#include <atomic>
#include <chrono>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <vector> // std::vector used for bucket_counts return type compatibility

namespace veloz::core {

// Metric type
enum class MetricType { Counter, Gauge, Histogram, Summary };

// Base metric class
class Metric {
public:
  explicit Metric(kj::StringPtr name, kj::StringPtr description)
      : name_(kj::heapString(name)), description_(kj::heapString(description)) {}
  virtual ~Metric() noexcept = default;

  [[nodiscard]] kj::StringPtr name() const noexcept {
    return name_;
  }
  [[nodiscard]] kj::StringPtr description() const noexcept {
    return description_;
  }
  [[nodiscard]] virtual MetricType type() const noexcept = 0;

private:
  kj::String name_;
  kj::String description_;
};

// Counter metric (monotonically increasing)
class Counter final : public Metric {
public:
  explicit Counter(kj::StringPtr name, kj::StringPtr description)
      : Metric(name, description) {}
  void increment(int64_t value = 1) noexcept {
    count_ += value;
  }

  [[nodiscard]] int64_t value() const noexcept {
    return count_.load();
  }

  [[nodiscard]] MetricType type() const noexcept override {
    return MetricType::Counter;
  }

private:
  std::atomic<int64_t> count_{0};
};

// Gauge metric (can increase or decrease)
class Gauge final : public Metric {
public:
  explicit Gauge(kj::StringPtr name, kj::StringPtr description)
      : Metric(name, description) {}
  void increment(int64_t value = 1) noexcept {
    value_ += value;
  }
  void decrement(int64_t value = 1) noexcept {
    value_ -= value;
  }
  void set(int64_t value) noexcept {
    value_.store(value);
  }

  [[nodiscard]] int64_t value() const noexcept {
    return value_.load();
  }

  [[nodiscard]] MetricType type() const noexcept override {
    return MetricType::Gauge;
  }

private:
  std::atomic<int64_t> value_{0};
};

// Timer (for measuring latency)
class Timer final {
public:
  explicit Timer(bool auto_start = true) {
    if (auto_start) {
      start();
    }
  }
  void start() noexcept {
    start_time_ = std::chrono::steady_clock::now();
  }

  [[nodiscard]] std::chrono::milliseconds elapsed_ms() const noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start_time_);
  }

  [[nodiscard]] std::chrono::microseconds elapsed_us() const noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                 start_time_);
  }

  [[nodiscard]] std::chrono::nanoseconds elapsed_ns() const noexcept {
    return std::chrono::steady_clock::now() - start_time_;
  }

private:
  std::chrono::steady_clock::time_point start_time_;
};

// Histogram metric (for distribution statistics)
class Histogram final : public Metric {
public:
  explicit Histogram(kj::StringPtr name, kj::StringPtr description,
                     std::vector<double> buckets = default_buckets())
      : Metric(name, description), buckets_(kj::mv(buckets)) {
    // Initialize bucket counts using unique_ptr array for atomic values
    bucket_count_ = buckets_.size();
    bucket_counts_vec_ = std::make_unique<std::atomic<int64_t>[]>(bucket_count_);
    for (size_t i = 0; i < bucket_count_; ++i) {
      bucket_counts_vec_[i].store(0);
    }
  }

  ~Histogram() noexcept override = default;

  void observe(double value) noexcept {
    count_++;
    sum_ += value;
    for (size_t i = 0; i < bucket_count_; ++i) {
      if (value <= buckets_[i]) {
        bucket_counts_vec_[i].fetch_add(1);
      }
    }
  }

  [[nodiscard]] int64_t count() const noexcept {
    return count_.load();
  }
  [[nodiscard]] double sum() const noexcept {
    return sum_.load();
  }
  [[nodiscard]] const std::vector<double>& buckets() const noexcept {
    return buckets_;
  }
  [[nodiscard]] std::vector<int64_t> bucket_counts() const {
    std::vector<int64_t> counts;
    counts.reserve(bucket_count_);
    for (size_t i = 0; i < bucket_count_; ++i) {
      counts.push_back(bucket_counts_vec_[i].load());
    }
    return counts;
  }

  [[nodiscard]] MetricType type() const noexcept override {
    return MetricType::Histogram;
  }

  static std::vector<double> default_buckets() {
    return {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1,   0.2,   0.5,
            1.0,   2.0,   5.0,   10.0, 30.0, 60.0, 120.0, 300.0, 600.0};
  }

private:
  std::vector<double> buckets_;
  // Use std::unique_ptr array for atomic values (std::atomic is not movable)
  std::unique_ptr<std::atomic<int64_t>[]> bucket_counts_vec_;
  size_t bucket_count_{0};
  std::atomic<int64_t> count_{0};
  std::atomic<double> sum_{0.0};
};

// Metrics registry
class MetricsRegistry final {
public:
  MetricsRegistry() = default;

  // Register metrics
  // Note: std::map<std::string, ...> kept for ordered map semantics (KJ HashMap is unordered)
  void register_counter(kj::StringPtr name, kj::StringPtr description) {
    auto lock = guarded_.lockExclusive();
    lock->counters.insert(kj::str(name), kj::heap<Counter>(name, description));
  }

  void register_gauge(kj::StringPtr name, kj::StringPtr description) {
    auto lock = guarded_.lockExclusive();
    lock->gauges.insert(kj::str(name), kj::heap<Gauge>(name, description));
  }

  void register_histogram(kj::StringPtr name, kj::StringPtr description,
                          std::vector<double> buckets = Histogram::default_buckets()) {
    auto lock = guarded_.lockExclusive();
    lock->histograms.insert(kj::str(name),
                            kj::heap<Histogram>(name, description, kj::mv(buckets)));
  }

  // Get metrics
  [[nodiscard]] Counter* counter(kj::StringPtr name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->counters.find(name);
    KJ_IF_SOME(value, it) {
      return value.get();
    }
    return nullptr;
  }

  [[nodiscard]] Gauge* gauge(kj::StringPtr name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->gauges.find(name);
    KJ_IF_SOME(value, it) {
      return value.get();
    }
    return nullptr;
  }

  [[nodiscard]] Histogram* histogram(kj::StringPtr name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->histograms.find(name);
    KJ_IF_SOME(value, it) {
      return value.get();
    }
    return nullptr;
  }

  // Get all metric names
  [[nodiscard]] kj::Vector<kj::String> counter_names() const {
    auto lock = guarded_.lockExclusive();
    kj::Vector<kj::String> names;
    names.reserve(lock->counters.size());
    for (const auto& entry : lock->counters) {
      names.add(kj::str(entry.key));
    }
    return names;
  }

  [[nodiscard]] kj::Vector<kj::String> gauge_names() const {
    auto lock = guarded_.lockExclusive();
    kj::Vector<kj::String> names;
    names.reserve(lock->gauges.size());
    for (const auto& entry : lock->gauges) {
      names.add(kj::str(entry.key));
    }
    return names;
  }

  [[nodiscard]] kj::Vector<kj::String> histogram_names() const {
    auto lock = guarded_.lockExclusive();
    kj::Vector<kj::String> names;
    names.reserve(lock->histograms.size());
    for (const auto& entry : lock->histograms) {
      names.add(kj::str(entry.key));
    }
    return names;
  }

  // Export metrics to Prometheus format
  [[nodiscard]] kj::String to_prometheus() const;

private:
  // Internal state for MetricsRegistry
  struct RegistryState {
    kj::TreeMap<kj::String, kj::Own<Counter>> counters;
    kj::TreeMap<kj::String, kj::Own<Gauge>> gauges;
    kj::TreeMap<kj::String, kj::Own<Histogram>> histograms;
  };

  kj::MutexGuarded<RegistryState> guarded_;
};

// Global metrics registry
[[nodiscard]] MetricsRegistry& global_metrics();

// Convenient metric access functions
inline void counter_inc(kj::StringPtr name, int64_t value = 1) {
  if (auto c = global_metrics().counter(name)) {
    c->increment(value);
  }
}

inline int64_t counter_get(kj::StringPtr name) {
  if (auto c = global_metrics().counter(name)) {
    return c->value();
  }
  return 0;
}

inline void gauge_set(kj::StringPtr name, int64_t value) {
  if (auto g = global_metrics().gauge(name)) {
    g->set(value);
  }
}

inline void gauge_inc(kj::StringPtr name, int64_t value = 1) {
  if (auto g = global_metrics().gauge(name)) {
    g->increment(value);
  }
}

inline void gauge_dec(kj::StringPtr name, int64_t value = 1) {
  if (auto g = global_metrics().gauge(name)) {
    g->decrement(value);
  }
}

inline int64_t gauge_get(kj::StringPtr name) {
  if (auto g = global_metrics().gauge(name)) {
    return g->value();
  }
  return 0;
}

inline void histogram_observe(kj::StringPtr name, double value) {
  if (auto h = global_metrics().histogram(name)) {
    h->observe(value);
  }
}

// Macro for measuring function execution time
#define MEASURE_TIME(name, func)                                                                   \
  do {                                                                                             \
    veloz::core::Timer _timer;                                                                     \
    func;                                                                                          \
    veloz::core::histogram_observe(name, _timer.elapsed_ms().count() / 1000.0);                    \
  } while (false)

} // namespace veloz::core
