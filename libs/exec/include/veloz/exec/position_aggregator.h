#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Position for a single symbol on a single exchange
struct ExchangePosition {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  veloz::common::SymbolId symbol;
  double quantity{0.0};           // Positive = long, negative = short
  double avg_entry_price{0.0};    // Average entry price
  double unrealized_pnl{0.0};     // Unrealized P&L
  double realized_pnl{0.0};       // Realized P&L
  std::int64_t last_update_ns{0}; // Last update timestamp
};

// Aggregated position across all exchanges for a symbol
struct AggregatedPosition {
  veloz::common::SymbolId symbol;
  double total_quantity{0.0};          // Net position across all exchanges
  double weighted_avg_price{0.0};      // Weighted average entry price
  double total_unrealized_pnl{0.0};    // Total unrealized P&L
  double total_realized_pnl{0.0};      // Total realized P&L
  kj::Vector<ExchangePosition> venues; // Per-venue breakdown
};

// Position discrepancy detected during reconciliation
struct PositionDiscrepancy {
  veloz::common::Venue venue;
  veloz::common::SymbolId symbol;
  double expected_quantity{0.0};
  double actual_quantity{0.0};
  std::int64_t detected_at_ns{0};
};

// Callback for position discrepancy alerts
using DiscrepancyCallback = kj::Function<void(const PositionDiscrepancy&)>;

// PositionAggregator maintains positions per exchange and provides consolidated views
class PositionAggregator final {
public:
  PositionAggregator() = default;

  // Update position from execution report (fill)
  void on_fill(veloz::common::Venue venue, const ExecutionReport& report, OrderSide side,
               double fill_qty, double fill_price);

  // Set position directly (for reconciliation with exchange)
  void set_position(veloz::common::Venue venue, const veloz::common::SymbolId& symbol,
                    double quantity, double avg_price);

  // Update unrealized P&L with current market price
  void update_mark_price(const veloz::common::SymbolId& symbol, double mark_price);

  // Get position for a specific venue and symbol
  [[nodiscard]] kj::Maybe<ExchangePosition>
  get_position(veloz::common::Venue venue, const veloz::common::SymbolId& symbol) const;

  // Get aggregated position across all venues for a symbol
  [[nodiscard]] kj::Maybe<AggregatedPosition>
  get_aggregated_position(const veloz::common::SymbolId& symbol) const;

  // Get all positions for a venue
  [[nodiscard]] kj::Vector<ExchangePosition>
  get_venue_positions(veloz::common::Venue venue) const;

  // Get all aggregated positions
  [[nodiscard]] kj::Vector<AggregatedPosition> get_all_positions() const;

  // Get total P&L across all venues
  [[nodiscard]] double get_total_unrealized_pnl() const;
  [[nodiscard]] double get_total_realized_pnl() const;

  // Reconciliation
  void reconcile_position(veloz::common::Venue venue, const veloz::common::SymbolId& symbol,
                          double exchange_quantity);

  // Get all discrepancies
  [[nodiscard]] kj::Vector<PositionDiscrepancy> get_discrepancies() const;

  // Clear discrepancies
  void clear_discrepancies();

  // Set discrepancy callback
  void set_discrepancy_callback(DiscrepancyCallback callback);

  // Clear all positions for a venue
  void clear_venue(veloz::common::Venue venue);

  // Clear all positions
  void clear_all();

private:
  // Key for position lookup: venue + symbol
  struct PositionKey {
    veloz::common::Venue venue;
    kj::String symbol;

    bool operator==(const PositionKey& other) const {
      return venue == other.venue && symbol == other.symbol;
    }
  };

  struct PositionKeyHasher {
    std::size_t operator()(const PositionKey& key) const {
      // Simple hash combining venue and symbol
      return static_cast<std::size_t>(key.venue) * 31 +
             kj::hashCode(key.symbol.asPtr());
    }
  };

  struct AggregatorState {
    kj::HashMap<veloz::common::Venue,
                kj::HashMap<kj::String, ExchangePosition>>
        positions;
    kj::Vector<PositionDiscrepancy> discrepancies;
    kj::Maybe<DiscrepancyCallback> discrepancy_callback;
  };

  kj::MutexGuarded<AggregatorState> guarded_;
};

} // namespace veloz::exec
