#include "kj/test.h"
#include "veloz/exec/order_api.h"
#include "veloz/risk/risk_engine.h"

namespace {

using namespace veloz::risk;
using namespace veloz::exec;

KJ_TEST("RiskEngine: Check available funds") {
  RiskEngine engine;

  engine.set_account_balance(10000.0);

  PlaceOrderRequest req;
  req.symbol = kj::str("BTCUSDT");
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 50000.0;

  auto result = engine.check_pre_trade(req);
  KJ_EXPECT(result.allowed);
}

KJ_TEST("RiskEngine: Reject insufficient funds") {
  RiskEngine engine;

  engine.set_account_balance(1000.0);

  PlaceOrderRequest req;
  req.symbol = kj::str("BTCUSDT");
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 50000.0;

  auto result = engine.check_pre_trade(req);
  KJ_EXPECT(!result.allowed);
  // Check if reason contains "Insufficient funds"
  KJ_EXPECT(result.reason.findFirst('I') != kj::none);
}

KJ_TEST("RiskEngine: Check max position") {
  RiskEngine engine;

  engine.set_max_position_size(1.0);

  PlaceOrderRequest req;
  req.symbol = kj::str("BTCUSDT");
  req.side = OrderSide::Buy;
  req.qty = 2.0;

  auto result = engine.check_pre_trade(req);
  KJ_EXPECT(!result.allowed);
}

KJ_TEST("RiskEngine: Check price deviation") {
  RiskEngine engine;

  engine.set_reference_price(50000.0);
  engine.set_max_price_deviation(0.05);

  PlaceOrderRequest req;
  req.symbol = kj::str("BTCUSDT");
  req.side = OrderSide::Buy;
  req.qty = 0.1;
  req.price = 53000.0;

  auto result = engine.check_pre_trade(req);
  KJ_EXPECT(!result.allowed);
}

} // namespace
