#pragma once

#include "veloz/oms/order_record.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/filesystem.h>
#include <kj/function.h>
#include <kj/io.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::oms {

// WAL entry types for order journaling
enum class WalEntryType : uint8_t {
  OrderNew = 1,    // New order placed
  OrderUpdate = 2, // Order status update
  OrderFill = 3,   // Order fill/execution
  OrderCancel = 4, // Order cancellation
  Checkpoint = 5,  // Full state checkpoint
  Rotation = 6     // WAL rotation marker
};

// WAL entry header structure (fixed size for easy parsing)
struct WalEntryHeader {
  uint32_t magic;        // Magic number for validation (0x57414C45 = "WALE")
  uint32_t version;      // WAL format version
  uint64_t sequence;     // Monotonic sequence number
  uint64_t timestamp_ns; // Timestamp in nanoseconds
  WalEntryType type;     // Entry type
  uint8_t reserved[3];   // Reserved for alignment
  uint32_t payload_size; // Size of payload following header
  uint32_t checksum;     // CRC32 checksum of header + payload

  static constexpr uint32_t MAGIC = 0x57414C45; // "WALE"
  static constexpr uint32_t VERSION = 1;
};

// WAL configuration
struct WalConfig {
  kj::String directory;         // Directory for WAL files
  kj::String file_prefix;       // Prefix for WAL files (e.g., "orders")
  uint64_t max_file_size;       // Max size before rotation (default: 64MB)
  uint32_t max_files;           // Max number of WAL files to keep (default: 10)
  bool sync_on_write;           // fsync after each write (default: true)
  uint64_t checkpoint_interval; // Entries between checkpoints (default: 1000)

  WalConfig()
      : directory(kj::heapString(".")), file_prefix(kj::heapString("orders")),
        max_file_size(64 * 1024 * 1024), max_files(10), sync_on_write(true),
        checkpoint_interval(1000) {}
};

// WAL statistics
struct WalStats {
  uint64_t entries_written{0};
  uint64_t entries_replayed{0};
  uint64_t bytes_written{0};
  uint64_t bytes_replayed{0};
  uint64_t rotations{0};
  uint64_t checkpoints{0};
  uint64_t corrupted_entries{0};
  uint64_t current_sequence{0};
};

// Callback for replaying WAL entries
using WalReplayCallback =
    kj::Function<void(WalEntryType type, kj::ArrayPtr<const kj::byte> payload)>;

// Order Write-Ahead Log for durability and recovery
class OrderWal final {
public:
  // Constructor with configuration
  explicit OrderWal(const kj::Directory& directory, WalConfig config = WalConfig());
  ~OrderWal();

  KJ_DISALLOW_COPY_AND_MOVE(OrderWal);

  // Write operations - all return sequence number
  uint64_t log_order_new(const veloz::exec::PlaceOrderRequest& request);
  uint64_t log_order_update(kj::StringPtr client_order_id, kj::StringPtr venue_order_id,
                            kj::StringPtr status, kj::StringPtr reason, int64_t ts_ns);
  uint64_t log_order_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty,
                          double price, int64_t ts_ns);
  uint64_t log_order_cancel(kj::StringPtr client_order_id, kj::StringPtr reason, int64_t ts_ns);

  // Checkpoint - write full state for faster recovery
  uint64_t write_checkpoint(const OrderStore& store);

  // Replay WAL entries on startup
  void replay(WalReplayCallback callback);

  // Replay directly into an OrderStore
  void replay_into(OrderStore& store);

  // Force sync to disk
  void sync();

  // Rotate WAL file
  void rotate();

  // Get current statistics
  [[nodiscard]] WalStats stats() const;

  // Get current sequence number
  [[nodiscard]] uint64_t current_sequence() const;

  // Check if WAL is healthy
  [[nodiscard]] bool is_healthy() const;

  // Cleanup old WAL files
  void cleanup_old_files();

private:
  // Internal write method
  uint64_t write_entry(WalEntryType type, kj::ArrayPtr<const kj::byte> payload);

  // Open or create current WAL file
  void open_current_file();

  // Close current WAL file
  void close_current_file();

  // Check if rotation is needed
  [[nodiscard]] bool needs_rotation() const;

  // Generate WAL filename
  [[nodiscard]] kj::String generate_filename(uint64_t sequence) const;

  // Parse WAL filename to extract sequence
  [[nodiscard]] kj::Maybe<uint64_t> parse_filename(kj::StringPtr filename) const;

  // List all WAL files in order
  [[nodiscard]] kj::Vector<kj::String> list_wal_files() const;

  // Calculate CRC32 checksum
  [[nodiscard]] uint32_t calculate_checksum(kj::ArrayPtr<const kj::byte> data) const;

  // Serialize order request to bytes
  [[nodiscard]] kj::Array<kj::byte>
  serialize_order_request(const veloz::exec::PlaceOrderRequest& request) const;

  // Serialize order update to bytes
  [[nodiscard]] kj::Array<kj::byte>
  serialize_order_update(kj::StringPtr client_order_id, kj::StringPtr venue_order_id,
                         kj::StringPtr status, kj::StringPtr reason, int64_t ts_ns) const;

  // Serialize order fill to bytes
  [[nodiscard]] kj::Array<kj::byte> serialize_order_fill(kj::StringPtr client_order_id,
                                                         kj::StringPtr symbol, double qty,
                                                         double price, int64_t ts_ns) const;

  // Serialize order cancel to bytes
  [[nodiscard]] kj::Array<kj::byte>
  serialize_order_cancel(kj::StringPtr client_order_id, kj::StringPtr reason, int64_t ts_ns) const;

  // Serialize checkpoint to bytes
  [[nodiscard]] kj::Array<kj::byte> serialize_checkpoint(const OrderStore& store) const;

  // Deserialize and apply to OrderStore
  void deserialize_order_new(kj::ArrayPtr<const kj::byte> payload, OrderStore& store) const;
  void deserialize_order_update(kj::ArrayPtr<const kj::byte> payload, OrderStore& store) const;
  void deserialize_order_fill(kj::ArrayPtr<const kj::byte> payload, OrderStore& store) const;
  void deserialize_checkpoint(kj::ArrayPtr<const kj::byte> payload, OrderStore& store) const;

  // Configuration
  WalConfig config_;

  // Directory reference
  const kj::Directory& directory_;

  // Current WAL file
  kj::Maybe<kj::Own<const kj::File>> current_file_;
  uint64_t current_file_size_{0};
  uint64_t current_file_start_sequence_{0};

  // State protected by mutex
  struct State {
    uint64_t sequence{0};
    uint64_t entries_since_checkpoint{0};
    WalStats stats;
    bool healthy{true};
  };
  kj::MutexGuarded<State> state_;
};

} // namespace veloz::oms
