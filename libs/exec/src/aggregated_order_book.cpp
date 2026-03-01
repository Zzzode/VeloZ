#include "veloz/exec/aggregated_order_book.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace veloz::exec {

void AggregatedOrderBook::update_venue(veloz::common::Venue venue,
                                       const veloz::market::BookData& book,
                                       std::int64_t timestamp_ns) {
  auto lock = guarded_.lockExclusive();

  // Get or create venue book
  VenueBook* venue_book = nullptr;
  KJ_IF_SOME(existing, lock->venues.find(venue)) {
    venue_book = &existing;
  }
  else {
    lock->venues.insert(venue, VenueBook{});
    KJ_IF_SOME(inserted, lock->venues.find(venue)) {
      venue_book = &inserted;
    }
  }

  if (venue_book == nullptr) {
    return;
  }

  // Copy book data
  venue_book->bids.clear();
  for (const auto& level : book.bids) {
    venue_book->bids.add(level);
  }

  venue_book->asks.clear();
  for (const auto& level : book.asks) {
    venue_book->asks.add(level);
  }

  // Update BBO
  venue_book->bbo.venue = venue;
  venue_book->bbo.timestamp_ns = timestamp_ns;
  venue_book->bbo.is_stale = false;

  if (!book.bids.empty()) {
    venue_book->bbo.bid_price = book.bids[0].price;
    venue_book->bbo.bid_qty = book.bids[0].qty;
  } else {
    venue_book->bbo.bid_price = 0.0;
    venue_book->bbo.bid_qty = 0.0;
  }

  if (!book.asks.empty()) {
    venue_book->bbo.ask_price = book.asks[0].price;
    venue_book->bbo.ask_qty = book.asks[0].qty;
  } else {
    venue_book->bbo.ask_price = 0.0;
    venue_book->bbo.ask_qty = 0.0;
  }

  venue_book->last_update_ns = timestamp_ns;
}

void AggregatedOrderBook::update_venue_bbo(veloz::common::Venue venue, double bid_price,
                                           double bid_qty, double ask_price, double ask_qty,
                                           std::int64_t timestamp_ns) {
  auto lock = guarded_.lockExclusive();

  // Get or create venue book
  VenueBook* venue_book = nullptr;
  KJ_IF_SOME(existing, lock->venues.find(venue)) {
    venue_book = &existing;
  }
  else {
    lock->venues.insert(venue, VenueBook{});
    KJ_IF_SOME(inserted, lock->venues.find(venue)) {
      venue_book = &inserted;
    }
  }

  if (venue_book == nullptr) {
    return;
  }

  venue_book->bbo.venue = venue;
  venue_book->bbo.bid_price = bid_price;
  venue_book->bbo.bid_qty = bid_qty;
  venue_book->bbo.ask_price = ask_price;
  venue_book->bbo.ask_qty = ask_qty;
  venue_book->bbo.timestamp_ns = timestamp_ns;
  venue_book->bbo.is_stale = false;
  venue_book->last_update_ns = timestamp_ns;
}

AggregatedBBO AggregatedOrderBook::get_aggregated_bbo() const {
  auto lock = guarded_.lockExclusive();

  AggregatedBBO result;
  result.best_bid_price = 0.0;
  result.best_ask_price = std::numeric_limits<double>::max();

  for (const auto& entry : lock->venues) {
    const auto& bbo = entry.value.bbo;

    if (bbo.is_stale) {
      continue;
    }

    // Add to venues list
    VenueBBO copy;
    copy.venue = bbo.venue;
    copy.bid_price = bbo.bid_price;
    copy.bid_qty = bbo.bid_qty;
    copy.ask_price = bbo.ask_price;
    copy.ask_qty = bbo.ask_qty;
    copy.timestamp_ns = bbo.timestamp_ns;
    copy.is_stale = bbo.is_stale;
    result.venues.add(kj::mv(copy));

    // Update best bid (highest)
    if (bbo.bid_price > result.best_bid_price && bbo.bid_qty > 0) {
      result.best_bid_price = bbo.bid_price;
      result.best_bid_qty = bbo.bid_qty;
      result.best_bid_venue = bbo.venue;
    }

    // Update best ask (lowest)
    if (bbo.ask_price < result.best_ask_price && bbo.ask_qty > 0) {
      result.best_ask_price = bbo.ask_price;
      result.best_ask_qty = bbo.ask_qty;
      result.best_ask_venue = bbo.venue;
    }
  }

  // Handle case where no valid data
  if (result.best_ask_price == std::numeric_limits<double>::max()) {
    result.best_ask_price = 0.0;
  }

  // Calculate spread and mid
  if (result.best_bid_price > 0 && result.best_ask_price > 0) {
    result.spread = result.best_ask_price - result.best_bid_price;
    result.mid_price = (result.best_bid_price + result.best_ask_price) / 2.0;
  }

  return result;
}

