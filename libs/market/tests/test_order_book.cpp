#include "kj/test.h"
#include "veloz/market/order_book.h"

#include <kj/vector.h>

namespace {

using namespace veloz::market;

KJ_TEST("OrderBook: Initialization") {
  OrderBook book;
  kj::Vector<BookLevel> empty_bids;
  kj::Vector<BookLevel> empty_asks;
  book.apply_snapshot(empty_bids, empty_asks, 0);

  KJ_EXPECT(book.bids().size() == 0);
  KJ_EXPECT(book.asks().size() == 0);
}

KJ_TEST("OrderBook: Apply snapshot") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  bids.add(BookLevel{49999.0, 2.0});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  asks.add(BookLevel{50002.0, 0.5});

  book.apply_snapshot(bids, asks, 100);

  KJ_EXPECT(book.bids().size() == 2);
  KJ_EXPECT(book.bids()[0].price == 50000.0);
  KJ_EXPECT(book.bids()[0].qty == 1.5);

  KJ_EXPECT(book.asks().size() == 2);
  KJ_EXPECT(book.asks()[0].price == 50001.0);
  KJ_EXPECT(book.sequence() == 100);
}

KJ_TEST("OrderBook: Apply delta") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Update bid qty
  book.apply_delta(BookLevel{50000.0, 1.0}, true, 101);

  KJ_EXPECT(book.bids()[0].qty == 1.0);

  // Remove ask
  book.apply_delta(BookLevel{50001.0, 0.0}, false, 102);

  KJ_EXPECT(book.asks().size() == 0);
}

KJ_TEST("OrderBook: Best bid ask") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  bids.add(BookLevel{49999.0, 2.0});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  auto best_bid = book.best_bid();
  auto best_ask = book.best_ask();

  KJ_IF_SOME(bid, best_bid) {
    KJ_EXPECT(bid.price == 50000.0);
  }
  KJ_IF_SOME(ask, best_ask) {
    KJ_EXPECT(ask.price == 50001.0);
  }
}

KJ_TEST("OrderBook: Clear") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  book.clear();

  KJ_EXPECT(book.bids().size() == 0);
  KJ_EXPECT(book.asks().size() == 0);
}

KJ_TEST("OrderBook: Sequence tracking") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  KJ_EXPECT(book.sequence() == 100);
}

KJ_TEST("OrderBook: State transitions") {
  OrderBook book;

  // Initial state should be Empty
  KJ_EXPECT(book.state() == OrderBookState::Empty);
  KJ_EXPECT(!book.is_synchronized());

  // Apply snapshot should transition to Synchronized
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  KJ_EXPECT(book.state() == OrderBookState::Synchronized);
  KJ_EXPECT(book.is_synchronized());
  KJ_EXPECT(book.expected_sequence() == 101);
}

KJ_TEST("OrderBook: Duplicate update rejection") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Apply update with same sequence (duplicate)
  auto result = book.apply_delta(BookLevel{50000.0, 2.0}, true, 100);
  KJ_EXPECT(result == UpdateResult::Duplicate);
  KJ_EXPECT(book.duplicate_count() == 1);

  // Bid should still be 1.5
  KJ_EXPECT(book.bids()[0].qty == 1.5);

  // Apply update with older sequence
  result = book.apply_delta(BookLevel{50000.0, 3.0}, true, 99);
  KJ_EXPECT(result == UpdateResult::Duplicate);
  KJ_EXPECT(book.duplicate_count() == 2);
}

KJ_TEST("OrderBook: Sequential updates") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Apply sequential updates
  auto result1 = book.apply_delta(BookLevel{50000.0, 2.0}, true, 101);
  KJ_EXPECT(result1 == UpdateResult::Applied);
  KJ_EXPECT(book.bids()[0].qty == 2.0);
  KJ_EXPECT(book.sequence() == 101);

  auto result2 = book.apply_delta(BookLevel{50001.0, 1.5}, false, 102);
  KJ_EXPECT(result2 == UpdateResult::Applied);
  KJ_EXPECT(book.asks()[0].qty == 1.5);
  KJ_EXPECT(book.sequence() == 102);
}

