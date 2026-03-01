#include "kj/test.h"
#include "veloz/market/managed_order_book.h"

#include <kj/async-io.h>
#include <kj/timer.h>
#include <kj/vector.h>

namespace {

using namespace veloz::market;

// Helper to create a BookData delta
BookData make_delta(int64_t first_u, int64_t final_u, kj::Vector<BookLevel> bids = {},
                    kj::Vector<BookLevel> asks = {}) {
  BookData data;
  data.first_update_id = first_u;
  data.sequence = final_u;
  data.is_snapshot = false;
  data.bids = kj::mv(bids);
  data.asks = kj::mv(asks);
  return data;
}

// Helper to create a BookData snapshot
BookData make_snapshot(int64_t last_update_id, kj::Vector<BookLevel> bids = {},
                       kj::Vector<BookLevel> asks = {}) {
  BookData data;
  data.sequence = last_update_id;
  data.first_update_id = 0;
  data.is_snapshot = true;
  data.bids = kj::mv(bids);
  data.asks = kj::mv(asks);
  return data;
}

KJ_TEST("ManagedOrderBook: Initial state") {
  auto io = kj::setupAsyncIo();
  ManagedOrderBook mob("BTCUSDT", io.provider->getTimer());

  KJ_EXPECT(mob.state() == SyncState::Disconnected);
  KJ_EXPECT(!mob.is_synchronized());
  KJ_EXPECT(mob.symbol() == "BTCUSDT");
}

KJ_TEST("ManagedOrderBook: Configuration") {
  auto io = kj::setupAsyncIo();
  ManagedOrderBook mob("BTCUSDT", io.provider->getTimer());

  mob.set_max_buffer_size(5000);
  mob.set_max_depth_levels(50);
  mob.set_snapshot_timeout_ms(10000);

  // Verify depth levels propagate to underlying order book
  KJ_EXPECT(mob.order_book().max_depth_levels() == 50);
}

KJ_TEST("ManagedOrderBook: Delta handling when not running") {
  auto io = kj::setupAsyncIo();
  ManagedOrderBook mob("BTCUSDT", io.provider->getTimer());

  // Create some test deltas
  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.0});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});

  // Without starting, deltas are silently ignored (running_ is false)
  mob.on_delta(make_delta(100, 100, kj::mv(bids), kj::mv(asks)));
  // No counting happens when not running
  KJ_EXPECT(mob.stats().delta_count == 0);
  KJ_EXPECT(mob.stats().dropped_delta_count == 0);
}

KJ_TEST("ManagedOrderBook: Stats tracking") {
  auto io = kj::setupAsyncIo();
  ManagedOrderBook mob("BTCUSDT", io.provider->getTimer());

  const auto& stats = mob.stats();
  KJ_EXPECT(stats.snapshot_count == 0);
  KJ_EXPECT(stats.delta_count == 0);
  KJ_EXPECT(stats.resync_count == 0);
}

KJ_TEST("OrderBook: Apply book data snapshot") {
  OrderBook book;

  kj::Vector<BookLevel> bids;
  bids.add(BookLevel{50000.0, 1.5});
  bids.add(BookLevel{49999.0, 2.0});
  kj::Vector<BookLevel> asks;
  asks.add(BookLevel{50001.0, 1.0});
  asks.add(BookLevel{50002.0, 0.5});

  BookData snapshot;
  snapshot.is_snapshot = true;
  snapshot.sequence = 100;
  snapshot.bids = kj::mv(bids);
  snapshot.asks = kj::mv(asks);

  auto result = book.apply_book_data(snapshot);
  KJ_EXPECT(result == UpdateResult::Applied);
  KJ_EXPECT(book.sequence() == 100);
  KJ_EXPECT(book.bids().size() == 2);
  KJ_EXPECT(book.asks().size() == 2);
}

KJ_TEST("OrderBook: Apply book data delta") {
  OrderBook book;

  // First apply a snapshot
  kj::Vector<BookLevel> snap_bids;
  snap_bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> snap_asks;
  snap_asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(snap_bids, snap_asks, 100);

  // Now apply a delta via apply_book_data
  kj::Vector<BookLevel> delta_bids;
  delta_bids.add(BookLevel{50000.0, 2.0}); // Update existing
  delta_bids.add(BookLevel{49999.0, 1.0}); // Add new
  kj::Vector<BookLevel> delta_asks;
  delta_asks.add(BookLevel{50001.0, 1.5}); // Update existing

  BookData delta;
  delta.is_snapshot = false;
  delta.first_update_id = 101;
  delta.sequence = 102;
  delta.bids = kj::mv(delta_bids);
  delta.asks = kj::mv(delta_asks);

  auto result = book.apply_book_data(delta);
  KJ_EXPECT(result == UpdateResult::Applied);
  KJ_EXPECT(book.sequence() == 102);
  KJ_EXPECT(book.bids().size() == 2);
  KJ_EXPECT(book.bids()[0].qty == 2.0);
}

