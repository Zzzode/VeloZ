#include "veloz/market/order_book.h"

#include <kj/debug.h>

namespace veloz::market {

void OrderBook::apply_snapshot(const kj::Vector<BookLevel>& bids, const kj::Vector<BookLevel>& asks,
                               int64_t sequence) {
  bids_.clear();
  asks_.clear();

  // Clear the update buffer since we're resetting with a snapshot
  update_buffer_.clear();

  for (const auto& level : bids) {
    if (level.qty > 0.0) {
      bids_.upsert(DescendingPrice(level.price), level.qty, [](double& existing, double newVal) {
        existing = newVal;
      });
    }
  }

  for (const auto& level : asks) {
    if (level.qty > 0.0) {
      asks_.upsert(level.price, level.qty, [](double& existing, double newVal) {
        existing = newVal;
      });
    }
  }

  sequence_ = sequence;
  expected_sequence_ = sequence + 1;
  state_ = OrderBookState::Synchronized;
  rebuild_cache();

  // Process any buffered updates that come after this snapshot
  process_buffered_updates();
}

UpdateResult OrderBook::apply_delta(const BookLevel& level, bool is_bid, int64_t sequence) {
  // Handle empty state - need snapshot first
  if (state_ == OrderBookState::Empty) {
    // Buffer the update and request snapshot
    if (update_buffer_.size() < max_buffer_size_) {
      update_buffer_.add(BufferedDelta{level, is_bid, sequence});
      trigger_snapshot_request();
      state_ = OrderBookState::Syncing;
      return UpdateResult::Buffered;
    }
    return UpdateResult::BufferOverflow;
  }

  // Handle syncing state - buffer updates until snapshot arrives
  if (state_ == OrderBookState::Syncing) {
    if (update_buffer_.size() < max_buffer_size_) {
      update_buffer_.add(BufferedDelta{level, is_bid, sequence});
      return UpdateResult::Buffered;
    }
    return UpdateResult::BufferOverflow;
  }

  // Check for duplicate/old update
  if (sequence <= sequence_) {
    duplicate_count_++;
    return UpdateResult::Duplicate;
  }

  // Check for sequence gap
  if (sequence > expected_sequence_) {
    int64_t gap = sequence - expected_sequence_;

    // If gap is within acceptable range, buffer the update
    if (gap <= max_sequence_gap_ && update_buffer_.size() < max_buffer_size_) {
      update_buffer_.add(BufferedDelta{level, is_bid, sequence});
      gap_count_++;
      return UpdateResult::GapDetected;
    }

    // Gap too large - need to resync
    gap_count_++;
    state_ = OrderBookState::Syncing;
    trigger_snapshot_request();

    // Buffer this update for after snapshot
    if (update_buffer_.size() < max_buffer_size_) {
      update_buffer_.add(BufferedDelta{level, is_bid, sequence});
      return UpdateResult::Buffered;
    }
    return UpdateResult::BufferOverflow;
  }

  // Apply the update (sequence matches expected)
  apply_level(level, is_bid);
  sequence_ = sequence;
  expected_sequence_ = sequence + 1;
  rebuild_cache();

  // Process any buffered updates that can now be applied
  process_buffered_updates();

  return UpdateResult::Applied;
}

UpdateResult OrderBook::apply_deltas(const kj::Vector<BookLevel>& bids,
                                     const kj::Vector<BookLevel>& asks, int64_t first_sequence,
                                     int64_t final_sequence) {
  // Handle empty state - need snapshot first
  if (state_ == OrderBookState::Empty) {
    state_ = OrderBookState::Syncing;
    trigger_snapshot_request();
    return UpdateResult::GapDetected;
  }

  // Handle syncing state
  if (state_ == OrderBookState::Syncing) {
    return UpdateResult::Buffered;
  }

  // Check for duplicate batch
  if (final_sequence <= sequence_) {
    duplicate_count_++;
    return UpdateResult::Duplicate;
  }

  // Check for sequence gap
  if (first_sequence > expected_sequence_) {
    int64_t gap = first_sequence - expected_sequence_;

    if (gap > max_sequence_gap_) {
      gap_count_++;
      state_ = OrderBookState::Syncing;
      trigger_snapshot_request();
      return UpdateResult::GapDetected;
    }

    gap_count_++;
    return UpdateResult::GapDetected;
  }

  // Apply all levels
  for (const auto& level : bids) {
    apply_level(level, true);
  }
  for (const auto& level : asks) {
    apply_level(level, false);
  }

  sequence_ = final_sequence;
  expected_sequence_ = final_sequence + 1;
  rebuild_cache();

  return UpdateResult::Applied;
}

void OrderBook::apply_level(const BookLevel& level, bool is_bid) {
  if (is_bid) {
    if (level.qty == 0.0) {
      bids_.erase(DescendingPrice(level.price));
    } else {
      bids_.upsert(DescendingPrice(level.price), level.qty, [](double& existing, double newVal) {
        existing = newVal;
      });
    }
  } else {
    if (level.qty == 0.0) {
      asks_.erase(level.price);
    } else {
      asks_.upsert(level.price, level.qty, [](double& existing, double newVal) {
        existing = newVal;
      });
    }
  }
}

void OrderBook::process_buffered_updates() {
  // Sort buffer by sequence (simple bubble sort for small buffers)
  // In production, would use a proper heap structure
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i + 1 < update_buffer_.size(); ++i) {
      if (update_buffer_[i].sequence > update_buffer_[i + 1].sequence) {
        auto tmp = update_buffer_[i];
        update_buffer_[i] = update_buffer_[i + 1];
        update_buffer_[i + 1] = tmp;
        changed = true;
      }
    }
  }

  // Process sorted buffer
  size_t i = 0;
  while (i < update_buffer_.size()) {
    const auto& item = update_buffer_[i];

    // If the buffered update is older than current sequence, skip it
    if (item.sequence <= sequence_) {
      ++i;
      continue;
    }

    // If the buffered update is the next expected sequence, apply it
    if (item.sequence == expected_sequence_) {
      apply_level(item.level, item.is_bid);
      sequence_ = item.sequence;
      expected_sequence_ = item.sequence + 1;
      ++i;
      rebuild_cache();
    } else {
      // Gap still exists, stop processing
      break;
    }
  }

  // Remove processed items
  if (i > 0) {
    kj::Vector<BufferedDelta> remaining;
    for (size_t j = i; j < update_buffer_.size(); ++j) {
      remaining.add(update_buffer_[j]);
    }
    update_buffer_ = kj::mv(remaining);
  }
}

