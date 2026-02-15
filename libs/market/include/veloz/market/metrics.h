#pragma once

#include <cstdint>
#include <kj/vector.h>

namespace veloz::market {

class MarketMetrics final {
public:
  MarketMetrics() = default;

  // Event tracking
  void record_event_latency_ns(int64_t latency_ns);
  void record_drop();
  void record_reconnect();
  void record_gap(int64_t expected_seq, int64_t actual_seq);

  // Query methods
  [[nodiscard]] size_t event_count() const;
  [[nodiscard]] size_t drop_count() const;
  [[nodiscard]] size_t reconnect_count() const;
  [[nodiscard]] int64_t avg_latency_ns() const;

  // Percentile calculation (p50, p99, p99.9)
  [[nodiscard]] int64_t percentile_ns(double percentile) const;

  // Reset metrics
  void reset();

private:
  size_t event_count_{0};
  size_t drop_count_{0};
  size_t reconnect_count_{0};

  // Circular buffer for latency samples (keep last N samples)
  // Using kj::Vector with manual circular buffer management
  kj::Vector<int64_t> latency_samples_;
  size_t sample_start_{0}; // Start index for circular buffer
  static constexpr size_t MAX_SAMPLES = 10000;
};

} // namespace veloz::market
