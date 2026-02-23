/**
 * @file time_sync.h
 * @brief Time synchronization and calibration for VeloZ
 *
 * This module provides:
 * - NTP-based time synchronization
 * - Exchange time offset calibration
 * - Clock drift monitoring
 * - High-precision timestamp utilities
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/timer.h>
#include <kj/vector.h>

namespace veloz::core {

// ============================================================================
// Time Source Configuration
// ============================================================================

/**
 * @brief Time source types for synchronization
 */
enum class TimeSource : uint8_t {
  System,   // System clock (default)
  NTP,      // Network Time Protocol
  Exchange, // Exchange server time
  GPS,      // GPS time (if available)
  PTP       // Precision Time Protocol
};

/**
 * @brief Time synchronization status
 */
enum class SyncStatus : uint8_t {
  Unknown,      // Not yet synchronized
  Syncing,      // Synchronization in progress
  Synchronized, // Successfully synchronized
  Degraded,     // Synchronized but with high drift
  Failed        // Synchronization failed
};

// ============================================================================
// Clock Offset and Drift
// ============================================================================

/**
 * @brief Clock offset measurement
 */
struct ClockOffset {
  int64_t offset_ns{0};       // Offset from reference (positive = local ahead)
  int64_t round_trip_ns{0};   // Round-trip time for measurement
  int64_t measurement_ns{0};  // When measurement was taken
  double confidence{0.0};     // Confidence level (0-1)

  [[nodiscard]] bool is_valid() const { return confidence > 0.0; }
};

/**
 * @brief Clock drift statistics
 */
struct ClockDrift {
  double drift_ppm{0.0};           // Drift in parts per million
  double drift_ns_per_sec{0.0};    // Drift in nanoseconds per second
  int64_t last_measurement_ns{0};  // When last measured
  size_t sample_count{0};          // Number of samples used

  [[nodiscard]] bool is_stable() const {
    return sample_count >= 10 && std::abs(drift_ppm) < 100.0;
  }
};

// ============================================================================
// Time Synchronization Manager
// ============================================================================

// ============================================================================
// Time Sync Manager Config
// ============================================================================

struct TimeSyncConfig {
  // NTP configuration
  kj::Vector<kj::String> ntp_servers;
  int64_t ntp_poll_interval_ms = 60000;  // 1 minute
  int64_t ntp_timeout_ms = 5000;         // 5 seconds

  // Drift monitoring
  int64_t drift_sample_interval_ms = 1000;  // 1 second
  size_t drift_sample_count = 60;           // 1 minute of samples
  double max_drift_ppm = 100.0;             // Max acceptable drift

  // Exchange calibration
  int64_t exchange_poll_interval_ms = 30000;  // 30 seconds
  size_t exchange_sample_count = 10;          // Samples per calibration

  // Alerts
  int64_t max_offset_ns = 1000000;  // 1ms max offset before alert
};

/**
 * @brief Manages time synchronization across the system
 *
 * Provides:
 * - NTP synchronization
 * - Exchange time calibration
 * - Clock drift monitoring
 * - Synchronized timestamps
 */
class TimeSyncManager {
public:
  using Config = TimeSyncConfig;

  explicit TimeSyncManager(Config config = {});
  ~TimeSyncManager() = default;

  KJ_DISALLOW_COPY_AND_MOVE(TimeSyncManager);

  // -------------------------------------------------------------------------
  // Synchronized Time Access
  // -------------------------------------------------------------------------

  /**
   * @brief Get current synchronized time in nanoseconds since epoch
   *
   * Returns system time adjusted by the current offset estimate.
   */
  [[nodiscard]] int64_t now_ns() const;

  /**
   * @brief Get current synchronized time as steady clock point
   *
   * For measuring durations, use steady clock which is monotonic.
   */
  [[nodiscard]] std::chrono::steady_clock::time_point now_steady() const;

  /**
   * @brief Get raw system time without synchronization adjustment
   */
  [[nodiscard]] static int64_t system_time_ns();

  /**
   * @brief Get high-resolution timestamp for latency measurement
   *
   * Uses RDTSC or equivalent for sub-microsecond precision.
   */
  [[nodiscard]] static uint64_t high_res_timestamp();

  /**
   * @brief Convert high-resolution timestamp to nanoseconds
   */
  [[nodiscard]] static int64_t high_res_to_ns(uint64_t timestamp);

  // -------------------------------------------------------------------------
  // Synchronization Control
  // -------------------------------------------------------------------------

  /**
   * @brief Start time synchronization
   *
   * Begins periodic NTP synchronization and drift monitoring.
   */
  void start(kj::Timer& timer, kj::Network& network);

  /**
   * @brief Stop time synchronization
   */
  void stop();

  /**
   * @brief Force immediate synchronization
   */
  kj::Promise<void> sync_now(kj::Timer& timer, kj::Network& network);

  // -------------------------------------------------------------------------
  // Exchange Time Calibration
  // -------------------------------------------------------------------------

  /**
   * @brief Calibrate time offset for a specific exchange
   *
   * @param exchange Exchange identifier
   * @param exchange_time_ns Exchange server time in nanoseconds
   * @param local_time_ns Local time when exchange time was received
   * @param round_trip_ns Round-trip time for the request
   */
  void calibrate_exchange(kj::StringPtr exchange, int64_t exchange_time_ns,
                          int64_t local_time_ns, int64_t round_trip_ns);

  /**
   * @brief Get calibrated offset for an exchange
   */
  [[nodiscard]] kj::Maybe<ClockOffset> get_exchange_offset(kj::StringPtr exchange) const;

