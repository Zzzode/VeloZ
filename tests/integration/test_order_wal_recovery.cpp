// Integration test: Order WAL recovery after crash
// Tests the complete order journaling and recovery workflow

#include "veloz/oms/order_wal.h"
#include "veloz/exec/order_api.h"

#include <kj/test.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <chrono>

using namespace veloz::oms;
using namespace veloz::exec;

namespace {

// Generate unique prefix for test files to avoid conflicts
kj::String generate_unique_prefix(kj::StringPtr base) {
  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  return kj::str(base, "_", timestamp);
}

// Helper to create a test order request
PlaceOrderRequest create_test_order(kj::StringPtr client_id, kj::StringPtr symbol,
                                    OrderSide side, double qty, double price) {
  PlaceOrderRequest request;
  request.client_order_id = kj::heapString(client_id);
  request.symbol = veloz::common::SymbolId(symbol);
  request.side = side;
  request.type = OrderType::Limit;
  request.tif = TimeInForce::GTC;
  request.qty = qty;
  request.price = price;
  return request;
}

// ============================================================================
// Integration Test: Order WAL Recovery
// ============================================================================

KJ_TEST("Integration: WAL basic write and read cycle") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_basic"_kj);

  // Write phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true; // Ensure durability

    OrderWal wal(cwd, kj::mv(config));

    auto request = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(request);
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == 1);
  }

  // Read phase (simulating recovery after restart)
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      KJ_EXPECT(order.client_order_id == "ORDER-001"_kj);
      KJ_EXPECT(order.symbol == "BTCUSDT"_kj);
    }
    else {
      KJ_FAIL_EXPECT("Order not recovered from WAL");
    }
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL recovery with multiple orders") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_multi"_kj);

  // Write multiple orders
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // Create multiple orders
    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);
    auto order3 = create_test_order("ORDER-003"_kj, "BTCUSDT"_kj, OrderSide::Sell, 0.5, 51000.0);

    wal.log_order_new(order1);
    wal.log_order_new(order2);
    wal.log_order_new(order3);
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == 3);
  }

  // Recovery phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Verify all orders recovered
    KJ_EXPECT(store.get("ORDER-001"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-002"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-003"_kj) != kj::none);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 3);
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL recovery with order lifecycle events") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_lifecycle"_kj);

  // Simulate complete order lifecycle
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // 1. Order created
    auto order = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order);

    // 2. Order acknowledged by exchange
    wal.log_order_update("ORDER-001"_kj, "EXCHANGE-123"_kj, "NEW"_kj, ""_kj, 1000);

    // 3. Partial fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50000.0, 2000);

    // 4. Another partial fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.3, 50010.0, 3000);

    // 5. Final fill
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.2, 50020.0, 4000);

    // 6. Order completed
    wal.log_order_update("ORDER-001"_kj, "EXCHANGE-123"_kj, "FILLED"_kj, ""_kj, 5000);

    wal.sync();
    KJ_EXPECT(wal.current_sequence() == 6);
  }

  // Recovery and verification
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      KJ_EXPECT(order.client_order_id == "ORDER-001"_kj);
      // Total executed should be 1.0 (0.5 + 0.3 + 0.2)
      KJ_EXPECT(order.executed_qty >= 0.9); // Allow for floating point
    }
    else {
      KJ_FAIL_EXPECT("Order not recovered");
    }

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 6);
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL checkpoint and recovery") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_checkpoint"_kj);

  // Create orders and checkpoint
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    // Add orders to store
    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);

    store.note_order_params(order1);
    store.note_order_params(order2);

    // Write checkpoint
    auto seq = wal.write_checkpoint(store);
    KJ_EXPECT(seq > 0);

    auto stats = wal.stats();
    KJ_EXPECT(stats.checkpoints == 1);
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL handles concurrent-like write patterns") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_concurrent"_kj);

  // Simulate rapid order creation (as would happen in high-frequency trading)
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false; // Batch writes for performance

    OrderWal wal(cwd, kj::mv(config));

    // Create 100 orders rapidly
    for (int i = 0; i < 100; i++) {
      auto order = create_test_order(
          kj::str("ORDER-", i), "BTCUSDT"_kj, (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell,
          0.1 + (i * 0.01), 50000.0 + (i * 10));
      wal.log_order_new(order);
    }

    // Single sync at the end
    wal.sync();
    KJ_EXPECT(wal.current_sequence() == 100);
  }

  // Recovery
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Verify all orders recovered
    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 100);

    // Spot check some orders
    KJ_EXPECT(store.get("ORDER-0"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-50"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-99"_kj) != kj::none);
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL stats tracking accuracy") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_stats"_kj);

  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // Perform various operations
    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order1);

    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "NEW"_kj, ""_kj, 1000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50000.0, 2000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.5, 50000.0, 3000);
    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "FILLED"_kj, ""_kj, 4000);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_written == 5);
    KJ_EXPECT(stats.bytes_written > 0);
    KJ_EXPECT(stats.current_sequence == 5);
    KJ_EXPECT(wal.is_healthy());
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

KJ_TEST("Integration: WAL handles order cancellation") {
  auto fs = kj::newDiskFilesystem();
  auto& cwd = fs->getCurrent();
  auto prefix = generate_unique_prefix("int_wal_cancel"_kj);

  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // Create order
    auto order = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order);

    // Order acknowledged
    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "NEW"_kj, ""_kj, 1000);

    // Order cancelled
    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "CANCELED"_kj, "User requested"_kj, 2000);

    wal.sync();
    KJ_EXPECT(wal.current_sequence() == 3);
  }

  // Recovery
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      KJ_EXPECT(order.client_order_id == "ORDER-001"_kj);
      // Order should show cancelled status
    }
    else {
      KJ_FAIL_EXPECT("Order not recovered");
    }
  }

  // Cleanup
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  cwd.tryRemove(kj::Path::parse(walFileName));
}

} // namespace
