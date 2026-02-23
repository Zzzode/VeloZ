#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/aggregated_order_book.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/latency_tracker.h"
#include "veloz/exec/order_api.h"
#include "veloz/exec/position_aggregator.h"

#include <kj/async.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Exchange status for health monitoring
struct ExchangeStatus {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  bool is_connected{false};
  bool is_healthy{false};
  kj::Maybe<LatencyStats> latency_stats;
  std::int64_t last_heartbeat_ns{0};
  kj::String status_message;
};

// Routing decision with rationale
struct RoutingDecision {
  veloz::common::Venue selected_venue{veloz::common::Venue::Unknown};
  double expected_price{0.0};
  kj::Duration expected_latency{0 * kj::NANOSECONDS};
  kj::String rationale;
  kj::Vector<veloz::common::Venue> fallback_venues;
};

// Routing strategy configuration
enum class RoutingStrategy : std::uint8_t {
  BestPrice = 0,      // Route to venue with best price
  LowestLatency = 1,  // Route to venue with lowest latency
  Balanced = 2,       // Balance between price and latency
  RoundRobin = 3,     // Distribute across venues
  WeightedRandom = 4, // Random with configurable weights
};

// Callback for execution reports from any venue
using ExecutionCallback = kj::Function<void(veloz::common::Venue, const ExecutionReport&)>;

// Callback for venue status changes
using StatusCallback = kj::Function<void(const ExchangeStatus&)>;

// ExchangeCoordinator manages multiple exchange adapters with intelligent routing
class ExchangeCoordinator final {
public:
  ExchangeCoordinator() = default;

  // Adapter management
  void register_adapter(veloz::common::Venue venue, kj::Own<ExchangeAdapter> adapter);
  void unregister_adapter(veloz::common::Venue venue);
  [[nodiscard]] bool has_adapter(veloz::common::Venue venue) const;
  [[nodiscard]] kj::Maybe<ExchangeAdapter&> get_adapter(veloz::common::Venue venue) const;
  [[nodiscard]] kj::Array<veloz::common::Venue> get_registered_venues() const;

  // Order routing with intelligent venue selection
  RoutingDecision select_venue(const veloz::common::SymbolId& symbol, OrderSide side,
                               double quantity);

  // Place order with automatic venue selection
  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req);

  // Place order on specific venue
  kj::Maybe<ExecutionReport> place_order(veloz::common::Venue venue, const PlaceOrderRequest& req);

  // Cancel order on specific venue
  kj::Maybe<ExecutionReport> cancel_order(veloz::common::Venue venue,
                                          const CancelOrderRequest& req);

  // Order book management
  void update_order_book(veloz::common::Venue venue, const veloz::common::SymbolId& symbol,
                         const veloz::market::BookData& book, std::int64_t timestamp_ns);

  void update_bbo(veloz::common::Venue venue, const veloz::common::SymbolId& symbol,
                  double bid_price, double bid_qty, double ask_price, double ask_qty,
                  std::int64_t timestamp_ns);

  [[nodiscard]] kj::Maybe<AggregatedBBO>
  get_aggregated_bbo(const veloz::common::SymbolId& symbol) const;

  [[nodiscard]] kj::Maybe<AggregatedOrderBook*>
  get_aggregated_book(const veloz::common::SymbolId& symbol) const;

  // Latency tracking
  void record_latency(veloz::common::Venue venue, kj::Duration latency, kj::TimePoint timestamp);
  [[nodiscard]] kj::Maybe<LatencyStats> get_latency_stats(veloz::common::Venue venue) const;
  [[nodiscard]] kj::Array<veloz::common::Venue> get_venues_by_latency() const;

  // Position management
  void on_fill(veloz::common::Venue venue, const ExecutionReport& report, OrderSide side);
  [[nodiscard]] kj::Maybe<AggregatedPosition>
  get_position(const veloz::common::SymbolId& symbol) const;
  [[nodiscard]] kj::Vector<AggregatedPosition> get_all_positions() const;
  [[nodiscard]] double get_total_pnl() const;

  // Exchange status
  [[nodiscard]] ExchangeStatus get_exchange_status(veloz::common::Venue venue) const;
  [[nodiscard]] kj::Vector<ExchangeStatus> get_all_exchange_status() const;

  // Configuration
  void set_routing_strategy(RoutingStrategy strategy);
  [[nodiscard]] RoutingStrategy routing_strategy() const;

  void set_default_venue(veloz::common::Venue venue);
  [[nodiscard]] kj::Maybe<veloz::common::Venue> default_venue() const;

  void set_latency_weight(double weight); // For balanced routing (0.0-1.0)
  [[nodiscard]] double latency_weight() const;

  void set_venue_weight(veloz::common::Venue venue, double weight);
  [[nodiscard]] double get_venue_weight(veloz::common::Venue venue) const;

  // Callbacks
  void set_execution_callback(ExecutionCallback callback);
  void set_status_callback(StatusCallback callback);

  // Health check
  void check_health(std::int64_t current_time_ns);

  // Symbol normalization (exchange-specific quirks)
  [[nodiscard]] kj::String normalize_symbol(veloz::common::Venue venue,
                                            const veloz::common::SymbolId& symbol) const;

private:
  struct CoordinatorState {
    kj::HashMap<veloz::common::Venue, kj::Own<ExchangeAdapter>> adapters;
    kj::HashMap<kj::String, kj::Own<AggregatedOrderBook>> order_books;
    LatencyTracker latency_tracker;
    PositionAggregator position_aggregator;

    RoutingStrategy routing_strategy{RoutingStrategy::BestPrice};
    kj::Maybe<veloz::common::Venue> default_venue;
    double latency_weight{0.3}; // For balanced routing
    kj::HashMap<veloz::common::Venue, double> venue_weights;

    kj::Maybe<ExecutionCallback> execution_callback;
    kj::Maybe<StatusCallback> status_callback;

    std::size_t round_robin_index{0};
  };

  kj::MutexGuarded<CoordinatorState> guarded_;

  // Internal helpers
  RoutingDecision select_by_best_price(const CoordinatorState& state,
                                       const veloz::common::SymbolId& symbol, OrderSide side,
                                       double quantity) const;

  RoutingDecision select_by_lowest_latency(const CoordinatorState& state,
                                           const veloz::common::SymbolId& symbol) const;

  RoutingDecision select_balanced(const CoordinatorState& state,
                                  const veloz::common::SymbolId& symbol, OrderSide side,
                                  double quantity) const;

  RoutingDecision select_round_robin(CoordinatorState& state) const;

  RoutingDecision select_weighted_random(const CoordinatorState& state) const;
};

} // namespace veloz::exec
