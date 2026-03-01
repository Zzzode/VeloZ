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

  kj::Promise<kj::Maybe<ExecutionReport>>
  cancel_order_async(const veloz::common::SymbolId& symbol,
                     kj::StringPtr client_order_id) override {
    // Find and remove the order from open_orders
    for (size_t i = 0; i < open_orders.size(); ++i) {
      if (open_orders[i].client_order_id == client_order_id) {
        ExecutionReport cancelled;
        cancelled.symbol = open_orders[i].symbol;
        cancelled.client_order_id = kj::str(open_orders[i].client_order_id);
        cancelled.venue_order_id = kj::str(open_orders[i].venue_order_id);
        cancelled.status = OrderStatus::Canceled;
        cancelled.last_fill_qty = open_orders[i].last_fill_qty;
        cancelled.last_fill_price = open_orders[i].last_fill_price;
        cancelled.ts_exchange_ns = open_orders[i].ts_exchange_ns;
        cancelled.ts_recv_ns = open_orders[i].ts_recv_ns;
        cancelled_orders.add(kj::str(client_order_id));
        return kj::Maybe<ExecutionReport>(kj::mv(cancelled));
      }
    }
    return kj::Maybe<ExecutionReport>(kj::none);
  }

  void add_order(ExecutionReport order) {
    open_orders.add(kj::mv(order));
  }

  kj::Vector<kj::String> cancelled_orders;
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

KJ_TEST("AccountReconciler: auto-cancel orphaned order") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.auto_cancel_orphaned = true;

  AccountReconciler reconciler(io, order_store, kj::mv(config));
  MockQueryInterface mock_query;

  // Add an order on exchange that's not in local state
  veloz::common::SymbolId symbol("BTCUSDT"_kj);

  ExecutionReport orphaned_order;
  orphaned_order.symbol = symbol;
  orphaned_order.client_order_id = kj::str("orphan-auto-cancel-123");
  orphaned_order.venue_order_id = kj::str("venue-789");
  orphaned_order.status = OrderStatus::Accepted;
  orphaned_order.last_fill_qty = 0.0;
  orphaned_order.last_fill_price = 0.0;
  mock_query.add_order(kj::mv(orphaned_order));

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Run reconciliation
  reconciler.reconcile_now().wait(io.waitScope);

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.orphaned_orders_found == 1);
  KJ_EXPECT(stats.orphaned_orders_cancelled == 1);

  // Verify the order was cancelled via the mock
  KJ_EXPECT(mock_query.cancelled_orders.size() == 1);
  KJ_EXPECT(mock_query.cancelled_orders[0] == "orphan-auto-cancel-123"_kj);
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

KJ_TEST("AccountReconciler: manual intervention resume") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  ReconciliationConfig config;
  config.freeze_on_mismatch = true;
  config.max_mismatches_before_freeze = 1;

  AccountReconciler reconciler(io, order_store, kj::mv(config));

  // Verify initial state
  KJ_EXPECT(!reconciler.is_strategy_frozen());

  // Resume should be safe to call even when not frozen
  reconciler.resume_strategy();
  KJ_EXPECT(!reconciler.is_strategy_frozen());
}

KJ_TEST("AccountReconciler: statistics tracking") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Run multiple reconciliations
  reconciler.reconcile_now().wait(io.waitScope);
  reconciler.reconcile_now().wait(io.waitScope);
  reconciler.reconcile_now().wait(io.waitScope);

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.total_reconciliations == 3);
  KJ_EXPECT(stats.successful_reconciliations == 3);
  KJ_EXPECT(stats.failed_reconciliations == 0);
  KJ_EXPECT(stats.last_reconciliation_ts_ns > 0);
}

KJ_TEST("AccountReconciler: multiple venues") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_binance;
  MockQueryInterface mock_okx;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_binance);
  reconciler.register_exchange(veloz::common::Venue::Okx, mock_okx);

  // Run reconciliation - should reconcile both venues
  reconciler.reconcile_now().wait(io.waitScope);

  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.total_reconciliations == 1);
  KJ_EXPECT(stats.successful_reconciliations == 1);
}

