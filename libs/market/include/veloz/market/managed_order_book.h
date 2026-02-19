#pragma once

#include "veloz/market/order_book.h"

#include <atomic>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/timer.h>
#include <kj/vector.h>

namespace veloz::market {

/**
 * @brief State of the managed order book synchronization
 */
enum class SyncState {
  Disconnected,     // Not connected to exchange
  Buffering,        // Connected, buffering deltas before snapshot
  FetchingSnapshot, // Fetching REST snapshot
  Synchronizing,    // Applying buffered deltas to snapshot
  Synchronized,     // Fully synchronized
  Resynchronizing   // Lost sync, re-fetching snapshot
};

/**
 * @brief Statistics for managed order book
 */
struct ManagedOrderBookStats {
  int64_t snapshot_count{0};      // Number of snapshots fetched
  int64_t delta_count{0};         // Number of deltas processed
  int64_t dropped_delta_count{0}; // Deltas dropped (before snapshot or stale)
  int64_t resync_count{0};        // Number of resynchronizations
  int64_t gap_count{0};           // Sequence gaps detected
  int64_t last_sync_time_ns{0};   // Last successful sync timestamp
};

/**
 * @brief Callback for snapshot requests (to be implemented by REST client)
 *
 * The callback should fetch a depth snapshot from the exchange REST API
 * and return it as BookData with is_snapshot=true.
 *
 * @param symbol The symbol to fetch snapshot for
 * @return Promise resolving to BookData snapshot
 */
using SnapshotFetcher = kj::Function<kj::Promise<BookData>(kj::StringPtr symbol)>;

/**
 * @brief Callback for order book updates
 */
using OrderBookUpdateCallback = kj::Function<void(const OrderBook&)>;

/**
 * @brief Managed order book with automatic synchronization
 *
 * Implements the Binance depth stream synchronization protocol:
 * 1. Open WebSocket connection and start buffering depth events
 * 2. Fetch REST depth snapshot
 * 3. Drop buffered events where u <= lastUpdateId from snapshot
 * 4. First processed event should have U <= lastUpdateId+1 AND u >= lastUpdateId+1
 * 5. Continue processing events, each new event's U should equal previous u+1
 *
 * This class manages the full lifecycle of order book synchronization,
 * including reconnection handling and automatic resync on sequence gaps.
 */
class ManagedOrderBook final {
public:
  /**
   * @brief Construct a managed order book
   * @param symbol Trading symbol (e.g., "BTCUSDT")
   * @param timer KJ timer for scheduling
   */
  explicit ManagedOrderBook(kj::StringPtr symbol, kj::Timer& timer);
  ~ManagedOrderBook();

  KJ_DISALLOW_COPY_AND_MOVE(ManagedOrderBook);

  // Configuration
  void set_snapshot_fetcher(SnapshotFetcher fetcher);
  void set_update_callback(OrderBookUpdateCallback callback);
  void set_max_buffer_size(size_t size);
  void set_max_depth_levels(size_t levels);
  void set_snapshot_timeout_ms(int64_t timeout_ms);

  // Lifecycle management
  kj::Promise<void> start();
  void stop();

  // Process incoming delta from WebSocket
  // Call this for each depth update received
  void on_delta(const BookData& delta);

  // Force resynchronization
  void request_resync();

  // Query methods
  [[nodiscard]] const OrderBook& order_book() const;
  [[nodiscard]] SyncState state() const;
  [[nodiscard]] bool is_synchronized() const;
  [[nodiscard]] kj::StringPtr symbol() const;
  [[nodiscard]] const ManagedOrderBookStats& stats() const;

private:
  // Internal state machine
  kj::Promise<void> fetch_and_sync_snapshot();
  void process_buffered_deltas();
  void apply_delta_internal(const BookData& delta);
  void transition_to(SyncState new_state);
  void notify_update();

  // Symbol
  kj::String symbol_;

  // Timer for scheduling
  kj::Timer& timer_;

  // Underlying order book
  OrderBook book_;

  // Synchronization state
  std::atomic<SyncState> state_{SyncState::Disconnected};

  // Buffer for deltas received before/during snapshot fetch
  struct BufferState {
    kj::Vector<BookData> buffer;
    int64_t snapshot_last_update_id{0};
    bool first_delta_processed{false};
  };
  kj::MutexGuarded<BufferState> buffer_state_;

  // Configuration
  size_t max_buffer_size_{10000};
  size_t max_depth_levels_{100};
  int64_t snapshot_timeout_ms_{5000};

  // Callbacks
  kj::Maybe<SnapshotFetcher> snapshot_fetcher_;
  kj::Maybe<OrderBookUpdateCallback> update_callback_;

  // Statistics
  ManagedOrderBookStats stats_;

  // Running flag
  std::atomic<bool> running_{false};
};

} // namespace veloz::market
