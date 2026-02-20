/**
 * @file market_quality.h
 * @brief Market data quality scoring and anomaly detection
 *
 * This file provides tools for assessing market data quality,
 * detecting anomalies (price spikes, volume anomalies), and
 * filtering/sampling market data.
 */

#pragma once

#include "veloz/market/market_event.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::market {

/**
 * @brief Types of anomalies that can be detected
 */
enum class AnomalyType : uint8_t {
  None = 0,
  PriceSpike = 1,       ///< Sudden large price movement
  VolumeSpike = 2,      ///< Abnormally high volume
  VolumeDrop = 3,       ///< Abnormally low volume
  SpreadWidening = 4,   ///< Spread exceeds normal range
  StaleData = 5,        ///< Data hasn't updated in expected time
  SequenceGap = 6,      ///< Gap in sequence numbers
  TimestampAnomaly = 7, ///< Timestamp out of expected range
};

/**
 * @brief Convert anomaly type to string
 */
[[nodiscard]] kj::StringPtr anomaly_type_to_string(AnomalyType type);

/**
 * @brief Detected anomaly information
 *
 * Note: Anomaly is move-only because kj::String is not copyable.
 * Use kj::mv() to transfer ownership or pass by const reference.
 */
struct Anomaly {
  AnomalyType type{AnomalyType::None};
  double severity{0.0};    ///< Severity score (0.0 - 1.0)
  double expected{0.0};    ///< Expected value
  double actual{0.0};      ///< Actual value
  int64_t timestamp_ns{0}; ///< When anomaly was detected
  kj::String description;  ///< Human-readable description

  // Move-only type (kj::String is not copyable)
  Anomaly() = default;
  Anomaly(Anomaly&&) = default;
  Anomaly& operator=(Anomaly&&) = default;
  Anomaly(const Anomaly&) = delete;
  Anomaly& operator=(const Anomaly&) = delete;
};

/**
 * @brief Quality score breakdown
 */
struct QualityScore {
  double overall{1.0};      ///< Overall quality score (0.0 - 1.0)
  double freshness{1.0};    ///< Data freshness score
  double completeness{1.0}; ///< Data completeness score
  double consistency{1.0};  ///< Data consistency score
  double reliability{1.0};  ///< Source reliability score
  int64_t anomaly_count{0}; ///< Number of anomalies detected
  int64_t sample_count{0};  ///< Number of samples analyzed
};

/**
 * @brief Callback for anomaly notifications
 */
using AnomalyCallback = kj::Function<void(const Anomaly&)>;

/**
 * @brief Market data quality analyzer
 *
 * Analyzes market data quality and detects anomalies in real-time.
 */
class MarketQualityAnalyzer final {
public:
  /**
   * @brief Configuration for the analyzer
   */
  struct Config {
    // Price spike detection
    double price_spike_threshold{0.05}; ///< 5% price change threshold
    size_t price_lookback_count{100};   ///< Trades to consider for baseline

    // Volume anomaly detection
    double volume_spike_multiplier{5.0}; ///< Volume > 5x average is spike
    double volume_drop_threshold{0.1};   ///< Volume < 10% average is drop
    size_t volume_lookback_count{100};   ///< Trades for volume baseline

    // Spread monitoring
    double max_spread_bps{100.0}; ///< Max acceptable spread in basis points

    // Staleness detection
    int64_t stale_threshold_ms{5000}; ///< Data older than 5s is stale

    // Timestamp validation
    int64_t max_clock_skew_ms{1000}; ///< Max acceptable clock skew

    // Quality scoring weights
    double freshness_weight{0.3};
    double completeness_weight{0.25};
    double consistency_weight{0.25};
    double reliability_weight{0.2};
  };

  MarketQualityAnalyzer();
  explicit MarketQualityAnalyzer(Config config);
  ~MarketQualityAnalyzer() = default;

  // Non-copyable
  MarketQualityAnalyzer(const MarketQualityAnalyzer&) = delete;
  MarketQualityAnalyzer& operator=(const MarketQualityAnalyzer&) = delete;

  /**
   * @brief Analyze a trade event
   * @param trade Trade data
   * @param timestamp_ns Event timestamp in nanoseconds
   * @return Detected anomalies (empty if none)
   */
  kj::Vector<Anomaly> analyze_trade(const TradeData& trade, int64_t timestamp_ns);

  /**
   * @brief Analyze a book update
   * @param best_bid Best bid price
   * @param best_ask Best ask price
   * @param timestamp_ns Event timestamp in nanoseconds
   * @return Detected anomalies (empty if none)
   */
  kj::Vector<Anomaly> analyze_book(double best_bid, double best_ask, int64_t timestamp_ns);

