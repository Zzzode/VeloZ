#include "kj/test.h"
#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"

namespace {

using namespace veloz::oms;
using namespace veloz::exec;

KJ_TEST("Position: Initialize") {
  Position pos({"BTCUSDT"});

  KJ_EXPECT(pos.size() == 0.0);
  KJ_EXPECT(pos.avg_price() == 0.0);
  KJ_EXPECT(pos.side() == PositionSide::None);
}

KJ_TEST("Position: OpenLong") {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  KJ_EXPECT(pos.size() == 1.0);
  KJ_EXPECT(pos.avg_price() == 50000.0);
  KJ_EXPECT(pos.side() == PositionSide::Long);
}

KJ_TEST("Position: AddToLong") {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Buy 0.5 @ 51000
  pos.apply_fill(OrderSide::Buy, 0.5, 51000.0);

  KJ_EXPECT(pos.size() == 1.5);
  KJ_EXPECT(pos.avg_price() ==
            50333.333333333336); // Weighted avg: (1.0 * 50000 + 0.5 * 51000) / 1.5
}

KJ_TEST("Position: PartialClose") {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Sell 0.3 @ 51000
  pos.apply_fill(OrderSide::Sell, 0.3, 51000.0);

  KJ_EXPECT(pos.size() == 0.7);
  KJ_EXPECT(pos.avg_price() == 50000.0);  // Avg price unchanged
  KJ_EXPECT(pos.realized_pnl() == 300.0); // 0.3 * (51000 - 50000)
}

KJ_TEST("Position: FullClose") {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Sell 1.0 @ 51000
  pos.apply_fill(OrderSide::Sell, 1.0, 51000.0);

  KJ_EXPECT(pos.size() == 0.0);
  KJ_EXPECT(pos.side() == PositionSide::None);
  KJ_EXPECT(pos.realized_pnl() == 1000.0);
}

KJ_TEST("Position: UnrealizedPnL") {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Current price: 51000
  KJ_EXPECT(pos.unrealized_pnl(51000.0) == 1000.0);
}

// ============================================================================
// FIFO Cost Basis Tests
// ============================================================================

KJ_TEST("Position FIFO: Basic open and close") {
  Position pos({"BTCUSDT"});
  pos.set_cost_basis_method(CostBasisMethod::FIFO);

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  KJ_EXPECT(pos.size() == 1.0);
  KJ_EXPECT(pos.avg_price() == 50000.0);
  KJ_EXPECT(pos.lot_count() == 1);

  // Sell 1.0 @ 51000 - close position
  pos.apply_fill(OrderSide::Sell, 1.0, 51000.0);

  KJ_EXPECT(pos.size() == 0.0);
  KJ_EXPECT(pos.realized_pnl() == 1000.0); // (51000 - 50000) * 1.0
  KJ_EXPECT(pos.lot_count() == 0);
}

KJ_TEST("Position FIFO: Multiple lots with partial close") {
  Position pos({"BTCUSDT"});
  pos.set_cost_basis_method(CostBasisMethod::FIFO);

  // Buy 1.0 @ 50000 (lot 1)
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Buy 1.0 @ 52000 (lot 2)
  pos.apply_fill(OrderSide::Buy, 1.0, 52000.0);

  KJ_EXPECT(pos.size() == 2.0);
  KJ_EXPECT(pos.lot_count() == 2);
  KJ_EXPECT(pos.avg_price() == 51000.0); // (50000 + 52000) / 2

  // Sell 0.5 @ 53000 - partial close of first lot (FIFO)
  pos.apply_fill(OrderSide::Sell, 0.5, 53000.0);

  KJ_EXPECT(pos.size() == 1.5);
  KJ_EXPECT(pos.realized_pnl() == 1500.0); // (53000 - 50000) * 0.5
  KJ_EXPECT(pos.lot_count() == 2);         // First lot partially consumed
}

KJ_TEST("Position FIFO: Close first lot completely then partial second") {
  Position pos({"BTCUSDT"});
  pos.set_cost_basis_method(CostBasisMethod::FIFO);

  // Buy 1.0 @ 50000 (lot 1)
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Buy 1.0 @ 52000 (lot 2)
  pos.apply_fill(OrderSide::Buy, 1.0, 52000.0);

  // Sell 1.5 @ 54000 - close lot 1 entirely, partial lot 2
  pos.apply_fill(OrderSide::Sell, 1.5, 54000.0);

  KJ_EXPECT(pos.size() == 0.5);
  // Realized PnL: (54000-50000)*1.0 + (54000-52000)*0.5 = 4000 + 1000 = 5000
  KJ_EXPECT(pos.realized_pnl() == 5000.0);
  KJ_EXPECT(pos.lot_count() == 1); // Only partial lot 2 remains
}

KJ_TEST("Position FIFO: Total PnL calculation") {
  Position pos({"BTCUSDT"});
  pos.set_cost_basis_method(CostBasisMethod::FIFO);

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Sell 0.5 @ 52000
  pos.apply_fill(OrderSide::Sell, 0.5, 52000.0);

  // Realized: (52000 - 50000) * 0.5 = 1000
  KJ_EXPECT(pos.realized_pnl() == 1000.0);

  // Current price 53000, remaining 0.5 @ 50000
  // Unrealized: (53000 - 50000) * 0.5 = 1500
  KJ_EXPECT(pos.unrealized_pnl(53000.0) == 1500.0);

  // Total: 1000 + 1500 = 2500
  KJ_EXPECT(pos.total_pnl(53000.0) == 2500.0);
}

KJ_TEST("Position: Snapshot") {
  Position pos({"BTCUSDT"});
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  auto snap = pos.snapshot(51000.0);

  KJ_EXPECT(snap.symbol == "BTCUSDT");
  KJ_EXPECT(snap.size == 1.0);
  KJ_EXPECT(snap.avg_price == 50000.0);
  KJ_EXPECT(snap.unrealized_pnl == 1000.0);
  KJ_EXPECT(snap.side == PositionSide::Long);
}

KJ_TEST("Position: Notional value") {
  Position pos({"BTCUSDT"});
  pos.apply_fill(OrderSide::Buy, 2.0, 50000.0);

  KJ_EXPECT(pos.notional_value(51000.0) == 102000.0); // 2.0 * 51000
}

KJ_TEST("Position: Is flat") {
  Position pos({"BTCUSDT"});
  KJ_EXPECT(pos.is_flat());

  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);
  KJ_EXPECT(!pos.is_flat());

