#include "veloz/exec/execution_algorithms.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/exchange_coordinator.h"
#include "veloz/exec/smart_order_router.h"

#include <kj/test.h>

namespace veloz::exec {
namespace {

// Mock adapter for testing
class MockAdapter final : public ExchangeAdapter {
public:
  explicit MockAdapter(veloz::common::Venue venue) : venue_(venue), connected_(true) {}

  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
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
      report.last_fill_price = 50000.0;
    }
    return report;
  }

  kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
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

KJ_TEST("SmartOrderRouter: set and get fees") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  ExchangeFees binance_fees;
  binance_fees.maker_fee = 0.0001;
  binance_fees.taker_fee = 0.0002;

  router.set_fees(veloz::common::Venue::Binance, binance_fees);

  KJ_IF_SOME(fees, router.get_fees(veloz::common::Venue::Binance)) {
    KJ_EXPECT(fees.maker_fee == 0.0001);
    KJ_EXPECT(fees.taker_fee == 0.0002);
  }
  else {
    KJ_FAIL_EXPECT("Expected fees for Binance");
  }

  // Unknown venue
  KJ_EXPECT(router.get_fees(veloz::common::Venue::Okx) == kj::none);
}

KJ_TEST("SmartOrderRouter: scoring weights") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  router.set_price_weight(0.4);
  router.set_fee_weight(0.2);
  router.set_latency_weight(0.2);
  router.set_liquidity_weight(0.1);
  router.set_reliability_weight(0.1);

  KJ_EXPECT(router.price_weight() == 0.4);
  KJ_EXPECT(router.fee_weight() == 0.2);
  KJ_EXPECT(router.latency_weight() == 0.2);
  KJ_EXPECT(router.liquidity_weight() == 0.1);
  KJ_EXPECT(router.reliability_weight() == 0.1);
}

KJ_TEST("SmartOrderRouter: minimum order size") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  router.set_min_order_size(veloz::common::Venue::Binance, 0.001);
  KJ_EXPECT(router.get_min_order_size(veloz::common::Venue::Binance) == 0.001);
  KJ_EXPECT(router.get_min_order_size(veloz::common::Venue::Okx) == 0.0);
}

KJ_TEST("SmartOrderRouter: execute order") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockAdapter>(veloz::common::Venue::Binance);
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));
  coordinator.set_default_venue(veloz::common::Venue::Binance);

  // Set up BBO
  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  coordinator.update_bbo(veloz::common::Venue::Binance, symbol, 50000.0, 1.0, 50100.0, 1.0,
                         1000000);

  SmartOrderRouter router(coordinator);

  PlaceOrderRequest req;
  req.symbol = symbol;
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 0.1;
  req.price = 50100.0;
  req.client_order_id = kj::heapString("test-1"_kj);

  KJ_IF_SOME(report, router.execute(req)) {
    KJ_EXPECT(report.status == OrderStatus::Filled);
    KJ_EXPECT(report.last_fill_qty == 0.1);
  }
  else {
    KJ_FAIL_EXPECT("Expected execution report");
  }
}

KJ_TEST("SmartOrderRouter: batch execution") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockAdapter>(veloz::common::Venue::Binance);
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));
  coordinator.set_default_venue(veloz::common::Venue::Binance);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  coordinator.update_bbo(veloz::common::Venue::Binance, symbol, 50000.0, 1.0, 50100.0, 1.0,
                         1000000);

  SmartOrderRouter router(coordinator);

  BatchOrderRequest batch;

  PlaceOrderRequest req1;
  req1.symbol = symbol;
  req1.side = OrderSide::Buy;
  req1.qty = 0.1;
  req1.price = 50100.0;
  req1.client_order_id = kj::heapString("batch-1"_kj);
  batch.orders.add(kj::mv(req1));

  PlaceOrderRequest req2;
  req2.symbol = symbol;
  req2.side = OrderSide::Buy;
  req2.qty = 0.2;
  req2.price = 50100.0;
  req2.client_order_id = kj::heapString("batch-2"_kj);
  batch.orders.add(kj::mv(req2));

  auto result = router.execute_batch(batch);

  KJ_EXPECT(result.success_count == 2);
  KJ_EXPECT(result.failure_count == 0);
  KJ_EXPECT(result.reports.size() == 2);
}

KJ_TEST("SmartOrderRouter: analytics tracking") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockAdapter>(veloz::common::Venue::Binance);
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));
  coordinator.set_default_venue(veloz::common::Venue::Binance);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  coordinator.update_bbo(veloz::common::Venue::Binance, symbol, 50000.0, 1.0, 50100.0, 1.0,
                         1000000);

  SmartOrderRouter router(coordinator);

  PlaceOrderRequest req;
  req.symbol = symbol;
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 50100.0;
  req.client_order_id = kj::heapString("test-1"_kj);

  router.execute(req);

  auto analytics = router.get_analytics();
  KJ_EXPECT(analytics.total_orders == 1);
  KJ_EXPECT(analytics.filled_orders == 1);
}

