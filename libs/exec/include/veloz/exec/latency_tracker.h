#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Latency statistics for a single exchange
struct LatencyStats {
  kj::Duration mean{0 * kj::NANOSECONDS};
  kj::Duration p50{0 * kj::NANOSECONDS};
  kj::Duration p95{0 * kj::NANOSECONDS};
  kj::Duration p99{0 * kj::NANOSECONDS};
  kj::Duration min{kj::maxValue};
  kj::Duration max{0 * kj::NANOSECONDS};
  std::uint64_t sample_count{0};
  kj::TimePoint last_update{kj::origin<kj::TimePoint>()};
};

// Latency sample with timestamp for sliding window
struct LatencySample {
  kj::Duration latency;
  kj::TimePoint timestamp;
};

// LatencyTracker monitors round-trip times per exchange for latency-aware routing
class LatencyTracker final {
public:
  LatencyTracker() = default;

  // Record a latency measurement for an exchange
  void record_latency(veloz::common::Venue venue, kj::Duration latency, kj::TimePoint timestamp);

  // Get latency statistics for an exchange
  [[nodiscard]] kj::Maybe<LatencyStats> get_stats(veloz::common::Venue venue) const;

  // Get expected latency for an exchange (uses p50 as default estimate)
  [[nodiscard]] kj::Maybe<kj::Duration> get_expected_latency(veloz::common::Venue venue) const;

  // Get all venues sorted by expected latency (fastest first)
  [[nodiscard]] kj::Array<veloz::common::Venue> get_venues_by_latency() const;

  // Check if an exchange is considered healthy (recent samples, acceptable latency)
  [[nodiscard]] bool is_healthy(veloz::common::Venue venue, kj::Duration max_latency,
                                kj::Duration max_staleness) const;

  // Clear all samples for an exchange
  void clear(veloz::common::Venue venue);

  // Clear all samples for all exchanges
  void clear_all();

  // Configuration
  void set_window_size(std::size_t size);
  void set_window_duration(kj::Duration duration);

  [[nodiscard]] std::size_t window_size() const;
  [[nodiscard]] kj::Duration window_duration() const;

private:
  void recalculate_stats(veloz::common::Venue venue);
  void prune_old_samples(veloz::common::Venue venue, kj::TimePoint now);

  struct VenueData {
    kj::Vector<LatencySample> samples;
    LatencyStats stats;
  };

  struct TrackerState {
    kj::HashMap<veloz::common::Venue, VenueData> venues;
    std::size_t window_size{1000};               // Max samples to keep
    kj::Duration window_duration{60 * kj::SECONDS}; // Time window for samples
  };

  kj::MutexGuarded<TrackerState> guarded_;
};

} // namespace veloz::exec
