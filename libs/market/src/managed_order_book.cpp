#include "veloz/market/managed_order_book.h"

#include <chrono>
#include <kj/debug.h>

namespace veloz::market {

ManagedOrderBook::ManagedOrderBook(kj::StringPtr symbol, kj::Timer& timer)
    : symbol_(kj::heapString(symbol)), timer_(timer) {
  book_.set_max_depth_levels(max_depth_levels_);
}

ManagedOrderBook::~ManagedOrderBook() {
  stop();
}

void ManagedOrderBook::set_snapshot_fetcher(SnapshotFetcher fetcher) {
  snapshot_fetcher_ = kj::mv(fetcher);
}

void ManagedOrderBook::set_update_callback(OrderBookUpdateCallback callback) {
  update_callback_ = kj::mv(callback);
}

void ManagedOrderBook::set_max_buffer_size(size_t size) {
  max_buffer_size_ = size;
}

void ManagedOrderBook::set_max_depth_levels(size_t levels) {
  max_depth_levels_ = levels;
  book_.set_max_depth_levels(levels);
}

void ManagedOrderBook::set_snapshot_timeout_ms(int64_t timeout_ms) {
  snapshot_timeout_ms_ = timeout_ms;
}

kj::Promise<void> ManagedOrderBook::start() {
  if (running_.exchange(true)) {
    return kj::READY_NOW; // Already running
  }

  KJ_LOG(INFO, "Starting ManagedOrderBook", symbol_);

  // Transition to buffering state and start collecting deltas
  transition_to(SyncState::Buffering);

  // Start snapshot fetch after a short delay to collect some deltas first
  return timer_.afterDelay(100 * kj::MILLISECONDS).then([this]() -> kj::Promise<void> {
    if (!running_) {
      return kj::READY_NOW;
    }
    return fetch_and_sync_snapshot();
  });
}

void ManagedOrderBook::stop() {
  running_ = false;
  transition_to(SyncState::Disconnected);

  // Clear buffer
  auto lock = buffer_state_.lockExclusive();
  lock->buffer.clear();
  lock->snapshot_last_update_id = 0;
  lock->first_delta_processed = false;
}

void ManagedOrderBook::on_delta(const BookData& delta) {
  if (!running_) {
    return;
  }

  stats_.delta_count++;

  SyncState current_state = state_.load();

  switch (current_state) {
  case SyncState::Disconnected:
    // Ignore deltas when disconnected
    stats_.dropped_delta_count++;
    break;

  case SyncState::Buffering:
  case SyncState::FetchingSnapshot:
  case SyncState::Synchronizing:
  case SyncState::Resynchronizing: {
    // Buffer the delta for later processing
    auto lock = buffer_state_.lockExclusive();
    if (lock->buffer.size() < max_buffer_size_) {
      BookData copy;
      copy.sequence = delta.sequence;
      copy.first_update_id = delta.first_update_id;
      copy.is_snapshot = delta.is_snapshot;
      for (const auto& bid : delta.bids) {
        copy.bids.add(bid);
      }
      for (const auto& ask : delta.asks) {
        copy.asks.add(ask);
      }
      lock->buffer.add(kj::mv(copy));
    } else {
      KJ_LOG(WARNING, "Delta buffer overflow, dropping delta", symbol_, delta.sequence);
      stats_.dropped_delta_count++;
    }
    break;
  }

  case SyncState::Synchronized:
    // Apply delta directly
    apply_delta_internal(delta);
    break;
  }
}

void ManagedOrderBook::request_resync() {
  if (!running_) {
    return;
  }

  KJ_LOG(INFO, "Resync requested", symbol_);
  stats_.resync_count++;
  transition_to(SyncState::Resynchronizing);

  // Clear buffer and book
  {
    auto lock = buffer_state_.lockExclusive();
    lock->buffer.clear();
    lock->snapshot_last_update_id = 0;
    lock->first_delta_processed = false;
  }
  book_.clear();

  // Transition to buffering to collect new deltas
  transition_to(SyncState::Buffering);
}

const OrderBook& ManagedOrderBook::order_book() const {
  return book_;
}

SyncState ManagedOrderBook::state() const {
  return state_.load();
}

bool ManagedOrderBook::is_synchronized() const {
  return state_.load() == SyncState::Synchronized;
}

kj::StringPtr ManagedOrderBook::symbol() const {
  return symbol_;
}

const ManagedOrderBookStats& ManagedOrderBook::stats() const {
  return stats_;
}

kj::Promise<void> ManagedOrderBook::fetch_and_sync_snapshot() {
  KJ_IF_SOME(fetcher, snapshot_fetcher_) {
    transition_to(SyncState::FetchingSnapshot);

    KJ_LOG(INFO, "Fetching snapshot", symbol_);

    return fetcher(symbol_).then(
        [this](BookData snapshot) -> void {
          if (!running_) {
            return;
          }

          KJ_LOG(INFO, "Snapshot received", symbol_, snapshot.sequence, snapshot.bids.size(),
                 snapshot.asks.size());

          stats_.snapshot_count++;

          // Store snapshot's lastUpdateId
          {
            auto lock = buffer_state_.lockExclusive();
            lock->snapshot_last_update_id = snapshot.sequence;
            lock->first_delta_processed = false;
          }

          // Apply snapshot to order book
          book_.apply_snapshot(snapshot.bids, snapshot.asks, snapshot.sequence);

          // Process buffered deltas
          transition_to(SyncState::Synchronizing);
          process_buffered_deltas();

          // If we successfully processed deltas, we're synchronized
          if (running_) {
            transition_to(SyncState::Synchronized);
            stats_.last_sync_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count();
            notify_update();
          }
        },
        [this](kj::Exception&& e) -> void {
          KJ_LOG(ERROR, "Failed to fetch snapshot", symbol_, e.getDescription());
          if (running_) {
            // Retry after delay
            transition_to(SyncState::Buffering);
          }
        });
  }
  else {
    KJ_LOG(ERROR, "No snapshot fetcher configured", symbol_);
    return kj::READY_NOW;
  }
}

void ManagedOrderBook::process_buffered_deltas() {
  auto lock = buffer_state_.lockExclusive();
  int64_t snapshot_last_id = lock->snapshot_last_update_id;

  KJ_LOG(INFO, "Processing buffered deltas", symbol_, lock->buffer.size(), snapshot_last_id);

  // Sort buffer by first_update_id
  // Simple bubble sort for correctness (buffer should be mostly sorted already)
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i + 1 < lock->buffer.size(); ++i) {
      if (lock->buffer[i].first_update_id > lock->buffer[i + 1].first_update_id) {
        // Swap
        BookData tmp = kj::mv(lock->buffer[i]);
        lock->buffer[i] = kj::mv(lock->buffer[i + 1]);
        lock->buffer[i + 1] = kj::mv(tmp);
        changed = true;
      }
    }
  }

  // Process deltas according to Binance protocol:
  // 1. Drop events where u <= lastUpdateId
  // 2. First processed event must have U <= lastUpdateId+1 AND u >= lastUpdateId+1
  // 3. Each subsequent event's U should equal previous u+1

  kj::Vector<BookData> remaining;
  bool found_first_valid = false;
  int64_t last_processed_u = snapshot_last_id;

  for (size_t i = 0; i < lock->buffer.size(); ++i) {
    const auto& delta = lock->buffer[i];

    // Skip deltas that are completely before the snapshot
    if (delta.sequence <= snapshot_last_id) {
      stats_.dropped_delta_count++;
      continue;
    }

    if (!found_first_valid) {
      // First valid delta must satisfy: U <= lastUpdateId+1 AND u >= lastUpdateId+1
      if (delta.first_update_id <= snapshot_last_id + 1 && delta.sequence >= snapshot_last_id + 1) {
        found_first_valid = true;
        lock->first_delta_processed = true;

        // Apply this delta
        auto result =
            book_.apply_deltas(delta.bids, delta.asks, delta.first_update_id, delta.sequence);
        if (result == UpdateResult::Applied) {
          last_processed_u = delta.sequence;
          KJ_LOG(INFO, "Applied first delta", symbol_, delta.first_update_id, delta.sequence);
        }
      } else {
        // This delta doesn't satisfy the first-delta condition
        // Keep it in case we need to resync
        remaining.add(kj::mv(lock->buffer[i]));
      }
    } else {
      // Subsequent deltas: U should equal previous u+1
      if (delta.first_update_id == last_processed_u + 1) {
        auto result =
            book_.apply_deltas(delta.bids, delta.asks, delta.first_update_id, delta.sequence);
        if (result == UpdateResult::Applied) {
          last_processed_u = delta.sequence;
        }
      } else if (delta.first_update_id > last_processed_u + 1) {
        // Gap detected - keep for later or trigger resync
        stats_.gap_count++;
        remaining.add(kj::mv(lock->buffer[i]));
      } else {
        // Stale delta
        stats_.dropped_delta_count++;
      }
    }
  }

  // Replace buffer with remaining deltas
  lock->buffer = kj::mv(remaining);

  if (!found_first_valid && lock->buffer.size() > 0) {
    KJ_LOG(WARNING, "No valid first delta found", symbol_, "buffered", lock->buffer.size());
  }
}

