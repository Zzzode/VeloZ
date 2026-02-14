#include "kj/test.h"
#include "veloz/oms/order_wal.h"

#include <chrono>
#include <kj/filesystem.h>

namespace {

using namespace veloz::oms;
using namespace veloz::exec;

KJ_TEST("OrderWal: Basic construction") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.directory = kj::heapString(".");
  config.file_prefix = kj::heapString("test_orders");
  config.sync_on_write = false; // Faster for tests

  OrderWal wal(cwd, kj::mv(config));

  KJ_EXPECT(wal.is_healthy());
  KJ_EXPECT(wal.current_sequence() == 0);
}

KJ_TEST("OrderWal: Log order new") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_new");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  PlaceOrderRequest request;
  request.client_order_id = kj::heapString("ORDER-001");
  request.symbol = veloz::common::SymbolId("BTCUSDT");
  request.side = OrderSide::Buy;
  request.type = OrderType::Limit;
  request.tif = TimeInForce::GTC;
  request.qty = 1.0;
  request.price = 50000.0;

  auto seq = wal.log_order_new(request);

  KJ_EXPECT(seq == 1);
  KJ_EXPECT(wal.current_sequence() == 1);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 1);
}

KJ_TEST("OrderWal: Log order fill") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_fill");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  auto seq = wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50100.0, 1234567890);

  KJ_EXPECT(seq == 1);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 1);
}

KJ_TEST("OrderWal: Log order update") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_update");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  auto seq = wal.log_order_update("ORDER-001"_kj, "VENUE-123"_kj, "PARTIALLY_FILLED"_kj, ""_kj,
                                  1234567890);

  KJ_EXPECT(seq == 1);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 1);
}

KJ_TEST("OrderWal: Multiple entries") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_multi");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  PlaceOrderRequest request;
  request.client_order_id = kj::heapString("ORDER-001");
  request.symbol = veloz::common::SymbolId("BTCUSDT");
  request.side = OrderSide::Buy;
  request.type = OrderType::Limit;
  request.qty = 1.0;
  request.price = 50000.0;

  wal.log_order_new(request);
  wal.log_order_update("ORDER-001"_kj, "VENUE-123"_kj, "NEW"_kj, ""_kj, 1000);
  wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50100.0, 2000);
  wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50050.0, 3000);
  wal.log_order_update("ORDER-001"_kj, "VENUE-123"_kj, "FILLED"_kj, ""_kj, 4000);

  KJ_EXPECT(wal.current_sequence() == 5);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 5);
}

KJ_TEST("OrderWal: Checkpoint") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_checkpoint");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));
  OrderStore store;

  // Add some orders to the store
  PlaceOrderRequest request1;
  request1.client_order_id = kj::heapString("ORDER-001");
  request1.symbol = veloz::common::SymbolId("BTCUSDT");
  request1.side = OrderSide::Buy;
  request1.qty = 1.0;
  request1.price = 50000.0;
  store.note_order_params(request1);

  PlaceOrderRequest request2;
  request2.client_order_id = kj::heapString("ORDER-002");
  request2.symbol = veloz::common::SymbolId("ETHUSDT");
  request2.side = OrderSide::Sell;
  request2.qty = 10.0;
  request2.price = 3000.0;
  store.note_order_params(request2);

  auto seq = wal.write_checkpoint(store);

  KJ_EXPECT(seq == 1);

  auto stats = wal.stats();
  KJ_EXPECT(stats.checkpoints == 1);
}

KJ_TEST("OrderWal: Replay into store") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  // Use unique prefix to avoid conflicts with previous test runs
  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_replay_", timestamp);

  // First, write some entries
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));

    PlaceOrderRequest request;
    request.client_order_id = kj::heapString("ORDER-001");
    request.symbol = veloz::common::SymbolId("BTCUSDT");
    request.side = OrderSide::Buy;
    request.type = OrderType::Limit;
    request.qty = 1.0;
    request.price = 50000.0;

    wal.log_order_new(request);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50100.0, 2000);
    wal.sync();
  }

  // Now replay into a new store
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Verify the order was restored
    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_MAYBE (order, maybeOrder) {
      KJ_EXPECT(order->client_order_id == "ORDER-001"_kj);
      KJ_EXPECT(order->symbol == "BTCUSDT"_kj);
      KJ_EXPECT(order->executed_qty == 0.5);
    } else {
      KJ_FAIL_EXPECT("Order not found after replay");
    }

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 2);
  }

  // Clean up test WAL files
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("OrderWal: Stats tracking") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.file_prefix = kj::heapString("test_stats");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  PlaceOrderRequest request;
  request.client_order_id = kj::heapString("ORDER-001");
  request.symbol = veloz::common::SymbolId("BTCUSDT");
  request.side = OrderSide::Buy;
  request.qty = 1.0;

  wal.log_order_new(request);
  wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 1.0, 50000.0, 1000);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 2);
  KJ_EXPECT(stats.bytes_written > 0);
  KJ_EXPECT(stats.current_sequence == 2);
}

} // namespace