void OrderBook::trigger_snapshot_request() {
  KJ_IF_SOME(callback, snapshot_callback_) {
    callback();
  }
}

const kj::Vector<BookLevel>& OrderBook::bids() const {
  return bids_cache_;
}

const kj::Vector<BookLevel>& OrderBook::asks() const {
  return asks_cache_;
}

kj::Maybe<BookLevel> OrderBook::best_bid() const {
  if (bids_cache_.size() == 0)
    return kj::none;
  return bids_cache_.front();
}

kj::Maybe<BookLevel> OrderBook::best_ask() const {
  if (asks_cache_.size() == 0)
    return kj::none;
  return asks_cache_.front();
}

double OrderBook::spread() const {
  // Use KJ_IF_SOME pattern for proper Maybe handling
  KJ_IF_SOME(bid, best_bid()) {
    KJ_IF_SOME(ask, best_ask()) {
      return ask.price - bid.price;
    }
  }
  return 0.0;
}

double OrderBook::mid_price() const {
  // Use KJ_IF_SOME pattern for proper Maybe handling
  KJ_IF_SOME(bid, best_bid()) {
    KJ_IF_SOME(ask, best_ask()) {
      return (bid.price + ask.price) / 2.0;
    }
  }
  return 0.0;
}

int64_t OrderBook::sequence() const {
  return sequence_;
}

kj::Vector<BookLevel> OrderBook::top_bids(size_t n) const {
  kj::Vector<BookLevel> result;
  size_t count = kj::min(n, bids_cache_.size());
  for (size_t i = 0; i < count; ++i) {
    result.add(bids_cache_[i]);
  }
  return result;
}

kj::Vector<BookLevel> OrderBook::top_asks(size_t n) const {
  kj::Vector<BookLevel> result;
  size_t count = kj::min(n, asks_cache_.size());
  for (size_t i = 0; i < count; ++i) {
    result.add(asks_cache_[i]);
  }
  return result;
}

