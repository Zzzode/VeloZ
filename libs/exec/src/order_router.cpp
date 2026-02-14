#include "veloz/exec/order_router.h"

namespace veloz::exec {

void OrderRouter::register_adapter(veloz::common::Venue venue, kj::Own<ExchangeAdapter> adapter) {
  // kj::Own<T> cannot be null by design - it's a non-null pointer wrapper
  // If someone tries to pass a null-constructed Own, that's a usage error
  auto lock = guarded_.lockExclusive();
  lock->adapters[venue] = kj::mv(adapter);
}

void OrderRouter::unregister_adapter(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->adapters.erase(venue);
}

void OrderRouter::set_default_venue(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->default_venue = venue;
}

bool OrderRouter::has_adapter(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  return lock->adapters.contains(venue);
}

kj::Maybe<ExecutionReport> OrderRouter::place_order(veloz::common::Venue venue,
                                                    const PlaceOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  auto* adapter_ptr = get_adapter(venue);
  if (adapter_ptr == nullptr) {
    return nullptr;
  }

  auto report = adapter_ptr->place_order(req);

  // If failover is enabled and primary venue fails, try other venues
  if (report == nullptr && lock->failover_enabled && lock->adapters.size() > 1) {
    for (auto& [other_venue, other_adapter] : lock->adapters) {
      if (other_venue != venue) {
        report = other_adapter->place_order(req);
        if (report != nullptr) {
          break;
        }
      }
    }
  }
  return report;
}

kj::Maybe<ExecutionReport> OrderRouter::place_order(const PlaceOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  // Use orDefault to get venue or throw exception if not set
  return place_order(lock->default_venue.orDefault(veloz::common::Venue::Unknown), req);
}

kj::Maybe<ExecutionReport> OrderRouter::cancel_order(veloz::common::Venue venue,
                                                     const CancelOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  auto* adapter_ptr = get_adapter(venue);
  if (adapter_ptr == nullptr) {
    return nullptr;
  }

  return adapter_ptr->cancel_order(req);
}

ExchangeAdapter* OrderRouter::get_adapter(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  auto it = lock->adapters.find(venue);
  if (it == lock->adapters.end()) {
    return nullptr;
  }
  return it->second.get();
}

kj::Array<veloz::common::Venue> OrderRouter::get_registered_venues() const {
  auto lock = guarded_.lockExclusive();
  auto builder = kj::heapArrayBuilder<veloz::common::Venue>(lock->adapters.size());
  for (const auto& [venue, _] : lock->adapters) {
    builder.add(venue);
  }
  return builder.finish();
}

void OrderRouter::set_order_timeout(std::chrono::milliseconds timeout) {
  auto lock = guarded_.lockExclusive();
  lock->order_timeout = timeout;
}

std::chrono::milliseconds OrderRouter::get_order_timeout() const {
  return guarded_.lockExclusive()->order_timeout;
}

void OrderRouter::set_failover_enabled(bool enabled) {
  auto lock = guarded_.lockExclusive();
  lock->failover_enabled = enabled;
}

bool OrderRouter::is_failover_enabled() const {
  return guarded_.lockExclusive()->failover_enabled;
}

} // namespace veloz::exec