KJ_TEST("SmartOrderRouter: cancel merged") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockAdapter>(veloz::common::Venue::Binance);
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));

  SmartOrderRouter router(coordinator);

  CancelMergeRequest req;
  req.venue = veloz::common::Venue::Binance;
  req.symbol = veloz::common::SymbolId("BTCUSDT"_kj);
  req.client_order_ids.add(kj::heapString("order-1"_kj));
  req.client_order_ids.add(kj::heapString("order-2"_kj));
  req.client_order_ids.add(kj::heapString("order-3"_kj));

  auto results = router.cancel_merged(req);
  KJ_EXPECT(results.size() == 3);
}

KJ_TEST("TwapAlgorithm: basic construction") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  TwapConfig config;
  config.duration = 60 * kj::SECONDS;
  config.slice_interval = 10 * kj::SECONDS;

  TwapAlgorithm algo(router, symbol, OrderSide::Buy, 1.0, config);

  auto progress = algo.get_progress();
  KJ_EXPECT(progress.type == AlgorithmType::TWAP);
  KJ_EXPECT(progress.state == AlgorithmState::Pending);
  KJ_EXPECT(progress.target_quantity == 1.0);
}

KJ_TEST("TwapAlgorithm: start and pause") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  TwapConfig config;
  config.duration = 60 * kj::SECONDS;
  config.slice_interval = 10 * kj::SECONDS;

  TwapAlgorithm algo(router, symbol, OrderSide::Buy, 1.0, config);

  algo.start();
  KJ_EXPECT(algo.get_progress().state == AlgorithmState::Running);

  algo.pause();
  KJ_EXPECT(algo.get_progress().state == AlgorithmState::Paused);

  algo.resume();
  KJ_EXPECT(algo.get_progress().state == AlgorithmState::Running);

  algo.cancel();
  KJ_EXPECT(algo.get_progress().state == AlgorithmState::Cancelled);
}

KJ_TEST("VwapAlgorithm: basic construction") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  VwapConfig config;
  config.duration = 60 * kj::SECONDS;
  config.slice_interval = 10 * kj::SECONDS;

  VwapAlgorithm algo(router, symbol, OrderSide::Sell, 2.0, config);

  auto progress = algo.get_progress();
  KJ_EXPECT(progress.type == AlgorithmType::VWAP);
  KJ_EXPECT(progress.state == AlgorithmState::Pending);
  KJ_EXPECT(progress.target_quantity == 2.0);
}

KJ_TEST("AlgorithmManager: start and manage algorithms") {
  ExchangeCoordinator coordinator;

  auto binance = kj::heap<MockAdapter>(veloz::common::Venue::Binance);
  coordinator.register_adapter(veloz::common::Venue::Binance, kj::mv(binance));
  coordinator.set_default_venue(veloz::common::Venue::Binance);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  coordinator.update_bbo(veloz::common::Venue::Binance, symbol, 50000.0, 1.0, 50100.0, 1.0,
                         1000000);

  SmartOrderRouter router(coordinator);
  AlgorithmManager manager(router);

  TwapConfig twap_config;
  twap_config.duration = 60 * kj::SECONDS;
  twap_config.slice_interval = 10 * kj::SECONDS;

  auto algo_id = manager.start_twap(symbol, OrderSide::Buy, 1.0, twap_config);
  KJ_EXPECT(algo_id.size() > 0);

  KJ_IF_SOME(progress, manager.get_progress(algo_id)) {
    KJ_EXPECT(progress.state == AlgorithmState::Running);
  }
  else {
    KJ_FAIL_EXPECT("Expected algorithm progress");
  }

  manager.pause(algo_id);
  KJ_IF_SOME(progress, manager.get_progress(algo_id)) {
    KJ_EXPECT(progress.state == AlgorithmState::Paused);
  }

  manager.cancel(algo_id);
  KJ_IF_SOME(progress, manager.get_progress(algo_id)) {
    KJ_EXPECT(progress.state == AlgorithmState::Cancelled);
  }
}

KJ_TEST("AlgorithmManager: get all progress") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);
  AlgorithmManager manager(router);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  TwapConfig config;
  config.duration = 60 * kj::SECONDS;

  manager.start_twap(symbol, OrderSide::Buy, 1.0, config);
  manager.start_twap(symbol, OrderSide::Sell, 0.5, config);

  auto all_progress = manager.get_all_progress();
  KJ_EXPECT(all_progress.size() == 2);
}

KJ_TEST("AlgorithmManager: cleanup completed") {
  ExchangeCoordinator coordinator;
  SmartOrderRouter router(coordinator);
  AlgorithmManager manager(router);

  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  TwapConfig config;
  config.duration = 60 * kj::SECONDS;

  auto algo_id = manager.start_twap(symbol, OrderSide::Buy, 1.0, config);
  manager.cancel(algo_id);

  KJ_EXPECT(manager.get_all_progress().size() == 1);

  manager.cleanup_completed();

  KJ_EXPECT(manager.get_all_progress().size() == 0);
}

} // namespace
} // namespace veloz::exec
