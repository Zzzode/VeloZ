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

} // namespace
