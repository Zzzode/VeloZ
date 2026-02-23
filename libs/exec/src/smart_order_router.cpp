#include "veloz/exec/smart_order_router.h"

#include <algorithm>
#include <cmath>
#include <kj/debug.h>

namespace veloz::exec {

SmartOrderRouter::SmartOrderRouter(ExchangeCoordinator& coordinator)
    : coordinator_(coordinator) {}

void SmartOrderRouter::set_fees(veloz::common::Venue venue, const ExchangeFees& fees) {
  auto lock = guarded_.lockExclusive();
  lock->fees.upsert(venue, fees,
                    [](ExchangeFees& existing, ExchangeFees&& replacement) {
                      existing = replacement;
                    });
}

kj::Maybe<ExchangeFees> SmartOrderRouter::get_fees(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(fees, lock->fees.find(venue)) {
    return fees;
  }
  return kj::none;
}

kj::Vector<RoutingScore>
SmartOrderRouter::score_venues(const veloz::common::SymbolId& symbol, OrderSide side,
                               double quantity) const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<RoutingScore> scores;

  auto venues = coordinator_.get_registered_venues();
  if (venues.size() == 0) {
    return scores;
  }

  // Get aggregated BBO for price comparison
  auto bbo_maybe = coordinator_.get_aggregated_bbo(symbol);
  if (bbo_maybe == kj::none) {
    return scores;
  }

  // Use reference to avoid copy
  auto& bbo = KJ_ASSERT_NONNULL(bbo_maybe);
  double reference_price = (side == OrderSide::Buy) ? bbo.best_ask_price : bbo.best_bid_price;

  if (reference_price <= 0) {
    return scores;
  }

  // Calculate scores for each venue
  for (const auto& venue : venues) {
    RoutingScore score;
    score.venue = venue;

    // Get venue BBO - reuse the same bbo since it's aggregated
    double venue_price = 0.0;
    for (const auto& v : bbo.venues) {
      if (v.venue == venue && !v.is_stale) {
        venue_price = (side == OrderSide::Buy) ? v.ask_price : v.bid_price;
        break;
      }
    }

    if (venue_price <= 0) {
      continue;
    }

    // Price score (0-1, higher is better)
    if (side == OrderSide::Buy) {
      // For buy, lower price is better
      score.price_score = reference_price / venue_price;
    } else {
      // For sell, higher price is better
      score.price_score = venue_price / reference_price;
    }
    score.price_score = std::clamp(score.price_score, 0.0, 1.0);

    // Fee score (0-1, lower fees = higher score)
    double fee_rate = 0.001; // Default
    KJ_IF_SOME(fees, lock->fees.find(venue)) {
      fee_rate = fees.taker_fee; // Assume taker for now
    }
    score.fee_score = std::max(0.0, 1.0 - fee_rate * 100); // Scale fees

    // Latency score
    KJ_IF_SOME(latency_stats, coordinator_.get_latency_stats(venue)) {
      // Lower latency = higher score
      double latency_ms = static_cast<double>(latency_stats.p50 / kj::MILLISECONDS);
      score.latency_score = std::max(0.0, 1.0 - latency_ms / 100.0);
    }
    else {
      score.latency_score = 0.5; // Unknown latency
    }

    // Liquidity score (doesn't need lock - only calls coordinator)
    score.liquidity_score = calculate_liquidity_score(venue, symbol, quantity, side);

    // Reliability score (use locked version to avoid deadlock)
    score.reliability_score = calculate_reliability_score_locked(*lock, venue);

    // Calculate total weighted score
    score.total_score = lock->price_weight * score.price_score +
                        lock->fee_weight * score.fee_score +
                        lock->latency_weight * score.latency_score +
                        lock->liquidity_weight * score.liquidity_score +
                        lock->reliability_weight * score.reliability_score;

    // Generate explanation
    score.explanation = kj::str(
        "Price: ", static_cast<int>(score.price_score * 100), "%, ",
        "Fee: ", static_cast<int>(score.fee_score * 100), "%, ",
        "Latency: ", static_cast<int>(score.latency_score * 100), "%, ",
        "Liquidity: ", static_cast<int>(score.liquidity_score * 100), "%, ",
        "Reliability: ", static_cast<int>(score.reliability_score * 100), "%");

    scores.add(kj::mv(score));
  }

  // Sort by total score descending
  std::sort(scores.begin(), scores.end(),
            [](const RoutingScore& a, const RoutingScore& b) {
              return a.total_score > b.total_score;
            });

  return scores;
}

