#pragma once

#include "veloz/market/market_event.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/vector.h>

namespace veloz::market {

// Wrapper for descending order keys in kj::TreeMap
// kj::TreeMap uses operator< for ordering, so we invert the comparison
struct DescendingPrice {
  double value;

  DescendingPrice() : value(0.0) {}
  explicit DescendingPrice(double v) : value(v) {}

  // Invert comparison for descending order
  bool operator<(const DescendingPrice& other) const {
    return value > other.value;
  }
  bool operator==(const DescendingPrice& other) const {
    return value == other.value;
  }

  // Allow comparison with raw double
  bool operator<(double other) const {
    return value > other;
  }
  bool operator==(double other) const {
    return value == other;
  }
};

// Liquidity point for liquidity profile analysis
// Using dedicated struct instead of std::pair for clarity
struct LiquidityPoint {
  double price{0.0};
  double cumulative_depth{0.0};

  auto operator<=>(const LiquidityPoint&) const = default;
};

// Order book update result for tracking sequence state
enum class UpdateResult {
  Applied,        // Update was applied successfully
  Duplicate,      // Update was a duplicate (same or older sequence)
  GapDetected,    // Gap in sequence detected, snapshot needed
  Buffered,       // Update was buffered for later application
  BufferOverflow, // Buffer is full, updates dropped
  InvalidState    // Order book is in invalid state
};

// Buffered delta update for out-of-order handling
struct BufferedDelta {
  BookLevel level;
  bool is_bid;
  int64_t sequence;

  // For priority queue ordering (lower sequence = higher priority)
  bool operator<(const BufferedDelta& other) const {
    return sequence > other.sequence;
  }
};

// Order book state for rebuild tracking
enum class OrderBookState {
  Empty,       // No data received yet
  Syncing,     // Waiting for snapshot after gap
  Synchronized // Fully synchronized with exchange
};

class OrderBook final {
public:
  // Callback type for requesting snapshot when gap detected
  using SnapshotRequestCallback = kj::Function<void()>;

  OrderBook() = default;

  // Apply full snapshot (replaces existing book)
  void apply_snapshot(const kj::Vector<BookLevel>& bids, const kj::Vector<BookLevel>& asks,
                      int64_t sequence);

  // Apply incremental delta (update/delete a level)
  // Returns result indicating what happened with the update
  UpdateResult apply_delta(const BookLevel& level, bool is_bid, int64_t sequence);

  // Apply batch of deltas (for efficiency)
  UpdateResult apply_deltas(const kj::Vector<BookLevel>& bids, const kj::Vector<BookLevel>& asks,
                            int64_t first_sequence, int64_t final_sequence);

  // Apply BookData directly (handles both snapshot and delta based on is_snapshot flag)
  UpdateResult apply_book_data(const BookData& data);

  // Query methods
  [[nodiscard]] const kj::Vector<BookLevel>& bids() const;
  [[nodiscard]] const kj::Vector<BookLevel>& asks() const;
  [[nodiscard]] kj::Maybe<BookLevel> best_bid() const;
  [[nodiscard]] kj::Maybe<BookLevel> best_ask() const;
  [[nodiscard]] double spread() const;
  [[nodiscard]] double mid_price() const;
  [[nodiscard]] int64_t sequence() const;

  // Get top N levels
  [[nodiscard]] kj::Vector<BookLevel> top_bids(size_t n) const;
  [[nodiscard]] kj::Vector<BookLevel> top_asks(size_t n) const;

  // Depth and liquidity calculations
  [[nodiscard]] double depth_at_price(double price, bool is_bid) const;
  [[nodiscard]] double total_depth(bool is_bid) const;
  [[nodiscard]] double cumulative_depth(double price, bool is_bid) const;
  [[nodiscard]] kj::Vector<LiquidityPoint> liquidity_profile(bool is_bid, double price_range,
                                                             double step) const;

  // Market impact and order book statistics
  [[nodiscard]] double market_impact(double qty, bool is_bid) const;
  [[nodiscard]] double volume_weighted_average_price(bool is_bid, double depth) const;
  [[nodiscard]] size_t level_count(bool is_bid) const;
  [[nodiscard]] double average_level_size(bool is_bid) const;

  // Clear order book
  void clear();
  [[nodiscard]] bool empty() const;

  // Sequence and state management
  [[nodiscard]] OrderBookState state() const;
  [[nodiscard]] bool is_synchronized() const;
  [[nodiscard]] int64_t expected_sequence() const;
  [[nodiscard]] size_t buffered_update_count() const;
  [[nodiscard]] int64_t gap_count() const;
  [[nodiscard]] int64_t duplicate_count() const;

  // Set callback for requesting snapshot when gap detected
  void set_snapshot_request_callback(SnapshotRequestCallback callback);

  // Configuration
  void set_max_buffer_size(size_t size);
  void set_max_sequence_gap(int64_t gap);

  // Depth level configuration
  /**
   * @brief Set maximum depth levels to maintain
   * @param levels Maximum number of price levels (0 = unlimited)
   */
  void set_max_depth_levels(size_t levels);

  /**
   * @brief Get current max depth levels setting
   */
  [[nodiscard]] size_t max_depth_levels() const {
    return max_depth_levels_;
  }

  // Snapshot functionality
  /**
   * @brief Get a snapshot of the order book at current state
   * @param depth Number of levels to include (0 = all)
   * @return BookData containing bid and ask levels
   */
  [[nodiscard]] BookData snapshot(size_t depth = 0) const;

  /**
   * @brief Get order book imbalance ratio
   * @param depth Number of levels to consider (0 = all)
   * @return Imbalance ratio: (bid_volume - ask_volume) / (bid_volume + ask_volume)
   */
  [[nodiscard]] double imbalance(size_t depth = 5) const;

  /**
   * @brief Get price levels within a percentage range from mid price
   * @param percent_range Percentage range (e.g., 0.01 for 1%)
   * @param is_bid true for bids, false for asks
   * @return Vector of levels within range
   */
  [[nodiscard]] kj::Vector<BookLevel> levels_within_range(double percent_range, bool is_bid) const;

  // Force rebuild request (e.g., on reconnection)
  void request_rebuild();

private:
  void rebuild_cache();
  void apply_level(const BookLevel& level, bool is_bid);
  void process_buffered_updates();
  void trigger_snapshot_request();

  // Using kj::TreeMap for ordered key lookup
  // bids_ uses DescendingPrice wrapper for descending order (best bid first)
  // asks_ uses double directly for ascending order (best ask first)
  kj::TreeMap<DescendingPrice, double> bids_;
  kj::TreeMap<double, double> asks_;

  // Cache for fast queries (bids[0] = best bid, asks[0] = best ask)
  kj::Vector<BookLevel> bids_cache_;
  kj::Vector<BookLevel> asks_cache_;

  // Sequence tracking
  int64_t sequence_{0};
  int64_t expected_sequence_{0}; // Next expected sequence number

  // State management
  OrderBookState state_{OrderBookState::Empty};

  // Buffer for out-of-order updates
  // Using kj::Vector with manual heap operations (KJ doesn't provide priority queue)
  kj::Vector<BufferedDelta> update_buffer_;
  size_t max_buffer_size_{1000};
  int64_t max_sequence_gap_{100}; // Max gap before requesting snapshot
  size_t max_depth_levels_{0};    // Max depth levels to maintain (0 = unlimited)

  // Statistics
  int64_t gap_count_{0};
  int64_t duplicate_count_{0};

  // Callback for snapshot requests
  kj::Maybe<SnapshotRequestCallback> snapshot_callback_;
};

} // namespace veloz::market
