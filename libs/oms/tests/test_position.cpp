#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"

#include <gtest/gtest.h>

using namespace veloz::oms;
using namespace veloz::exec;

TEST(Position, Initialize) {
  Position pos({"BTCUSDT"});

  EXPECT_EQ(pos.size(), 0.0);
  EXPECT_EQ(pos.avg_price(), 0.0);
  EXPECT_EQ(pos.side(), PositionSide::None);
}

TEST(Position, OpenLong) {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  EXPECT_EQ(pos.size(), 1.0);
  EXPECT_EQ(pos.avg_price(), 50000.0);
  EXPECT_EQ(pos.side(), PositionSide::Long);
}

TEST(Position, AddToLong) {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Buy 0.5 @ 51000
  pos.apply_fill(OrderSide::Buy, 0.5, 51000.0);

  EXPECT_EQ(pos.size(), 1.5);
  EXPECT_NEAR(pos.avg_price(), 50333.33, 0.01); // Weighted avg
}

TEST(Position, PartialClose) {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Sell 0.3 @ 51000
  pos.apply_fill(OrderSide::Sell, 0.3, 51000.0);

  EXPECT_EQ(pos.size(), 0.7);
  EXPECT_EQ(pos.avg_price(), 50000.0);  // Avg price unchanged
  EXPECT_EQ(pos.realized_pnl(), 300.0); // 0.3 * (51000 - 50000)
}

TEST(Position, FullClose) {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Sell 1.0 @ 51000
  pos.apply_fill(OrderSide::Sell, 1.0, 51000.0);

  EXPECT_EQ(pos.size(), 0.0);
  EXPECT_EQ(pos.side(), PositionSide::None);
  EXPECT_EQ(pos.realized_pnl(), 1000.0);
}

TEST(Position, UnrealizedPnL) {
  Position pos({"BTCUSDT"});

  // Buy 1.0 @ 50000
  pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

  // Current price: 51000
  EXPECT_EQ(pos.unrealized_pnl(51000.0), 1000.0);
}
