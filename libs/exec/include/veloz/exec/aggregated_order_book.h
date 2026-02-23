#pragma once

#include "veloz/common/types.h"
#include "veloz/market/market_event.h"

#include <cstdint>
#include <utility>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Best bid/ask from a single exchange
struct VenueBBO {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  double bid_price{0.0};
  double bid_qty{0.0};
  double ask_price{0.0};
  double ask_qty{0.0};
  std::int64_t timestamp_ns{0};
  bool is_stale{false};
};

// Aggregated best bid/ask across all exchanges
struct AggregatedBBO {
  // Best bid across all venues
  double best_bid_price{0.0};
  double best_bid_qty{0.0};
  veloz::common::Venue best_bid_venue{veloz::common::Venue::Unknown};

  // Best ask across all venues
  double best_ask_price{0.0};
  double best_ask_qty{0.0};
  veloz::common::Venue best_ask_venue{veloz::common::Venue::Unknown};

  // Spread
  double spread{0.0};
  double mid_price{0.0};

  // Per-venue breakdown
  kj::Vector<VenueBBO> venues;
};

// Price level with venue attribution
struct AggregatedLevel {
  double price{0.0};
  double total_qty{0.0};
  kj::Vector<std::pair<veloz::common::Venue, double>> venue_breakdown;
};

// Configuration for staleness detection
struct StalenessConfig {
  kj::Duration max_age{5 * kj::SECONDS};     // Max age before marking stale
  kj::Duration warning_age{2 * kj::SECONDS}; // Age to start warning
};

// AggregatedOrderBook merges order books from multiple exchanges
class AggregatedOrderBook final {
public:
  AggregatedOrderBook() = default;

  // Update order book for a venue
  void update_venue(veloz::common::Venue venue, const veloz::market::BookData& book,
                    std::int64_t timestamp_ns);

  // Update BBO only (more efficient for top-of-book strategies)
  void update_venue_bbo(veloz::common::Venue venue, double bid_price, double bid_qty,
                        double ask_price, double ask_qty, std::int64_t timestamp_ns);

  // Get aggregated BBO
  [[nodiscard]] AggregatedBBO get_aggregated_bbo() const;

  // Get aggregated order book (merged levels)
  [[nodiscard]] kj::Vector<AggregatedLevel> get_aggregated_bids(std::size_t depth = 10) const;
  [[nodiscard]] kj::Vector<AggregatedLevel> get_aggregated_asks(std::size_t depth = 10) const;

  // Get BBO for a specific venue
  [[nodiscard]] kj::Maybe<VenueBBO> get_venue_bbo(veloz::common::Venue venue) const;

  // Check if a venue has data
  [[nodiscard]] bool has_venue(veloz::common::Venue venue) const;

  // Get all venues with data
  [[nodiscard]] kj::Array<veloz::common::Venue> get_venues() const;

  // Mark venue as stale (e.g., on disconnect)
  void mark_stale(veloz::common::Venue venue);

  // Remove venue data
  void remove_venue(veloz::common::Venue venue);

  // Check staleness based on timestamp
  void check_staleness(std::int64_t current_time_ns);

  // Configuration
  void set_staleness_config(const StalenessConfig& config);
  [[nodiscard]] StalenessConfig staleness_config() const;

  // Clear all data
  void clear();

private:
  struct VenueBook {
    kj::Vector<veloz::market::BookLevel> bids;
    kj::Vector<veloz::market::BookLevel> asks;
    VenueBBO bbo;
    std::int64_t last_update_ns{0};
  };

  struct BookState {
    kj::HashMap<veloz::common::Venue, VenueBook> venues;
    StalenessConfig staleness_config;
  };

  kj::MutexGuarded<BookState> guarded_;
};

} // namespace veloz::exec
