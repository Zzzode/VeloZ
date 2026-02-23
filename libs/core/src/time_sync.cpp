/**
 * @file time_sync.cpp
 * @brief Implementation of time synchronization and latency profiling
 */

#include "veloz/core/time_sync.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <kj/string-tree.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#define HAS_RDTSC 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define HAS_RDTSC 0
#else
#define HAS_RDTSC 0
#endif

namespace veloz::core {

// ============================================================================
// Static Members
// ============================================================================

std::atomic<double> TimeSyncManager::tsc_frequency_{0.0};
std::atomic<bool> TimeSyncManager::tsc_calibrated_{false};

// ============================================================================
// TimeSyncManager Implementation
// ============================================================================

TimeSyncManager::TimeSyncManager(Config config) : config_(kj::mv(config)) {
  // Initialize with default NTP servers if none provided
  auto lock = state_.lockExclusive();
  if (config_.ntp_servers.size() == 0) {
    config_.ntp_servers.add(kj::str("time.google.com"));
    config_.ntp_servers.add(kj::str("time.cloudflare.com"));
    config_.ntp_servers.add(kj::str("pool.ntp.org"));
  }

  // Calibrate TSC if available
  if (!tsc_calibrated_.load(std::memory_order_relaxed)) {
    calibrate_tsc();
  }
}

int64_t TimeSyncManager::now_ns() const {
  int64_t system_ns = system_time_ns();
  auto lock = state_.lockExclusive();
  if (lock->status == SyncStatus::Synchronized || lock->status == SyncStatus::Degraded) {
    return system_ns - lock->current_offset.offset_ns;
  }
  return system_ns;
}

std::chrono::steady_clock::time_point TimeSyncManager::now_steady() const {
  return std::chrono::steady_clock::now();
}

int64_t TimeSyncManager::system_time_ns() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

uint64_t TimeSyncManager::high_res_timestamp() {
#if HAS_RDTSC
  return __rdtsc();
#else
  // Use steady_clock for non-x86 platforms
  auto now = std::chrono::steady_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
#endif
}

int64_t TimeSyncManager::high_res_to_ns(uint64_t timestamp) {
#if HAS_RDTSC
  double freq = tsc_frequency_.load(std::memory_order_relaxed);
  if (freq > 0.0) {
    return static_cast<int64_t>(static_cast<double>(timestamp) / freq * 1e9);
  }
#endif
  // For non-TSC platforms, timestamp is already in nanoseconds
  return static_cast<int64_t>(timestamp);
}

void TimeSyncManager::calibrate_tsc() {
#if HAS_RDTSC
  // Calibrate TSC frequency by measuring against steady_clock
  auto start_time = std::chrono::steady_clock::now();
  uint64_t start_tsc = __rdtsc();

  // Busy wait for ~10ms
  while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(10)) {
    // Spin
  }

  auto end_time = std::chrono::steady_clock::now();
  uint64_t end_tsc = __rdtsc();

  auto elapsed_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
  uint64_t tsc_diff = end_tsc - start_tsc;

  if (elapsed_ns > 0) {
    double freq = static_cast<double>(tsc_diff) / static_cast<double>(elapsed_ns) * 1e9;
    tsc_frequency_.store(freq, std::memory_order_relaxed);
    tsc_calibrated_.store(true, std::memory_order_relaxed);
  }
#else
  tsc_frequency_.store(1.0, std::memory_order_relaxed);
  tsc_calibrated_.store(true, std::memory_order_relaxed);
#endif
}

void TimeSyncManager::start(kj::Timer& timer, kj::Network& network) {
  auto lock = state_.lockExclusive();
  lock->running = true;
  lock->status = SyncStatus::Syncing;
}

void TimeSyncManager::stop() {
  auto lock = state_.lockExclusive();
  lock->running = false;
}

kj::Promise<void> TimeSyncManager::sync_now(kj::Timer& timer, kj::Network& network) {
  // For now, just update status - actual NTP implementation would go here
  auto lock = state_.lockExclusive();
  lock->status = SyncStatus::Synchronized;
  lock->current_offset.confidence = 0.9;
  lock->current_offset.measurement_ns = system_time_ns();
  return kj::READY_NOW;
}

