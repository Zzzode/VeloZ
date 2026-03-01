#include "veloz/exec/aggregated_order_book.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/exchange_coordinator.h"
#include "veloz/exec/latency_tracker.h"
#include "veloz/exec/position_aggregator.h"

#include <kj/test.h>

namespace veloz::exec {
namespace {

// Mock adapter for testing
class MockExchangeAdapter final : public ExchangeAdapter {
public:
  explicit MockExchangeAdapter(veloz::common::Venue venue) : venue_(venue), connected_(false) {}

  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
    if (!connected_) {
      return kj::none;
    }

    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = kj::heapString(req.client_order_id);
    report.venue_order_id = kj::str("MOCK-", order_count_++);
    report.status = OrderStatus::Filled;
    report.last_fill_qty = req.qty;
    KJ_IF_SOME(price, req.price) {
      report.last_fill_price = price;
    }
    else {
      report.last_fill_price = 50000.0; // Default price
    }
    return report;
  }

  kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
    if (!connected_) {
      return kj::none;
    }

    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = kj::heapString(req.client_order_id);
    report.status = OrderStatus::Canceled;
    return report;
  }

  bool is_connected() const override {
    return connected_;
  }

  void connect() override {
    connected_ = true;
  }

  void disconnect() override {
    connected_ = false;
  }

  kj::StringPtr name() const override {
    return veloz::common::to_string(venue_);
  }

  kj::StringPtr version() const override {
    return "1.0.0"_kj;
  }

private:
  veloz::common::Venue venue_;
  bool connected_;
  int order_count_{0};
};

KJ_TEST("LatencyTracker: record and retrieve latency stats") {
  LatencyTracker tracker;

  auto now = kj::systemPreciseMonotonicClock().now();

  // Record some latencies
  tracker.record_latency(veloz::common::Venue::Binance, 10 * kj::MILLISECONDS, now);
  tracker.record_latency(veloz::common::Venue::Binance, 15 * kj::MILLISECONDS, now);
  tracker.record_latency(veloz::common::Venue::Binance, 12 * kj::MILLISECONDS, now);
  tracker.record_latency(veloz::common::Venue::Binance, 8 * kj::MILLISECONDS, now);
  tracker.record_latency(veloz::common::Venue::Binance, 20 * kj::MILLISECONDS, now);

  KJ_IF_SOME(stats, tracker.get_stats(veloz::common::Venue::Binance)) {
    KJ_EXPECT(stats.sample_count == 5);
    KJ_EXPECT(stats.min == 8 * kj::MILLISECONDS);
    KJ_EXPECT(stats.max == 20 * kj::MILLISECONDS);
  }
  else {
    KJ_FAIL_EXPECT("Expected stats for Binance");
  }

  // Unknown venue should return none
  KJ_EXPECT(tracker.get_stats(veloz::common::Venue::Okx) == kj::none);
}

KJ_TEST("LatencyTracker: get venues by latency") {
  LatencyTracker tracker;

  auto now = kj::systemPreciseMonotonicClock().now();

  // Binance: ~10ms average
  for (int i = 0; i < 10; ++i) {
    tracker.record_latency(veloz::common::Venue::Binance, 10 * kj::MILLISECONDS, now);
  }

  // OKX: ~20ms average
  for (int i = 0; i < 10; ++i) {
    tracker.record_latency(veloz::common::Venue::Okx, 20 * kj::MILLISECONDS, now);
  }

  // Bybit: ~5ms average
  for (int i = 0; i < 10; ++i) {
    tracker.record_latency(veloz::common::Venue::Bybit, 5 * kj::MILLISECONDS, now);
  }

  auto venues = tracker.get_venues_by_latency();
  KJ_EXPECT(venues.size() == 3);
  KJ_EXPECT(venues[0] == veloz::common::Venue::Bybit);   // Fastest
  KJ_EXPECT(venues[1] == veloz::common::Venue::Binance); // Middle
  KJ_EXPECT(venues[2] == veloz::common::Venue::Okx);     // Slowest
}