RoutingDecision SmartOrderRouter::route_order(const PlaceOrderRequest& req) const {
  auto scores = score_venues(req.symbol, req.side, req.qty);

  RoutingDecision decision;
  decision.rationale = kj::heapString("Smart routing"_kj);

  if (!scores.empty()) {
    decision.selected_venue = scores[0].venue;

    // Get expected price
    auto bbo_maybe = coordinator_.get_aggregated_bbo(req.symbol);
    KJ_IF_SOME(bbo, bbo_maybe) {
      for (const auto& v : bbo.venues) {
        if (v.venue == decision.selected_venue) {
          decision.expected_price = (req.side == OrderSide::Buy) ? v.ask_price : v.bid_price;
          break;
        }
      }
    }

    // Add fallbacks
    for (std::size_t i = 1; i < scores.size(); ++i) {
      decision.fallback_venues.add(scores[i].venue);
    }

    decision.rationale = kj::str("Smart routing: ", scores[0].explanation);
  }

  return decision;
}

kj::Vector<OrderSplit>
SmartOrderRouter::split_order(const veloz::common::SymbolId& symbol, OrderSide side,
                              double quantity, double max_single_venue_pct) const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<OrderSplit> splits;

  auto scores = score_venues(symbol, side, quantity);
  if (scores.empty()) {
    return splits;
  }

  // Calculate how much to allocate to each venue
  double remaining = quantity;
  double max_per_venue = quantity * max_single_venue_pct;

  for (const auto& score : scores) {
    if (remaining <= 0) {
      break;
    }

    // Check minimum order size
    double min_size = 0.0;
    KJ_IF_SOME(size, lock->min_order_sizes.find(score.venue)) {
      min_size = size;
    }

    // Calculate allocation based on score
    double allocation = std::min(remaining, max_per_venue);

    // Check liquidity available
    auto bbo_maybe = coordinator_.get_aggregated_bbo(symbol);
    KJ_IF_SOME(bbo, bbo_maybe) {
      for (const auto& v : bbo.venues) {
        if (v.venue == score.venue && !v.is_stale) {
          double available = (side == OrderSide::Buy) ? v.ask_qty : v.bid_qty;
          allocation = std::min(allocation, available * 0.8); // Don't take all liquidity
          break;
        }
      }
    }

    if (allocation < min_size) {
      continue;
    }

    OrderSplit split;
    split.venue = score.venue;
    split.quantity = allocation;

    // Get expected price
    KJ_IF_SOME(bbo, bbo_maybe) {
      for (const auto& v : bbo.venues) {
        if (v.venue == score.venue) {
          split.expected_price = (side == OrderSide::Buy) ? v.ask_price : v.bid_price;
          break;
        }
      }
    }

    // Calculate expected fee
    KJ_IF_SOME(fees, lock->fees.find(score.venue)) {
      split.expected_fee = allocation * split.expected_price * fees.taker_fee;
    }

    splits.add(kj::mv(split));
    remaining -= allocation;
  }

  return splits;
}

kj::Maybe<ExecutionReport> SmartOrderRouter::execute(const PlaceOrderRequest& req) {
  auto decision = route_order(req);

  if (decision.selected_venue == veloz::common::Venue::Unknown) {
    return kj::none;
  }

  auto start_time = kj::systemPreciseMonotonicClock().now();
  auto result = coordinator_.place_order(decision.selected_venue, req);
  auto end_time = kj::systemPreciseMonotonicClock().now();

  // Record execution quality
  KJ_IF_SOME(report, result) {
    record_execution(decision.selected_venue, report, decision.expected_price,
                     end_time - start_time);
  }

  return result;
}

kj::Vector<kj::Maybe<ExecutionReport>>
SmartOrderRouter::execute_split(const veloz::common::SymbolId& symbol, OrderSide side,
                                double quantity, kj::StringPtr client_order_id_prefix) {
  kj::Vector<kj::Maybe<ExecutionReport>> results;

  auto splits = split_order(symbol, side, quantity);

  int split_index = 0;
  for (const auto& split : splits) {
    PlaceOrderRequest req;
    req.symbol = symbol;
    req.side = side;
    req.type = OrderType::Limit;
    req.qty = split.quantity;
    req.price = split.expected_price;
    req.client_order_id = kj::str(client_order_id_prefix, "-", split_index++);

    auto start_time = kj::systemPreciseMonotonicClock().now();
    auto result = coordinator_.place_order(split.venue, req);
    auto end_time = kj::systemPreciseMonotonicClock().now();

    KJ_IF_SOME(report, result) {
      record_execution(split.venue, report, split.expected_price, end_time - start_time);
    }

    results.add(kj::mv(result));
  }

  return results;
}