void TimeSyncManager::calibrate_exchange(kj::StringPtr exchange, int64_t exchange_time_ns,
                                         int64_t local_time_ns, int64_t round_trip_ns) {
  auto lock = state_.lockExclusive();

  // Calculate offset: positive means local is ahead of exchange
  // Account for network latency by assuming symmetric delay
  int64_t one_way_delay = round_trip_ns / 2;
  int64_t offset = local_time_ns - exchange_time_ns - one_way_delay;

  ClockOffset sample;
  sample.offset_ns = offset;
  sample.round_trip_ns = round_trip_ns;
  sample.measurement_ns = local_time_ns;
  sample.confidence =
      1.0 - (static_cast<double>(round_trip_ns) / 1e9); // Lower confidence for high RTT
  if (sample.confidence < 0.1)
    sample.confidence = 0.1;

  // Get or create exchange data
  kj::String exchange_key = kj::heapString(exchange);
  KJ_IF_SOME(data, lock->exchange_offsets.find(exchange_key)) {
    data.samples.add(sample);

    // Keep only recent samples
    if (data.samples.size() > config_.exchange_sample_count) {
      auto new_samples = kj::Vector<ClockOffset>();
      size_t start = data.samples.size() - config_.exchange_sample_count;
      for (size_t i = start; i < data.samples.size(); ++i) {
        new_samples.add(data.samples[i]);
      }
      data.samples = kj::mv(new_samples);
    }

    // Calculate weighted average offset
    double total_weight = 0.0;
    double weighted_offset = 0.0;
    for (const auto& s : data.samples) {
      weighted_offset += static_cast<double>(s.offset_ns) * s.confidence;
      total_weight += s.confidence;
    }

    if (total_weight > 0.0) {
      data.calibrated_offset.offset_ns = static_cast<int64_t>(weighted_offset / total_weight);
      data.calibrated_offset.confidence = total_weight / static_cast<double>(data.samples.size());
      data.calibrated_offset.measurement_ns = local_time_ns;
    }
  }
  else {
    State::ExchangeData new_data;
    new_data.samples.add(sample);
    new_data.calibrated_offset = sample;
    lock->exchange_offsets.insert(kj::mv(exchange_key), kj::mv(new_data));
  }
}

kj::Maybe<ClockOffset> TimeSyncManager::get_exchange_offset(kj::StringPtr exchange) const {
  auto lock = state_.lockExclusive();
  KJ_IF_SOME(data, lock->exchange_offsets.find(exchange)) {
    if (data.calibrated_offset.is_valid()) {
      return data.calibrated_offset;
    }
  }
  return kj::none;
}

int64_t TimeSyncManager::to_exchange_time(kj::StringPtr exchange, int64_t local_time_ns) const {
  KJ_IF_SOME(offset, get_exchange_offset(exchange)) {
    return local_time_ns - offset.offset_ns;
  }
  return local_time_ns;
}

int64_t TimeSyncManager::from_exchange_time(kj::StringPtr exchange,
                                            int64_t exchange_time_ns) const {
  KJ_IF_SOME(offset, get_exchange_offset(exchange)) {
    return exchange_time_ns + offset.offset_ns;
  }
  return exchange_time_ns;
}

SyncStatus TimeSyncManager::status() const {
  return state_.lockExclusive()->status;
}

ClockOffset TimeSyncManager::current_offset() const {
  return state_.lockExclusive()->current_offset;
}

ClockDrift TimeSyncManager::current_drift() const {
  return state_.lockExclusive()->current_drift;
}

bool TimeSyncManager::is_synchronized(int64_t tolerance_ns) const {
  auto lock = state_.lockExclusive();
  if (lock->status != SyncStatus::Synchronized && lock->status != SyncStatus::Degraded) {
    return false;
  }
  return std::abs(lock->current_offset.offset_ns) <= tolerance_ns;
}

void TimeSyncManager::set_status_callback(StatusCallback callback) {
  auto lock = state_.lockExclusive();
  lock->status_callback = kj::mv(callback);
}

