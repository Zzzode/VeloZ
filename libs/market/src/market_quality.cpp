#include "veloz/market/market_quality.h"

#include <cmath> // std::abs - standard C++ math function (no KJ equivalent)
#include <kj/debug.h>

namespace {
// Helper to build description strings using kj::str then convert to std::string
// (Anomaly.description is std::string because kj::String is not copyable)
template <typename... Args> std::string make_description(Args&&... args) {
  kj::String kjStr = kj::str(std::forward<Args>(args)...);
  return std::string(kjStr.cStr());
}
} // namespace

namespace veloz::market {

kj::StringPtr anomaly_type_to_string(AnomalyType type) {
  switch (type) {
  case AnomalyType::None:
    return "None"_kj;
  case AnomalyType::PriceSpike:
    return "PriceSpike"_kj;
  case AnomalyType::VolumeSpike:
    return "VolumeSpike"_kj;
  case AnomalyType::VolumeDrop:
    return "VolumeDrop"_kj;
  case AnomalyType::SpreadWidening:
    return "SpreadWidening"_kj;
  case AnomalyType::StaleData:
    return "StaleData"_kj;
  case AnomalyType::SequenceGap:
    return "SequenceGap"_kj;
  case AnomalyType::TimestampAnomaly:
    return "TimestampAnomaly"_kj;
  }
  return "Unknown"_kj;
}

// ============================================================================
// MarketQualityAnalyzer Implementation
// ============================================================================

MarketQualityAnalyzer::MarketQualityAnalyzer() : MarketQualityAnalyzer(Config{}) {}

MarketQualityAnalyzer::MarketQualityAnalyzer(Config config) : config_(config) {}

kj::Vector<Anomaly> MarketQualityAnalyzer::analyze_trade(const TradeData& trade,
                                                         int64_t timestamp_ns) {
  kj::Vector<Anomaly> anomalies;
  total_events_++;

  // Check price spike
  KJ_IF_SOME(anomaly, check_price_spike(trade.price, timestamp_ns)) {
    anomalies.add(kj::mv(anomaly));
  }

  // Check volume anomaly
  KJ_IF_SOME(anomaly, check_volume_anomaly(trade.qty, timestamp_ns)) {
    anomalies.add(kj::mv(anomaly));
  }

  // Update tracking data
  recent_prices_.add(trade.price);
  price_sum_ += trade.price;
  if (recent_prices_.size() > config_.price_lookback_count) {
    price_sum_ -= recent_prices_[0];
    kj::Vector<double> new_prices;
    for (size_t i = 1; i < recent_prices_.size(); ++i) {
      new_prices.add(recent_prices_[i]);
    }
    recent_prices_ = kj::mv(new_prices);
  }

  recent_volumes_.add(trade.qty);
  volume_sum_ += trade.qty;
  if (recent_volumes_.size() > config_.volume_lookback_count) {
    volume_sum_ -= recent_volumes_[0];
    kj::Vector<double> new_volumes;
    for (size_t i = 1; i < recent_volumes_.size(); ++i) {
      new_volumes.add(recent_volumes_[i]);
    }
    recent_volumes_ = kj::mv(new_volumes);
  }

  // Update timing
  if (first_event_time_ns_ == 0) {
    first_event_time_ns_ = timestamp_ns;
  }
  last_event_time_ns_ = timestamp_ns;

  // Record anomalies
  for (const auto& anomaly : anomalies) {
    record_anomaly(anomaly);
  }

  update_quality_metrics(!anomalies.empty(), timestamp_ns);
  return anomalies;
}

kj::Vector<Anomaly> MarketQualityAnalyzer::analyze_book(double best_bid, double best_ask,
                                                        int64_t timestamp_ns) {
  kj::Vector<Anomaly> anomalies;
  total_events_++;

  // Check spread
  KJ_IF_SOME(anomaly, check_spread(best_bid, best_ask, timestamp_ns)) {
    anomalies.add(kj::mv(anomaly));
  }

  // Update timing
  if (first_event_time_ns_ == 0) {
    first_event_time_ns_ = timestamp_ns;
  }
  last_event_time_ns_ = timestamp_ns;

  // Record anomalies
  for (const auto& anomaly : anomalies) {
    record_anomaly(anomaly);
  }

  update_quality_metrics(!anomalies.empty(), timestamp_ns);
  return anomalies;
}

kj::Vector<Anomaly> MarketQualityAnalyzer::analyze_event(const MarketEvent& event) {
  kj::Vector<Anomaly> result;

  // Process event data using kj::OneOf
  if (event.data.is<TradeData>()) {
    // TradeData implies Trade event type
    const auto& trade = event.data.get<TradeData>();
    result = analyze_trade(trade, event.ts_exchange_ns);
  } else if (event.data.is<BookData>()) {
    // BookData implies BookTop or BookDelta event type
    const auto& book = event.data.get<BookData>();
    if (book.bids.size() > 0 && book.asks.size() > 0) {
      result = analyze_book(book.bids[0].price, book.asks[0].price, event.ts_exchange_ns);
    }
  }
  // EmptyData and KlineData are ignored

  return result;
}

kj::Maybe<Anomaly> MarketQualityAnalyzer::check_staleness(int64_t current_time_ns) {
  if (last_event_time_ns_ == 0) {
    return kj::none;
  }

  int64_t age_ms = (current_time_ns - last_event_time_ns_) / 1000000;
  if (age_ms > config_.stale_threshold_ms) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::StaleData;
    anomaly.severity = kj::min(1.0, static_cast<double>(age_ms) / (config_.stale_threshold_ms * 5));
    anomaly.expected = static_cast<double>(config_.stale_threshold_ms);
    anomaly.actual = static_cast<double>(age_ms);
    anomaly.timestamp_ns = current_time_ns;
    anomaly.description = make_description("Data stale for ", age_ms,
                                           "ms (threshold: ", config_.stale_threshold_ms, "ms)");
    stale_count_++;
    record_anomaly(anomaly);
    return anomaly;
  }
  return kj::none;
}

