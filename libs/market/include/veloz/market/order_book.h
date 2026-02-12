#pragma once

#include "veloz/market/market_event.h"

#include <map>
#include <optional>
#include <vector>

namespace veloz::market {

class OrderBook final {
public:
  OrderBook() = default;

  // Apply full snapshot (replaces existing book)
  void apply_snapshot(const std::vector<BookLevel>& bids, const std::vector<BookLevel>& asks,
                      std::int64_t sequence);

  // Apply incremental delta (update/delete a level)
  void apply_delta(const BookLevel& level, bool is_bid, std::int64_t sequence);

  // Query methods
  [[nodiscard]] const std::vector<BookLevel>& bids() const;
  [[nodiscard]] const std::vector<BookLevel>& asks() const;
  [[nodiscard]] std::optional<BookLevel> best_bid() const;
  [[nodiscard]] std::optional<BookLevel> best_ask() const;
  [[nodiscard]] double spread() const;
  [[nodiscard]] double mid_price() const;
  [[nodiscard]] std::int64_t sequence() const;

  // Get top N levels
  [[nodiscard]] std::vector<BookLevel> top_bids(std::size_t n) const;
  [[nodiscard]] std::vector<BookLevel> top_asks(std::size_t n) const;

  // Depth and liquidity calculations
  [[nodiscard]] double depth_at_price(double price, bool is_bid) const;
  [[nodiscard]] double total_depth(bool is_bid) const;
  [[nodiscard]] double cumulative_depth(double price, bool is_bid) const;
  [[nodiscard]] std::vector<std::pair<double, double>>
  liquidity_profile(bool is_bid, double price_range, double step) const;

  // Market impact and order book statistics
  [[nodiscard]] double market_impact(double qty, bool is_bid) const;
  [[nodiscard]] double volume_weighted_average_price(bool is_bid, double depth) const;
  [[nodiscard]] std::size_t level_count(bool is_bid) const;
  [[nodiscard]] double average_level_size(bool is_bid) const;

  // Clear order book
  void clear();
  [[nodiscard]] bool empty() const;

private:
  void rebuild_cache();

  std::map<double, double, std::greater<double>> bids_; // descending
  std::map<double, double> asks_;                       // ascending

  // Cache for fast queries (bids[0] = best bid, asks[0] = best ask)
  std::vector<BookLevel> bids_cache_;
  std::vector<BookLevel> asks_cache_;

  std::int64_t sequence_{0};
};

} // namespace veloz::market
