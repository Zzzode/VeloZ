#include "veloz/market/order_book.h"

#include <kj/debug.h>

namespace veloz::market {

void OrderBook::apply_snapshot(const kj::Vector<BookLevel>& bids, const kj::Vector<BookLevel>& asks,
                               int64_t sequence) {
  bids_.clear();
  asks_.clear();

  // Clear the update buffer since we're resetting with a snapshot
  while (!update_buffer_.empty()) {
    update_buffer_.pop();
  }

  for (const auto& level : bids) {
    if (level.qty > 0.0) {
      bids_[level.price] = level.qty;
    }
  }

  for (const auto& level : asks) {
    if (level.qty > 0.0) {
      asks_[level.price] = level.qty;
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
      update_buffer_.push(BufferedDelta{level, is_bid, sequence});
      trigger_snapshot_request();
      state_ = OrderBookState::Syncing;
      return UpdateResult::Buffered;
    }
    return UpdateResult::BufferOverflow;
  }

  // Handle syncing state - buffer updates until snapshot arrives
  if (state_ == OrderBookState::Syncing) {
    if (update_buffer_.size() < max_buffer_size_) {
      update_buffer_.push(BufferedDelta{level, is_bid, sequence});
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
      update_buffer_.push(BufferedDelta{level, is_bid, sequence});
      gap_count_++;
      return UpdateResult::GapDetected;
    }

    // Gap too large - need to resync
    gap_count_++;
    state_ = OrderBookState::Syncing;
    trigger_snapshot_request();

    // Buffer this update for after snapshot
    if (update_buffer_.size() < max_buffer_size_) {
      update_buffer_.push(BufferedDelta{level, is_bid, sequence});
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
      bids_.erase(level.price);
    } else {
      bids_[level.price] = level.qty;
    }
  } else {
    if (level.qty == 0.0) {
      asks_.erase(level.price);
    } else {
      asks_[level.price] = level.qty;
    }
  }
}

void OrderBook::process_buffered_updates() {
  while (!update_buffer_.empty()) {
    const auto& top = update_buffer_.top();

    // If the buffered update is older than current sequence, discard it
    if (top.sequence <= sequence_) {
      update_buffer_.pop();
      continue;
    }

    // If the buffered update is the next expected sequence, apply it
    if (top.sequence == expected_sequence_) {
      apply_level(top.level, top.is_bid);
      sequence_ = top.sequence;
      expected_sequence_ = top.sequence + 1;
      update_buffer_.pop();
      rebuild_cache();
    } else {
      // Gap still exists, stop processing
      break;
    }
  }
}

void OrderBook::trigger_snapshot_request() {
  KJ_IF_MAYBE (callback, snapshot_callback_) {
    (*callback)();
  }
}

const kj::Vector<BookLevel>& OrderBook::bids() const {
  return bids_cache_;
}

const kj::Vector<BookLevel>& OrderBook::asks() const {
  return asks_cache_;
}

kj::Maybe<BookLevel> OrderBook::best_bid() const {
  if (bids_cache_.empty())
    return nullptr;
  return bids_cache_.front();
}

kj::Maybe<BookLevel> OrderBook::best_ask() const {
  if (asks_cache_.empty())
    return nullptr;
  return asks_cache_.front();
}

double OrderBook::spread() const {
  // Use KJ_IF_MAYBE pattern for proper Maybe handling
  KJ_IF_MAYBE (bid, best_bid()) {
    KJ_IF_MAYBE (ask, best_ask()) {
      return ask->price - bid->price;
    }
  }
  return 0.0;
}

double OrderBook::mid_price() const {
  // Use KJ_IF_MAYBE pattern for proper Maybe handling
  KJ_IF_MAYBE (bid, best_bid()) {
    KJ_IF_MAYBE (ask, best_ask()) {
      return (bid->price + ask->price) / 2.0;
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
    auto it = bids_.find(price);
    return (it != bids_.end()) ? it->second : 0.0;
  } else {
    auto it = asks_.find(price);
    return (it != asks_.end()) ? it->second : 0.0;
  }
}

double OrderBook::total_depth(bool is_bid) const {
  double total = 0.0;
  if (is_bid) {
    for (const auto& [_, qty] : bids_) {
      total += qty;
    }
  } else {
    for (const auto& [_, qty] : asks_) {
      total += qty;
    }
  }
  return total;
}

double OrderBook::cumulative_depth(double price, bool is_bid) const {
  double cumulative = 0.0;

  if (is_bid) {
    for (const auto& [level_price, qty] : bids_) {
      if (level_price >= price) {
        cumulative += qty;
      } else {
        break;
      }
    }
  } else {
    for (const auto& [level_price, qty] : asks_) {
      if (level_price <= price) {
        cumulative += qty;
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

  // Use KJ_IF_MAYBE pattern for proper Maybe handling
  KJ_IF_MAYBE (ref_level, is_bid ? best_bid() : best_ask()) {
    double start_price = is_bid ? (ref_level->price - price_range) : ref_level->price;
    double end_price = is_bid ? ref_level->price : (ref_level->price + price_range);

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
  while (!update_buffer_.empty()) {
    update_buffer_.pop();
  }

  sequence_ = 0;
  expected_sequence_ = 0;
  state_ = OrderBookState::Empty;
  gap_count_ = 0;
  duplicate_count_ = 0;
}

bool OrderBook::empty() const {
  return bids_.empty() && asks_.empty();
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

void OrderBook::request_rebuild() {
  state_ = OrderBookState::Syncing;
  trigger_snapshot_request();
}

void OrderBook::rebuild_cache() {
  bids_cache_.clear();
  for (const auto& [price, qty] : bids_) {
    bids_cache_.add(BookLevel{price, qty});
  }

  asks_cache_.clear();
  for (const auto& [price, qty] : asks_) {
    asks_cache_.add(BookLevel{price, qty});
  }
}

} // namespace veloz::market
