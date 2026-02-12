#include "veloz/market/metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace veloz::market {

void MarketMetrics::record_event_latency_ns(std::int64_t latency_ns) {
  ++event_count_;
  latency_samples_.push_back(latency_ns);
  if (latency_samples_.size() > MAX_SAMPLES) {
    latency_samples_.pop_front();
  }
}

void MarketMetrics::record_drop() {
  ++drop_count_;
}

void MarketMetrics::record_reconnect() {
  ++reconnect_count_;
}

void MarketMetrics::record_gap(std::int64_t expected_seq, std::int64_t actual_seq) {
  // Could track gap size for analytics
  ++drop_count_;
}

std::size_t MarketMetrics::event_count() const {
  return event_count_;
}

std::size_t MarketMetrics::drop_count() const {
  return drop_count_;
}

std::size_t MarketMetrics::reconnect_count() const {
  return reconnect_count_;
}

std::int64_t MarketMetrics::avg_latency_ns() const {
  if (latency_samples_.empty())
    return 0;
  std::int64_t sum = std::accumulate(latency_samples_.begin(), latency_samples_.end(), 0LL);
  return sum / static_cast<std::int64_t>(latency_samples_.size());
}

std::int64_t MarketMetrics::percentile_ns(double percentile) const {
  if (latency_samples_.empty())
    return 0;

  std::vector<std::int64_t> sorted(latency_samples_.begin(), latency_samples_.end());
  std::sort(sorted.begin(), sorted.end());

  std::size_t index = static_cast<std::size_t>(std::ceil(percentile / 100.0 * sorted.size()) - 1);
  index = std::min(index, sorted.size() - 1);

  return sorted[index];
}

void MarketMetrics::reset() {
  event_count_ = 0;
  drop_count_ = 0;
  reconnect_count_ = 0;
  latency_samples_.clear();
}

} // namespace veloz::market