kj::Vector<AggregatedLevel> AggregatedOrderBook::get_aggregated_bids(std::size_t depth) const {
  auto lock = guarded_.lockExclusive();

  // Collect all bid levels with venue attribution
  kj::HashMap<std::int64_t, AggregatedLevel> price_levels; // Use int64 for price key (scaled)

  for (const auto& entry : lock->venues) {
    if (entry.value.bbo.is_stale) {
      continue;
    }

    std::size_t count = 0;
    for (const auto& level : entry.value.bids) {
      if (count >= depth) {
        break;
      }

      // Scale price to int64 for map key (8 decimal places)
      std::int64_t price_key = static_cast<std::int64_t>(level.price * 100000000);

      AggregatedLevel* agg_level = nullptr;
      KJ_IF_SOME(existing, price_levels.find(price_key)) {
        agg_level = &existing;
      }
      else {
        AggregatedLevel new_level;
        new_level.price = level.price;
        price_levels.insert(price_key, kj::mv(new_level));
        KJ_IF_SOME(inserted, price_levels.find(price_key)) {
          agg_level = &inserted;
        }
      }

      if (agg_level != nullptr) {
        agg_level->total_qty += level.qty;
        agg_level->venue_breakdown.add(std::make_pair(entry.key, level.qty));
      }

      ++count;
    }
  }

  // Sort by price descending (best bid first)
  kj::Vector<AggregatedLevel> result;
  kj::Vector<std::pair<std::int64_t, AggregatedLevel*>> sorted;

  for (auto& entry : price_levels) {
    sorted.add(std::make_pair(entry.key, &entry.value));
  }

  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::size_t count = 0;
  for (const auto& s : sorted) {
    if (count >= depth) {
      break;
    }
    result.add(kj::mv(*s.second));
    ++count;
  }

  return result;
}

kj::Vector<AggregatedLevel> AggregatedOrderBook::get_aggregated_asks(std::size_t depth) const {
  auto lock = guarded_.lockExclusive();

  // Collect all ask levels with venue attribution
  kj::HashMap<std::int64_t, AggregatedLevel> price_levels;

  for (const auto& entry : lock->venues) {
    if (entry.value.bbo.is_stale) {
      continue;
    }

    std::size_t count = 0;
    for (const auto& level : entry.value.asks) {
      if (count >= depth) {
        break;
      }

      std::int64_t price_key = static_cast<std::int64_t>(level.price * 100000000);

      AggregatedLevel* agg_level = nullptr;
      KJ_IF_SOME(existing, price_levels.find(price_key)) {
        agg_level = &existing;
      }
      else {
        AggregatedLevel new_level;
        new_level.price = level.price;
        price_levels.insert(price_key, kj::mv(new_level));
        KJ_IF_SOME(inserted, price_levels.find(price_key)) {
          agg_level = &inserted;
        }
      }

      if (agg_level != nullptr) {
        agg_level->total_qty += level.qty;
        agg_level->venue_breakdown.add(std::make_pair(entry.key, level.qty));
      }

      ++count;
    }
  }

  // Sort by price ascending (best ask first)
  kj::Vector<AggregatedLevel> result;
  kj::Vector<std::pair<std::int64_t, AggregatedLevel*>> sorted;

  for (auto& entry : price_levels) {
    sorted.add(std::make_pair(entry.key, &entry.value));
  }

  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::size_t count = 0;
  for (const auto& s : sorted) {
    if (count >= depth) {
      break;
    }
    result.add(kj::mv(*s.second));
    ++count;
  }

  return result;
}

kj::Maybe<VenueBBO> AggregatedOrderBook::get_venue_bbo(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(venue_book, lock->venues.find(venue)) {
    VenueBBO copy;
    copy.venue = venue_book.bbo.venue;
    copy.bid_price = venue_book.bbo.bid_price;
    copy.bid_qty = venue_book.bbo.bid_qty;
    copy.ask_price = venue_book.bbo.ask_price;
    copy.ask_qty = venue_book.bbo.ask_qty;
    copy.timestamp_ns = venue_book.bbo.timestamp_ns;
    copy.is_stale = venue_book.bbo.is_stale;
    return copy;
  }

  return kj::none;
}

bool AggregatedOrderBook::has_venue(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  return lock->venues.find(venue) != kj::none;
}

kj::Array<veloz::common::Venue> AggregatedOrderBook::get_venues() const {
  auto lock = guarded_.lockExclusive();
  auto builder = kj::heapArrayBuilder<veloz::common::Venue>(lock->venues.size());

  for (const auto& entry : lock->venues) {
    builder.add(entry.key);
  }

  return builder.finish();
}

void AggregatedOrderBook::mark_stale(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(venue_book, lock->venues.find(venue)) {
    venue_book.bbo.is_stale = true;
  }
}

void AggregatedOrderBook::remove_venue(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->venues.erase(venue);
}

void AggregatedOrderBook::check_staleness(std::int64_t current_time_ns) {
  auto lock = guarded_.lockExclusive();

  std::int64_t max_age_ns = lock->staleness_config.max_age / kj::NANOSECONDS;

  for (auto& entry : lock->venues) {
    std::int64_t age_ns = current_time_ns - entry.value.last_update_ns;
    if (age_ns > max_age_ns) {
      entry.value.bbo.is_stale = true;
    }
  }
}

void AggregatedOrderBook::set_staleness_config(const StalenessConfig& config) {
  auto lock = guarded_.lockExclusive();
  lock->staleness_config = config;
}

StalenessConfig AggregatedOrderBook::staleness_config() const {
  auto lock = guarded_.lockExclusive();
  return lock->staleness_config;
}

void AggregatedOrderBook::clear() {
  auto lock = guarded_.lockExclusive();
  lock->venues.clear();
}

} // namespace veloz::exec
