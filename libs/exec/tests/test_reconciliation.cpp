#include "veloz/exec/reconciliation.h"
#include "veloz/oms/order_record.h"

#include <kj/async-io.h>
#include <kj/test.h>

namespace veloz::exec {
namespace {

// Mock query interface for testing
class MockQueryInterface final : public ReconciliationQueryInterface {
public:
  ~MockQueryInterface() noexcept override = default;

  kj::Vector<ExecutionReport> open_orders;

  kj::Promise<kj::Vector<ExecutionReport>>
  query_open_orders_async(const veloz::common::SymbolId& symbol) override {
    kj::Vector<ExecutionReport> result;
    for (const auto& order : open_orders) {
      ExecutionReport copy;
      copy.symbol = order.symbol;
      copy.client_order_id = kj::str(order.client_order_id);
      copy.venue_order_id = kj::str(order.venue_order_id);
      copy.status = order.status;
      copy.last_fill_qty = order.last_fill_qty;
      copy.last_fill_price = order.last_fill_price;
      copy.ts_exchange_ns = order.ts_exchange_ns;
      copy.ts_recv_ns = order.ts_recv_ns;
      result.add(kj::mv(copy));
    }
    return kj::mv(result);
  }

  kj::Promise<kj::Maybe<ExecutionReport>>
  query_order_async(const veloz::common::SymbolId& symbol, kj::StringPtr client_order_id) override {
    for (const auto& order : open_orders) {
      if (order.client_order_id == client_order_id) {
        ExecutionReport copy;
        copy.symbol = order.symbol;
        copy.client_order_id = kj::str(order.client_order_id);
        copy.venue_order_id = kj::str(order.venue_order_id);
        copy.status = order.status;
        copy.last_fill_qty = order.last_fill_qty;
        copy.last_fill_price = order.last_fill_price;
        copy.ts_exchange_ns = order.ts_exchange_ns;
        copy.ts_recv_ns = order.ts_recv_ns;
        return kj::Maybe<ExecutionReport>(kj::mv(copy));
      }
    }
    return kj::Maybe<ExecutionReport>(kj::none);
  }

  kj::Promise<kj::Vector<ExecutionReport>> query_orders_async(const veloz::common::SymbolId& symbol,
                                                              std::int64_t start_time_ms,
                                                              std::int64_t end_time_ms) override {
    return kj::Vector<ExecutionReport>();
  }

  void add_order(ExecutionReport order) {
    open_orders.add(kj::mv(order));
  }
};

KJ_TEST("AccountReconciler: basic construction") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.reconciliation_interval = 1 * kj::SECONDS;

  AccountReconciler reconciler(io, order_store, kj::mv(config));

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.total_reconciliations == 0);
  KJ_EXPECT(stats.successful_reconciliations == 0);
  KJ_EXPECT(!reconciler.is_strategy_frozen());
}

KJ_TEST("AccountReconciler: register and unregister exchange") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);
  reconciler.unregister_exchange(veloz::common::Venue::Binance);
}

KJ_TEST("AccountReconciler: reconcile with no orders") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.reconciliation_interval = 100 * kj::MILLISECONDS;

  AccountReconciler reconciler(io, order_store, kj::mv(config));
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Run reconciliation
  reconciler.reconcile_now().wait(io.waitScope);

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.total_reconciliations == 1);
  KJ_EXPECT(stats.successful_reconciliations == 1);
  KJ_EXPECT(stats.mismatches_detected == 0);
}

KJ_TEST("AccountReconciler: detect orphaned order") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.auto_cancel_orphaned = false;

  AccountReconciler reconciler(io, order_store, kj::mv(config));
  MockQueryInterface mock_query;

  // Add an order on exchange that's not in local state
  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  ExecutionReport orphaned_order;
  orphaned_order.symbol = symbol;
  orphaned_order.client_order_id = kj::str("orphan-123");
  orphaned_order.venue_order_id = kj::str("venue-456");
  orphaned_order.status = OrderStatus::Accepted;
  orphaned_order.last_fill_qty = 0.0;
  orphaned_order.last_fill_price = 0.0;
  mock_query.add_order(kj::mv(orphaned_order));

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Run reconciliation
  reconciler.reconcile_now().wait(io.waitScope);

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.orphaned_orders_found == 1);
}

KJ_TEST("AccountReconciler: strategy freeze on multiple mismatches") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.freeze_on_mismatch = true;
  config.max_mismatches_before_freeze = 2;

  AccountReconciler reconciler(io, order_store, kj::mv(config));

  bool freeze_called = false;
  reconciler.set_freeze_callback(
      [&freeze_called](bool freeze, kj::StringPtr reason) { freeze_called = freeze; });

  KJ_EXPECT(!reconciler.is_strategy_frozen());

  // Manually freeze for testing
  reconciler.resume_strategy();
  KJ_EXPECT(!reconciler.is_strategy_frozen());
}

KJ_TEST("AccountReconciler: event callback") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  kj::Vector<ReconciliationEventType> received_events;
  reconciler.set_event_callback(
      [&received_events](const ReconciliationEvent& event) { received_events.add(event.type); });

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);
  reconciler.reconcile_now().wait(io.waitScope);

  // Should have received Started and Completed events
  KJ_EXPECT(received_events.size() >= 2);
}

KJ_TEST("AccountReconciler: get recent events") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);
  reconciler.reconcile_now().wait(io.waitScope);

  auto events = reconciler.get_recent_events(10);
  KJ_EXPECT(events.size() > 0);
}

KJ_TEST("ReconciliationEventType to_string") {
  KJ_EXPECT(to_string(ReconciliationEventType::Started) == "Started"_kj);
  KJ_EXPECT(to_string(ReconciliationEventType::Completed) == "Completed"_kj);
  KJ_EXPECT(to_string(ReconciliationEventType::StateMismatch) == "StateMismatch"_kj);
  KJ_EXPECT(to_string(ReconciliationEventType::OrphanedOrderFound) == "OrphanedOrderFound"_kj);
  KJ_EXPECT(to_string(ReconciliationEventType::StrategyFrozen) == "StrategyFrozen"_kj);
}

KJ_TEST("ReconciliationAction to_string") {
  KJ_EXPECT(to_string(ReconciliationAction::None) == "None"_kj);
  KJ_EXPECT(to_string(ReconciliationAction::UpdateLocalState) == "UpdateLocalState"_kj);
  KJ_EXPECT(to_string(ReconciliationAction::CancelOrphanedOrder) == "CancelOrphanedOrder"_kj);
  KJ_EXPECT(to_string(ReconciliationAction::FreezeStrategy) == "FreezeStrategy"_kj);
}

} // namespace
} // namespace veloz::exec