  /**
   * @brief Analyze a market event
   * @param event The market event to analyze
   * @return Detected anomalies (empty if none)
   */
  kj::Vector<Anomaly> analyze_event(const MarketEvent& event);

  /**
   * @brief Check for stale data
   * @param current_time_ns Current time in nanoseconds
   * @return Stale data anomaly if detected, nullptr otherwise
   */
  kj::Maybe<Anomaly> check_staleness(int64_t current_time_ns);

  /**
   * @brief Get current quality score
   * @return Quality score breakdown
   */
  [[nodiscard]] QualityScore quality_score() const;

  /**
   * @brief Get recent anomalies
   * @param count Maximum number to return (0 = all)
   * @return Vector of recent anomalies (newest first)
   */
  [[nodiscard]] kj::Vector<Anomaly> recent_anomalies(size_t count = 10) const;

  /**
   * @brief Set callback for anomaly notifications
   */
  void set_anomaly_callback(AnomalyCallback callback);

  /**
   * @brief Clear anomaly callback
   */
  void clear_anomaly_callback();

  /**
   * @brief Reset analyzer state
   */
  void reset();

  /**
   * @brief Get statistics
   */
  [[nodiscard]] int64_t total_events_analyzed() const {
    return total_events_;
  }
  [[nodiscard]] int64_t total_anomalies_detected() const {
    return total_anomalies_;
  }

private:
  /**
   * @brief Check for price spike
   */
  kj::Maybe<Anomaly> check_price_spike(double price, int64_t timestamp_ns);

  /**
   * @brief Check for volume anomaly
   */
  kj::Maybe<Anomaly> check_volume_anomaly(double volume, int64_t timestamp_ns);

  /**
   * @brief Check spread
   */
  kj::Maybe<Anomaly> check_spread(double bid, double ask, int64_t timestamp_ns);

  /**
   * @brief Check timestamp validity
   */
  kj::Maybe<Anomaly> check_timestamp(int64_t event_ts_ns, int64_t current_ts_ns);

  /**
   * @brief Record anomaly and notify callback
   */
  void record_anomaly(const Anomaly& anomaly);

  /**
   * @brief Update quality metrics
   */
  void update_quality_metrics(bool has_anomaly, int64_t timestamp_ns);

  Config config_;

  // Price tracking
  kj::Vector<double> recent_prices_;
  double price_sum_{0.0};

  // Volume tracking
  kj::Vector<double> recent_volumes_;
  double volume_sum_{0.0};

  // Timing tracking
  int64_t last_event_time_ns_{0};
  int64_t first_event_time_ns_{0};

  // Anomaly history
  kj::Vector<Anomaly> anomaly_history_;
  static constexpr size_t kMaxAnomalyHistory = 1000;

  // Quality metrics
  int64_t total_events_{0};
  int64_t total_anomalies_{0};
  int64_t stale_count_{0};
  int64_t gap_count_{0};

  // Callback
  kj::Maybe<AnomalyCallback> anomaly_callback_;
};

/**
 * @brief Data sampler for reducing data rate
 *
 * Provides configurable sampling strategies for high-frequency data.
 */
class DataSampler final {
public:
  /**
   * @brief Sampling strategy
   */
  enum class Strategy {
    None,          ///< No sampling (pass all data)
    TimeInterval,  ///< Sample at fixed time intervals
    CountInterval, ///< Sample every N events
    Adaptive,      ///< Adaptive sampling based on volatility
  };

  /**
   * @brief Configuration for the sampler
   */
  struct Config {
    Strategy strategy{Strategy::None};
    int64_t time_interval_ms{100};     ///< For TimeInterval strategy
    size_t count_interval{10};         ///< For CountInterval strategy
    double volatility_threshold{0.01}; ///< For Adaptive strategy
  };

  DataSampler();
  explicit DataSampler(Config config);

  /**
   * @brief Check if an event should be sampled (passed through)
   * @param timestamp_ns Event timestamp
   * @param price Optional price for adaptive sampling
   * @return true if event should be kept, false if filtered
   */
  bool should_sample(int64_t timestamp_ns, kj::Maybe<double> price = kj::none);

  /**
   * @brief Reset sampler state
   */
  void reset();

  /**
   * @brief Get sampling statistics
   */
  [[nodiscard]] int64_t total_events() const {
    return total_events_;
  }
  [[nodiscard]] int64_t sampled_events() const {
    return sampled_events_;
  }
  [[nodiscard]] double sample_rate() const {
    return total_events_ > 0 ? static_cast<double>(sampled_events_) / total_events_ : 1.0;
  }

private:
  Config config_;
  int64_t last_sample_time_ns_{0};
  size_t event_count_{0};
  double last_price_{0.0};
  int64_t total_events_{0};
  int64_t sampled_events_{0};
};

} // namespace veloz::market
