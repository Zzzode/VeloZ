#include "veloz/market/metrics.h"

#include <gtest/gtest.h>

using namespace veloz::market;

TEST(MarketMetrics, Initialize) {
  MarketMetrics metrics;
  EXPECT_EQ(metrics.event_count(), 0);
  EXPECT_EQ(metrics.drop_count(), 0);
}

TEST(MarketMetrics, RecordEvent) {
  MarketMetrics metrics;
  metrics.record_event_latency_ns(1000);
  metrics.record_event_latency_ns(2000);
  metrics.record_event_latency_ns(3000);

  EXPECT_EQ(metrics.event_count(), 3);
  EXPECT_EQ(metrics.avg_latency_ns(), 2000);
}

TEST(MarketMetrics, RecordDrop) {
  MarketMetrics metrics;
  metrics.record_drop();

  EXPECT_EQ(metrics.drop_count(), 1);
}

TEST(MarketMetrics, Percentiles) {
  MarketMetrics metrics;
  for (int i = 1; i <= 100; ++i) {
    metrics.record_event_latency_ns(i * 1000); // 1ms to 100ms
  }

  auto p50 = metrics.percentile_ns(50);
  auto p99 = metrics.percentile_ns(99);

  EXPECT_NEAR(p50, 50000, 1000); // 50ms
  EXPECT_NEAR(p99, 99000, 1000); // 99ms
}

TEST(MarketMetrics, ReconnectTracking) {
  MarketMetrics metrics;
  metrics.record_reconnect();

  EXPECT_EQ(metrics.reconnect_count(), 1);
}