BatchOrderResult SmartOrderRouter::execute_batch(const BatchOrderRequest& batch) {
  BatchOrderResult result;

  for (const auto& req : batch.orders) {
    auto report = execute(req);

    KJ_IF_SOME(r, report) {
      if (r.status == OrderStatus::Filled || r.status == OrderStatus::PartiallyFilled ||
          r.status == OrderStatus::Accepted || r.status == OrderStatus::New) {
        result.success_count++;
      } else {
        result.failure_count++;
        if (batch.atomic) {
          // For atomic batch, stop on first failure
          break;
        }
      }
    }
    else {
      result.failure_count++;
      if (batch.atomic) {
        break;
      }
    }

    result.reports.add(kj::mv(report));
  }

  return result;
}

kj::Vector<kj::Maybe<ExecutionReport>>
SmartOrderRouter::cancel_merged(const CancelMergeRequest& req) {
  kj::Vector<kj::Maybe<ExecutionReport>> results;

  // For now, execute cancels sequentially
  // In a real implementation, this could batch cancels to the exchange API
  for (const auto& client_order_id : req.client_order_ids) {
    CancelOrderRequest cancel_req;
    cancel_req.symbol = req.symbol;
    cancel_req.client_order_id = kj::heapString(client_order_id);

    auto result = coordinator_.cancel_order(req.venue, cancel_req);
    results.add(kj::mv(result));
  }

  return results;
}

void SmartOrderRouter::record_execution(veloz::common::Venue venue,
                                        const ExecutionReport& report, double expected_price,
                                        kj::Duration execution_time) {
  auto lock = guarded_.lockExclusive();

  // Update venue quality
  VenueQuality* quality = nullptr;
  KJ_IF_SOME(existing, lock->quality.find(venue)) {
    quality = &existing;
  }
  else {
    lock->quality.insert(venue, VenueQuality{});
    KJ_IF_SOME(inserted, lock->quality.find(venue)) {
      quality = &inserted;
    }
  }

  if (quality != nullptr) {
    quality->sample_count++;
    quality->total_execution_time_ns += execution_time / kj::NANOSECONDS;

    if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
      quality->success_count++;

      // Calculate slippage
      if (expected_price > 0 && report.last_fill_price > 0) {
        double slippage = std::abs(report.last_fill_price - expected_price) / expected_price;
        quality->total_slippage += slippage;
      }

      // Fill rate (for partial fills)
      quality->total_fill_rate += 1.0; // Simplified
    } else if (report.status == OrderStatus::Rejected) {
      quality->failure_count++;
    }
  }

  // Update analytics
  lock->analytics.total_orders++;

  if (report.status == OrderStatus::Filled) {
    lock->analytics.filled_orders++;
    lock->analytics.total_volume += report.last_fill_qty * report.last_fill_price;
  } else if (report.status == OrderStatus::PartiallyFilled) {
    lock->analytics.partial_fills++;
    lock->analytics.total_volume += report.last_fill_qty * report.last_fill_price;
  } else if (report.status == OrderStatus::Rejected) {
    lock->analytics.rejected_orders++;
  }
}

kj::Maybe<ExecutionQuality>
SmartOrderRouter::get_venue_quality(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(quality, lock->quality.find(venue)) {
    if (quality.sample_count == 0) {
      return kj::none;
    }

    ExecutionQuality result;
    result.slippage = quality.total_slippage / static_cast<double>(quality.sample_count);
    result.fill_rate = quality.total_fill_rate / static_cast<double>(quality.sample_count);
    result.execution_time =
        (quality.total_execution_time_ns / static_cast<std::int64_t>(quality.sample_count)) *
        kj::NANOSECONDS;

    return result;
  }

  return kj::none;
}

ExecutionAnalytics SmartOrderRouter::get_analytics() const {
  auto lock = guarded_.lockExclusive();

  ExecutionAnalytics result = lock->analytics;

  // Calculate averages
  if (result.total_orders > 0) {
    // Calculate average slippage and fill rate from venue quality
    double total_slippage = 0.0;
    double total_fill_rate = 0.0;
    std::int64_t total_time_ns = 0;
    std::size_t sample_count = 0;

    for (const auto& entry : lock->quality) {
      if (entry.value.sample_count > 0) {
        total_slippage += entry.value.total_slippage;
        total_fill_rate += entry.value.total_fill_rate;
        total_time_ns += entry.value.total_execution_time_ns;
        sample_count += entry.value.sample_count;
      }
    }

    if (sample_count > 0) {
      result.average_slippage = total_slippage / static_cast<double>(sample_count);
      result.average_fill_rate = total_fill_rate / static_cast<double>(sample_count);
      result.average_execution_time =
          (total_time_ns / static_cast<std::int64_t>(sample_count)) * kj::NANOSECONDS;
    }
  }

  return result;
}

