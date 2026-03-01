#include "veloz/exec/exchange_coordinator.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace veloz::exec {

void ExchangeCoordinator::register_adapter(veloz::common::Venue venue,
                                           kj::Own<ExchangeAdapter> adapter) {
  auto lock = guarded_.lockExclusive();
  lock->adapters.upsert(
      venue, kj::mv(adapter),
      [](kj::Own<ExchangeAdapter>& existing, kj::Own<ExchangeAdapter>&& replacement) {
        existing = kj::mv(replacement);
      });
}

void ExchangeCoordinator::unregister_adapter(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->adapters.erase(venue);
}

bool ExchangeCoordinator::has_adapter(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  return lock->adapters.find(venue) != kj::none;
}

kj::Maybe<ExchangeAdapter&> ExchangeCoordinator::get_adapter(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(adapter, lock->adapters.find(venue)) {
    return *adapter;
  }
  return kj::none;
}

kj::Array<veloz::common::Venue> ExchangeCoordinator::get_registered_venues() const {
  auto lock = guarded_.lockExclusive();
  auto builder = kj::heapArrayBuilder<veloz::common::Venue>(lock->adapters.size());
  for (const auto& entry : lock->adapters) {
    builder.add(entry.key);
  }
  return builder.finish();
}

RoutingDecision ExchangeCoordinator::select_venue(const veloz::common::SymbolId& symbol,
                                                  OrderSide side, double quantity) {
  auto lock = guarded_.lockExclusive();

  switch (lock->routing_strategy) {
  case RoutingStrategy::BestPrice:
    return select_by_best_price(*lock, symbol, side, quantity);
  case RoutingStrategy::LowestLatency:
    return select_by_lowest_latency(*lock, symbol);
  case RoutingStrategy::Balanced:
    return select_balanced(*lock, symbol, side, quantity);
  case RoutingStrategy::RoundRobin:
    return select_round_robin(*lock);
  case RoutingStrategy::WeightedRandom:
    return select_weighted_random(*lock);
  default:
    return select_by_best_price(*lock, symbol, side, quantity);
  }
}

kj::Maybe<ExecutionReport> ExchangeCoordinator::place_order(const PlaceOrderRequest& req) {
  // Select venue based on routing strategy
  auto decision = select_venue(req.symbol, req.side, req.qty);

  if (decision.selected_venue == veloz::common::Venue::Unknown) {
    return kj::none;
  }

  return place_order(decision.selected_venue, req);
}

kj::Maybe<ExecutionReport> ExchangeCoordinator::place_order(veloz::common::Venue venue,
                                                            const PlaceOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(adapter, lock->adapters.find(venue)) {
    auto start_time = kj::systemPreciseMonotonicClock().now();
    auto result = adapter->place_order(req);
    auto end_time = kj::systemPreciseMonotonicClock().now();

    // Record latency
    lock->latency_tracker.record_latency(venue, end_time - start_time, end_time);

    // Handle fill and callback
    KJ_IF_SOME(report, result) {
      if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
        lock->position_aggregator.on_fill(venue, report, req.side, report.last_fill_qty,
                                          report.last_fill_price);
      }

      KJ_IF_SOME(callback, lock->execution_callback) {
        callback(venue, report);
      }
    }

    return result;
  }

  return kj::none;
}

kj::Maybe<ExecutionReport> ExchangeCoordinator::cancel_order(veloz::common::Venue venue,
                                                             const CancelOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(adapter, lock->adapters.find(venue)) {
    auto start_time = kj::systemPreciseMonotonicClock().now();
    auto result = adapter->cancel_order(req);
    auto end_time = kj::systemPreciseMonotonicClock().now();

    // Record latency
    lock->latency_tracker.record_latency(venue, end_time - start_time, end_time);

    KJ_IF_SOME(report, result) {
      KJ_IF_SOME(callback, lock->execution_callback) {
        callback(venue, report);
      }
    }

    return result;
  }

  return kj::none;
}

