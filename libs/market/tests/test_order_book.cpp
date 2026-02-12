#include "veloz/market/market_event.h"
#include "veloz/market/order_book.h"

#include <gtest/gtest.h>

using namespace veloz::market;

TEST(OrderBook, Initialization) {
  OrderBook book;
  book.apply_snapshot({}, {}, 0);
  EXPECT_TRUE(book.bids().empty());
  EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBook, ApplySnapshot) {
  OrderBook book;
  std::vector<BookLevel> bids = {{50000.0, 1.5}, {49999.0, 2.0}};
  std::vector<BookLevel> asks = {{50001.0, 1.0}, {50002.0, 0.5}};

  book.apply_snapshot(bids, asks, 100);

  EXPECT_EQ(book.bids().size(), 2);
  EXPECT_EQ(book.bids()[0].price, 50000.0);
  EXPECT_EQ(book.bids()[0].qty, 1.5);

  EXPECT_EQ(book.asks().size(), 2);
  EXPECT_EQ(book.asks()[0].price, 50001.0);
  EXPECT_EQ(book.sequence(), 100);
}

TEST(OrderBook, ApplyDelta) {
  OrderBook book;
  std::vector<BookLevel> bids = {{50000.0, 1.5}};
  std::vector<BookLevel> asks = {{50001.0, 1.0}};
  book.apply_snapshot(bids, asks, 100);

  // Update bid qty
  book.apply_delta(BookLevel{50000.0, 1.0}, true, 101);
  EXPECT_EQ(book.bids()[0].qty, 1.0);

  // Remove ask
  book.apply_delta(BookLevel{50001.0, 0.0}, false, 102);
  EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBook, BestBidAsk) {
  OrderBook book;
  std::vector<BookLevel> bids = {{50000.0, 1.5}, {49999.0, 2.0}};
  std::vector<BookLevel> asks = {{50001.0, 1.0}};
  book.apply_snapshot(bids, asks, 100);

  auto best_bid = book.best_bid();
  auto best_ask = book.best_ask();

  ASSERT_TRUE(best_bid.has_value());
  ASSERT_TRUE(best_ask.has_value());
  EXPECT_EQ(best_bid->price, 50000.0);
  EXPECT_EQ(best_ask->price, 50001.0);
}