KJ_TEST("BookData: First update ID tracking") {
  BookData delta;
  delta.first_update_id = 100;
  delta.sequence = 105;
  delta.is_snapshot = false;

  KJ_EXPECT(delta.first_update_id == 100);
  KJ_EXPECT(delta.sequence == 105);
  KJ_EXPECT(!delta.is_snapshot);

  BookData snapshot;
  snapshot.sequence = 99;
  snapshot.is_snapshot = true;

  KJ_EXPECT(snapshot.first_update_id == 0);
  KJ_EXPECT(snapshot.sequence == 99);
  KJ_EXPECT(snapshot.is_snapshot);
}

KJ_TEST("BookData: Equality comparison") {
  kj::Vector<BookLevel> bids1;
  bids1.add(BookLevel{100.0, 1.0});
  kj::Vector<BookLevel> asks1;
  asks1.add(BookLevel{101.0, 1.0});

  kj::Vector<BookLevel> bids2;
  bids2.add(BookLevel{100.0, 1.0});
  kj::Vector<BookLevel> asks2;
  asks2.add(BookLevel{101.0, 1.0});

  BookData data1;
  data1.sequence = 100;
  data1.first_update_id = 99;
  data1.is_snapshot = false;
  data1.bids = kj::mv(bids1);
  data1.asks = kj::mv(asks1);

  BookData data2;
  data2.sequence = 100;
  data2.first_update_id = 99;
  data2.is_snapshot = false;
  data2.bids = kj::mv(bids2);
  data2.asks = kj::mv(asks2);

  KJ_EXPECT(data1 == data2);

  // Different first_update_id should not be equal
  data2.first_update_id = 98;
  KJ_EXPECT(!(data1 == data2));
}

KJ_TEST("OrderBook: Binance-style sequence validation") {
  OrderBook book;

  // Apply snapshot with lastUpdateId = 100
  kj::Vector<BookLevel> snap_bids;
  snap_bids.add(BookLevel{50000.0, 1.5});
  kj::Vector<BookLevel> snap_asks;
  snap_asks.add(BookLevel{50001.0, 1.0});
  book.apply_snapshot(snap_bids, snap_asks, 100);

  KJ_EXPECT(book.expected_sequence() == 101);

  // First delta: U=101, u=103 (valid: U <= lastUpdateId+1 AND u >= lastUpdateId+1)
  kj::Vector<BookLevel> d1_bids;
  d1_bids.add(BookLevel{50000.0, 2.0});
  kj::Vector<BookLevel> d1_asks;

  auto result1 = book.apply_deltas(d1_bids, d1_asks, 101, 103);
  KJ_EXPECT(result1 == UpdateResult::Applied);
  KJ_EXPECT(book.sequence() == 103);
  KJ_EXPECT(book.expected_sequence() == 104);

  // Second delta: U=104, u=106 (valid: U == previous_u + 1)
  kj::Vector<BookLevel> d2_bids;
  d2_bids.add(BookLevel{49999.0, 1.0});
  kj::Vector<BookLevel> d2_asks;

  auto result2 = book.apply_deltas(d2_bids, d2_asks, 104, 106);
  KJ_EXPECT(result2 == UpdateResult::Applied);
  KJ_EXPECT(book.sequence() == 106);
}

KJ_TEST("SyncState: Enum values") {
  KJ_EXPECT(static_cast<int>(SyncState::Disconnected) == 0);
  KJ_EXPECT(static_cast<int>(SyncState::Buffering) == 1);
  KJ_EXPECT(static_cast<int>(SyncState::FetchingSnapshot) == 2);
  KJ_EXPECT(static_cast<int>(SyncState::Synchronizing) == 3);
  KJ_EXPECT(static_cast<int>(SyncState::Synchronized) == 4);
  KJ_EXPECT(static_cast<int>(SyncState::Resynchronizing) == 5);
}

} // namespace
