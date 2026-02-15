#pragma once

#include <atomic>
#include <chrono>
#include <kj/common.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <map>
#include <source_location>
#include <string> // std::string used for std::map key compatibility
#include <variant>
#include <vector> // std::vector used for bucket_counts return type compatibility

namespace veloz::core {

// Metric type
enum class MetricType { Counter, Gauge, Histogram, Summary };

// Base metric class
class Metric {
public:
  explicit Metric(std::string name, std::string description)
      : name_(kj::mv(name)), description_(kj::mv(description)) {}
  virtual ~Metric() = default;

  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const std::string& description() const noexcept {
    return description_;
  }
  [[nodiscard]] virtual MetricType type() const noexcept = 0;

private:
  std::string name_;
  std::string description_;
};

// Counter metric (monotonically increasing)
class Counter final : public Metric {
public:
  explicit Counter(std::string name, std::string description)
      : Metric(kj::mv(name), kj::mv(description)) {}
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
  explicit Gauge(std::string name, std::string description)
      : Metric(kj::mv(name), kj::mv(description)) {}
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
  explicit Histogram(std::string name, std::string description,
                     std::vector<double> buckets = default_buckets())
      : Metric(kj::mv(name), kj::mv(description)), buckets_(kj::mv(buckets)) {
    bucket_counts_.reset(new std::atomic<int64_t>[buckets_.size()]);
    for (size_t i = 0; i < buckets_.size(); ++i) {
      bucket_counts_[i].store(0);
    }
  }

  void observe(double value) noexcept {
    count_++;
    sum_ += value;
    for (size_t i = 0; i < buckets_.size(); ++i) {
      if (value <= buckets_[i]) {
        bucket_counts_[i]++;
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
    counts.reserve(buckets_.size());
    for (size_t i = 0; i < buckets_.size(); ++i) {
      counts.push_back(bucket_counts_[i].load());
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
  std::unique_ptr<std::atomic<int64_t>[]> bucket_counts_;
  std::atomic<int64_t> count_{0};
  std::atomic<double> sum_{0.0};
};

// Metrics registry
class MetricsRegistry final {
public:
  MetricsRegistry() = default;

  // Register metrics
  // Uses std::make_unique for map storage (kj::Own lacks release())
  void register_counter(std::string name, std::string description) {
    auto lock = guarded_.lockExclusive();
    lock->counters[kj::mv(name)] = std::make_unique<Counter>(name, kj::mv(description));
  }

  void register_gauge(std::string name, std::string description) {
    auto lock = guarded_.lockExclusive();
    lock->gauges[kj::mv(name)] = std::make_unique<Gauge>(name, kj::mv(description));
  }

  void register_histogram(std::string name, std::string description,
                          std::vector<double> buckets = Histogram::default_buckets()) {
    auto lock = guarded_.lockExclusive();
    lock->histograms[kj::mv(name)] =
        std::make_unique<Histogram>(name, kj::mv(description), kj::mv(buckets));
  }

  // Get metrics
  [[nodiscard]] Counter* counter(std::string_view name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->counters.find(std::string(name));
    return it != lock->counters.end() ? it->second.get() : nullptr;
  }

  [[nodiscard]] Gauge* gauge(std::string_view name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->gauges.find(std::string(name));
    return it != lock->gauges.end() ? it->second.get() : nullptr;
  }

  [[nodiscard]] Histogram* histogram(std::string_view name) {
    auto lock = guarded_.lockExclusive();
    auto it = lock->histograms.find(std::string(name));
    return it != lock->histograms.end() ? it->second.get() : nullptr;
  }

  // Get all metric names
  [[nodiscard]] std::vector<std::string> counter_names() const {
    auto lock = guarded_.lockExclusive();
    std::vector<std::string> names;
    names.reserve(lock->counters.size());
    for (const auto& [name, metric] : lock->counters) {
      names.push_back(name);
    }
    return names;
  }

  [[nodiscard]] std::vector<std::string> gauge_names() const {
    auto lock = guarded_.lockExclusive();
    std::vector<std::string> names;
    names.reserve(lock->gauges.size());
    for (const auto& [name, metric] : lock->gauges) {
      names.push_back(name);
    }
    return names;
  }

  [[nodiscard]] std::vector<std::string> histogram_names() const {
    auto lock = guarded_.lockExclusive();
    std::vector<std::string> names;
    names.reserve(lock->histograms.size());
    for (const auto& [name, metric] : lock->histograms) {
      names.push_back(name);
    }
    return names;
  }

  // Export metrics to Prometheus format
  [[nodiscard]] std::string to_prometheus() const;

private:
  // Internal state for MetricsRegistry
  struct RegistryState {
    std::map<std::string, std::unique_ptr<Counter>> counters;
    std::map<std::string, std::unique_ptr<Gauge>> gauges;
    std::map<std::string, std::unique_ptr<Histogram>> histograms;
  };

  kj::MutexGuarded<RegistryState> guarded_;
};

// Global metrics registry
[[nodiscard]] MetricsRegistry& global_metrics();

// Convenient metric access functions
inline void counter_inc(std::string_view name, int64_t value = 1) {
  if (auto c = global_metrics().counter(name)) {
    c->increment(value);
  }
}

inline int64_t counter_get(std::string_view name) {
  if (auto c = global_metrics().counter(name)) {
    return c->value();
  }
  return 0;
}

inline void gauge_set(std::string_view name, int64_t value) {
  if (auto g = global_metrics().gauge(name)) {
    g->set(value);
  }
}

inline void gauge_inc(std::string_view name, int64_t value = 1) {
  if (auto g = global_metrics().gauge(name)) {
    g->increment(value);
  }
}

inline void gauge_dec(std::string_view name, int64_t value = 1) {
  if (auto g = global_metrics().gauge(name)) {
    g->decrement(value);
  }
}

inline int64_t gauge_get(std::string_view name) {
  if (auto g = global_metrics().gauge(name)) {
    return g->value();
  }
  return 0;
}

inline void histogram_observe(std::string_view name, double value) {
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