void ExchangeCoordinator::update_order_book(veloz::common::Venue venue,
                                            const veloz::common::SymbolId& symbol,
                                            const veloz::market::BookData& book,
                                            std::int64_t timestamp_ns) {
  auto lock = guarded_.lockExclusive();

  kj::String symbol_key = kj::heapString(symbol.value);

  // Get or create aggregated order book for symbol
  AggregatedOrderBook* agg_book = nullptr;
  KJ_IF_SOME(existing, lock->order_books.find(symbol_key)) {
    agg_book = existing.get();
  }
  else {
    auto new_book = kj::heap<AggregatedOrderBook>();
    agg_book = new_book.get();
    lock->order_books.insert(kj::heapString(symbol_key), kj::mv(new_book));
  }

  if (agg_book != nullptr) {
    agg_book->update_venue(venue, book, timestamp_ns);
  }
}

void ExchangeCoordinator::update_bbo(veloz::common::Venue venue,
                                     const veloz::common::SymbolId& symbol, double bid_price,
                                     double bid_qty, double ask_price, double ask_qty,
                                     std::int64_t timestamp_ns) {
  auto lock = guarded_.lockExclusive();

  kj::String symbol_key = kj::heapString(symbol.value);

  AggregatedOrderBook* agg_book = nullptr;
  KJ_IF_SOME(existing, lock->order_books.find(symbol_key)) {
    agg_book = existing.get();
  }
  else {
    auto new_book = kj::heap<AggregatedOrderBook>();
    agg_book = new_book.get();
    lock->order_books.insert(kj::heapString(symbol_key), kj::mv(new_book));
  }

  if (agg_book != nullptr) {
    agg_book->update_venue_bbo(venue, bid_price, bid_qty, ask_price, ask_qty, timestamp_ns);
  }
}

kj::Maybe<AggregatedBBO>
ExchangeCoordinator::get_aggregated_bbo(const veloz::common::SymbolId& symbol) const {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(book, lock->order_books.find(symbol.value)) {
    return book->get_aggregated_bbo();
  }

  return kj::none;
}

kj::Maybe<AggregatedOrderBook*>
ExchangeCoordinator::get_aggregated_book(const veloz::common::SymbolId& symbol) const {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(book, lock->order_books.find(symbol.value)) {
    return book.get();
  }

  return kj::none;
}

void ExchangeCoordinator::record_latency(veloz::common::Venue venue, kj::Duration latency,
                                         kj::TimePoint timestamp) {
  auto lock = guarded_.lockExclusive();
  lock->latency_tracker.record_latency(venue, latency, timestamp);
}

kj::Maybe<LatencyStats> ExchangeCoordinator::get_latency_stats(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  return lock->latency_tracker.get_stats(venue);
}

kj::Array<veloz::common::Venue> ExchangeCoordinator::get_venues_by_latency() const {
  auto lock = guarded_.lockExclusive();
  return lock->latency_tracker.get_venues_by_latency();
}

void ExchangeCoordinator::on_fill(veloz::common::Venue venue, const ExecutionReport& report,
                                  OrderSide side) {
  auto lock = guarded_.lockExclusive();
  lock->position_aggregator.on_fill(venue, report, side, report.last_fill_qty,
                                    report.last_fill_price);
}

kj::Maybe<AggregatedPosition>
ExchangeCoordinator::get_position(const veloz::common::SymbolId& symbol) const {
  auto lock = guarded_.lockExclusive();
  return lock->position_aggregator.get_aggregated_position(symbol);
}

kj::Vector<AggregatedPosition> ExchangeCoordinator::get_all_positions() const {
  auto lock = guarded_.lockExclusive();
  return lock->position_aggregator.get_all_positions();
}

double ExchangeCoordinator::get_total_pnl() const {
  auto lock = guarded_.lockExclusive();
  return lock->position_aggregator.get_total_unrealized_pnl() +
         lock->position_aggregator.get_total_realized_pnl();
}

ExchangeStatus ExchangeCoordinator::get_exchange_status(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();

  ExchangeStatus status;
  status.venue = venue;

  KJ_IF_SOME(adapter, lock->adapters.find(venue)) {
    status.is_connected = adapter->is_connected();
    status.latency_stats = lock->latency_tracker.get_stats(venue);

    // Check health based on latency
    status.is_healthy = lock->latency_tracker.is_healthy(venue, 1 * kj::SECONDS, 30 * kj::SECONDS);

    status.status_message =
        status.is_healthy ? kj::heapString("OK"_kj) : kj::heapString("Degraded"_kj);
  }
  else {
    status.is_connected = false;
    status.is_healthy = false;
    status.status_message = kj::heapString("Not registered"_kj);
  }

  return status;
}

