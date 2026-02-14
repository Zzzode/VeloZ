#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"
#include "veloz/risk/risk_engine.h"

#include <gtest/gtest.h> // Kept for gtest framework compatibility
#include <kj/string.h>
#include <string> // Kept for RiskCheckResult::reason (std::string)

using namespace veloz::risk;
using namespace veloz::exec;

TEST(RiskEngine, CheckAvailableFunds) {
  RiskEngine engine;

  engine.set_account_balance(10000.0); // USDT

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 50000.0; // Needs 5000 USDT

  auto result = engine.check_pre_trade(req);
  EXPECT_TRUE(result.allowed);
}

TEST(RiskEngine, RejectInsufficientFunds) {
  RiskEngine engine;

  engine.set_account_balance(1000.0); // Only 1000 USDT

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 50000.0; // Needs 5000 USDT

  auto result = engine.check_pre_trade(req);
  EXPECT_FALSE(result.allowed);
  EXPECT_TRUE(result.reason.find("Insufficient funds") != std::string::npos);
}

TEST(RiskEngine, CheckMaxPosition) {
  RiskEngine engine;

  engine.set_max_position_size(1.0);

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.side = OrderSide::Buy;
  req.qty = 2.0; // Exceeds max

  auto result = engine.check_pre_trade(req);
  EXPECT_FALSE(result.allowed);
}

TEST(RiskEngine, CheckPriceDeviation) {
  RiskEngine engine;

  engine.set_reference_price(50000.0);
  engine.set_max_price_deviation(0.05); // 5%

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 53000.0; // 6% above reference

  auto result = engine.check_pre_trade(req);
  EXPECT_FALSE(result.allowed);
}
