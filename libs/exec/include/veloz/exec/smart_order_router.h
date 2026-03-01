#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_coordinator.h"
#include "veloz/exec/order_api.h"

#include <cstdint>
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

// Fee structure for an exchange
struct ExchangeFees {
  double maker_fee{0.001}; // 0.1% default
  double taker_fee{0.001}; // 0.1% default
  double withdrawal_fee{0.0};
  bool fee_in_quote{true}; // Fee deducted from quote currency
};

// Execution quality metrics
struct ExecutionQuality {
  double slippage{0.0};  // Price slippage from expected
  double fill_rate{0.0}; // Percentage filled
  kj::Duration execution_time{0 * kj::NANOSECONDS};
  double effective_fee{0.0};     // Actual fee paid
  double price_improvement{0.0}; // Improvement vs NBBO
};

// Smart routing score breakdown
struct RoutingScore {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  double total_score{0.0};
  double price_score{0.0};
  double fee_score{0.0};
  double latency_score{0.0};
  double liquidity_score{0.0};
  double reliability_score{0.0};
  kj::String explanation;
};

// Order split for large orders
struct OrderSplit {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  double quantity{0.0};
  double expected_price{0.0};
  double expected_fee{0.0};
};

// Batch order request
struct BatchOrderRequest {
  kj::Vector<PlaceOrderRequest> orders;
  bool atomic{false}; // If true, all or nothing
};

// Batch order result
struct BatchOrderResult {
  kj::Vector<kj::Maybe<ExecutionReport>> reports;
  std::size_t success_count{0};
  std::size_t failure_count{0};
};

// Cancel merge request
struct CancelMergeRequest {
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  veloz::common::SymbolId symbol;
  kj::Vector<kj::String> client_order_ids;
};

// Execution analytics
struct ExecutionAnalytics {
  std::size_t total_orders{0};
  std::size_t filled_orders{0};
  std::size_t partial_fills{0};
  std::size_t rejected_orders{0};
  double total_volume{0.0};
  double total_fees{0.0};
  double average_slippage{0.0};
  double average_fill_rate{0.0};
  kj::Duration average_execution_time{0 * kj::NANOSECONDS};
};

// SmartOrderRouter provides intelligent order routing with fee and liquidity awareness
class SmartOrderRouter final {
public:
  explicit SmartOrderRouter(ExchangeCoordinator& coordinator);

  // Set fee structure for a venue
  void set_fees(veloz::common::Venue venue, const ExchangeFees& fees);
  [[nodiscard]] kj::Maybe<ExchangeFees> get_fees(veloz::common::Venue venue) const;

  // Smart routing with full score breakdown
  [[nodiscard]] kj::Vector<RoutingScore> score_venues(const veloz::common::SymbolId& symbol,
                                                      OrderSide side, double quantity) const;

  // Get optimal venue considering all factors
  [[nodiscard]] RoutingDecision route_order(const PlaceOrderRequest& req) const;

  // Split large order across venues for better execution
  [[nodiscard]] kj::Vector<OrderSplit> split_order(const veloz::common::SymbolId& symbol,
                                                   OrderSide side, double quantity,
                                                   double max_single_venue_pct = 0.5) const;

  // Execute order with smart routing
  kj::Maybe<ExecutionReport> execute(const PlaceOrderRequest& req);

  // Execute split order across venues
  kj::Vector<kj::Maybe<ExecutionReport>> execute_split(const veloz::common::SymbolId& symbol,
                                                       OrderSide side, double quantity,
                                                       kj::StringPtr client_order_id_prefix);

  // Batch operations
  BatchOrderResult execute_batch(const BatchOrderRequest& batch);

  // Cancel merging - combine multiple cancels into efficient API calls
  kj::Vector<kj::Maybe<ExecutionReport>> cancel_merged(const CancelMergeRequest& req);

  // Execution quality tracking
  void record_execution(veloz::common::Venue venue, const ExecutionReport& report,
                        double expected_price, kj::Duration execution_time);

  [[nodiscard]] kj::Maybe<ExecutionQuality> get_venue_quality(veloz::common::Venue venue) const;

  [[nodiscard]] ExecutionAnalytics get_analytics() const;

  void reset_analytics();

  // Configuration
  void set_price_weight(double weight);
  void set_fee_weight(double weight);
  void set_latency_weight(double weight);
  void set_liquidity_weight(double weight);
  void set_reliability_weight(double weight);

  [[nodiscard]] double price_weight() const;
  [[nodiscard]] double fee_weight() const;
  [[nodiscard]] double latency_weight() const;
  [[nodiscard]] double liquidity_weight() const;
  [[nodiscard]] double reliability_weight() const;

  // Minimum order size per venue
  void set_min_order_size(veloz::common::Venue venue, double size);
  [[nodiscard]] double get_min_order_size(veloz::common::Venue venue) const;

private:
  double calculate_effective_price(veloz::common::Venue venue, double price, double quantity,
                                   OrderSide side) const;

  double calculate_liquidity_score(veloz::common::Venue venue,
                                   const veloz::common::SymbolId& symbol, double quantity,
                                   OrderSide side) const;

  double calculate_reliability_score(veloz::common::Venue venue) const;

  struct VenueQuality {
    std::size_t sample_count{0};
    double total_slippage{0.0};
    double total_fill_rate{0.0};
    std::int64_t total_execution_time_ns{0};
    double total_fees{0.0};
    std::size_t success_count{0};
    std::size_t failure_count{0};
  };

  struct RouterState {
    kj::HashMap<veloz::common::Venue, ExchangeFees> fees;
    kj::HashMap<veloz::common::Venue, VenueQuality> quality;
    kj::HashMap<veloz::common::Venue, double> min_order_sizes;

    // Scoring weights (should sum to 1.0)
    double price_weight{0.35};
    double fee_weight{0.20};
    double latency_weight{0.15};
    double liquidity_weight{0.20};
    double reliability_weight{0.10};

    // Analytics
    ExecutionAnalytics analytics;
  };

  // Internal helper that works with already-locked state (avoids deadlock)
  double calculate_reliability_score_locked(const RouterState& state,
                                            veloz::common::Venue venue) const;

  ExchangeCoordinator& coordinator_;
  kj::MutexGuarded<RouterState> guarded_;
};

} // namespace veloz::exec
