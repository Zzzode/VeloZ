#include "veloz/market/market_quality.h"

#include <cmath> // std::abs - standard C++ math function (no KJ equivalent)
#include <kj/debug.h>

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

  // Track spread statistics
  if (best_bid > 0 && best_ask > 0 && best_bid < best_ask) {
    double mid = (best_bid + best_ask) / 2.0;
    double spread_bps = ((best_ask - best_bid) / mid) * 10000.0;

    // Update spread tracking
    current_spread_ = spread_bps;
    if (recent_spreads_.size() == 0) {
      min_spread_ = spread_bps;
      max_spread_ = spread_bps;
    } else {
      if (spread_bps < min_spread_)
        min_spread_ = spread_bps;
      if (spread_bps > max_spread_)
        max_spread_ = spread_bps;
    }

    recent_spreads_.add(spread_bps);
    spread_sum_ += spread_bps;

    // Trim spread history
    while (recent_spreads_.size() > kMaxSpreadSamples) {
      spread_sum_ -= recent_spreads_[0];
      kj::Vector<double> new_spreads;
      for (size_t i = 1; i < recent_spreads_.size(); ++i) {
        new_spreads.add(recent_spreads_[i]);
      }
      recent_spreads_ = kj::mv(new_spreads);
    }

    // Check for spread anomaly
    if (spread_bps > config_.max_spread_bps) {
      wide_spread_count_++;
      Anomaly anomaly;
      anomaly.type = AnomalyType::SpreadWidening;
      anomaly.severity = kj::min(1.0, spread_bps / (config_.max_spread_bps * 3));
      anomaly.expected = config_.max_spread_bps;
      anomaly.actual = spread_bps;
      anomaly.timestamp_ns = timestamp_ns;
      anomaly.description = kj::str("Spread widening: ", spread_bps,
                                    " bps (threshold: ", config_.max_spread_bps, ")");
      anomalies.add(kj::mv(anomaly));
    }
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
    anomaly.description =
        kj::str("Data stale for ", age_ms, "ms (threshold: ", config_.stale_threshold_ms, "ms)");
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
  // Note: This method is marked const but needs to move from anomaly_history_
  // which is technically const-correct because we're returning a copy (by move)
  // The const_cast is safe here as we're only moving data out, not modifying state
  auto& history = const_cast<kj::Vector<Anomaly>&>(anomaly_history_);
  kj::Vector<Anomaly> result;
  size_t to_copy = (count == 0 || count > history.size()) ? history.size() : count;

  // Return newest first (move from end towards beginning)
  for (size_t i = 0; i < to_copy; ++i) {
    size_t idx = history.size() - 1 - i;
    // We need to construct new Anomalies since we can't move from a const reference
    // This requires cloning the description string
    Anomaly anomaly;
    anomaly.type = history[idx].type;
    anomaly.severity = history[idx].severity;
    anomaly.expected = history[idx].expected;
    anomaly.actual = history[idx].actual;
    anomaly.timestamp_ns = history[idx].timestamp_ns;
    anomaly.description = kj::str(history[idx].description);
    result.add(kj::mv(anomaly));
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

  // Reset latency tracking
  recent_latencies_.clear();
  latency_sum_ = 0;
  min_latency_ = 0;
  max_latency_ = 0;
  last_latency_ = 0;

  // Reset spread tracking
  recent_spreads_.clear();
  spread_sum_ = 0.0;
  min_spread_ = 0.0;
  max_spread_ = 0.0;
  current_spread_ = 0.0;
  wide_spread_count_ = 0;

  // Reset liquidity tracking
  total_bid_depth_ = 0.0;
  total_ask_depth_ = 0.0;
  bid_depth_sum_ = 0.0;
  ask_depth_sum_ = 0.0;
  depth_sample_count_ = 0;
}

kj::Vector<Anomaly> MarketQualityAnalyzer::analyze_depth(const kj::Vector<BookLevel>& bids,
                                                         const kj::Vector<BookLevel>& asks,
                                                         int64_t timestamp_ns) {
  kj::Vector<Anomaly> anomalies;
  total_events_++;

  // Calculate total depth
  double bid_depth = 0.0;
  double ask_depth = 0.0;

  for (const auto& level : bids) {
    bid_depth += level.qty;
  }
  for (const auto& level : asks) {
    ask_depth += level.qty;
  }

  // Update liquidity tracking
  total_bid_depth_ = bid_depth;
  total_ask_depth_ = ask_depth;
  bid_depth_sum_ += bid_depth;
  ask_depth_sum_ += ask_depth;
  depth_sample_count_++;

  // Check spread if we have both sides
  if (bids.size() > 0 && asks.size() > 0) {
    double best_bid = bids[0].price;
    double best_ask = asks[0].price;

    if (best_bid > 0 && best_ask > 0 && best_bid < best_ask) {
      double mid = (best_bid + best_ask) / 2.0;
      double spread_bps = ((best_ask - best_bid) / mid) * 10000.0;

      // Update spread tracking
      current_spread_ = spread_bps;
      if (recent_spreads_.size() == 0) {
        min_spread_ = spread_bps;
        max_spread_ = spread_bps;
      } else {
        if (spread_bps < min_spread_)
          min_spread_ = spread_bps;
        if (spread_bps > max_spread_)
          max_spread_ = spread_bps;
      }

      recent_spreads_.add(spread_bps);
      spread_sum_ += spread_bps;

      // Trim spread history
      while (recent_spreads_.size() > kMaxSpreadSamples) {
        spread_sum_ -= recent_spreads_[0];
        kj::Vector<double> new_spreads;
        for (size_t i = 1; i < recent_spreads_.size(); ++i) {
          new_spreads.add(recent_spreads_[i]);
        }
        recent_spreads_ = kj::mv(new_spreads);
      }

      // Check for spread anomaly
      if (spread_bps > config_.max_spread_bps) {
        wide_spread_count_++;
        Anomaly anomaly;
        anomaly.type = AnomalyType::SpreadWidening;
        anomaly.severity = kj::min(1.0, spread_bps / (config_.max_spread_bps * 3));
        anomaly.expected = config_.max_spread_bps;
        anomaly.actual = spread_bps;
        anomaly.timestamp_ns = timestamp_ns;
        anomaly.description = kj::str("Spread widening: ", spread_bps,
                                      " bps (threshold: ", config_.max_spread_bps, ")");
        anomalies.add(kj::mv(anomaly));
      }
    }
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

void MarketQualityAnalyzer::record_latency(int64_t exchange_ts_ns, int64_t recv_ts_ns) {
  if (exchange_ts_ns <= 0 || recv_ts_ns <= 0) {
    return;
  }

  int64_t latency = recv_ts_ns - exchange_ts_ns;
  if (latency < 0) {
    // Clock skew - exchange timestamp is in the future
    latency = 0;
  }

  last_latency_ = latency;

  // Update min/max
  if (recent_latencies_.size() == 0) {
    min_latency_ = latency;
    max_latency_ = latency;
  } else {
    if (latency < min_latency_)
      min_latency_ = latency;
    if (latency > max_latency_)
      max_latency_ = latency;
  }

  // Add to recent latencies
  recent_latencies_.add(latency);
  latency_sum_ += latency;

  // Trim history
  while (recent_latencies_.size() > kMaxLatencySamples) {
    latency_sum_ -= recent_latencies_[0];
    kj::Vector<int64_t> new_latencies;
    for (size_t i = 1; i < recent_latencies_.size(); ++i) {
      new_latencies.add(recent_latencies_[i]);
    }
    recent_latencies_ = kj::mv(new_latencies);
  }
}

LatencyStats MarketQualityAnalyzer::latency_stats() const {
  LatencyStats stats;
  stats.sample_count = static_cast<int64_t>(recent_latencies_.size());
  stats.last_latency_ns = last_latency_;
  stats.stale_count = stale_count_;

  if (recent_latencies_.size() == 0) {
    return stats;
  }

  stats.min_latency_ns = min_latency_;
  stats.max_latency_ns = max_latency_;
  stats.avg_latency_ns = latency_sum_ / static_cast<int64_t>(recent_latencies_.size());

  // Calculate percentiles by sorting a copy
  kj::Vector<int64_t> sorted;
  for (const auto& l : recent_latencies_) {
    sorted.add(l);
  }

  // Simple bubble sort (good enough for ~1000 elements)
  for (size_t i = 0; i < sorted.size(); ++i) {
    for (size_t j = i + 1; j < sorted.size(); ++j) {
      if (sorted[j] < sorted[i]) {
        int64_t tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  size_t n = sorted.size();
  stats.p50_latency_ns = sorted[n / 2];
  stats.p95_latency_ns = sorted[n * 95 / 100];
  stats.p99_latency_ns = sorted[n * 99 / 100];

  return stats;
}

SpreadStats MarketQualityAnalyzer::spread_stats() const {
  SpreadStats stats;
  stats.sample_count = static_cast<int64_t>(recent_spreads_.size());
  stats.current_spread_bps = current_spread_;
  stats.wide_spread_count = wide_spread_count_;

  if (recent_spreads_.size() == 0) {
    return stats;
  }

  stats.min_spread_bps = min_spread_;
  stats.max_spread_bps = max_spread_;
  stats.avg_spread_bps = spread_sum_ / static_cast<double>(recent_spreads_.size());

  return stats;
}

LiquidityStats MarketQualityAnalyzer::liquidity_stats() const {
  LiquidityStats stats;
  stats.sample_count = depth_sample_count_;
  stats.total_bid_depth = total_bid_depth_;
  stats.total_ask_depth = total_ask_depth_;

  if (depth_sample_count_ == 0) {
    return stats;
  }

  stats.avg_bid_depth = bid_depth_sum_ / static_cast<double>(depth_sample_count_);
  stats.avg_ask_depth = ask_depth_sum_ / static_cast<double>(depth_sample_count_);

  // Calculate bid/ask ratio and imbalance
  if (total_ask_depth_ > 0) {
    stats.bid_ask_ratio = total_bid_depth_ / total_ask_depth_;
  }

  double total_depth = total_bid_depth_ + total_ask_depth_;
  if (total_depth > 0) {
    stats.depth_imbalance = (total_bid_depth_ - total_ask_depth_) / total_depth;
  }

  return stats;
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
        kj::str("Price spike: ", price_change * 100,
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
    anomaly.description = kj::str("Volume spike: ", volume_ratio, "x average");
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
    anomaly.description = kj::str("Volume drop: ", volume_ratio * 100, "% of average");
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
    anomaly.description =
        kj::str("Spread widening: ", spread_bps, " bps (threshold: ", config_.max_spread_bps, ")");
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
    anomaly.description =
        kj::str("Timestamp skew: ", skew_ms, "ms (threshold: ", config_.max_clock_skew_ms, "ms)");
    return anomaly;
  }
  return kj::none;
}

void MarketQualityAnalyzer::record_anomaly(const Anomaly& anomaly) {
  total_anomalies_++;

  // Clone the Anomaly (copy all fields, clone description string)
  Anomaly cloned;
  cloned.type = anomaly.type;
  cloned.severity = anomaly.severity;
  cloned.expected = anomaly.expected;
  cloned.actual = anomaly.actual;
  cloned.timestamp_ns = anomaly.timestamp_ns;
  cloned.description = kj::str(anomaly.description);
  anomaly_history_.add(kj::mv(cloned));

  // Trim history if needed
  while (anomaly_history_.size() > kMaxAnomalyHistory) {
    kj::Vector<Anomaly> new_history;
    for (size_t i = 1; i < anomaly_history_.size(); ++i) {
      new_history.add(kj::mv(anomaly_history_[i]));
    }
    anomaly_history_ = kj::mv(new_history);
  }

  // Notify callback (callback takes const Anomaly&, so original anomaly is fine)
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