QualityScore MarketQualityAnalyzer::quality_score() const {
  QualityScore score;
  score.sample_count = total_events_;
  score.anomaly_count = total_anomalies_;

  if (total_events_ == 0) {
    return score;
  }

  // Freshness: based on staleness ratio
  double stale_ratio = static_cast<double>(stale_count_) / total_events_;
  score.freshness = kj::max(0.0, 1.0 - stale_ratio * 5.0);

  // Completeness: based on gap ratio
  double gap_ratio = static_cast<double>(gap_count_) / total_events_;
  score.completeness = kj::max(0.0, 1.0 - gap_ratio * 5.0);

  // Consistency: based on anomaly ratio
  double anomaly_ratio = static_cast<double>(total_anomalies_) / total_events_;
  score.consistency = kj::max(0.0, 1.0 - anomaly_ratio * 2.0);

  // Reliability: combination of other factors
  score.reliability = (score.freshness + score.completeness + score.consistency) / 3.0;

  // Overall weighted score
  score.overall = score.freshness * config_.freshness_weight +
                  score.completeness * config_.completeness_weight +
                  score.consistency * config_.consistency_weight +
                  score.reliability * config_.reliability_weight;

  return score;
}

kj::Vector<Anomaly> MarketQualityAnalyzer::recent_anomalies(size_t count) const {
  kj::Vector<Anomaly> result;
  size_t to_copy =
      (count == 0 || count > anomaly_history_.size()) ? anomaly_history_.size() : count;

  // Return newest first
  for (size_t i = 0; i < to_copy; ++i) {
    result.add(anomaly_history_[anomaly_history_.size() - 1 - i]);
  }
  return result;
}

void MarketQualityAnalyzer::set_anomaly_callback(AnomalyCallback callback) {
  anomaly_callback_ = kj::mv(callback);
}

void MarketQualityAnalyzer::clear_anomaly_callback() {
  anomaly_callback_ = kj::none;
}

void MarketQualityAnalyzer::reset() {
  recent_prices_.clear();
  recent_volumes_.clear();
  price_sum_ = 0.0;
  volume_sum_ = 0.0;
  last_event_time_ns_ = 0;
  first_event_time_ns_ = 0;
  anomaly_history_.clear();
  total_events_ = 0;
  total_anomalies_ = 0;
  stale_count_ = 0;
  gap_count_ = 0;
}

kj::Maybe<Anomaly> MarketQualityAnalyzer::check_price_spike(double price, int64_t timestamp_ns) {
  if (recent_prices_.size() < 2) {
    return kj::none;
  }

  double avg_price = price_sum_ / recent_prices_.size();
  double price_change = std::abs(price - avg_price) / avg_price;

  if (price_change > config_.price_spike_threshold) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::PriceSpike;
    anomaly.severity = kj::min(1.0, price_change / (config_.price_spike_threshold * 3));
    anomaly.expected = avg_price;
    anomaly.actual = price;
    anomaly.timestamp_ns = timestamp_ns;
    anomaly.description =
        make_description("Price spike: ", price_change * 100,
                         "% change (threshold: ", config_.price_spike_threshold * 100, "%)");
    return anomaly;
  }
  return kj::none;
}

kj::Maybe<Anomaly> MarketQualityAnalyzer::check_volume_anomaly(double volume,
                                                               int64_t timestamp_ns) {
  if (recent_volumes_.size() < 10) {
    return kj::none;
  }

  double avg_volume = volume_sum_ / recent_volumes_.size();
  if (avg_volume <= 0) {
    return kj::none;
  }

  double volume_ratio = volume / avg_volume;

  if (volume_ratio > config_.volume_spike_multiplier) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::VolumeSpike;
    anomaly.severity = kj::min(1.0, volume_ratio / (config_.volume_spike_multiplier * 2));
    anomaly.expected = avg_volume;
    anomaly.actual = volume;
    anomaly.timestamp_ns = timestamp_ns;
    anomaly.description = make_description("Volume spike: ", volume_ratio, "x average");
    return anomaly;
  }

  if (volume_ratio < config_.volume_drop_threshold) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::VolumeDrop;
    anomaly.severity = kj::min(1.0, (config_.volume_drop_threshold - volume_ratio) /
                                        config_.volume_drop_threshold);
    anomaly.expected = avg_volume;
    anomaly.actual = volume;
    anomaly.timestamp_ns = timestamp_ns;
    anomaly.description = make_description("Volume drop: ", volume_ratio * 100, "% of average");
    return anomaly;
  }

  return kj::none;
}