  /**
   * @brief Convert local time to exchange time
   */
  [[nodiscard]] int64_t to_exchange_time(kj::StringPtr exchange, int64_t local_time_ns) const;

  /**
   * @brief Convert exchange time to local time
   */
  [[nodiscard]] int64_t from_exchange_time(kj::StringPtr exchange, int64_t exchange_time_ns) const;

  // -------------------------------------------------------------------------
  // Status and Monitoring
  // -------------------------------------------------------------------------

  /**
   * @brief Get current synchronization status
   */
  [[nodiscard]] SyncStatus status() const;

  /**
   * @brief Get current clock offset from reference
   */
  [[nodiscard]] ClockOffset current_offset() const;

  /**
   * @brief Get current clock drift estimate
   */
  [[nodiscard]] ClockDrift current_drift() const;

  /**
   * @brief Check if time is synchronized within tolerance
   */
  [[nodiscard]] bool is_synchronized(int64_t tolerance_ns = 1000000) const;

  /**
   * @brief Set callback for synchronization status changes
   */
  using StatusCallback = kj::Function<void(SyncStatus, const ClockOffset&)>;
  void set_status_callback(StatusCallback callback);

  /**
   * @brief Get synchronization statistics as JSON
   */
  [[nodiscard]] kj::String stats_json() const;

private:
  Config config_;

  struct State {
    SyncStatus status{SyncStatus::Unknown};
    ClockOffset current_offset;
    ClockDrift current_drift;

    // Offset samples for drift calculation
    kj::Vector<std::pair<int64_t, int64_t>> offset_samples;  // (time, offset)

    // Exchange calibration data
    struct ExchangeData {
      kj::Vector<ClockOffset> samples;
      ClockOffset calibrated_offset;
    };
    kj::HashMap<kj::String, ExchangeData> exchange_offsets;

    // Callbacks
    kj::Maybe<StatusCallback> status_callback;

    // Control
    bool running{false};
  };

  kj::MutexGuarded<State> state_;

  // High-resolution timestamp calibration
  static std::atomic<double> tsc_frequency_;
  static std::atomic<bool> tsc_calibrated_;

  void update_status(SyncStatus new_status, const ClockOffset& offset);
  void update_drift();
  static void calibrate_tsc();
};

// ============================================================================
// Latency Profiler
// ============================================================================

/**
 * @brief Latency profiling point
 */
struct LatencyCheckpoint {
  kj::StringPtr name;
  uint64_t timestamp;
};

/**
 * @brief Latency profile result
 */
struct LatencyProfile {
  kj::String name;
  kj::Vector<std::pair<kj::String, int64_t>> segments;  // (name, duration_ns)
  int64_t total_ns{0};

  [[nodiscard]] kj::String to_string() const;
  [[nodiscard]] kj::String to_json() const;
};

/**
 * @brief Latency profiler for measuring code path latencies
 *
 * Usage:
 *   LatencyProfiler profiler("order_processing");
 *   profiler.checkpoint("validation");
 *   // ... validation code ...
 *   profiler.checkpoint("risk_check");
 *   // ... risk check code ...
 *   profiler.checkpoint("execution");
 *   // ... execution code ...
 *   auto profile = profiler.finish();
 */
class LatencyProfiler {
public:
  explicit LatencyProfiler(kj::StringPtr name);

  /**
   * @brief Add a checkpoint
   */
  void checkpoint(kj::StringPtr name);

  /**
   * @brief Finish profiling and get results
   */
  [[nodiscard]] LatencyProfile finish();

  /**
   * @brief Get elapsed time since start
   */
  [[nodiscard]] int64_t elapsed_ns() const;

private:
  kj::String name_;
  uint64_t start_timestamp_;
  kj::Vector<LatencyCheckpoint> checkpoints_;
};

// ============================================================================
// Scoped Latency Measurement
// ============================================================================

/**
 * @brief RAII-style latency measurement
 *
 * Usage:
 *   {
 *     ScopedLatency latency("operation_name", [](int64_t ns) {
 *       histogram.record(ns);
 *     });
 *     // ... code to measure ...
 *   }  // Callback invoked with duration
 */
class ScopedLatency {
public:
  using Callback = kj::Function<void(int64_t)>;

  explicit ScopedLatency(kj::StringPtr name, Callback callback);
  ~ScopedLatency();

  KJ_DISALLOW_COPY_AND_MOVE(ScopedLatency);

  /**
   * @brief Get elapsed time so far
   */
  [[nodiscard]] int64_t elapsed_ns() const;

  /**
   * @brief Cancel measurement (callback won't be invoked)
   */
  void cancel();

private:
  kj::String name_;
  uint64_t start_timestamp_;
  Callback callback_;
  bool cancelled_{false};
};

// ============================================================================
// Global Time Sync Access
// ============================================================================

/**
 * @brief Get the global time sync manager
 */
TimeSyncManager& global_time_sync();

/**
 * @brief Initialize global time sync with custom config
 */
void init_time_sync(TimeSyncManager::Config config);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Format nanoseconds as human-readable string
 */
[[nodiscard]] kj::String format_duration_ns(int64_t ns);

/**
 * @brief Parse duration string to nanoseconds
 *
 * Supports: "1ms", "100us", "1s", "1.5ms", etc.
 */
[[nodiscard]] kj::Maybe<int64_t> parse_duration_ns(kj::StringPtr str);

} // namespace veloz::core
