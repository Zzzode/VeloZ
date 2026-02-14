#include "kj/test.h"
#include "veloz/market/metrics.h"

using namespace veloz::market;

namespace {

KJ_TEST("MarketMetrics: Initialize") {
  MarketMetrics metrics;

  KJ_EXPECT(metrics.event_count() == 0);
  KJ_EXPECT(metrics.drop_count() == 0);
}

KJ_TEST("MarketMetrics: Record event") {
  MarketMetrics metrics;
  metrics.record_event_latency_ns(1000);
  metrics.record_event_latency_ns(2000);
  metrics.record_event_latency_ns(3000);

  KJ_EXPECT(metrics.event_count() == 3);
  KJ_EXPECT(metrics.avg_latency_ns() == 2000);
}

KJ_TEST("MarketMetrics: Record drop") {
  MarketMetrics metrics;
  metrics.record_drop();

  KJ_EXPECT(metrics.drop_count() == 1);
}

KJ_TEST("MarketMetrics: Percentiles") {
  MarketMetrics metrics;
  for (int i = 1; i <= 100; ++i) {
    metrics.record_event_latency_ns(i * 1000); // 1ms to 100ms
  }

  auto p50 = metrics.percentile_ns(50);
  auto p99 = metrics.percentile_ns(99);

  KJ_EXPECT(p50 >= 50000 - 1000); // 50ms
  KJ_EXPECT(p99 >= 99000 - 1000); // 99ms
}

KJ_TEST("MarketMetrics: Reconnect tracking") {
  MarketMetrics metrics;
  metrics.record_reconnect();

  KJ_EXPECT(metrics.reconnect_count() == 1);
}

} // namespace