void SmartOrderRouter::reset_analytics() {
  auto lock = guarded_.lockExclusive();
  lock->analytics = ExecutionAnalytics{};
  lock->quality.clear();
}

void SmartOrderRouter::set_price_weight(double weight) {
  guarded_.lockExclusive()->price_weight = std::clamp(weight, 0.0, 1.0);
}

void SmartOrderRouter::set_fee_weight(double weight) {
  guarded_.lockExclusive()->fee_weight = std::clamp(weight, 0.0, 1.0);
}

void SmartOrderRouter::set_latency_weight(double weight) {
  guarded_.lockExclusive()->latency_weight = std::clamp(weight, 0.0, 1.0);
}

void SmartOrderRouter::set_liquidity_weight(double weight) {
  guarded_.lockExclusive()->liquidity_weight = std::clamp(weight, 0.0, 1.0);
}

void SmartOrderRouter::set_reliability_weight(double weight) {
  guarded_.lockExclusive()->reliability_weight = std::clamp(weight, 0.0, 1.0);
}

double SmartOrderRouter::price_weight() const {
  return guarded_.lockExclusive()->price_weight;
}

double SmartOrderRouter::fee_weight() const {
  return guarded_.lockExclusive()->fee_weight;
}

double SmartOrderRouter::latency_weight() const {
  return guarded_.lockExclusive()->latency_weight;
}

double SmartOrderRouter::liquidity_weight() const {
  return guarded_.lockExclusive()->liquidity_weight;
}

double SmartOrderRouter::reliability_weight() const {
  return guarded_.lockExclusive()->reliability_weight;
}

void SmartOrderRouter::set_min_order_size(veloz::common::Venue venue, double size) {
  auto lock = guarded_.lockExclusive();
  lock->min_order_sizes.upsert(venue, size,
                               [](double& existing, double&& replacement) {
                                 existing = replacement;
                               });
}

double SmartOrderRouter::get_min_order_size(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(size, lock->min_order_sizes.find(venue)) {
    return size;
  }
  return 0.0;
}

double SmartOrderRouter::calculate_effective_price(veloz::common::Venue venue, double price,
                                                   double quantity, OrderSide side) const {
  auto lock = guarded_.lockExclusive();

  double fee_rate = 0.001; // Default
  KJ_IF_SOME(fees, lock->fees.find(venue)) {
    fee_rate = fees.taker_fee;
  }

  // Effective price includes fees
  if (side == OrderSide::Buy) {
    return price * (1.0 + fee_rate);
  } else {
    return price * (1.0 - fee_rate);
  }
}

double SmartOrderRouter::calculate_liquidity_score(veloz::common::Venue venue,
                                                   const veloz::common::SymbolId& symbol,
                                                   double quantity, OrderSide side) const {
  auto bbo_maybe = coordinator_.get_aggregated_bbo(symbol);
  if (bbo_maybe == kj::none) {
    return 0.5; // Unknown
  }

  auto& bbo = KJ_ASSERT_NONNULL(bbo_maybe);

  for (const auto& v : bbo.venues) {
    if (v.venue == venue && !v.is_stale) {
      double available = (side == OrderSide::Buy) ? v.ask_qty : v.bid_qty;
      if (available <= 0) {
        return 0.0;
      }

      // Score based on how much of our order can be filled
      double fill_ratio = std::min(1.0, available / quantity);
      return fill_ratio;
    }
  }

  return 0.0;
}

// Internal helper that works with already-locked state
double SmartOrderRouter::calculate_reliability_score_locked(
    const RouterState& state, veloz::common::Venue venue) const {
  KJ_IF_SOME(quality, state.quality.find(venue)) {
    if (quality.sample_count == 0) {
      return 0.5; // Unknown
    }

    double total = static_cast<double>(quality.success_count + quality.failure_count);
    if (total == 0) {
      return 0.5;
    }

    return static_cast<double>(quality.success_count) / total;
  }

  return 0.5; // Unknown venue
}

double SmartOrderRouter::calculate_reliability_score(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  return calculate_reliability_score_locked(*lock, venue);
}

} // namespace veloz::exec