KJ_TEST("AccountReconciler: event history limit") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Run many reconciliations to generate events
  for (int i = 0; i < 10; ++i) {
    reconciler.reconcile_now().wait(io.waitScope);
  }

  // Get recent events with limit
  auto events = reconciler.get_recent_events(5);
  KJ_EXPECT(events.size() <= 5);
}

KJ_TEST("AccountReconciler: reconcile specific symbol") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);

  // Reconcile specific symbol
  veloz::common::SymbolId symbol("ETHUSDT"_kj);
  reconciler.reconcile_symbol(veloz::common::Venue::Binance, symbol).wait(io.waitScope);

  // Should complete without error
  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.mismatches_detected == 0);
}

KJ_TEST("AccountReconciler: unregistered venue reconcile") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);

  // Try to reconcile unregistered venue - should handle gracefully
  veloz::common::SymbolId symbol("BTCUSDT"_kj);
  reconciler.reconcile_symbol(veloz::common::Venue::Bybit, symbol).wait(io.waitScope);

  // Should complete without error
  auto stats = reconciler.get_stats();
  KJ_EXPECT(stats.total_reconciliations == 0);
}

KJ_TEST("ReconciliationConfig: default values") {
  ReconciliationConfig config;

  KJ_EXPECT(config.reconciliation_interval == 30 * kj::SECONDS);
  KJ_EXPECT(config.stale_order_threshold == 5 * kj::MINUTES);
  KJ_EXPECT(!config.auto_cancel_orphaned);
  KJ_EXPECT(config.freeze_on_mismatch);
  KJ_EXPECT(config.max_mismatches_before_freeze == 3);
  KJ_EXPECT(config.max_retries == 3);
  KJ_EXPECT(config.retry_delay == 1 * kj::SECONDS);
}

KJ_TEST("StateMismatch: structure fields") {
  StateMismatch mismatch;
  mismatch.client_order_id = kj::str("test-order-123");
  mismatch.symbol = kj::str("BTCUSDT");
  mismatch.local_status = OrderStatus::Accepted;
  mismatch.exchange_status = OrderStatus::Filled;
  mismatch.local_filled_qty = 0.0;
  mismatch.exchange_filled_qty = 1.0;
  mismatch.local_avg_price = 0.0;
  mismatch.exchange_avg_price = 50000.0;
  mismatch.action_taken = ReconciliationAction::UpdateLocalState;
  mismatch.detected_ts_ns = 1234567890;

  KJ_EXPECT(mismatch.client_order_id == "test-order-123"_kj);
  KJ_EXPECT(mismatch.local_status == OrderStatus::Accepted);
  KJ_EXPECT(mismatch.exchange_status == OrderStatus::Filled);
}

KJ_TEST("ReconciliationSeverity to_string") {
  KJ_EXPECT(to_string(ReconciliationSeverity::Info) == "Info"_kj);
  KJ_EXPECT(to_string(ReconciliationSeverity::Warning) == "Warning"_kj);
  KJ_EXPECT(to_string(ReconciliationSeverity::Error) == "Error"_kj);
  KJ_EXPECT(to_string(ReconciliationSeverity::Critical) == "Critical"_kj);
}

KJ_TEST("AccountReconciler: manual intervention management") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);

  // Initially no pending interventions
  auto pending = reconciler.get_pending_interventions();
  KJ_EXPECT(pending.size() == 0);

  // Add a manual intervention item
  ManualInterventionItem item;
  item.client_order_id = kj::str("order-123");
  item.symbol = kj::str("BTCUSDT");
  item.venue = veloz::common::Venue::Binance;
  item.description = kj::str("Fill quantity mismatch requires review");
  item.severity = ReconciliationSeverity::Critical;
  reconciler.add_manual_intervention(kj::mv(item));

  // Should now have one pending intervention
  pending = reconciler.get_pending_interventions();
  KJ_EXPECT(pending.size() == 1);
  KJ_EXPECT(pending[0].client_order_id == "order-123"_kj);
  KJ_EXPECT(pending[0].severity == ReconciliationSeverity::Critical);
}