  pos.apply_fill(OrderSide::Sell, 1.0, 51000.0);
  KJ_EXPECT(pos.is_flat());
}

// ============================================================================
// PositionManager Tests
// ============================================================================

KJ_TEST("PositionManager: Get or create position") {
  PositionManager mgr;

  auto& pos = mgr.get_or_create_position({"BTCUSDT"});
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  KJ_EXPECT(mgr.position_count() == 1);

  // Get same position again
  auto& pos2 = mgr.get_or_create_position({"BTCUSDT"});
  KJ_EXPECT(pos2.size() == 1.0);

  // Create different position
  auto& pos3 = mgr.get_or_create_position({"ETHUSDT"});
  pos3.apply_fill(OrderSide::Buy, 10.0, 3000.0);

  KJ_EXPECT(mgr.position_count() == 2);
}

KJ_TEST("PositionManager: Total realized PnL") {
  PositionManager mgr;

  auto& btc = mgr.get_or_create_position({"BTCUSDT"});
  btc.apply_fill(OrderSide::Buy, 1.0, 50000.0);
  btc.apply_fill(OrderSide::Sell, 1.0, 51000.0); // +1000

  auto& eth = mgr.get_or_create_position({"ETHUSDT"});
  eth.apply_fill(OrderSide::Buy, 10.0, 3000.0);
  eth.apply_fill(OrderSide::Sell, 10.0, 3200.0); // +2000

  KJ_EXPECT(mgr.total_realized_pnl() == 3000.0);
}

