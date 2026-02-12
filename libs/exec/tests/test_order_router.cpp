#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_router.h"

#include <gtest/gtest.h>

using namespace veloz::exec;

class MockAdapter : public ExchangeAdapter {
public:
  std::string name_;

  MockAdapter(const std::string& name) : name_(name) {}

  std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.venue_order_id = name_ + "_" + req.client_order_id;
    report.status = OrderStatus::Accepted;
    return report;
  }

  std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.status = OrderStatus::Canceled;
    return report;
  }

  bool is_connected() const override {
    return true;
  }
  void connect() override {}
  void disconnect() override {}

  const char* name() const override {
    return name_.c_str();
  }
  const char* version() const override {
    return "1.0.0";
  }
};

TEST(OrderRouter, RegisterAdapter) {
  OrderRouter router;
  auto adapter = std::make_shared<MockAdapter>("binance");

  router.register_adapter(veloz::common::Venue::Binance, adapter);

  EXPECT_TRUE(router.has_adapter(veloz::common::Venue::Binance));
}

TEST(OrderRouter, RouteOrder) {
  OrderRouter router;
  auto adapter = std::make_shared<MockAdapter>("binance");
  router.register_adapter(veloz::common::Venue::Binance, adapter);

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.client_order_id = "CLIENT123";

  auto report = router.place_order(veloz::common::Venue::Binance, req);
  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->venue_order_id, "binance_CLIENT123");
}

TEST(OrderRouter, RouteToDefaultVenue) {
  OrderRouter router;
  auto adapter = std::make_shared<MockAdapter>("binance");
  router.register_adapter(veloz::common::Venue::Binance, adapter);
  router.set_default_venue(veloz::common::Venue::Binance);

  PlaceOrderRequest req;
  req.symbol = {"BTCUSDT"};
  req.client_order_id = "CLIENT123";

  // Route without specifying venue
  auto report = router.place_order(req);
  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->venue_order_id, "binance_CLIENT123");
}
