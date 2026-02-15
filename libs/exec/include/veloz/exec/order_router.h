#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::exec {

class OrderRouter final {
public:
  OrderRouter() = default;

  // Register an adapter for a venue
  void register_adapter(veloz::common::Venue venue, kj::Own<ExchangeAdapter> adapter);

  // Unregister an adapter
  void unregister_adapter(veloz::common::Venue venue);

  // Set default venue (when not specified in order request)
  void set_default_venue(veloz::common::Venue venue);

  // Check if adapter exists for venue
  [[nodiscard]] bool has_adapter(veloz::common::Venue venue) const;

  // Route order to specific venue or default
  kj::Maybe<ExecutionReport> place_order(veloz::common::Venue venue, const PlaceOrderRequest& req);
  kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req);

  // Route cancel to specific venue
  kj::Maybe<ExecutionReport> cancel_order(veloz::common::Venue venue,
                                          const CancelOrderRequest& req);

  // Get adapter for venue (returns non-owning reference, valid only while map exists)
  [[nodiscard]] kj::Maybe<ExchangeAdapter&> get_adapter(veloz::common::Venue venue) const;

  // Get all registered venues
  [[nodiscard]] kj::Array<veloz::common::Venue> get_registered_venues() const;

  // Set order timeout
  void set_order_timeout(kj::Duration timeout);

  // Get order timeout
  [[nodiscard]] kj::Duration get_order_timeout() const;

  // Enable/disable automatic failover
  void set_failover_enabled(bool enabled);

  // Get failover status
  [[nodiscard]] bool is_failover_enabled() const;

private:
  // Internal state structure
  struct RouterState {
    kj::HashMap<veloz::common::Venue, kj::Own<ExchangeAdapter>> adapters;
    kj::Maybe<veloz::common::Venue> default_venue = kj::none;
    kj::Duration order_timeout{30 * kj::SECONDS};
    bool failover_enabled{true};
  };

  kj::MutexGuarded<RouterState> guarded_;
};

} // namespace veloz::exec