KJ_TEST("PositionManager: Total unrealized PnL") {
  PositionManager mgr;

  auto& btc = mgr.get_or_create_position({"BTCUSDT"});
  btc.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  auto& eth = mgr.get_or_create_position({"ETHUSDT"});
  eth.apply_fill(OrderSide::Buy, 10.0, 3000.0);

  kj::HashMap<kj::String, double> prices;
  prices.insert(kj::str("BTCUSDT"), 51000.0); // +1000 unrealized
  prices.insert(kj::str("ETHUSDT"), 3100.0);  // +1000 unrealized

  KJ_EXPECT(mgr.total_unrealized_pnl(prices) == 2000.0);
}

KJ_TEST("PositionManager: Apply execution report") {
  PositionManager mgr;

  ExecutionReport report;
  report.symbol = veloz::common::SymbolId{"BTCUSDT"};
  report.client_order_id = kj::str("order-1");
  report.last_fill_qty = 1.0;
  report.last_fill_price = 50000.0;
  report.status = OrderStatus::Filled;

  mgr.apply_execution_report(report, OrderSide::Buy);

  KJ_EXPECT(mgr.position_count() == 1);

  KJ_IF_SOME(pos, mgr.get_position("BTCUSDT")) {
    KJ_EXPECT(pos.size() == 1.0);
    KJ_EXPECT(pos.avg_price() == 50000.0);
  }
  else {
    KJ_FAIL_EXPECT("Position not found");
  }
}

KJ_TEST("PositionManager: Position update callback") {
  PositionManager mgr;

  int callback_count = 0;
  double last_size = 0.0;

  mgr.set_position_update_callback([&](const Position& pos) {
    callback_count++;
    last_size = pos.size();
  });

  ExecutionReport report;
  report.symbol = veloz::common::SymbolId{"BTCUSDT"};
  report.client_order_id = kj::str("order-1");
  report.last_fill_qty = 1.0;
  report.last_fill_price = 50000.0;

  mgr.apply_execution_report(report, OrderSide::Buy);

  KJ_EXPECT(callback_count == 1);
  KJ_EXPECT(last_size == 1.0);

  report.last_fill_qty = 0.5;
  report.last_fill_price = 51000.0;
  mgr.apply_execution_report(report, OrderSide::Buy);

  KJ_EXPECT(callback_count == 2);
  KJ_EXPECT(last_size == 1.5);
}

KJ_TEST("PositionManager: For each position") {
  PositionManager mgr;

  mgr.get_or_create_position({"BTCUSDT"}).apply_fill(OrderSide::Buy, 1.0, 50000.0);
  mgr.get_or_create_position({"ETHUSDT"}).apply_fill(OrderSide::Buy, 10.0, 3000.0);

  int count = 0;
  double total_size = 0.0;

  mgr.for_each_position([&](const Position& pos) {
    count++;
    total_size += pos.size();
  });

  KJ_EXPECT(count == 2);
  KJ_EXPECT(total_size == 11.0);
}

KJ_TEST("PositionManager: Clear positions") {
  PositionManager mgr;

  mgr.get_or_create_position({"BTCUSDT"}).apply_fill(OrderSide::Buy, 1.0, 50000.0);
  mgr.get_or_create_position({"ETHUSDT"}).apply_fill(OrderSide::Buy, 10.0, 3000.0);

  KJ_EXPECT(mgr.position_count() == 2);

  mgr.clear();

  KJ_EXPECT(mgr.position_count() == 0);
}

KJ_TEST("PositionManager: Default cost basis method") {
  PositionManager mgr;
  mgr.set_default_cost_basis_method(CostBasisMethod::FIFO);

  auto& pos = mgr.get_or_create_position({"BTCUSDT"});
  KJ_EXPECT(pos.cost_basis_method() == CostBasisMethod::FIFO);
}

} // namespace