kj::Vector<ExchangeStatus> ExchangeCoordinator::get_all_exchange_status() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<ExchangeStatus> result;

  for (const auto& entry : lock->adapters) {
    ExchangeStatus status;
    status.venue = entry.key;
    status.is_connected = entry.value->is_connected();
    status.latency_stats = lock->latency_tracker.get_stats(entry.key);
    status.is_healthy =
        lock->latency_tracker.is_healthy(entry.key, 1 * kj::SECONDS, 30 * kj::SECONDS);
    status.status_message =
        status.is_healthy ? kj::heapString("OK"_kj) : kj::heapString("Degraded"_kj);
    result.add(kj::mv(status));
  }

  return result;
}

void ExchangeCoordinator::set_routing_strategy(RoutingStrategy strategy) {
  auto lock = guarded_.lockExclusive();
  lock->routing_strategy = strategy;
}

RoutingStrategy ExchangeCoordinator::routing_strategy() const {
  return guarded_.lockExclusive()->routing_strategy;
}

void ExchangeCoordinator::set_default_venue(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->default_venue = venue;
}

kj::Maybe<veloz::common::Venue> ExchangeCoordinator::default_venue() const {
  return guarded_.lockExclusive()->default_venue;
}

void ExchangeCoordinator::set_latency_weight(double weight) {
  auto lock = guarded_.lockExclusive();
  lock->latency_weight = std::clamp(weight, 0.0, 1.0);
}

double ExchangeCoordinator::latency_weight() const {
  return guarded_.lockExclusive()->latency_weight;
}

void ExchangeCoordinator::set_venue_weight(veloz::common::Venue venue, double weight) {
  auto lock = guarded_.lockExclusive();
  lock->venue_weights.upsert(
      venue, weight, [](double& existing, double&& replacement) { existing = replacement; });
}

double ExchangeCoordinator::get_venue_weight(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(weight, lock->venue_weights.find(venue)) {
    return weight;
  }
  return 1.0; // Default weight
}

void ExchangeCoordinator::set_execution_callback(ExecutionCallback callback) {
  auto lock = guarded_.lockExclusive();
  lock->execution_callback = kj::mv(callback);
}

void ExchangeCoordinator::set_status_callback(StatusCallback callback) {
  auto lock = guarded_.lockExclusive();
  lock->status_callback = kj::mv(callback);
}

void ExchangeCoordinator::check_health(std::int64_t current_time_ns) {
  auto lock = guarded_.lockExclusive();

  // Check staleness of order books
  for (auto& entry : lock->order_books) {
    entry.value->check_staleness(current_time_ns);
  }

  // Notify status changes
  KJ_IF_SOME(callback, lock->status_callback) {
    for (const auto& adapter_entry : lock->adapters) {
      ExchangeStatus status;
      status.venue = adapter_entry.key;
      status.is_connected = adapter_entry.value->is_connected();
      status.latency_stats = lock->latency_tracker.get_stats(adapter_entry.key);
      status.is_healthy =
          lock->latency_tracker.is_healthy(adapter_entry.key, 1 * kj::SECONDS, 30 * kj::SECONDS);
      status.status_message =
          status.is_healthy ? kj::heapString("OK"_kj) : kj::heapString("Degraded"_kj);
      callback(status);
    }
  }
}

