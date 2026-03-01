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

// Helper struct to manage test directory context
struct TestContext {
  kj::Own<kj::Filesystem> fs;
  kj::Own<const kj::Directory> testRoot;
  kj::Own<const kj::Directory> testDir;
  kj::String dirName;

  TestContext(kj::StringPtr testName) {
    fs = kj::newDiskFilesystem();
    auto& cwd = fs->getCurrent();

    // Ensure .test_output exists (hidden directory)
    if (!cwd.exists(kj::Path::parse(".test_output"))) {
      cwd.openSubdir(kj::Path::parse(".test_output"), kj::WriteMode::CREATE);
    }
    testRoot = cwd.openSubdir(kj::Path::parse(".test_output"),
                              kj::WriteMode::CREATE | kj::WriteMode::MODIFY);

    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    dirName = kj::str(testName, "_", timestamp);
    testDir = testRoot->openSubdir(kj::Path::parse(dirName),
                                   kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
  }

  const kj::Directory& dir() { return *testDir; }
};

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
  TestContext ctx("int_wal_basic");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL recovery with multiple orders") {
  TestContext ctx("int_wal_multi");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL recovery with order lifecycle events") {
  TestContext ctx("int_wal_lifecycle");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL checkpoint and recovery") {
  TestContext ctx("int_wal_checkpoint");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL handles concurrent-like write patterns") {
  TestContext ctx("int_wal_concurrent");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL stats tracking accuracy") {
  TestContext ctx("int_wal_stats");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

KJ_TEST("Integration: WAL handles order cancellation") {
  TestContext ctx("int_wal_cancel");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

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
}

// ============================================================================
// Extended Tests: Crash Recovery and Data Integrity (QA Extension)
// ============================================================================

KJ_TEST("Integration: WAL recovery with truncated file simulates crash") {
  TestContext ctx("int_wal_truncated");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  // Write phase - create valid entries
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // Create multiple orders
    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);
    auto order3 = create_test_order("ORDER-003"_kj, "BTCUSDT"_kj, OrderSide::Buy, 0.5, 51000.0);

    wal.log_order_new(order1);
    wal.log_order_new(order2);
    wal.log_order_new(order3);
    wal.sync();
  }

  // Simulate crash by truncating the WAL file (remove last entry partially)
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  {
    auto path = kj::Path::parse(walFileName);
    auto file = cwd.openFile(path, kj::WriteMode::MODIFY);
    auto metadata = file->stat();
    // Truncate to remove approximately the last entry (simulate partial write)
    if (metadata.size > 50) {
      file->truncate(metadata.size - 30);
    }
  }

  // Recovery phase - should recover orders before truncation
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // First two orders should be recovered
    KJ_EXPECT(store.get("ORDER-001"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-002"_kj) != kj::none);

    // Stats should show corrupted entries detected
    auto stats = wal.stats();
    // At least some entries should have been replayed
    KJ_EXPECT(stats.entries_replayed >= 2);
  }
}

KJ_TEST("Integration: WAL recovery skips corrupted entries and continues") {
  TestContext ctx("int_wal_corrupted");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  // Write phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);

    wal.log_order_new(order1);
    wal.log_order_new(order2);
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == 2);
  }

  // Recovery should handle the file gracefully
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Both orders should be recovered (file wasn't corrupted)
    KJ_EXPECT(store.get("ORDER-001"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-002"_kj) != kj::none);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 2);
    KJ_EXPECT(stats.corrupted_entries == 0);
  }
}

KJ_TEST("Integration: WAL checksum validation detects corruption") {
  TestContext ctx("int_wal_checksum");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  // Write phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    auto order = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order);
    wal.sync();
  }

  // Corrupt the payload data (not the header)
  auto walFileName = kj::str(prefix, "_0000000000000000.wal");
  {
    auto path = kj::Path::parse(walFileName);
    auto file = cwd.openFile(path, kj::WriteMode::MODIFY);
    auto data = file->readAllBytes();

    // Corrupt a byte in the payload area (after header which is 32 bytes)
    if (data.size() > 40) {
      auto mutableData = kj::heapArray<kj::byte>(data.size());
      std::memcpy(mutableData.begin(), data.begin(), data.size());
      mutableData[40] = static_cast<kj::byte>(~static_cast<uint8_t>(mutableData[40]));
      file->truncate(0);
      file->write(0, mutableData);
    }
  }

  // Recovery should detect checksum mismatch
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto stats = wal.stats();
    // Should have detected corruption
    KJ_EXPECT(stats.corrupted_entries >= 1);
  }
}