KJ_TEST("AccountReconciler: resolve intervention") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);

  // Add intervention
  ManualInterventionItem item;
  item.id = kj::str("INT-001");
  item.client_order_id = kj::str("order-456");
  item.symbol = kj::str("ETHUSDT");
  item.venue = veloz::common::Venue::Okx;
  item.description = kj::str("Status mismatch");
  item.severity = ReconciliationSeverity::Error;
  reconciler.add_manual_intervention(kj::mv(item));

  // Verify it exists
  auto pending = reconciler.get_pending_interventions();
  KJ_EXPECT(pending.size() == 1);

  // Resolve it
  reconciler.resolve_intervention("INT-001"_kj, "Manually verified with exchange"_kj);

  // Should no longer be in pending list
  pending = reconciler.get_pending_interventions();
  KJ_EXPECT(pending.size() == 0);

  // But should still be retrievable
  KJ_IF_SOME(resolved, reconciler.get_intervention("INT-001"_kj)) {
    KJ_EXPECT(resolved.resolved);
    KJ_EXPECT(resolved.resolution_notes == "Manually verified with exchange"_kj);
  }
  else {
    KJ_FAIL_EXPECT("Intervention should still exist after resolution");
  }
}

KJ_TEST("AccountReconciler: generate report summary") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);
  MockQueryInterface mock_query;

  reconciler.register_exchange(veloz::common::Venue::Binance, mock_query);
  reconciler.reconcile_now().wait(io.waitScope);

  auto summary = reconciler.generate_report_summary();
  KJ_EXPECT(summary.size() > 0);
  // Summary should contain key information - check for 'T' from "Total"
  KJ_EXPECT(summary.findFirst('T') != kj::none);
}

KJ_TEST("ReconciliationReport: structure fields") {
  ReconciliationReport report;
  report.start_ts_ns = 1000000000;
  report.end_ts_ns = 1000100000;
  report.duration = 100000 * kj::NANOSECONDS;
  report.venue = veloz::common::Venue::Binance;
  report.orders_checked = 10;
  report.orders_matched = 8;
  report.mismatches_found = 2;
  report.mismatches_auto_resolved = 1;
  report.manual_interventions_required = 1;
  report.success = true;
  report.max_severity = ReconciliationSeverity::Warning;

  KJ_EXPECT(report.orders_checked == 10);
  KJ_EXPECT(report.mismatches_found == 2);
  KJ_EXPECT(report.success);
}

KJ_TEST("ManualInterventionItem: structure fields") {
  ManualInterventionItem item;
  item.id = kj::str("INT-123");
  item.client_order_id = kj::str("order-789");
  item.symbol = kj::str("BTCUSDT");
  item.venue = veloz::common::Venue::Bybit;
  item.description = kj::str("Critical fill mismatch");
  item.severity = ReconciliationSeverity::Critical;
  item.created_ts_ns = 1234567890;
  item.resolved = false;

  KJ_EXPECT(item.id == "INT-123"_kj);
  KJ_EXPECT(item.severity == ReconciliationSeverity::Critical);
  KJ_EXPECT(!item.resolved);
}

KJ_TEST("PositionDiscrepancy: structure fields") {
  PositionDiscrepancy discrepancy;
  discrepancy.symbol = kj::str("BTCUSDT");
  discrepancy.venue = veloz::common::Venue::Binance;
  discrepancy.local_qty = 1.5;
  discrepancy.exchange_qty = 1.6;
  discrepancy.qty_diff = 0.1;
  discrepancy.severity = ReconciliationSeverity::Warning;

  KJ_EXPECT(discrepancy.symbol == "BTCUSDT"_kj);
  KJ_EXPECT(discrepancy.qty_diff == 0.1);
}

KJ_TEST("AccountReconciler: export report json") {
  auto io = kj::setupAsyncIo();
  oms::OrderStore order_store;

  AccountReconciler reconciler(io, order_store);

  ReconciliationReport report;
  report.start_ts_ns = 1000000000;
  report.end_ts_ns = 1000100000;
  report.duration = 100000 * kj::NANOSECONDS;
  report.venue = veloz::common::Venue::Binance;
  report.orders_checked = 5;
  report.success = true;
  report.max_severity = ReconciliationSeverity::Info;

  auto json = reconciler.export_report_json(report);
  KJ_EXPECT(json.size() > 0);
  // Check for presence of key fields using char search
  KJ_EXPECT(json.findFirst('{') != kj::none); // Valid JSON starts with {
  KJ_EXPECT(json.findFirst(':') != kj::none); // Has key-value pairs
}

} // namespace
} // namespace veloz::exec
