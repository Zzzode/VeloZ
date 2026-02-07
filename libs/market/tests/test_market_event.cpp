#include <gtest/gtest.h>
#include "veloz/market/market_event.h"

using namespace veloz::market;

TEST(MarketEvent, TradeEventSerialization) {
    MarketEvent event;
    event.type = MarketEventType::Trade;
    event.venue = veloz::common::Venue::Binance;
    event.market = veloz::common::MarketKind::Spot;
    event.symbol = {"BTCUSDT"};
    event.ts_exchange_ns = 1700000000000000000LL;
    event.ts_recv_ns = 1700000000000001000LL;
    event.ts_pub_ns = 1700000000000002000LL;

    // Trade payload: {"price": "50000.5", "qty": "0.1", "is_buyer_maker": false}
    event.payload = R"({"price": "50000.5", "qty": "0.1", "is_buyer_maker": false})";

    EXPECT_EQ(event.type, MarketEventType::Trade);
    EXPECT_EQ(event.venue, veloz::common::Venue::Binance);
    EXPECT_FALSE(event.payload.empty());
}

TEST(MarketEvent, BookEvent) {
    MarketEvent event;
    event.type = MarketEventType::BookTop;
    event.symbol = {"ETHUSDT"};
    // Book payload: {"bids": [["3000.0", "1.0"]], "asks": [["3001.0", "1.0"]], "seq": 123456}
    event.payload = R"({"bids": [["3000.0", "1.0"]], "asks": [["3001.0", "1.0"]], "seq": 123456})";

    EXPECT_EQ(event.type, MarketEventType::BookTop);
    EXPECT_EQ(event.symbol.value, "ETHUSDT");
}

TEST(MarketEvent, LatencyHelpers) {
    MarketEvent event;
    event.ts_exchange_ns = 1000000000LL;
    event.ts_recv_ns = 1000001000LL;
    event.ts_pub_ns = 1000002000LL;

    // These methods don't exist yet - will fail compilation
    EXPECT_EQ(event.exchange_to_pub_ns(), 2000LL);
    EXPECT_EQ(event.recv_to_pub_ns(), 1000LL);
}

TEST(MarketEvent, TypedTradeData) {
    MarketEvent event;
    event.type = MarketEventType::Trade;

    // This variant field doesn't exist yet - will fail compilation
    TradeData trade;
    trade.price = 50000.5;
    trade.qty = 0.1;
    trade.is_buyer_maker = false;
    trade.trade_id = 123456;

    event.data = trade;

    ASSERT_TRUE(std::holds_alternative<TradeData>(event.data));
    const auto& stored_trade = std::get<TradeData>(event.data);
    EXPECT_DOUBLE_EQ(stored_trade.price, 50000.5);
    EXPECT_DOUBLE_EQ(stored_trade.qty, 0.1);
    EXPECT_FALSE(stored_trade.is_buyer_maker);
    EXPECT_EQ(stored_trade.trade_id, 123456);
}

TEST(MarketEvent, TypedBookData) {
    MarketEvent event;
    event.type = MarketEventType::BookTop;

    // These types don't exist yet - will fail compilation
    BookData book;
    book.bids.push_back({3000.0, 1.0});
    book.asks.push_back({3001.0, 1.0});
    book.sequence = 123456;

    event.data = book;

    ASSERT_TRUE(std::holds_alternative<BookData>(event.data));
    const auto& stored_book = std::get<BookData>(event.data);
    EXPECT_EQ(stored_book.bids.size(), 1);
    EXPECT_EQ(stored_book.asks.size(), 1);
    EXPECT_DOUBLE_EQ(stored_book.bids[0].price, 3000.0);
    EXPECT_DOUBLE_EQ(stored_book.asks[0].price, 3001.0);
    EXPECT_EQ(stored_book.sequence, 123456);
}

TEST(MarketEvent, TypedKlineData) {
    MarketEvent event;
    event.type = MarketEventType::Kline;

    // This type doesn't exist yet - will fail compilation
    KlineData kline;
    kline.open = 50000.0;
    kline.high = 51000.0;
    kline.low = 49500.0;
    kline.close = 50500.0;
    kline.volume = 100.5;
    kline.start_time = 1700000000000LL;
    kline.close_time = 1700000060000LL;

    event.data = kline;

    ASSERT_TRUE(std::holds_alternative<KlineData>(event.data));
    const auto& stored_kline = std::get<KlineData>(event.data);
    EXPECT_DOUBLE_EQ(stored_kline.open, 50000.0);
    EXPECT_DOUBLE_EQ(stored_kline.high, 51000.0);
    EXPECT_DOUBLE_EQ(stored_kline.low, 49500.0);
    EXPECT_DOUBLE_EQ(stored_kline.close, 50500.0);
    EXPECT_DOUBLE_EQ(stored_kline.volume, 100.5);
}