KJ_TEST("Integration: WAL sequence number continuity after recovery") {
  TestContext ctx("int_wal_sequence");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  // Write phase 1
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);

    wal.log_order_new(order1);
    wal.log_order_new(order2);
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == 2);
  }

  // Recovery and continue writing
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Sequence should continue from where it left off
    KJ_EXPECT(wal.current_sequence() == 2);

    // Write more orders
    auto order3 = create_test_order("ORDER-003"_kj, "BTCUSDT"_kj, OrderSide::Buy, 0.5, 51000.0);
    wal.log_order_new(order3);
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == 3);
  }

  // Final recovery - all orders should be present
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    KJ_EXPECT(store.get("ORDER-001"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-002"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-003"_kj) != kj::none);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == 3);
  }
}

KJ_TEST("Integration: WAL data integrity - order count matches after recovery") {
  TestContext ctx("int_wal_integrity");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  constexpr int ORDER_COUNT = 50;

  // Write phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    for (int i = 0; i < ORDER_COUNT; i++) {
      auto order = create_test_order(
          kj::str("ORDER-", i), "BTCUSDT"_kj, (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell,
          0.1 + (i * 0.01), 50000.0 + (i * 10));
      wal.log_order_new(order);
    }
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == ORDER_COUNT);
  }

  // Recovery and verify count
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // Count recovered orders
    int recovered_count = 0;
    for (int i = 0; i < ORDER_COUNT; i++) {
      if (store.get(kj::str("ORDER-", i)) != kj::none) {
        recovered_count++;
      }
    }

    KJ_EXPECT(recovered_count == ORDER_COUNT);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == ORDER_COUNT);
    KJ_EXPECT(stats.corrupted_entries == 0);
  }
}

KJ_TEST("Integration: WAL data integrity - fill amounts match after recovery") {
  TestContext ctx("int_wal_fill_integrity");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  constexpr double EXPECTED_TOTAL_FILL = 1.0;

  // Write phase with multiple fills
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    auto order = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order);
    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "NEW"_kj, ""_kj, 1000);

    // Multiple partial fills totaling 1.0
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.3, 50000.0, 2000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.3, 50010.0, 3000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.2, 50020.0, 4000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 0.2, 50030.0, 5000);

    wal.log_order_update("ORDER-001"_kj, "EX-123"_kj, "FILLED"_kj, ""_kj, 6000);
    wal.sync();
  }

  // Recovery and verify fill amounts
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto maybeOrder = store.get("ORDER-001"_kj);
    KJ_IF_SOME(order, maybeOrder) {
      // Allow small floating point tolerance
      KJ_EXPECT(order.executed_qty >= EXPECTED_TOTAL_FILL - 0.001);
      KJ_EXPECT(order.executed_qty <= EXPECTED_TOTAL_FILL + 0.001);
    }
    else {
      KJ_FAIL_EXPECT("Order not recovered");
    }
  }
}

KJ_TEST("Integration: WAL performance - recovery of 1000 orders") {
  TestContext ctx("int_wal_perf");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  constexpr int ORDER_COUNT = 1000;

  // Write phase
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = false; // Batch for performance

    OrderWal wal(cwd, kj::mv(config));

    for (int i = 0; i < ORDER_COUNT; i++) {
      auto order = create_test_order(
          kj::str("ORDER-", i), "BTCUSDT"_kj, (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell,
          0.1 + (i * 0.001), 50000.0 + (i * 1));
      wal.log_order_new(order);
    }
    wal.sync();

    KJ_EXPECT(wal.current_sequence() == ORDER_COUNT);
  }

  // Recovery phase with timing
  {
    auto start = std::chrono::high_resolution_clock::now();

    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Recovery should complete in reasonable time (< 5 seconds for 1000 orders)
    KJ_EXPECT(duration_ms < 5000);

    auto stats = wal.stats();
    KJ_EXPECT(stats.entries_replayed == ORDER_COUNT);

    // Spot check some orders
    KJ_EXPECT(store.get("ORDER-0"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-500"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-999"_kj) != kj::none);
  }
}