KJ_TEST("OrderBook: Gap detection and buffering") {
  OrderBook book;
  book.set_max_sequence_gap(10);
  book.set_max_buffer_size(100);

  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Skip sequence 101, apply 102 (gap of 1)
  auto result = book.apply_delta(BookLevel{50000.0, 2.0}, true, 102);
  KJ_EXPECT(result == UpdateResult::GapDetected);
  KJ_EXPECT(book.gap_count() == 1);
  KJ_EXPECT(book.buffered_update_count() == 1);

  // Sequence should still be 100 (update was buffered)
  KJ_EXPECT(book.sequence() == 100);

  // Now apply the missing update 101
  auto result2 = book.apply_delta(BookLevel{49999.0, 1.0}, true, 101);
  KJ_EXPECT(result2 == UpdateResult::Applied);

  // Both updates should now be applied
  KJ_EXPECT(book.sequence() == 102);
  KJ_EXPECT(book.buffered_update_count() == 0);
}

KJ_TEST("OrderBook: Large gap triggers resync") {
  OrderBook book;
  book.set_max_sequence_gap(5);

  bool snapshot_requested = false;
  book.set_snapshot_request_callback([&snapshot_requested]() { snapshot_requested = true; });

  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Apply update with large gap (> max_sequence_gap)
  auto result = book.apply_delta(BookLevel{50000.0, 2.0}, true, 110);

  // Should trigger resync
  KJ_EXPECT(book.state() == OrderBookState::Syncing);
  KJ_EXPECT(snapshot_requested);
}

KJ_TEST("OrderBook: Snapshot clears buffer") {
  OrderBook book;
  book.set_max_sequence_gap(10);

  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Buffer some out-of-order updates
  book.apply_delta(BookLevel{50000.0, 2.0}, true, 105);
  book.apply_delta(BookLevel{50000.0, 2.5}, true, 106);
  KJ_EXPECT(book.buffered_update_count() == 2);

  // Apply new snapshot (should clear buffer)
  kj::Vector<BookLevel> new_bids;
  new_bids.add(BookLevel{51000.0, 3.0});
  kj::Vector<BookLevel> new_asks;
  new_asks.add(BookLevel{51001.0, 2.0});
  book.apply_snapshot(new_bids, new_asks, 200);

  KJ_EXPECT(book.buffered_update_count() == 0);
  KJ_EXPECT(book.sequence() == 200);
  KJ_EXPECT(book.bids()[0].price == 51000.0);
}

KJ_TEST("OrderBook: Request rebuild") {
  OrderBook book;

  bool snapshot_requested = false;
  book.set_snapshot_request_callback([&snapshot_requested]() { snapshot_requested = true; });

  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  KJ_EXPECT(book.is_synchronized());

  // Request rebuild
  book.request_rebuild();

  KJ_EXPECT(book.state() == OrderBookState::Syncing);
  KJ_EXPECT(snapshot_requested);
}

KJ_TEST("OrderBook: Batch delta application") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Apply batch of deltas
  kj::Vector<BookLevel> bid_deltas;
  bid_deltas.add(BookLevel{50000.0, 2.0});
  bid_deltas.add(BookLevel{49999.0, 1.0});
  kj::Vector<BookLevel> ask_deltas;
  ask_deltas.add(BookLevel{50001.0, 1.5});
  ask_deltas.add(BookLevel{50002.0, 0.5});

  auto result = book.apply_deltas(bid_deltas, ask_deltas, 101, 102);
  KJ_EXPECT(result == UpdateResult::Applied);
  KJ_EXPECT(book.sequence() == 102);
  KJ_EXPECT(book.bids().size() == 2);
  KJ_EXPECT(book.asks().size() == 2);
}

KJ_TEST("OrderBook: Clear resets state") {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(bids, asks, 100);

  // Generate some statistics
  book.apply_delta(BookLevel{50000.0, 2.0}, true, 100); // duplicate
  book.apply_delta(BookLevel{50000.0, 2.0}, true, 105); // gap

  KJ_EXPECT(book.duplicate_count() > 0);
  KJ_EXPECT(book.gap_count() > 0);

  // Clear should reset everything
  book.clear();

  KJ_EXPECT(book.state() == OrderBookState::Empty);
  KJ_EXPECT(book.sequence() == 0);
  KJ_EXPECT(book.expected_sequence() == 0);
  KJ_EXPECT(book.gap_count() == 0);
  KJ_EXPECT(book.duplicate_count() == 0);
  KJ_EXPECT(book.buffered_update_count() == 0);
}

} // namespace