KJ_TEST("PositionAggregator: track fills and calculate P&L") {
  PositionAggregator aggregator;

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  // Buy 1 BTC at 50000
  ExecutionReport fill1;
  fill1.symbol = symbol;
  fill1.client_order_id = kj::heapString("order1"_kj);
  fill1.status = OrderStatus::Filled;
  fill1.last_fill_qty = 1.0;
  fill1.last_fill_price = 50000.0;

  aggregator.on_fill(veloz::common::Venue::Binance, fill1, OrderSide::Buy, 1.0, 50000.0);

  KJ_IF_SOME(pos, aggregator.get_position(veloz::common::Venue::Binance, symbol)) {
    KJ_EXPECT(pos.quantity == 1.0);
    KJ_EXPECT(pos.avg_entry_price == 50000.0);
  }
  else {
    KJ_FAIL_EXPECT("Expected position for Binance BTCUSDT");
  }

  // Buy another 1 BTC at 51000
  ExecutionReport fill2;
  fill2.symbol = symbol;
  fill2.client_order_id = kj::heapString("order2"_kj);
  fill2.status = OrderStatus::Filled;
  fill2.last_fill_qty = 1.0;
  fill2.last_fill_price = 51000.0;

  aggregator.on_fill(veloz::common::Venue::Binance, fill2, OrderSide::Buy, 1.0, 51000.0);

  KJ_IF_SOME(pos, aggregator.get_position(veloz::common::Venue::Binance, symbol)) {
    KJ_EXPECT(pos.quantity == 2.0);
    KJ_EXPECT(pos.avg_entry_price == 50500.0); // Weighted average
  }
  else {
    KJ_FAIL_EXPECT("Expected position for Binance BTCUSDT");
  }
}

KJ_TEST("PositionAggregator: aggregate positions across venues") {
  PositionAggregator aggregator;

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  // Position on Binance
  aggregator.set_position(veloz::common::Venue::Binance, symbol, 1.0, 50000.0);

  // Position on OKX
  aggregator.set_position(veloz::common::Venue::Okx, symbol, 0.5, 51000.0);

  KJ_IF_SOME(agg, aggregator.get_aggregated_position(symbol)) {
    KJ_EXPECT(agg.total_quantity == 1.5);
    KJ_EXPECT(agg.venues.size() == 2);
  }
  else {
    KJ_FAIL_EXPECT("Expected aggregated position");
  }
}

KJ_TEST("AggregatedOrderBook: merge order books from multiple venues") {
  AggregatedOrderBook book;

  veloz::market::BookData binance_book;
  binance_book.bids.add(veloz::market::BookLevel{50000.0, 1.0});
  binance_book.bids.add(veloz::market::BookLevel{49900.0, 2.0});
  binance_book.asks.add(veloz::market::BookLevel{50100.0, 1.5});
  binance_book.asks.add(veloz::market::BookLevel{50200.0, 2.5});

  book.update_venue(veloz::common::Venue::Binance, binance_book, 1000000);

  veloz::market::BookData okx_book;
  okx_book.bids.add(veloz::market::BookLevel{50050.0, 0.5}); // Better bid
  okx_book.bids.add(veloz::market::BookLevel{49950.0, 1.0});
  okx_book.asks.add(veloz::market::BookLevel{50080.0, 0.8}); // Better ask
  okx_book.asks.add(veloz::market::BookLevel{50150.0, 1.2});

  book.update_venue(veloz::common::Venue::Okx, okx_book, 1000000);

  auto bbo = book.get_aggregated_bbo();

  // Best bid should be from OKX (50050)
  KJ_EXPECT(bbo.best_bid_price == 50050.0);
  KJ_EXPECT(bbo.best_bid_venue == veloz::common::Venue::Okx);

  // Best ask should be from OKX (50080)
  KJ_EXPECT(bbo.best_ask_price == 50080.0);
  KJ_EXPECT(bbo.best_ask_venue == veloz::common::Venue::Okx);

  KJ_EXPECT(bbo.venues.size() == 2);
}

KJ_TEST("AggregatedOrderBook: handle stale data") {
  AggregatedOrderBook book;

  veloz::market::BookData binance_book;
  binance_book.bids.add(veloz::market::BookLevel{50000.0, 1.0});
  binance_book.asks.add(veloz::market::BookLevel{50100.0, 1.5});

  book.update_venue(veloz::common::Venue::Binance, binance_book, 1000000);

  // Mark as stale
  book.mark_stale(veloz::common::Venue::Binance);

  auto bbo = book.get_aggregated_bbo();

  // Stale venue should be excluded from best price calculation
  KJ_EXPECT(bbo.best_bid_price == 0.0);
  KJ_EXPECT(bbo.best_ask_price == 0.0);
}

KJ_TEST("ExchangeCoordinator: register and route orders") {
  ExchangeCoordinator coordinator;

  // Register mock adapters
  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  auto okx = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Okx);
  okx->connect();
  coordinator.register_adapter(veloz::common::Venue::Okx, kj::mv(okx));

  KJ_EXPECT(coordinator.has_adapter(veloz::common::Venue::Binance));
  KJ_EXPECT(coordinator.has_adapter(veloz::common::Venue::Okx));
  KJ_EXPECT(!coordinator.has_adapter(veloz::common::Venue::Bybit));

  auto venues = coordinator.get_registered_venues();
  KJ_EXPECT(venues.size() == 2);
}