double OrderBook::depth_at_price(double price, bool is_bid) const {
  if (is_bid) {
    KJ_IF_SOME(qty, bids_.find(DescendingPrice(price))) {
      return qty;
    }
    return 0.0;
  } else {
    KJ_IF_SOME(qty, asks_.find(price)) {
      return qty;
    }
    return 0.0;
  }
}

double OrderBook::total_depth(bool is_bid) const {
  double total = 0.0;
  if (is_bid) {
    for (const auto& entry : bids_) {
      total += entry.value;
    }
  } else {
    for (const auto& entry : asks_) {
      total += entry.value;
    }
  }
  return total;
}

double OrderBook::cumulative_depth(double price, bool is_bid) const {
  double cumulative = 0.0;

  if (is_bid) {
    for (const auto& entry : bids_) {
      // bids_ uses DescendingPrice, so entry.key.value is the actual price
      if (entry.key.value >= price) {
        cumulative += entry.value;
      } else {
        break;
      }
    }
  } else {
    for (const auto& entry : asks_) {
      if (entry.key <= price) {
        cumulative += entry.value;
      } else {
        break;
      }
    }
  }

  return cumulative;
}

kj::Vector<LiquidityPoint> OrderBook::liquidity_profile(bool is_bid, double price_range,
                                                        double step) const {
  kj::Vector<LiquidityPoint> profile;

  // Use KJ_IF_SOME pattern for proper Maybe handling
  KJ_IF_SOME(ref_level, is_bid ? best_bid() : best_ask()) {
    double start_price = is_bid ? (ref_level.price - price_range) : ref_level.price;
    double end_price = is_bid ? ref_level.price : (ref_level.price + price_range);

    for (double price = start_price; price <= end_price; price += step) {
      double depth = cumulative_depth(price, is_bid);
      profile.add(LiquidityPoint{price, depth});
    }
  }

  return profile;
}

double OrderBook::market_impact(double qty, bool is_bid) const {
  double cumulative_qty = 0.0;
  double avg_price = 0.0;

  const auto& cache = is_bid ? bids_cache_ : asks_cache_;

  for (const auto& level : cache) {
    if (cumulative_qty >= qty) {
      break;
    }

    double available_qty = level.qty;
    if (cumulative_qty + available_qty > qty) {
      available_qty = qty - cumulative_qty;
    }

    avg_price = (avg_price * cumulative_qty + level.price * available_qty) /
                (cumulative_qty + available_qty);
    cumulative_qty += available_qty;
  }

  if (cumulative_qty < qty) {
    return 0.0; // Not enough liquidity
  }

  return avg_price;
}

double OrderBook::volume_weighted_average_price(bool is_bid, double depth) const {
  double total_qty = 0.0;
  double total_notional = 0.0;

  const auto& cache = is_bid ? bids_cache_ : asks_cache_;

  for (const auto& level : cache) {
    if (total_qty >= depth) {
      break;
    }

    double available_qty = level.qty;
    if (total_qty + available_qty > depth) {
      available_qty = depth - total_qty;
    }

    total_qty += available_qty;
    total_notional += level.price * available_qty;
  }

  if (total_qty == 0.0) {
    return 0.0;
  }

  return total_notional / total_qty;
}

size_t OrderBook::level_count(bool is_bid) const {
  return is_bid ? bids_.size() : asks_.size();
}

double OrderBook::average_level_size(bool is_bid) const {
  double total = this->total_depth(is_bid);
  size_t levels = level_count(is_bid);

  if (levels == 0) {
    return 0.0;
  }

  return total / static_cast<double>(levels);
}

void OrderBook::clear() {
  bids_.clear();
  asks_.clear();
  bids_cache_.clear();
  asks_cache_.clear();

  // Clear the update buffer
  update_buffer_.clear();

  sequence_ = 0;
  expected_sequence_ = 0;
  state_ = OrderBookState::Empty;
  gap_count_ = 0;
  duplicate_count_ = 0;
}

bool OrderBook::empty() const {
  return bids_.size() == 0 && asks_.size() == 0;
}

OrderBookState OrderBook::state() const {
  return state_;
}

bool OrderBook::is_synchronized() const {
  return state_ == OrderBookState::Synchronized;
}

int64_t OrderBook::expected_sequence() const {
  return expected_sequence_;
}

size_t OrderBook::buffered_update_count() const {
  return update_buffer_.size();
}

int64_t OrderBook::gap_count() const {
  return gap_count_;
}