void ManagedOrderBook::apply_delta_internal(const BookData& delta) {
  auto lock = buffer_state_.lockExclusive();

  // Check sequence continuity
  int64_t expected_first = book_.sequence() + 1;

  if (delta.first_update_id != expected_first) {
    if (delta.first_update_id > expected_first) {
      // Gap detected
      stats_.gap_count++;
      KJ_LOG(WARNING, "Sequence gap detected", symbol_, expected_first, delta.first_update_id);

      // Buffer this delta and trigger resync
      if (lock->buffer.size() < max_buffer_size_) {
        BookData copy;
        copy.sequence = delta.sequence;
        copy.first_update_id = delta.first_update_id;
        copy.is_snapshot = delta.is_snapshot;
        for (const auto& bid : delta.bids) {
          copy.bids.add(bid);
        }
        for (const auto& ask : delta.asks) {
          copy.asks.add(ask);
        }
        lock->buffer.add(kj::mv(copy));
      }

      // Request resync (unlock first to avoid deadlock)
      lock.release();
      request_resync();
      return;
    } else {
      // Stale delta, ignore
      stats_.dropped_delta_count++;
      return;
    }
  }

  // Apply the delta
  auto result = book_.apply_deltas(delta.bids, delta.asks, delta.first_update_id, delta.sequence);
  if (result == UpdateResult::Applied) {
    notify_update();
  }
}

void ManagedOrderBook::transition_to(SyncState new_state) {
  SyncState old_state = state_.exchange(new_state);
  if (old_state != new_state) {
    KJ_LOG(INFO, "State transition", symbol_, static_cast<int>(old_state),
           static_cast<int>(new_state));
  }
}

void ManagedOrderBook::notify_update() {
  KJ_IF_SOME(callback, update_callback_) {
    callback(book_);
  }
}

} // namespace veloz::market
