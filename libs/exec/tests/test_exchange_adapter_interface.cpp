#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

#include <gtest/gtest.h>

using namespace veloz::exec;

// Mock adapter for testing
class MockExchangeAdapter : public ExchangeAdapter {
public:
  std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.venue_order_id = "VENUE123";
    report.status = OrderStatus::Accepted;
    return report;
  }

  std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.venue_order_id = "VENUE123";
    report.status = OrderStatus::Canceled;
    return report;
  }

  bool is_connected() const override {
    return true;
  }
  void connect() override {}
  void disconnect() override {}

  const char* name() const override {
    return "MockExchange";
  }
  const char* version() const override {
    return "1.0.0";
  }
};

TEST(ExchangeAdapter, PlaceOrder) {
  MockExchangeAdapter adapter;

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.side = OrderSide::Buy;
  req.type = OrderType::Limit;
  req.qty = 0.5;
  req.price = 50000.0;
  req.client_order_id = "CLIENT123";

  auto report = adapter.place_order(req);
  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->status, OrderStatus::Accepted);
  EXPECT_EQ(report->venue_order_id, "VENUE123");
}

TEST(ExchangeAdapter, CancelOrder) {
  MockExchangeAdapter adapter;

  CancelOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.client_order_id = "CLIENT123";

  auto report = adapter.cancel_order(req);
  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->status, OrderStatus::Canceled);
}