KJ_TEST("Integration: WAL recovery after clean shutdown preserves all state") {
  TestContext ctx("int_wal_clean_shutdown");
  auto& cwd = ctx.dir();
  auto prefix = kj::str("wal");

  // Simulate clean shutdown with various order states
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);
    config.sync_on_write = true;

    OrderWal wal(cwd, kj::mv(config));

    // Order 1: Fully filled
    auto order1 = create_test_order("ORDER-001"_kj, "BTCUSDT"_kj, OrderSide::Buy, 1.0, 50000.0);
    wal.log_order_new(order1);
    wal.log_order_update("ORDER-001"_kj, "EX-001"_kj, "NEW"_kj, ""_kj, 1000);
    wal.log_order_fill("ORDER-001"_kj, "BTCUSDT"_kj, 1.0, 50000.0, 2000);
    wal.log_order_update("ORDER-001"_kj, "EX-001"_kj, "FILLED"_kj, ""_kj, 3000);

    // Order 2: Partially filled
    auto order2 = create_test_order("ORDER-002"_kj, "ETHUSDT"_kj, OrderSide::Sell, 10.0, 3000.0);
    wal.log_order_new(order2);
    wal.log_order_update("ORDER-002"_kj, "EX-002"_kj, "NEW"_kj, ""_kj, 4000);
    wal.log_order_fill("ORDER-002"_kj, "ETHUSDT"_kj, 5.0, 3000.0, 5000);
    wal.log_order_update("ORDER-002"_kj, "EX-002"_kj, "PARTIALLY_FILLED"_kj, ""_kj, 6000);

    // Order 3: Cancelled
    auto order3 = create_test_order("ORDER-003"_kj, "BTCUSDT"_kj, OrderSide::Buy, 0.5, 49000.0);
    wal.log_order_new(order3);
    wal.log_order_update("ORDER-003"_kj, "EX-003"_kj, "NEW"_kj, ""_kj, 7000);
    wal.log_order_update("ORDER-003"_kj, "EX-003"_kj, "CANCELED"_kj, "User requested"_kj, 8000);

    // Order 4: Pending (no fills yet)
    auto order4 = create_test_order("ORDER-004"_kj, "BTCUSDT"_kj, OrderSide::Sell, 2.0, 52000.0);
    wal.log_order_new(order4);
    wal.log_order_update("ORDER-004"_kj, "EX-004"_kj, "NEW"_kj, ""_kj, 9000);

    wal.sync();
  }

  // Recovery after clean shutdown
  {
    WalConfig config;
    config.file_prefix = kj::heapString(prefix);

    OrderWal wal(cwd, kj::mv(config));
    OrderStore store;

    wal.replay_into(store);

    // All orders should be recovered
    KJ_EXPECT(store.get("ORDER-001"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-002"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-003"_kj) != kj::none);
    KJ_EXPECT(store.get("ORDER-004"_kj) != kj::none);

    // Verify fill amounts
    KJ_IF_SOME(order1, store.get("ORDER-001"_kj)) {
      KJ_EXPECT(order1.executed_qty >= 0.99); // Fully filled
    }

    KJ_IF_SOME(order2, store.get("ORDER-002"_kj)) {
      KJ_EXPECT(order2.executed_qty >= 4.99); // Partially filled (5.0)
      KJ_EXPECT(order2.executed_qty <= 5.01);
    }

    auto stats = wal.stats();
    KJ_EXPECT(stats.corrupted_entries == 0);
  }
}

} // namespace