kj::Maybe<Anomaly> MarketQualityAnalyzer::check_spread(double bid, double ask,
                                                       int64_t timestamp_ns) {
  if (bid <= 0 || ask <= 0 || bid >= ask) {
    return kj::none;
  }

  double mid = (bid + ask) / 2.0;
  double spread_bps = ((ask - bid) / mid) * 10000.0;

  if (spread_bps > config_.max_spread_bps) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::SpreadWidening;
    anomaly.severity = kj::min(1.0, spread_bps / (config_.max_spread_bps * 3));
    anomaly.expected = config_.max_spread_bps;
    anomaly.actual = spread_bps;
    anomaly.timestamp_ns = timestamp_ns;
    anomaly.description = make_description("Spread widening: ", spread_bps,
                                           " bps (threshold: ", config_.max_spread_bps, ")");
    return anomaly;
  }
  return kj::none;
}

kj::Maybe<Anomaly> MarketQualityAnalyzer::check_timestamp(int64_t event_ts_ns,
                                                          int64_t current_ts_ns) {
  int64_t skew_ms = std::abs(event_ts_ns - current_ts_ns) / 1000000;

  if (skew_ms > config_.max_clock_skew_ms) {
    Anomaly anomaly;
    anomaly.type = AnomalyType::TimestampAnomaly;
    anomaly.severity = kj::min(1.0, static_cast<double>(skew_ms) / (config_.max_clock_skew_ms * 5));
    anomaly.expected = static_cast<double>(config_.max_clock_skew_ms);
    anomaly.actual = static_cast<double>(skew_ms);
    anomaly.timestamp_ns = current_ts_ns;
    anomaly.description = make_description("Timestamp skew: ", skew_ms,
                                           "ms (threshold: ", config_.max_clock_skew_ms, "ms)");
    return anomaly;
  }
  return kj::none;
}

void MarketQualityAnalyzer::record_anomaly(const Anomaly& anomaly) {
  total_anomalies_++;
  anomaly_history_.add(anomaly);

  // Trim history if needed
  while (anomaly_history_.size() > kMaxAnomalyHistory) {
    kj::Vector<Anomaly> new_history;
    for (size_t i = 1; i < anomaly_history_.size(); ++i) {
      new_history.add(kj::mv(anomaly_history_[i]));
    }
    anomaly_history_ = kj::mv(new_history);
  }

  // Notify callback
  KJ_IF_SOME(cb, anomaly_callback_) {
    cb(anomaly);
  }
}

void MarketQualityAnalyzer::update_quality_metrics(bool has_anomaly, int64_t timestamp_ns) {
  // Quality metrics are updated in quality_score() based on counters
  (void)has_anomaly;
  (void)timestamp_ns;
}

// ============================================================================
// DataSampler Implementation
// ============================================================================

DataSampler::DataSampler() : DataSampler(Config{}) {}

DataSampler::DataSampler(Config config) : config_(config) {}

bool DataSampler::should_sample(int64_t timestamp_ns, kj::Maybe<double> price) {
  total_events_++;

  if (config_.strategy == Strategy::None) {
    sampled_events_++;
    return true;
  }

  bool should_keep = false;

  switch (config_.strategy) {
  case Strategy::TimeInterval: {
    int64_t interval_ns = config_.time_interval_ms * 1000000LL;
    if (last_sample_time_ns_ == 0 || (timestamp_ns - last_sample_time_ns_) >= interval_ns) {
      should_keep = true;
      last_sample_time_ns_ = timestamp_ns;
    }
    break;
  }

  case Strategy::CountInterval: {
    event_count_++;
    if (event_count_ >= config_.count_interval) {
      should_keep = true;
      event_count_ = 0;
    }
    break;
  }

  case Strategy::Adaptive: {
    KJ_IF_SOME(p, price) {
      if (last_price_ == 0.0) {
        should_keep = true;
        last_price_ = p;
      } else {
        double change = std::abs(p - last_price_) / last_price_;
        if (change >= config_.volatility_threshold) {
          should_keep = true;
          last_price_ = p;
        }
      }
    }
    else {
      // No price provided, fall back to time-based
      int64_t interval_ns = config_.time_interval_ms * 1000000LL;
      if (last_sample_time_ns_ == 0 || (timestamp_ns - last_sample_time_ns_) >= interval_ns) {
        should_keep = true;
        last_sample_time_ns_ = timestamp_ns;
      }
    }
    break;
  }

  case Strategy::None:
    should_keep = true;
    break;
  }

  if (should_keep) {
    sampled_events_++;
    last_sample_time_ns_ = timestamp_ns;
  }

  return should_keep;
}

void DataSampler::reset() {
  last_sample_time_ns_ = 0;
  event_count_ = 0;
  last_price_ = 0.0;
  total_events_ = 0;
  sampled_events_ = 0;
}

} // namespace veloz::market