int64_t OrderBook::duplicate_count() const {
  return duplicate_count_;
}

void OrderBook::set_snapshot_request_callback(SnapshotRequestCallback callback) {
  snapshot_callback_ = kj::mv(callback);
}

void OrderBook::set_max_buffer_size(size_t size) {
  max_buffer_size_ = size;
}

void OrderBook::set_max_sequence_gap(int64_t gap) {
  max_sequence_gap_ = gap;
}

void OrderBook::set_max_depth_levels(size_t levels) {
  max_depth_levels_ = levels;
  // Trim existing data if needed
  if (max_depth_levels_ > 0) {
    // Collect keys to remove (kj::TreeMap doesn't support std::prev(end()))
    // For bids: keep first max_depth_levels_ entries (highest prices due to DescendingPrice)
    // For asks: keep first max_depth_levels_ entries (lowest prices)
    if (bids_.size() > max_depth_levels_) {
      kj::Vector<DescendingPrice> keys_to_remove;
      size_t count = 0;
      for (const auto& entry : bids_) {
        if (count >= max_depth_levels_) {
          keys_to_remove.add(entry.key);
        }
        ++count;
      }
      for (const auto& key : keys_to_remove) {
        bids_.erase(key);
      }
    }
    if (asks_.size() > max_depth_levels_) {
      kj::Vector<double> keys_to_remove;
      size_t count = 0;
      for (const auto& entry : asks_) {
        if (count >= max_depth_levels_) {
          keys_to_remove.add(entry.key);
        }
        ++count;
      }
      for (const auto& key : keys_to_remove) {
        asks_.erase(key);
      }
    }
    rebuild_cache();
  }
}

BookData OrderBook::snapshot(size_t depth) const {
  BookData data;
  data.sequence = sequence_;

  size_t bid_count = (depth == 0 || depth > bids_cache_.size()) ? bids_cache_.size() : depth;
  size_t ask_count = (depth == 0 || depth > asks_cache_.size()) ? asks_cache_.size() : depth;

  for (size_t i = 0; i < bid_count; ++i) {
    data.bids.add(bids_cache_[i]);
  }
  for (size_t i = 0; i < ask_count; ++i) {
    data.asks.add(asks_cache_[i]);
  }

  return data;
}

double OrderBook::imbalance(size_t depth) const {
  double bid_volume = 0.0;
  double ask_volume = 0.0;

  size_t bid_count = (depth == 0 || depth > bids_cache_.size()) ? bids_cache_.size() : depth;
  size_t ask_count = (depth == 0 || depth > asks_cache_.size()) ? asks_cache_.size() : depth;

  for (size_t i = 0; i < bid_count; ++i) {
    bid_volume += bids_cache_[i].qty;
  }
  for (size_t i = 0; i < ask_count; ++i) {
    ask_volume += asks_cache_[i].qty;
  }

  double total = bid_volume + ask_volume;
  if (total == 0.0) {
    return 0.0;
  }
  return (bid_volume - ask_volume) / total;
}

kj::Vector<BookLevel> OrderBook::levels_within_range(double percent_range, bool is_bid) const {
  kj::Vector<BookLevel> result;
  double mid = mid_price();
  if (mid == 0.0) {
    return result;
  }

  double range = mid * percent_range;
  const auto& cache = is_bid ? bids_cache_ : asks_cache_;

  for (const auto& level : cache) {
    double distance = is_bid ? (mid - level.price) : (level.price - mid);
    if (distance <= range) {
      result.add(level);
    } else {
      break; // Cache is sorted, so we can stop early
    }
  }

  return result;
}

void OrderBook::request_rebuild() {
  state_ = OrderBookState::Syncing;
  trigger_snapshot_request();
}

void OrderBook::rebuild_cache() {
  bids_cache_.clear();
  size_t count = 0;
  for (const auto& entry : bids_) {
    if (max_depth_levels_ > 0 && count >= max_depth_levels_) {
      break;
    }
    bids_cache_.add(BookLevel{entry.key.value, entry.value});
    ++count;
  }

  asks_cache_.clear();
  count = 0;
  for (const auto& entry : asks_) {
    if (max_depth_levels_ > 0 && count >= max_depth_levels_) {
      break;
    }
    asks_cache_.add(BookLevel{entry.key, entry.value});
    ++count;
  }
}

} // namespace veloz::market
