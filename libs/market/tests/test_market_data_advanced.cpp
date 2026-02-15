/**
 * @file test_market_data_advanced.cpp
 * @brief Tests for advanced market data features
 *
 * Tests K-line aggregation, market quality scoring, and enhanced order book features.
 */

#include "veloz/market/kline_aggregator.h"
#include "veloz/market/market_quality.h"
#include "veloz/market/order_book.h"

#include <kj/test.h>

namespace {

using namespace veloz::market;

// ============================================================================
// KlineAggregator Tests
// ============================================================================

KJ_TEST("KlineAggregator: Enable and disable intervals") {
  KlineAggregator aggregator;

  KJ_EXPECT(!aggregator.is_interval_enabled(KlineInterval::Min1));

  aggregator.enable_interval(KlineInterval::Min1);
  aggregator.enable_interval(KlineInterval::Min5);

  KJ_EXPECT(aggregator.is_interval_enabled(KlineInterval::Min1));
  KJ_EXPECT(aggregator.is_interval_enabled(KlineInterval::Min5));
  KJ_EXPECT(!aggregator.is_interval_enabled(KlineInterval::Hour1));

  auto enabled = aggregator.enabled_intervals();
  KJ_EXPECT(enabled.size() == 2);

  aggregator.disable_interval(KlineInterval::Min1);
  KJ_EXPECT(!aggregator.is_interval_enabled(KlineInterval::Min1));
}

KJ_TEST("KlineAggregator: Process single trade") {
  KlineAggregator aggregator;
  aggregator.enable_interval(KlineInterval::Min1);

  TradeData trade;
  trade.price = 50000.0;
  trade.qty = 1.0;
  trade.is_buyer_maker = false;

  int64_t timestamp_ms = 1700000000000LL; // Some timestamp
  aggregator.process_trade(trade, timestamp_ms);

  auto current = aggregator.current_kline(KlineInterval::Min1);
  KJ_IF_SOME(kline, current) {
    KJ_EXPECT(kline.kline.open == 50000.0);
    KJ_EXPECT(kline.kline.high == 50000.0);
    KJ_EXPECT(kline.kline.low == 50000.0);
    KJ_EXPECT(kline.kline.close == 50000.0);
    KJ_EXPECT(kline.kline.volume == 1.0);
    KJ_EXPECT(kline.trade_count == 1);
    KJ_EXPECT(kline.buy_volume == 1.0);
    KJ_EXPECT(kline.sell_volume == 0.0);
    KJ_EXPECT(!kline.is_closed);
  } else {
    KJ_FAIL_EXPECT("Expected current kline to be present");
  }
}

KJ_TEST("KlineAggregator: OHLC calculation") {
  KlineAggregator aggregator;
  aggregator.enable_interval(KlineInterval::Min1);

  int64_t base_time = 1700000000000LL;

  // Trade 1: Open
  TradeData trade1;
  trade1.price = 50000.0;
  trade1.qty = 1.0;
  aggregator.process_trade(trade1, base_time);

  // Trade 2: High
  TradeData trade2;
  trade2.price = 50500.0;
  trade2.qty = 0.5;
  aggregator.process_trade(trade2, base_time + 10000);

  // Trade 3: Low
  TradeData trade3;
  trade3.price = 49500.0;
  trade3.qty = 0.5;
  aggregator.process_trade(trade3, base_time + 20000);

  // Trade 4: Close
  TradeData trade4;
  trade4.price = 50200.0;
  trade4.qty = 1.0;
  aggregator.process_trade(trade4, base_time + 30000);

  auto current = aggregator.current_kline(KlineInterval::Min1);
  KJ_IF_SOME(kline, current) {
    KJ_EXPECT(kline.kline.open == 50000.0);
    KJ_EXPECT(kline.kline.high == 50500.0);
    KJ_EXPECT(kline.kline.low == 49500.0);
    KJ_EXPECT(kline.kline.close == 50200.0);
    KJ_EXPECT(kline.kline.volume == 3.0);
    KJ_EXPECT(kline.trade_count == 4);
  } else {
    KJ_FAIL_EXPECT("Expected current kline to be present");
  }
}

KJ_TEST("KlineAggregator: Candle close on new interval") {
  KlineAggregator aggregator;
  aggregator.enable_interval(KlineInterval::Min1);

  int64_t base_time = 1700000000000LL;
  int64_t one_minute = 60 * 1000LL;

  // Trade in first candle
  TradeData trade1;
  trade1.price = 50000.0;
  trade1.qty = 1.0;
  aggregator.process_trade(trade1, base_time);

  // Trade in second candle (should close first)
  TradeData trade2;
  trade2.price = 51000.0;
  trade2.qty = 1.0;
  aggregator.process_trade(trade2, base_time + one_minute + 1000);

  // Check history
  auto history = aggregator.history(KlineInterval::Min1);
  KJ_EXPECT(history.size() == 1);
  KJ_EXPECT(history[0].is_closed);
  KJ_EXPECT(history[0].kline.close == 50000.0);

  // Check current
  auto current = aggregator.current_kline(KlineInterval::Min1);
  KJ_IF_SOME(kline, current) {
    KJ_EXPECT(kline.kline.open == 51000.0);
    KJ_EXPECT(!kline.is_closed);
  } else {
    KJ_FAIL_EXPECT("Expected current kline to be present");
  }

  KJ_EXPECT(aggregator.total_candles_closed() == 1);
}

KJ_TEST("KlineAggregator: Multiple timeframes") {
  KlineAggregator aggregator;
  aggregator.enable_interval(KlineInterval::Min1);
  aggregator.enable_interval(KlineInterval::Min5);

  TradeData trade;
  trade.price = 50000.0;
  trade.qty = 1.0;
  aggregator.process_trade(trade, 1700000000000LL);

  auto min1 = aggregator.current_kline(KlineInterval::Min1);
  auto min5 = aggregator.current_kline(KlineInterval::Min5);

  KJ_EXPECT(min1 != kj::none);
  KJ_EXPECT(min5 != kj::none);
}

KJ_TEST("KlineAggregator: Buy/sell volume tracking") {
  KlineAggregator aggregator;
  aggregator.enable_interval(KlineInterval::Min1);

  int64_t base_time = 1700000000000LL;

  // Taker buy (is_buyer_maker = false)
  TradeData buy;
  buy.price = 50000.0;
  buy.qty = 2.0;
  buy.is_buyer_maker = false;
  aggregator.process_trade(buy, base_time);

  // Taker sell (is_buyer_maker = true)
  TradeData sell;
  sell.price = 50000.0;
  sell.qty = 1.0;
  sell.is_buyer_maker = true;
  aggregator.process_trade(sell, base_time + 1000);

  auto current = aggregator.current_kline(KlineInterval::Min1);
  KJ_IF_SOME(kline, current) {
    KJ_EXPECT(kline.buy_volume == 2.0);
    KJ_EXPECT(kline.sell_volume == 1.0);
  } else {
    KJ_FAIL_EXPECT("Expected current kline to be present");
  }
}

KJ_TEST("KlineAggregator: Interval to string conversion") {
  KJ_EXPECT(interval_to_string(KlineInterval::Min1) == "1m"_kj);
  KJ_EXPECT(interval_to_string(KlineInterval::Min5) == "5m"_kj);
  KJ_EXPECT(interval_to_string(KlineInterval::Hour1) == "1h"_kj);
  KJ_EXPECT(interval_to_string(KlineInterval::Day1) == "1d"_kj);
}

KJ_TEST("KlineAggregator: Interval to milliseconds") {
  KJ_EXPECT(interval_to_ms(KlineInterval::Min1) == 60000LL);
  KJ_EXPECT(interval_to_ms(KlineInterval::Min5) == 300000LL);
  KJ_EXPECT(interval_to_ms(KlineInterval::Hour1) == 3600000LL);
  KJ_EXPECT(interval_to_ms(KlineInterval::Day1) == 86400000LL);
}

// ============================================================================
// MarketQualityAnalyzer Tests
// ============================================================================

KJ_TEST("MarketQualityAnalyzer: Initialize") {
  MarketQualityAnalyzer analyzer;
  KJ_EXPECT(analyzer.total_events_analyzed() == 0);
  KJ_EXPECT(analyzer.total_anomalies_detected() == 0);
}

KJ_TEST("MarketQualityAnalyzer: Normal trade no anomaly") {
  MarketQualityAnalyzer::Config config;
  config.price_spike_threshold = 0.05; // 5%
  MarketQualityAnalyzer analyzer(config);

  // Add baseline trades
  for (int i = 0; i < 100; ++i) {
    TradeData trade;
    trade.price = 50000.0 + (i % 10) * 10; // Small variation
    trade.qty = 1.0;
    analyzer.analyze_trade(trade, 1700000000000LL + i * 1000000000LL);
  }

  // Normal trade within threshold
  TradeData normal_trade;
  normal_trade.price = 50050.0;
  normal_trade.qty = 1.0;
  auto anomalies = analyzer.analyze_trade(normal_trade, 1700000100000000000LL);

  KJ_EXPECT(anomalies.empty());
}

KJ_TEST("MarketQualityAnalyzer: Detect price spike") {
  MarketQualityAnalyzer::Config config;
  config.price_spike_threshold = 0.02; // 2%
  MarketQualityAnalyzer analyzer(config);

  // Add baseline trades
  for (int i = 0; i < 100; ++i) {
    TradeData trade;
    trade.price = 50000.0;
    trade.qty = 1.0;
    analyzer.analyze_trade(trade, 1700000000000LL + i * 1000000000LL);
  }

  // Spike trade (5% change)
  TradeData spike_trade;
  spike_trade.price = 52500.0; // 5% increase
  spike_trade.qty = 1.0;
  auto anomalies = analyzer.analyze_trade(spike_trade, 1700000100000000000LL);

  KJ_EXPECT(anomalies.size() >= 1);
  bool found_price_spike = false;
  for (const auto& a : anomalies) {
    if (a.type == AnomalyType::PriceSpike) {
      found_price_spike = true;
      KJ_EXPECT(a.severity > 0.0);
    }
  }
  KJ_EXPECT(found_price_spike);
}

KJ_TEST("MarketQualityAnalyzer: Detect volume spike") {
  MarketQualityAnalyzer::Config config;
  config.volume_spike_multiplier = 3.0;
  MarketQualityAnalyzer analyzer(config);

  // Add baseline trades with normal volume
  for (int i = 0; i < 100; ++i) {
    TradeData trade;
    trade.price = 50000.0;
    trade.qty = 1.0;
    analyzer.analyze_trade(trade, 1700000000000LL + i * 1000000000LL);
  }

  // Volume spike (10x normal)
  TradeData spike_trade;
  spike_trade.price = 50000.0;
  spike_trade.qty = 10.0;
  auto anomalies = analyzer.analyze_trade(spike_trade, 1700000100000000000LL);

  KJ_EXPECT(anomalies.size() >= 1);
  bool found_volume_spike = false;
  for (const auto& a : anomalies) {
    if (a.type == AnomalyType::VolumeSpike) {
      found_volume_spike = true;
    }
  }
  KJ_EXPECT(found_volume_spike);
}

KJ_TEST("MarketQualityAnalyzer: Detect spread widening") {
  MarketQualityAnalyzer::Config config;
  config.max_spread_bps = 50.0; // 50 basis points
  MarketQualityAnalyzer analyzer(config);

  // Wide spread (100 bps = 1%)
  double bid = 50000.0;
  double ask = 50500.0; // ~100 bps spread (500/50250 * 10000 = ~99.5 bps)
  auto anomalies = analyzer.analyze_book(bid, ask, 1700000000000000000LL);

  KJ_EXPECT(anomalies.size() >= 1);
  bool found_spread = false;
  for (const auto& a : anomalies) {
    if (a.type == AnomalyType::SpreadWidening) {
      found_spread = true;
    }
  }
  KJ_EXPECT(found_spread);
}

KJ_TEST("MarketQualityAnalyzer: Quality score calculation") {
  MarketQualityAnalyzer analyzer;

  // Add some trades
  for (int i = 0; i < 50; ++i) {
    TradeData trade;
    trade.price = 50000.0;
    trade.qty = 1.0;
    analyzer.analyze_trade(trade, 1700000000000LL + i * 1000000000LL);
  }

  auto score = analyzer.quality_score();
  KJ_EXPECT(score.sample_count == 50);
  KJ_EXPECT(score.overall >= 0.0 && score.overall <= 1.0);
  KJ_EXPECT(score.freshness >= 0.0 && score.freshness <= 1.0);
}

KJ_TEST("MarketQualityAnalyzer: Anomaly type to string") {
  KJ_EXPECT(anomaly_type_to_string(AnomalyType::None) == "None"_kj);
  KJ_EXPECT(anomaly_type_to_string(AnomalyType::PriceSpike) == "PriceSpike"_kj);
  KJ_EXPECT(anomaly_type_to_string(AnomalyType::VolumeSpike) == "VolumeSpike"_kj);
  KJ_EXPECT(anomaly_type_to_string(AnomalyType::SpreadWidening) == "SpreadWidening"_kj);
}

// ============================================================================
// DataSampler Tests
// ============================================================================

KJ_TEST("DataSampler: No sampling passes all") {
  DataSampler::Config config;
  config.strategy = DataSampler::Strategy::None;
  DataSampler sampler(config);

  for (int i = 0; i < 100; ++i) {
    KJ_EXPECT(sampler.should_sample(i * 1000000LL, kj::none));
  }

  KJ_EXPECT(sampler.total_events() == 100);
  KJ_EXPECT(sampler.sampled_events() == 100);
  KJ_EXPECT(sampler.sample_rate() == 1.0);
}

KJ_TEST("DataSampler: Time interval sampling") {
  DataSampler::Config config;
  config.strategy = DataSampler::Strategy::TimeInterval;
  config.time_interval_ms = 100; // 100ms
  DataSampler sampler(config);

  int sampled = 0;
  for (int i = 0; i < 100; ++i) {
    // Events every 10ms
    if (sampler.should_sample(i * 10 * 1000000LL, kj::none)) {
      sampled++;
    }
  }

  // Should sample roughly every 10 events (100ms / 10ms)
  KJ_EXPECT(sampled >= 9 && sampled <= 11);
}

KJ_TEST("DataSampler: Count interval sampling") {
  DataSampler::Config config;
  config.strategy = DataSampler::Strategy::CountInterval;
  config.count_interval = 5;
  DataSampler sampler(config);

  int sampled = 0;
  for (int i = 0; i < 100; ++i) {
    if (sampler.should_sample(i * 1000000LL, kj::none)) {
      sampled++;
    }
  }

  KJ_EXPECT(sampled == 20); // 100 / 5 = 20
}

KJ_TEST("DataSampler: Adaptive sampling") {
  DataSampler::Config config;
  config.strategy = DataSampler::Strategy::Adaptive;
  config.volatility_threshold = 0.01; // 1%
  DataSampler sampler(config);

  int sampled = 0;

  // First sample always passes
  KJ_EXPECT(sampler.should_sample(0, 50000.0));
  sampled++;

  // Small changes should be filtered
  KJ_EXPECT(!sampler.should_sample(1000000LL, 50010.0)); // 0.02% change

  // Large change should pass
  KJ_EXPECT(sampler.should_sample(2000000LL, 50600.0)); // 1.2% change
  sampled++;

  KJ_EXPECT(sampled == 2);
}

// ============================================================================
// Enhanced OrderBook Tests
// ============================================================================

KJ_TEST("OrderBook: Set max depth levels") {
  OrderBook book;

  // Apply snapshot with many levels
  kj::Vector<BookLevel> bids;
  kj::Vector<BookLevel> asks;
  for (int i = 0; i < 20; ++i) {
    bids.add(BookLevel{50000.0 - i * 10, 1.0});
    asks.add(BookLevel{50010.0 + i * 10, 1.0});
  }
  book.apply_snapshot(bids, asks, 1);

  KJ_EXPECT(book.level_count(true) == 20);
  KJ_EXPECT(book.level_count(false) == 20);

  // Set max depth
  book.set_max_depth_levels(10);
  KJ_EXPECT(book.max_depth_levels() == 10);
  KJ_EXPECT(book.level_count(true) == 10);
  KJ_EXPECT(book.level_count(false) == 10);
}

KJ_TEST("OrderBook: Snapshot with depth") {
  OrderBook book;

  kj::Vector<BookLevel> bids;
  kj::Vector<BookLevel> asks;
  for (int i = 0; i < 10; ++i) {
    bids.add(BookLevel{50000.0 - i * 10, 1.0});
    asks.add(BookLevel{50010.0 + i * 10, 1.0});
  }
  book.apply_snapshot(bids, asks, 1);

  // Full snapshot
  auto full = book.snapshot();
  KJ_EXPECT(full.bids.size() == 10);
  KJ_EXPECT(full.asks.size() == 10);
  KJ_EXPECT(full.sequence == 1);

  // Limited depth snapshot
  auto limited = book.snapshot(5);
  KJ_EXPECT(limited.bids.size() == 5);
  KJ_EXPECT(limited.asks.size() == 5);
}

KJ_TEST("OrderBook: Imbalance calculation") {
  OrderBook book;

  kj::Vector<BookLevel> bids;
  kj::Vector<BookLevel> asks;

  // More bid volume
  bids.add(BookLevel{50000.0, 10.0});
  bids.add(BookLevel{49990.0, 5.0});
  asks.add(BookLevel{50010.0, 5.0});
  asks.add(BookLevel{50020.0, 2.0});

  book.apply_snapshot(bids, asks, 1);

  // Imbalance with all levels: (15 - 7) / (15 + 7) = 8/22 ≈ 0.36
  double imbalance = book.imbalance(0);
  KJ_EXPECT(imbalance > 0.3 && imbalance < 0.4);

  // Imbalance with top 1 level: (10 - 5) / (10 + 5) = 5/15 ≈ 0.33
  double top1_imbalance = book.imbalance(1);
  KJ_EXPECT(top1_imbalance > 0.3 && top1_imbalance < 0.35);
}

KJ_TEST("OrderBook: Levels within range") {
  OrderBook book;

  kj::Vector<BookLevel> bids;
  kj::Vector<BookLevel> asks;

  // Bids from 49900 to 50000
  for (int i = 0; i < 11; ++i) {
    bids.add(BookLevel{50000.0 - i * 10, 1.0});
  }
  // Asks from 50010 to 50110
  for (int i = 0; i < 11; ++i) {
    asks.add(BookLevel{50010.0 + i * 10, 1.0});
  }

  book.apply_snapshot(bids, asks, 1);

  // Mid price is ~50005
  // 0.1% range = 50 units
  auto bids_in_range = book.levels_within_range(0.001, true);
  auto asks_in_range = book.levels_within_range(0.001, false);

  // Should get levels within ~50 units of mid
  KJ_EXPECT(bids_in_range.size() >= 4 && bids_in_range.size() <= 6);
  KJ_EXPECT(asks_in_range.size() >= 4 && asks_in_range.size() <= 6);
}

} // namespace
