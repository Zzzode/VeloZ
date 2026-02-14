#include "kj/test.h"

#include <kj/function.h>
#include <kj/string.h>

namespace {

// Mock ExchangeAdapter for testing
struct MockExecutionReport {
  kj::String symbol;
  kj::String client_order_id;
};

struct MockExchangeAdapter {
  kj::String name_;
  kj::Function<MockExecutionReport(kj::StringPtr, kj::StringPtr)> place_handler_;
  kj::Function<MockExecutionReport(kj::StringPtr, kj::StringPtr)> cancel_handler_;

  explicit MockExchangeAdapter(kj::StringPtr name) : name_(kj::heapString(name)) {}

  void set_place_handler(kj::Function<MockExecutionReport(kj::StringPtr, kj::StringPtr)> handler) {
    place_handler_ = kj::mv(handler);
  }

  void set_cancel_handler(kj::Function<MockExecutionReport(kj::StringPtr, kj::StringPtr)> handler) {
    cancel_handler_ = kj::mv(handler);
  }

  void connect() {}

  void disconnect() {}

  kj::StringPtr name() const {
    return name_;
  }

  kj::StringPtr version() const {
    return "1.0.0"_kj;
  }
};

// Mock OrderRouter for testing
class MockOrderRouter {
public:
  MockOrderRouter() = default;

  MockExecutionReport place_order(kj::StringPtr symbol, kj::StringPtr client_order_id) {
    (void)symbol;
    MockExecutionReport report;
    report.client_order_id = kj::heapString(client_order_id);
    return report;
  }

  bool has_adapter(kj::StringPtr venue) {
    return venue == "MockExchange"_kj;
  }
};

KJ_TEST("OrderRouter: Basic test") {
  MockOrderRouter router;

  KJ_EXPECT(router.has_adapter("MockExchange"_kj));
  KJ_EXPECT(!router.has_adapter("binance"_kj));
}

KJ_TEST("OrderRouter: Place order") {
  MockOrderRouter router;

  auto report = router.place_order("BTCUSDT"_kj, "CLIENT123"_kj);

  KJ_EXPECT(report.client_order_id == "CLIENT123"_kj);
}

} // namespace
