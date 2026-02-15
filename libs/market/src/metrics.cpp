#include "veloz/market/metrics.h"

#include <cmath> // std::ceil - standard C++ math function (no KJ equivalent)

namespace veloz::market {

void MarketMetrics::record_event_latency_ns(int64_t latency_ns) {
  ++event_count_;

  // Implement circular buffer using kj::Vector
  if (latency_samples_.size() < MAX_SAMPLES) {
    latency_samples_.add(latency_ns);
  } else {
    // Overwrite oldest sample
    latency_samples_[sample_start_] = latency_ns;
    sample_start_ = (sample_start_ + 1) % MAX_SAMPLES;
  }
}

void MarketMetrics::record_drop() {
  ++drop_count_;
}

void MarketMetrics::record_reconnect() {
  ++reconnect_count_;
}

void MarketMetrics::record_gap(int64_t expected_seq, int64_t actual_seq) {
  // Could track gap size for analytics
  (void)expected_seq;
  (void)actual_seq;
  ++drop_count_;
}

size_t MarketMetrics::event_count() const {
  return event_count_;
}

size_t MarketMetrics::drop_count() const {
  return drop_count_;
}

size_t MarketMetrics::reconnect_count() const {
  return reconnect_count_;
}

int64_t MarketMetrics::avg_latency_ns() const {
  if (latency_samples_.size() == 0)
    return 0;

  int64_t sum = 0;
  for (size_t i = 0; i < latency_samples_.size(); ++i) {
    sum += latency_samples_[i];
  }
  return sum / static_cast<int64_t>(latency_samples_.size());
}

int64_t MarketMetrics::percentile_ns(double percentile) const {
  if (latency_samples_.size() == 0)
    return 0;

  // Copy to a temporary vector for sorting
  kj::Vector<int64_t> sorted;
  for (size_t i = 0; i < latency_samples_.size(); ++i) {
    sorted.add(latency_samples_[i]);
  }

  // Simple insertion sort (good enough for metrics)
  for (size_t i = 1; i < sorted.size(); ++i) {
    int64_t key = sorted[i];
    size_t j = i;
    while (j > 0 && sorted[j - 1] > key) {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = key;
  }

  size_t index = static_cast<size_t>(std::ceil(percentile / 100.0 * sorted.size()) - 1);
  index = kj::min(index, sorted.size() - 1);

  return sorted[index];
}

void MarketMetrics::reset() {
  event_count_ = 0;
  drop_count_ = 0;
  reconnect_count_ = 0;
  latency_samples_.clear();
  sample_start_ = 0;
}

} // namespace veloz::market