kj::String ExchangeCoordinator::normalize_symbol(veloz::common::Venue venue,
                                                 const veloz::common::SymbolId& symbol) const {
  // Handle exchange-specific symbol formats
  switch (venue) {
  case veloz::common::Venue::Binance:
    // Binance uses BTCUSDT format (no separator)
    return kj::heapString(symbol.value);

  case veloz::common::Venue::Okx:
    // OKX uses BTC-USDT format (hyphen separator)
    // Simple conversion: insert hyphen before last 4 chars if it looks like a pair
    if (symbol.value.size() > 4) {
      auto base = symbol.value.slice(0, symbol.value.size() - 4);
      auto quote = symbol.value.slice(symbol.value.size() - 4);
      return kj::str(base, "-", quote);
    }
    return kj::heapString(symbol.value);

  case veloz::common::Venue::Bybit:
    // Bybit uses BTCUSDT format (same as Binance)
    return kj::heapString(symbol.value);

  default:
    return kj::heapString(symbol.value);
  }
}

// Private routing implementations

RoutingDecision ExchangeCoordinator::select_by_best_price(const CoordinatorState& state,
                                                          const veloz::common::SymbolId& symbol,
                                                          OrderSide side, double quantity) const {
  RoutingDecision decision;
  decision.rationale = kj::heapString("Best price routing"_kj);

  KJ_IF_SOME(book, state.order_books.find(symbol.value)) {
    auto bbo = book->get_aggregated_bbo();

    if (side == OrderSide::Buy) {
      // For buy orders, we want the lowest ask
      if (bbo.best_ask_venue != veloz::common::Venue::Unknown) {
        decision.selected_venue = bbo.best_ask_venue;
        decision.expected_price = bbo.best_ask_price;
      }
    } else {
      // For sell orders, we want the highest bid
      if (bbo.best_bid_venue != veloz::common::Venue::Unknown) {
        decision.selected_venue = bbo.best_bid_venue;
        decision.expected_price = bbo.best_bid_price;
      }
    }

    // Add fallback venues
    for (const auto& venue_bbo : bbo.venues) {
      if (venue_bbo.venue != decision.selected_venue && !venue_bbo.is_stale) {
        decision.fallback_venues.add(venue_bbo.venue);
      }
    }
  }

  // Fall back to default venue if no book data
  if (decision.selected_venue == veloz::common::Venue::Unknown) {
    KJ_IF_SOME(default_v, state.default_venue) {
      decision.selected_venue = default_v;
      decision.rationale = kj::heapString("Default venue (no book data)"_kj);
    }
  }

  return decision;
}

RoutingDecision
ExchangeCoordinator::select_by_lowest_latency(const CoordinatorState& state,
                                              const veloz::common::SymbolId& symbol) const {
  RoutingDecision decision;
  decision.rationale = kj::heapString("Lowest latency routing"_kj);

  auto venues_by_latency = state.latency_tracker.get_venues_by_latency();

  for (const auto& venue : venues_by_latency) {
    // Check if adapter exists and is connected
    KJ_IF_SOME(adapter, state.adapters.find(venue)) {
      if (adapter->is_connected()) {
        if (decision.selected_venue == veloz::common::Venue::Unknown) {
          decision.selected_venue = venue;
          KJ_IF_SOME(latency, state.latency_tracker.get_expected_latency(venue)) {
            decision.expected_latency = latency;
          }
        } else {
          decision.fallback_venues.add(venue);
        }
      }
    }
  }

  // Fall back to default venue
  if (decision.selected_venue == veloz::common::Venue::Unknown) {
    KJ_IF_SOME(default_v, state.default_venue) {
      decision.selected_venue = default_v;
      decision.rationale = kj::heapString("Default venue (no latency data)"_kj);
    }
  }

  return decision;
}

