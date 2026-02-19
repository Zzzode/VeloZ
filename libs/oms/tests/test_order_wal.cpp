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
  config.directory = kj::heapString("test_wal");
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
  config.directory = kj::heapString("test_wal");
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
  config.directory = kj::heapString("test_wal");
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
  config.directory = kj::heapString("test_wal");
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
  config.directory = kj::heapString("test_wal");
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
  config.directory = kj::heapString("test_wal");
  config.file_prefix = kj::heapString("test_checkpoint");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));
  OrderStore store;

  // Add some orders to store
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

    // Verify that order was restored
    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      KJ_EXPECT(order.client_order_id == "ORDER-001"_kj);
      KJ_EXPECT(order.symbol == "BTCUSDT"_kj);
      KJ_EXPECT(order.executed_qty == 0.5);
    }
    else {
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
  config.directory = kj::heapString("test_wal");
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

KJ_TEST("OrderWal: Log order cancel") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  WalConfig config;
  config.directory = kj::heapString("test_wal");
  config.file_prefix = kj::heapString("test_cancel");
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  auto seq = wal.log_order_cancel("ORDER-001"_kj, "User requested"_kj, 1234567890);

  KJ_EXPECT(seq == 1);

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 1);
}

KJ_TEST("OrderWal: Crash recovery with checkpoint") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_crash_recovery_", timestamp);

  // Phase 1: Create orders, write checkpoint, then add more orders
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    // Create initial orders
    PlaceOrderRequest request1;
    request1.client_order_id = kj::heapString("ORDER-001");
    request1.symbol = veloz::common::SymbolId("BTCUSDT");
    request1.side = OrderSide::Buy;
    request1.type = OrderType::Limit;
    request1.qty = 1.0;
    request1.price = 50000.0;

    wal.log_order_new(request1);
    store.note_order_params(request1);
    store.apply_order_update("ORDER-001"_kj, "BTCUSDT"_kj, "BUY"_kj, "VENUE-001"_kj, "NEW"_kj,
                             ""_kj, 1000);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "NEW"_kj, ""_kj, 1000);

    // Write checkpoint
    wal.write_checkpoint(store);

    // Add more orders after checkpoint (simulating activity before crash)
    PlaceOrderRequest request2;
    request2.client_order_id = kj::heapString("ORDER-002");
    request2.symbol = veloz::common::SymbolId("ETHUSDT");
    request2.side = OrderSide::Sell;
    request2.type = OrderType::Market;
    request2.qty = 5.0;

    wal.log_order_new(request2);
    wal.log_order_fill("ORDER-002"_kj, "ETHUSDT"_kj, 5.0, 3000.0, 2000);

    wal.sync();
  }

  // Phase 2: Simulate crash recovery - replay WAL into fresh store
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Verify ORDER-001 was restored from checkpoint
    auto maybeOrder1 = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order1, maybeOrder1) {
      KJ_EXPECT(order1.client_order_id == "ORDER-001"_kj);
      KJ_EXPECT(order1.symbol == "BTCUSDT"_kj);
    }
    else {
      KJ_FAIL_EXPECT("ORDER-001 not found after crash recovery");
    }

    // Verify ORDER-002 was restored from incremental log after checkpoint
    auto maybeOrder2 = store.get("ORDER-002"_kj);
    KJ_IF_SOME(order2, maybeOrder2) {
      KJ_EXPECT(order2.client_order_id == "ORDER-002"_kj);
      KJ_EXPECT(order2.symbol == "ETHUSDT"_kj);
      KJ_EXPECT(order2.executed_qty == 5.0);
    }
    else {
      KJ_FAIL_EXPECT("ORDER-002 not found after crash recovery");
    }

    KJ_EXPECT(store.count() == 2);
  }

  // Clean up
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("OrderWal: File rotation") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_rotation_", timestamp);

  WalConfig config;
  config.file_prefix = kj::heapString(prefix);
  config.sync_on_write = false;
  config.max_file_size = 1024; // Small size to trigger rotation

  OrderWal wal(cwd, kj::mv(config));

  // Write enough entries to trigger rotation
  for (int i = 0; i < 50; ++i) {
    PlaceOrderRequest request;
    request.client_order_id = kj::str("ORDER-", i);
    request.symbol = veloz::common::SymbolId("BTCUSDT");
    request.side = OrderSide::Buy;
    request.type = OrderType::Limit;
    request.qty = 1.0;
    request.price = 50000.0;

    wal.log_order_new(request);
  }

  auto stats = wal.stats();
  KJ_EXPECT(stats.entries_written == 50);
  // Rotation should have occurred due to small max_file_size
  KJ_EXPECT(stats.rotations >= 1);

  // Clean up WAL files
  auto walFileName1 = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName1));
  // Additional rotated files may exist
  for (int i = 1; i <= 10; ++i) {
    char hex_buf[17];
    snprintf(hex_buf, sizeof(hex_buf), "%016x", i);
    auto walFileName = kj::str(prefix, "_", hex_buf, ".wal");
    cwd.tryRemove(kj::Path::parse(walFileName));
  }
}

