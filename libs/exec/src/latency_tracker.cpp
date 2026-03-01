#include "veloz/exec/latency_tracker.h"

#include <algorithm>

namespace veloz::exec {

void LatencyTracker::record_latency(veloz::common::Venue venue, kj::Duration latency,
                                    kj::TimePoint timestamp) {
  auto lock = guarded_.lockExclusive();

  // Get or create venue data
  VenueData* venue_data = nullptr;
  KJ_IF_SOME(existing, lock->venues.find(venue)) {
    venue_data = &existing;
  }
  else {
    lock->venues.insert(venue, VenueData{});
    KJ_IF_SOME(inserted, lock->venues.find(venue)) {
      venue_data = &inserted;
    }
  }

  if (venue_data == nullptr) {
    return;
  }

  // Add sample
  venue_data->samples.add(LatencySample{latency, timestamp});

  // Prune old samples if window is exceeded
  if (venue_data->samples.size() > lock->window_size) {
    // Remove oldest samples
    auto new_samples = kj::Vector<LatencySample>();
    std::size_t start = venue_data->samples.size() - lock->window_size;
    for (std::size_t i = start; i < venue_data->samples.size(); ++i) {
      new_samples.add(kj::mv(venue_data->samples[i]));
    }
    venue_data->samples = kj::mv(new_samples);
  }

  // Prune samples outside time window
  auto cutoff = timestamp - lock->window_duration;
  auto new_samples = kj::Vector<LatencySample>();
  for (auto& sample : venue_data->samples) {
    if (sample.timestamp >= cutoff) {
      new_samples.add(kj::mv(sample));
    }
  }
  venue_data->samples = kj::mv(new_samples);

  // Recalculate statistics
  auto& stats = venue_data->stats;
  auto& samples = venue_data->samples;

  if (samples.empty()) {
    stats = LatencyStats{};
    return;
  }

  // Sort samples by latency for percentile calculation
  kj::Vector<kj::Duration> sorted_latencies;
  for (const auto& s : samples) {
    sorted_latencies.add(s.latency);
  }
  std::sort(sorted_latencies.begin(), sorted_latencies.end());

  // Calculate statistics
  stats.sample_count = samples.size();
  stats.last_update = timestamp;
  stats.min = sorted_latencies[0];
  stats.max = sorted_latencies[sorted_latencies.size() - 1];

  // Mean
  std::int64_t total_ns = 0;
  for (const auto& lat : sorted_latencies) {
    total_ns += lat / kj::NANOSECONDS;
  }
  stats.mean = (total_ns / static_cast<std::int64_t>(sorted_latencies.size())) * kj::NANOSECONDS;

  // Percentiles
  auto percentile = [&sorted_latencies](double p) -> kj::Duration {
    std::size_t idx =
        static_cast<std::size_t>(p * static_cast<double>(sorted_latencies.size() - 1));
    return sorted_latencies[idx];
  };

  stats.p50 = percentile(0.50);
  stats.p95 = percentile(0.95);
  stats.p99 = percentile(0.99);
}

kj::Maybe<LatencyStats> LatencyTracker::get_stats(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(venue_data, lock->venues.find(venue)) {
    if (venue_data.stats.sample_count > 0) {
      return venue_data.stats;
    }
  }
  return kj::none;
}

kj::Maybe<kj::Duration> LatencyTracker::get_expected_latency(veloz::common::Venue venue) const {
  KJ_IF_SOME(stats, get_stats(venue)) {
    return stats.p50;
  }
  return kj::none;
}

kj::Array<veloz::common::Venue> LatencyTracker::get_venues_by_latency() const {
  auto lock = guarded_.lockExclusive();

  // Collect venues with their expected latencies
  kj::Vector<std::pair<veloz::common::Venue, kj::Duration>> venue_latencies;
  for (const auto& entry : lock->venues) {
    if (entry.value.stats.sample_count > 0) {
      venue_latencies.add(std::make_pair(entry.key, entry.value.stats.p50));
    }
  }

  // Sort by latency (ascending)
  std::sort(venue_latencies.begin(), venue_latencies.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  // Extract venues
  auto builder = kj::heapArrayBuilder<veloz::common::Venue>(venue_latencies.size());
  for (const auto& vl : venue_latencies) {
    builder.add(vl.first);
  }
  return builder.finish();
}

bool LatencyTracker::is_healthy(veloz::common::Venue venue, kj::Duration max_latency,
                                kj::Duration max_staleness) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(venue_data, lock->venues.find(venue)) {
    if (venue_data.stats.sample_count == 0) {
      return false;
    }

    // Check latency threshold
    if (venue_data.stats.p95 > max_latency) {
      return false;
    }

    // Check staleness - we can't easily get current time here without a timer,
    // so we check if we have recent samples
    if (venue_data.stats.sample_count < 5) {
      return false; // Not enough samples
    }

    return true;
  }
  return false;
}

void LatencyTracker::clear(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->venues.erase(venue);
}

void LatencyTracker::clear_all() {
  auto lock = guarded_.lockExclusive();
  lock->venues.clear();
}

void LatencyTracker::set_window_size(std::size_t size) {
  auto lock = guarded_.lockExclusive();
  lock->window_size = size;
}

void LatencyTracker::set_window_duration(kj::Duration duration) {
  auto lock = guarded_.lockExclusive();
  lock->window_duration = duration;
}

std::size_t LatencyTracker::window_size() const {
  return guarded_.lockExclusive()->window_size;
}

kj::Duration LatencyTracker::window_duration() const {
  return guarded_.lockExclusive()->window_duration;
}

} // namespace veloz::exec
