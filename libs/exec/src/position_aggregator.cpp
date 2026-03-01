#include "veloz/exec/position_aggregator.h"

#include <algorithm>
#include <cmath>

namespace veloz::exec {

void PositionAggregator::on_fill(veloz::common::Venue venue, const ExecutionReport& report,
                                 OrderSide side, double fill_qty, double fill_price) {
  auto lock = guarded_.lockExclusive();

  // Get or create venue map
  if (lock->positions.find(venue) == kj::none) {
    lock->positions.insert(venue, kj::HashMap<kj::String, ExchangePosition>());
  }

  KJ_IF_SOME(venue_map, lock->positions.find(venue)) {
    kj::String symbol_key = kj::heapString(report.symbol.value);

    // Get or create position
    ExchangePosition* pos = nullptr;
    KJ_IF_SOME(existing, venue_map.find(symbol_key)) {
      pos = &existing;
    }
    else {
      ExchangePosition new_pos;
      new_pos.venue = venue;
      new_pos.symbol = report.symbol;
      venue_map.insert(kj::heapString(symbol_key), kj::mv(new_pos));
      KJ_IF_SOME(inserted, venue_map.find(symbol_key)) {
        pos = &inserted;
      }
    }

    if (pos == nullptr) {
      return;
    }

    // Calculate signed quantity change
    double qty_change = (side == OrderSide::Buy) ? fill_qty : -fill_qty;
    double old_qty = pos->quantity;
    double new_qty = old_qty + qty_change;

    // Calculate realized P&L if reducing position
    if ((old_qty > 0 && qty_change < 0) || (old_qty < 0 && qty_change > 0)) {
      // Reducing or closing position
      double closed_qty = (std::min)(std::abs(old_qty), std::abs(qty_change));
      double pnl_per_unit =
          (old_qty > 0) ? (fill_price - pos->avg_entry_price) : (pos->avg_entry_price - fill_price);
      pos->realized_pnl += closed_qty * pnl_per_unit;
    }

    // Update average entry price
    if (std::abs(new_qty) > 1e-10) {
      if ((old_qty >= 0 && new_qty > old_qty) || (old_qty <= 0 && new_qty < old_qty)) {
        // Adding to position - update weighted average
        double old_value = std::abs(old_qty) * pos->avg_entry_price;
        double new_value = std::abs(qty_change) * fill_price;
        pos->avg_entry_price = (old_value + new_value) / std::abs(new_qty);
      }
      // If reducing position, keep the same avg entry price
    } else {
      // Position closed
      pos->avg_entry_price = 0.0;
    }

    pos->quantity = new_qty;
    pos->last_update_ns = report.ts_recv_ns;
  }
}

void PositionAggregator::set_position(veloz::common::Venue venue,
                                      const veloz::common::SymbolId& symbol, double quantity,
                                      double avg_price) {
  auto lock = guarded_.lockExclusive();

  // Get or create venue map
  if (lock->positions.find(venue) == kj::none) {
    lock->positions.insert(venue, kj::HashMap<kj::String, ExchangePosition>());
  }

  KJ_IF_SOME(venue_map, lock->positions.find(venue)) {
    kj::String symbol_key = kj::heapString(symbol.value);

    ExchangePosition pos;
    pos.venue = venue;
    pos.symbol = symbol;
    pos.quantity = quantity;
    pos.avg_entry_price = avg_price;

    venue_map.upsert(kj::mv(symbol_key), kj::mv(pos),
                     [&](ExchangePosition& existing, ExchangePosition&& replacement) {
                       existing.quantity = replacement.quantity;
                       existing.avg_entry_price = replacement.avg_entry_price;
                     });
  }
}

void PositionAggregator::update_mark_price(const veloz::common::SymbolId& symbol,
                                           double mark_price) {
  auto lock = guarded_.lockExclusive();

  for (auto& venue_entry : lock->positions) {
    KJ_IF_SOME(pos, venue_entry.value.find(symbol.value)) {
      if (std::abs(pos.quantity) > 1e-10) {
        pos.unrealized_pnl = pos.quantity * (mark_price - pos.avg_entry_price);
      } else {
        pos.unrealized_pnl = 0.0;
      }
    }
  }
}

kj::Maybe<ExchangePosition>
PositionAggregator::get_position(veloz::common::Venue venue,
                                 const veloz::common::SymbolId& symbol) const {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(venue_map, lock->positions.find(venue)) {
    KJ_IF_SOME(pos, venue_map.find(symbol.value)) {
      // Return a copy
      ExchangePosition copy;
      copy.venue = pos.venue;
      copy.symbol = pos.symbol;
      copy.quantity = pos.quantity;
      copy.avg_entry_price = pos.avg_entry_price;
      copy.unrealized_pnl = pos.unrealized_pnl;
      copy.realized_pnl = pos.realized_pnl;
      copy.last_update_ns = pos.last_update_ns;
      return copy;
    }
  }
  return kj::none;
}

kj::Maybe<AggregatedPosition>
PositionAggregator::get_aggregated_position(const veloz::common::SymbolId& symbol) const {
  auto lock = guarded_.lockExclusive();

  AggregatedPosition agg;
  agg.symbol = symbol;
  bool found = false;

  for (const auto& venue_entry : lock->positions) {
    KJ_IF_SOME(pos, venue_entry.value.find(symbol.value)) {
      found = true;

      ExchangePosition copy;
      copy.venue = pos.venue;
      copy.symbol = pos.symbol;
      copy.quantity = pos.quantity;
      copy.avg_entry_price = pos.avg_entry_price;
      copy.unrealized_pnl = pos.unrealized_pnl;
      copy.realized_pnl = pos.realized_pnl;
      copy.last_update_ns = pos.last_update_ns;
      agg.venues.add(kj::mv(copy));

      agg.total_quantity += pos.quantity;
      agg.total_unrealized_pnl += pos.unrealized_pnl;
      agg.total_realized_pnl += pos.realized_pnl;
    }
  }

  if (!found) {
    return kj::none;
  }

  // Calculate weighted average price
  double total_value = 0.0;
  double total_abs_qty = 0.0;
  for (const auto& pos : agg.venues) {
    total_value += std::abs(pos.quantity) * pos.avg_entry_price;
    total_abs_qty += std::abs(pos.quantity);
  }
  if (total_abs_qty > 1e-10) {
    agg.weighted_avg_price = total_value / total_abs_qty;
  }

  return agg;
}

kj::Vector<ExchangePosition>
PositionAggregator::get_venue_positions(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<ExchangePosition> result;

  KJ_IF_SOME(venue_map, lock->positions.find(venue)) {
    for (const auto& entry : venue_map) {
      ExchangePosition copy;
      copy.venue = entry.value.venue;
      copy.symbol = entry.value.symbol;
      copy.quantity = entry.value.quantity;
      copy.avg_entry_price = entry.value.avg_entry_price;
      copy.unrealized_pnl = entry.value.unrealized_pnl;
      copy.realized_pnl = entry.value.realized_pnl;
      copy.last_update_ns = entry.value.last_update_ns;
      result.add(kj::mv(copy));
    }
  }

  return result;
}

kj::Vector<AggregatedPosition> PositionAggregator::get_all_positions() const {
  auto lock = guarded_.lockExclusive();

  // Collect all unique symbols
  kj::HashMap<kj::String, bool> symbols;
  for (const auto& venue_entry : lock->positions) {
    for (const auto& pos_entry : venue_entry.value) {
      symbols.upsert(kj::heapString(pos_entry.key), true,
                     [](bool& existing, bool&& replacement) { existing = replacement; });
    }
  }

  kj::Vector<AggregatedPosition> result;
  for (const auto& symbol_entry : symbols) {
    veloz::common::SymbolId symbol(kj::heapString(symbol_entry.key));
    // Release lock temporarily to call get_aggregated_position
    // Note: This is safe because we're iterating over a snapshot
  }

  // Actually, let's compute inline to avoid lock issues
  for (const auto& symbol_entry : symbols) {
    AggregatedPosition agg;
    agg.symbol = veloz::common::SymbolId(kj::heapString(symbol_entry.key));

    for (const auto& venue_entry : lock->positions) {
      KJ_IF_SOME(pos, venue_entry.value.find(symbol_entry.key)) {
        ExchangePosition copy;
        copy.venue = pos.venue;
        copy.symbol = pos.symbol;
        copy.quantity = pos.quantity;
        copy.avg_entry_price = pos.avg_entry_price;
        copy.unrealized_pnl = pos.unrealized_pnl;
        copy.realized_pnl = pos.realized_pnl;
        copy.last_update_ns = pos.last_update_ns;
        agg.venues.add(kj::mv(copy));

        agg.total_quantity += pos.quantity;
        agg.total_unrealized_pnl += pos.unrealized_pnl;
        agg.total_realized_pnl += pos.realized_pnl;
      }
    }

    // Calculate weighted average price
    double total_value = 0.0;
    double total_abs_qty = 0.0;
    for (const auto& pos : agg.venues) {
      total_value += std::abs(pos.quantity) * pos.avg_entry_price;
      total_abs_qty += std::abs(pos.quantity);
    }
    if (total_abs_qty > 1e-10) {
      agg.weighted_avg_price = total_value / total_abs_qty;
    }

    result.add(kj::mv(agg));
  }

  return result;
}

double PositionAggregator::get_total_unrealized_pnl() const {
  auto lock = guarded_.lockExclusive();
  double total = 0.0;

  for (const auto& venue_entry : lock->positions) {
    for (const auto& pos_entry : venue_entry.value) {
      total += pos_entry.value.unrealized_pnl;
    }
  }

  return total;
}

double PositionAggregator::get_total_realized_pnl() const {
  auto lock = guarded_.lockExclusive();
  double total = 0.0;

  for (const auto& venue_entry : lock->positions) {
    for (const auto& pos_entry : venue_entry.value) {
      total += pos_entry.value.realized_pnl;
    }
  }

  return total;
}

void PositionAggregator::reconcile_position(veloz::common::Venue venue,
                                            const veloz::common::SymbolId& symbol,
                                            double exchange_quantity) {
  auto lock = guarded_.lockExclusive();

  double our_quantity = 0.0;
  KJ_IF_SOME(venue_map, lock->positions.find(venue)) {
    KJ_IF_SOME(pos, venue_map.find(symbol.value)) {
      our_quantity = pos.quantity;
    }
  }

  // Check for discrepancy
  if (std::abs(our_quantity - exchange_quantity) > 1e-8) {
    PositionDiscrepancy disc;
    disc.venue = venue;
    disc.symbol = symbol;
    disc.expected_quantity = our_quantity;
    disc.actual_quantity = exchange_quantity;
    // Note: timestamp would need to be passed in or obtained from a clock
    disc.detected_at_ns = 0;

    lock->discrepancies.add(kj::mv(disc));

    // Trigger callback if set
    KJ_IF_SOME(callback, lock->discrepancy_callback) {
      // Make a copy for the callback
      PositionDiscrepancy disc_copy;
      disc_copy.venue = venue;
      disc_copy.symbol = symbol;
      disc_copy.expected_quantity = our_quantity;
      disc_copy.actual_quantity = exchange_quantity;
      disc_copy.detected_at_ns = 0;
      callback(disc_copy);
    }
  }
}

kj::Vector<PositionDiscrepancy> PositionAggregator::get_discrepancies() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<PositionDiscrepancy> result;

  for (const auto& disc : lock->discrepancies) {
    PositionDiscrepancy copy;
    copy.venue = disc.venue;
    copy.symbol = disc.symbol;
    copy.expected_quantity = disc.expected_quantity;
    copy.actual_quantity = disc.actual_quantity;
    copy.detected_at_ns = disc.detected_at_ns;
    result.add(kj::mv(copy));
  }

  return result;
}

void PositionAggregator::clear_discrepancies() {
  auto lock = guarded_.lockExclusive();
  lock->discrepancies.clear();
}

void PositionAggregator::set_discrepancy_callback(DiscrepancyCallback callback) {
  auto lock = guarded_.lockExclusive();
  lock->discrepancy_callback = kj::mv(callback);
}

void PositionAggregator::clear_venue(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->positions.erase(venue);
}

void PositionAggregator::clear_all() {
  auto lock = guarded_.lockExclusive();
  lock->positions.clear();
}

} // namespace veloz::exec