RoutingDecision ExchangeCoordinator::select_balanced(const CoordinatorState& state,
                                                     const veloz::common::SymbolId& symbol,
                                                     OrderSide side, double quantity) const {
  RoutingDecision decision;
  decision.rationale = kj::heapString("Balanced routing (price + latency)"_kj);

  // Score each venue based on price and latency
  kj::Vector<std::pair<veloz::common::Venue, double>> venue_scores;

  KJ_IF_SOME(book, state.order_books.find(symbol.value)) {
    auto bbo = book->get_aggregated_bbo();

    // Find best price for normalization
    double best_price = (side == OrderSide::Buy) ? bbo.best_ask_price : bbo.best_bid_price;

    // Find best latency for normalization
    kj::Duration best_latency = kj::maxValue;
    for (const auto& venue_bbo : bbo.venues) {
      KJ_IF_SOME(latency, state.latency_tracker.get_expected_latency(venue_bbo.venue)) {
        if (latency < best_latency) {
          best_latency = latency;
        }
      }
    }

    // Score each venue
    for (const auto& venue_bbo : bbo.venues) {
      if (venue_bbo.is_stale) {
        continue;
      }

      double price = (side == OrderSide::Buy) ? venue_bbo.ask_price : venue_bbo.bid_price;
      if (price <= 0) {
        continue;
      }

      // Price score (lower is better for buy, higher is better for sell)
      double price_score = 0.0;
      if (best_price > 0) {
        if (side == OrderSide::Buy) {
          price_score = best_price / price; // Higher when price is lower
        } else {
          price_score = price / best_price; // Higher when price is higher
        }
      }

      // Latency score
      double latency_score = 0.0;
      KJ_IF_SOME(latency, state.latency_tracker.get_expected_latency(venue_bbo.venue)) {
        if (best_latency > 0 * kj::NANOSECONDS) {
          latency_score = static_cast<double>(best_latency / kj::NANOSECONDS) /
                          static_cast<double>(latency / kj::NANOSECONDS);
        }
      }

      // Combined score
      double combined_score =
          (1.0 - state.latency_weight) * price_score + state.latency_weight * latency_score;

      venue_scores.add(std::make_pair(venue_bbo.venue, combined_score));
    }
  }

  // Sort by score descending
  std::sort(venue_scores.begin(), venue_scores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Select best venue
  if (!venue_scores.empty()) {
    decision.selected_venue = venue_scores[0].first;

    for (std::size_t i = 1; i < venue_scores.size(); ++i) {
      decision.fallback_venues.add(venue_scores[i].first);
    }
  }

  // Fall back to default venue
  if (decision.selected_venue == veloz::common::Venue::Unknown) {
    KJ_IF_SOME(default_v, state.default_venue) {
      decision.selected_venue = default_v;
      decision.rationale = kj::heapString("Default venue (no scoring data)"_kj);
    }
  }

  return decision;
}

RoutingDecision ExchangeCoordinator::select_round_robin(CoordinatorState& state) const {
  RoutingDecision decision;
  decision.rationale = kj::heapString("Round-robin routing"_kj);

  kj::Vector<veloz::common::Venue> connected_venues;
  for (const auto& entry : state.adapters) {
    if (entry.value->is_connected()) {
      connected_venues.add(entry.key);
    }
  }

  if (!connected_venues.empty()) {
    state.round_robin_index = (state.round_robin_index + 1) % connected_venues.size();
    decision.selected_venue = connected_venues[state.round_robin_index];

    for (std::size_t i = 0; i < connected_venues.size(); ++i) {
      if (i != state.round_robin_index) {
        decision.fallback_venues.add(connected_venues[i]);
      }
    }
  }

  return decision;
}

RoutingDecision ExchangeCoordinator::select_weighted_random(const CoordinatorState& state) const {
  RoutingDecision decision;
  decision.rationale = kj::heapString("Weighted random routing"_kj);

  // Collect connected venues with weights
  kj::Vector<std::pair<veloz::common::Venue, double>> venue_weights;
  double total_weight = 0.0;

  for (const auto& entry : state.adapters) {
    if (entry.value->is_connected()) {
      double weight = 1.0;
      KJ_IF_SOME(w, state.venue_weights.find(entry.key)) {
        weight = w;
      }
      venue_weights.add(std::make_pair(entry.key, weight));
      total_weight += weight;
    }
  }

  if (venue_weights.empty() || total_weight <= 0) {
    return decision;
  }

  // Random selection based on weights
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<double> dist(0.0, total_weight);
  double random_value = dist(gen);

  double cumulative = 0.0;
  for (const auto& vw : venue_weights) {
    cumulative += vw.second;
    if (random_value <= cumulative) {
      decision.selected_venue = vw.first;
      break;
    }
  }

  // Add fallbacks
  for (const auto& vw : venue_weights) {
    if (vw.first != decision.selected_venue) {
      decision.fallback_venues.add(vw.first);
    }
  }

  return decision;
}

} // namespace veloz::exec