void TimeSyncManager::update_status(SyncStatus new_status, const ClockOffset& offset) {
  auto lock = state_.lockExclusive();
  SyncStatus old_status = lock->status;
  lock->status = new_status;
  lock->current_offset = offset;

  if (old_status != new_status) {
    KJ_IF_SOME(callback, lock->status_callback) {
      callback(new_status, offset);
    }
  }
}

void TimeSyncManager::update_drift() {
  auto lock = state_.lockExclusive();

  if (lock->offset_samples.size() < 2) {
    return;
  }

  // Calculate drift using linear regression
  double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
  size_t n = lock->offset_samples.size();

  for (const auto& sample : lock->offset_samples) {
    double x = static_cast<double>(sample.first);  // time
    double y = static_cast<double>(sample.second); // offset
    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_xx += x * x;
  }

  double slope = (static_cast<double>(n) * sum_xy - sum_x * sum_y) /
                 (static_cast<double>(n) * sum_xx - sum_x * sum_x);

  lock->current_drift.drift_ns_per_sec = slope * 1e9; // Convert to ns/s
  lock->current_drift.drift_ppm = slope * 1e6;        // Convert to ppm
  lock->current_drift.last_measurement_ns = system_time_ns();
  lock->current_drift.sample_count = n;
}

kj::String TimeSyncManager::stats_json() const {
  auto lock = state_.lockExclusive();

  const char* status_str = "unknown";
  switch (lock->status) {
  case SyncStatus::Unknown:
    status_str = "unknown";
    break;
  case SyncStatus::Syncing:
    status_str = "syncing";
    break;
  case SyncStatus::Synchronized:
    status_str = "synchronized";
    break;
  case SyncStatus::Degraded:
    status_str = "degraded";
    break;
  case SyncStatus::Failed:
    status_str = "failed";
    break;
  }

  return kj::str("{", "\"status\":\"", status_str, "\",",
                 "\"offset_ns\":", lock->current_offset.offset_ns, ",",
                 "\"offset_confidence\":", lock->current_offset.confidence, ",",
                 "\"drift_ppm\":", lock->current_drift.drift_ppm, ",",
                 "\"drift_samples\":", lock->current_drift.sample_count, ",",
                 "\"exchange_count\":", lock->exchange_offsets.size(), "}");
}

// ============================================================================
// LatencyProfiler Implementation
// ============================================================================

LatencyProfiler::LatencyProfiler(kj::StringPtr name)
    : name_(kj::heapString(name)), start_timestamp_(TimeSyncManager::high_res_timestamp()) {}

void LatencyProfiler::checkpoint(kj::StringPtr name) {
  checkpoints_.add(LatencyCheckpoint{name, TimeSyncManager::high_res_timestamp()});
}

LatencyProfile LatencyProfiler::finish() {
  uint64_t end_timestamp = TimeSyncManager::high_res_timestamp();

  LatencyProfile profile;
  profile.name = kj::heapString(name_);

  uint64_t prev_timestamp = start_timestamp_;
  for (const auto& cp : checkpoints_) {
    int64_t duration = TimeSyncManager::high_res_to_ns(cp.timestamp) -
                       TimeSyncManager::high_res_to_ns(prev_timestamp);
    profile.segments.add(std::make_pair(kj::heapString(cp.name), duration));
    prev_timestamp = cp.timestamp;
  }

  // Add final segment if there were checkpoints
  if (checkpoints_.size() > 0) {
    int64_t final_duration = TimeSyncManager::high_res_to_ns(end_timestamp) -
                             TimeSyncManager::high_res_to_ns(prev_timestamp);
    profile.segments.add(std::make_pair(kj::str("end"), final_duration));
  }

  profile.total_ns = TimeSyncManager::high_res_to_ns(end_timestamp) -
                     TimeSyncManager::high_res_to_ns(start_timestamp_);

  return profile;
}

int64_t LatencyProfiler::elapsed_ns() const {
  return TimeSyncManager::high_res_to_ns(TimeSyncManager::high_res_timestamp()) -
         TimeSyncManager::high_res_to_ns(start_timestamp_);
}

