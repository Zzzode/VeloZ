#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace veloz::market {

class MarketMetrics final {
public:
  MarketMetrics() = default;

  // Event tracking
  void record_event_latency_ns(std::int64_t latency_ns);
  void record_drop();
  void record_reconnect();
  void record_gap(std::int64_t expected_seq, std::int64_t actual_seq);

  // Query methods
  [[nodiscard]] std::size_t event_count() const;
  [[nodiscard]] std::size_t drop_count() const;
  [[nodiscard]] std::size_t reconnect_count() const;
  [[nodiscard]] std::int64_t avg_latency_ns() const;

  // Percentile calculation (p50, p99, p99.9)
  [[nodiscard]] std::int64_t percentile_ns(double percentile) const;

  // Reset metrics
  void reset();

private:
  std::size_t event_count_{0};
  std::size_t drop_count_{0};
  std::size_t reconnect_count_{0};

  // Circular buffer for latency samples (keep last N samples)
  std::deque<std::int64_t> latency_samples_;
  static constexpr std::size_t MAX_SAMPLES = 10000;
};

} // namespace veloz::market