KJ_TEST("OrderWal: Manual rotation") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_manual_rotation_", timestamp);

  WalConfig config;
  config.file_prefix = kj::heapString(prefix);
  config.sync_on_write = false;

  OrderWal wal(cwd, kj::mv(config));

  PlaceOrderRequest request;
  request.client_order_id = kj::heapString("ORDER-001");
  request.symbol = veloz::common::SymbolId("BTCUSDT");
  request.side = OrderSide::Buy;
  request.qty = 1.0;

  wal.log_order_new(request);

  auto statsBefore = wal.stats();
  KJ_EXPECT(statsBefore.rotations == 0);

  wal.rotate();

  auto statsAfter = wal.stats();
  KJ_EXPECT(statsAfter.rotations == 1);

  // Clean up
  auto walFileName1 = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName1));
  auto walFileName2 = kj::str(prefix, "_0000000000000002.wal");
  cwd.tryRemove(kj::Path::parse(walFileName2));
}

KJ_TEST("OrderWal: Full order lifecycle recovery") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_lifecycle_", timestamp);

  // Phase 1: Complete order lifecycle
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));

    // New order
    PlaceOrderRequest request;
    request.client_order_id = kj::heapString("ORDER-001");
    request.symbol = veloz::common::SymbolId("BTCUSDT");
    request.side = OrderSide::Buy;
    request.type = OrderType::Limit;
    request.qty = 2.0;
    request.price = 50000.0;

    wal.log_order_new(request);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "NEW"_kj, ""_kj, 1000);

    // Partial fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50000.0, 2000);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "PARTIALLY_FILLED"_kj, ""_kj, 2000);

    // Another partial fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 1.0, 49990.0, 3000);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "PARTIALLY_FILLED"_kj, ""_kj, 3000);

    // Final fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50010.0, 4000);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "FILLED"_kj, ""_kj, 4000);

    wal.sync();
  }

  // Phase 2: Recover and verify
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      KJ_EXPECT(order.client_order_id == "ORDER-001"_kj);
      KJ_EXPECT(order.symbol == "BTCUSDT"_kj);
      KJ_EXPECT(order.executed_qty == 2.0);
      KJ_EXPECT(order.status == "FILLED"_kj);
      // Verify VWAP: (0.5*50000 + 1.0*49990 + 0.5*50010) / 2.0 = 49997.5
      KJ_EXPECT(order.avg_price > 49997.0 && order.avg_price < 49998.0);
    }
    else {
      KJ_FAIL_EXPECT("Order not found after lifecycle recovery");
    }

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 8);
  }

  // Clean up
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("OrderWal: Multiple orders recovery") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();

  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto prefix = kj::str("test_multi_orders_", timestamp);

  // Phase 1: Create multiple orders with different states
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));

    // Order 1: Filled
    PlaceOrderRequest request1;
    request1.client_order_id = kj::heapString("ORDER-001");
    request1.symbol = veloz::common::SymbolId("BTCUSDT");
    request1.side = OrderSide::Buy;
    request1.type = OrderType::Limit;
    request1.qty = 1.0;
    request1.price = 50000.0;
    wal.log_order_new(request1);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 1.0, 50000.0, 1000);
    wal.log_order_update("ORDER-001"_kj, "VENUE-001"_kj, "FILLED"_kj, ""_kj, 1000);

    // Order 2: Partially filled
    PlaceOrderRequest request2;
    request2.client_order_id = kj::heapString("ORDER-002");
    request2.symbol = veloz::common::SymbolId("ETHUSDT");
    request2.side = OrderSide::Sell;
    request2.type = OrderType::Limit;
    request2.qty = 10.0;
    request2.price = 3000.0;
    wal.log_order_new(request2);
    wal.log_order_fill("ORDER-002"_kj, "ETHUSDT"_kj, 5.0, 3000.0, 2000);
    wal.log_order_update("ORDER-002"_kj, "VENUE-002"_kj, "PARTIALLY_FILLED"_kj, ""_kj, 2000);

    // Order 3: New (pending)
    PlaceOrderRequest request3;
    request3.client_order_id = kj::heapString("ORDER-003");
    request3.symbol = veloz::common::SymbolId("SOLUSDT");
    request3.side = OrderSide::Buy;
    request3.type = OrderType::Limit;
    request3.qty = 100.0;
    request3.price = 100.0;
    wal.log_order_new(request3);
    wal.log_order_update("ORDER-003"_kj, "VENUE-003"_kj, "NEW"_kj, ""_kj, 3000);

    wal.sync();
  }

  // Phase 2: Recover and verify all orders
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    KJ_EXPECT(store.count() == 3);

    // Verify Order 1
    auto maybeOrder1 = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order1, maybeOrder1) {
      KJ_EXPECT(order1.status == "FILLED"_kj);
      KJ_EXPECT(order1.executed_qty == 1.0);
    }
    else {
      KJ_FAIL_EXPECT("ORDER-001 not found");
    }

    // Verify Order 2
    auto maybeOrder2 = store.get("ORDER-002"_kj);
    KJ_IF_SOME(order2, maybeOrder2) {
      KJ_EXPECT(order2.status == "PARTIALLY_FILLED"_kj);
      KJ_EXPECT(order2.executed_qty == 5.0);
    }
    else {
      KJ_FAIL_EXPECT("ORDER-002 not found");
    }

    // Verify Order 3
    auto maybeOrder3 = store.get("ORDER-003"_kj);
    KJ_IF_SOME(order3, maybeOrder3) {
      KJ_EXPECT(order3.status == "NEW"_kj);
      KJ_EXPECT(order3.executed_qty == 0.0);
    }
    else {
      KJ_FAIL_EXPECT("ORDER-003 not found");
    }

    // Verify pending/terminal counts
    KJ_EXPECT(store.count_pending() == 2);  // ORDER-002 (partial) and ORDER-003 (new)
    KJ_EXPECT(store.count_terminal() == 1); // ORDER-001 (filled)
  }

  // Clean up
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

} // namespace