kj::String LatencyProfile::to_string() const {
  kj::StringTree tree = kj::strTree("Profile: ", name, " (total: ", total_ns, " ns)\n");
  for (const auto& seg : segments) {
    double pct = total_ns > 0
                     ? 100.0 * static_cast<double>(seg.second) / static_cast<double>(total_ns)
                     : 0.0;
    tree = kj::strTree(kj::mv(tree), "  ", seg.first, ": ", seg.second, " ns (", pct, "%)\n");
  }
  return tree.flatten();
}

kj::String LatencyProfile::to_json() const {
  kj::StringTree tree =
      kj::strTree("{\"name\":\"", name, "\",\"total_ns\":", total_ns, ",\"segments\":[");
  bool first = true;
  for (const auto& seg : segments) {
    if (!first)
      tree = kj::strTree(kj::mv(tree), ",");
    tree = kj::strTree(kj::mv(tree), "{\"name\":\"", seg.first, "\",\"duration_ns\":", seg.second,
                       "}");
    first = false;
  }
  tree = kj::strTree(kj::mv(tree), "]}");
  return tree.flatten();
}

// ============================================================================
// ScopedLatency Implementation
// ============================================================================

ScopedLatency::ScopedLatency(kj::StringPtr name, Callback callback)
    : name_(kj::heapString(name)), start_timestamp_(TimeSyncManager::high_res_timestamp()),
      callback_(kj::mv(callback)) {}

ScopedLatency::~ScopedLatency() {
  if (!cancelled_) {
    int64_t duration = elapsed_ns();
    callback_(duration);
  }
}

int64_t ScopedLatency::elapsed_ns() const {
  return TimeSyncManager::high_res_to_ns(TimeSyncManager::high_res_timestamp()) -
         TimeSyncManager::high_res_to_ns(start_timestamp_);
}

void ScopedLatency::cancel() {
  cancelled_ = true;
}

// ============================================================================
// Global Time Sync
// ============================================================================

namespace {
kj::Maybe<kj::Own<TimeSyncManager>> global_time_sync_instance;
}

TimeSyncManager& global_time_sync() {
  KJ_IF_SOME(instance, global_time_sync_instance) {
    return *instance;
  }
  // Create default instance
  global_time_sync_instance = kj::heap<TimeSyncManager>();
  return *KJ_ASSERT_NONNULL(global_time_sync_instance);
}

void init_time_sync(TimeSyncManager::Config config) {
  global_time_sync_instance = kj::heap<TimeSyncManager>(kj::mv(config));
}

// ============================================================================
// Utility Functions
// ============================================================================

kj::String format_duration_ns(int64_t ns) {
  if (ns < 1000) {
    return kj::str(ns, "ns");
  } else if (ns < 1000000) {
    return kj::str(static_cast<double>(ns) / 1000.0, "us");
  } else if (ns < 1000000000) {
    return kj::str(static_cast<double>(ns) / 1000000.0, "ms");
  } else {
    return kj::str(static_cast<double>(ns) / 1000000000.0, "s");
  }
}

kj::Maybe<int64_t> parse_duration_ns(kj::StringPtr str) {
  if (str.size() == 0) {
    return kj::none;
  }

  // Find where the number ends
  size_t num_end = 0;
  bool has_decimal = false;
  for (size_t i = 0; i < str.size(); ++i) {
    char c = str[i];
    if (c >= '0' && c <= '9') {
      num_end = i + 1;
    } else if (c == '.' && !has_decimal) {
      has_decimal = true;
      num_end = i + 1;
    } else {
      break;
    }
  }

  if (num_end == 0) {
    return kj::none;
  }

  // Parse the number
  double value = std::strtod(str.begin(), nullptr);

  // Parse the unit
  kj::StringPtr unit = str.slice(num_end);
  if (unit == "ns" || unit.size() == 0) {
    return static_cast<int64_t>(value);
  } else if (unit == "us") {
    return static_cast<int64_t>(value * 1000.0);
  } else if (unit == "ms") {
    return static_cast<int64_t>(value * 1000000.0);
  } else if (unit == "s") {
    return static_cast<int64_t>(value * 1000000000.0);
  }

  return kj::none;
}

} // namespace veloz::core
