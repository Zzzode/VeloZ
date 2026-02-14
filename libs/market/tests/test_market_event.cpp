#include "kj/test.h"
#include "veloz/market/market_event.h"

#include <kj/string.h>

namespace {

using namespace veloz::market;

KJ_TEST("MarketEvent: Trade event serialization") {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.venue = veloz::common::Venue::Binance;
  event.market = veloz::common::MarketKind::Spot;
  event.symbol = "BTCUSDT";
  event.ts_exchange_ns = 1700000000000000LL;
  event.ts_recv_ns = 1700000000000001000LL;
  event.ts_pub_ns = 1700000000000002000LL;

  // Trade payload
  event.payload = kj::heapString(R"({"price": "50000.5", "qty": "0.1", "is_buyer_maker": false})");

  KJ_EXPECT(event.type == MarketEventType::Trade);
  KJ_EXPECT(event.venue == veloz::common::Venue::Binance);
  KJ_EXPECT(event.market == veloz::common::MarketKind::Spot);
  KJ_EXPECT(event.payload.size() > 0);
}

KJ_TEST("MarketEvent: Book event serialization") {
  MarketEvent event;
  event.type = MarketEventType::BookTop;
  event.symbol = "ETHUSDT";

  // Book payload
  event.payload = kj::heapString(
      R"({"bids": [["3000.0", "1.0"]], "asks": [["3001.0", "1.0"]], "seq": 123456})");

  KJ_EXPECT(event.type == MarketEventType::BookTop);
  KJ_EXPECT(event.symbol == "ETHUSDT");
}

KJ_TEST("MarketEvent: Latency helpers") {
  MarketEvent event;
  event.ts_exchange_ns = 1000000000LL;
  event.ts_recv_ns = 1000001000LL;
  event.ts_pub_ns = 1000002000LL;

  auto exchange_to_pub = event.exchange_to_pub_ns();
  auto recv_to_pub = event.recv_to_pub_ns();

  KJ_EXPECT(exchange_to_pub == 2000LL);
  KJ_EXPECT(recv_to_pub == 1000LL);
}

KJ_TEST("MarketEvent: Symbol access") {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.symbol = "BTCUSDT";

  KJ_EXPECT(event.symbol == "BTCUSDT");
  KJ_EXPECT(event.type == MarketEventType::Trade);
}

KJ_TEST("MarketEvent: Payload access") {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.payload = kj::heapString(R"({"price": "50000.5", "qty": "0.1", "is_buyer_maker": false})");

  KJ_EXPECT(event.payload.size() > 0);
  // Use kj::StringPtr::findFirst to search for substring
  kj::StringPtr payloadPtr = event.payload;
  KJ_EXPECT(payloadPtr.findFirst('p') != nullptr);
}

KJ_TEST("MarketEvent: Timestamp operations") {
  MarketEvent event;
  event.ts_exchange_ns = 123456789LL;
  event.ts_recv_ns = 123456789LL;
  event.ts_pub_ns = 123456789LL;

  KJ_EXPECT(event.ts_exchange_ns == 123456789LL);
}

} // namespace
