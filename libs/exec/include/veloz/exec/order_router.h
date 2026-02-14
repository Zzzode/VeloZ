#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <unordered_map>
#include <vector>

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

  // Get adapter for venue (returns non-owning pointer, valid only while map exists)
  [[nodiscard]] ExchangeAdapter* get_adapter(veloz::common::Venue venue) const;

  // Get all registered venues
  [[nodiscard]] kj::Array<veloz::common::Venue> get_registered_venues() const;

  // Set order timeout
  void set_order_timeout(std::chrono::milliseconds timeout);

  // Get order timeout
  [[nodiscard]] std::chrono::milliseconds get_order_timeout() const;

  // Enable/disable automatic failover
  void set_failover_enabled(bool enabled);

  // Get failover status
  [[nodiscard]] bool is_failover_enabled() const;

private:
  // Internal state structure
  struct RouterState {
    std::unordered_map<veloz::common::Venue, kj::Own<ExchangeAdapter>> adapters;
    kj::Maybe<veloz::common::Venue> default_venue = nullptr;
    std::chrono::milliseconds order_timeout{std::chrono::seconds(30)};
    bool failover_enabled{true};
  };

  kj::MutexGuarded<RouterState> guarded_;
};

} // namespace veloz::exec