KJ_TEST("ExchangeCoordinator: place order on specific venue") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId("BTCUSDT"_kj);
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 0.1;
  req.price = 50000.0;
  req.client_order_id = kj::heapString("test-order-1"_kj);

  KJ_IF_SOME(report, coordinator.place_order(veloz::common::Venue::Binance, req)) {
    KJ_EXPECT(report.status == OrderStatus::Filled);
    KJ_EXPECT(report.last_fill_qty == 0.1);
  }
  else {
    KJ_FAIL_EXPECT("Expected execution report");
  }
}

KJ_TEST("ExchangeCoordinator: routing strategies") {
  ExchangeCoordinator coordinator;

  // Register adapters
  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  auto okx = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Okx);
  okx->connect();
  coordinator.register_adapter(veloz::common::Venue::Okx, kj::mv(okx));

  // Set up order books
  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  // Binance: bid 50000, ask 50100
  coordinator.update_bbo(veloz::common::Venue::Binance, symbol, 50000.0, 1.0, 50100.0, 1.0,
                         1000000);

  // OKX: bid 50050, ask 50080 (better prices)
  coordinator.update_bbo(veloz::common::Venue::Okx, symbol, 50050.0, 1.0, 50080.0, 1.0, 1000000);

  // Test best price routing for buy order (should select OKX with lower ask)
  coordinator.set_routing_strategy(RoutingStrategy::BestPrice);
  auto decision = coordinator.select_venue(symbol, OrderSide::Buy, 0.1);
  KJ_EXPECT(decision.selected_venue == veloz::common::Venue::Okx);
  KJ_EXPECT(decision.expected_price == 50080.0);

  // Test best price routing for sell order (should select OKX with higher bid)
  decision = coordinator.select_venue(symbol, OrderSide::Sell, 0.1);
  KJ_EXPECT(decision.selected_venue == veloz::common::Venue::Okx);
  KJ_EXPECT(decision.expected_price == 50050.0);
}

KJ_TEST("ExchangeCoordinator: latency-based routing") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  auto okx = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Okx);
  okx->connect();
  coordinator.register_adapter(veloz::common::Venue::Okx, kj::mv(okx));

  auto now = kj::systemPreciseMonotonicClock().now();

  // Record latencies (Binance faster)
  for (int i = 0; i < 10; ++i) {
    coordinator.record_latency(veloz::common::Venue::Binance, 5 * kj::MILLISECONDS, now);
    coordinator.record_latency(veloz::common::Venue::Okx, 15 * kj::MILLISECONDS, now);
  }

  coordinator.set_routing_strategy(RoutingStrategy::LowestLatency);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  auto decision = coordinator.select_venue(symbol, OrderSide::Buy, 0.1);

  KJ_EXPECT(decision.selected_venue == veloz::common::Venue::Binance);
}

KJ_TEST("ExchangeCoordinator: round-robin routing") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  auto okx = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Okx);
  okx->connect();
  coordinator.register_adapter(veloz::common::Venue::Okx, kj::mv(okx));

  coordinator.set_routing_strategy(RoutingStrategy::RoundRobin);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  // Should alternate between venues
  auto decision1 = coordinator.select_venue(symbol, OrderSide::Buy, 0.1);
  auto decision2 = coordinator.select_venue(symbol, OrderSide::Buy, 0.1);

  KJ_EXPECT(decision1.selected_venue != decision2.selected_venue);
}

KJ_TEST("ExchangeCoordinator: symbol normalization") {
  ExchangeCoordinator coordinator;

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  // Binance format (no change)
  auto binance_symbol = coordinator.normalize_symbol(veloz::common::Venue::Binance, symbol);
  KJ_EXPECT(binance_symbol == "BTCUSDT"_kj);

  // OKX format (add hyphen)
  auto okx_symbol = coordinator.normalize_symbol(veloz::common::Venue::Okx, symbol);
  KJ_EXPECT(okx_symbol == "BTC-USDT"_kj);
}

KJ_TEST("ExchangeCoordinator: exchange status") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockExchangeAdapter>(veloz::common::Venue::Binance);
  binance->connect();
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  auto status = coordinator.get_exchange_status(veloz::common::Venue::Binance);
  KJ_EXPECT(status.venue == veloz::common::Venue::Binance);
  KJ_EXPECT(status.is_connected == true);

  // Unknown venue
  auto unknown_status = coordinator.get_exchange_status(veloz::common::Venue::Bybit);
  KJ_EXPECT(unknown_status.is_connected == false);
}

} // namespace
} // namespace veloz::exec
