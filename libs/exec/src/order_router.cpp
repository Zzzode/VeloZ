#include "veloz/exec/order_router.h"

namespace veloz::exec {

void OrderRouter::register_adapter(veloz::common::Venue venue, kj::Own<ExchangeAdapter> adapter) {
  // kj::Own<T> cannot be null by design - it's a non-null pointer wrapper
  // If someone tries to pass a null-constructed Own, that's a usage error
  auto lock = guarded_.lockExclusive();
  lock->adapters.upsert(venue, kj::mv(adapter), [](kj::Own<ExchangeAdapter>& existing,
                                                   kj::Own<ExchangeAdapter>&& replacement) {
    existing = kj::mv(replacement);
  });
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
  return lock->adapters.find(venue) != kj::none;
}

kj::Maybe<ExecutionReport> OrderRouter::place_order(veloz::common::Venue venue,
                                                    const PlaceOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(adapter_ref, get_adapter(venue)) {
    auto report = adapter_ref.place_order(req);

    // If failover is enabled and primary venue fails, try other venues
    if (report == kj::none && lock->failover_enabled && lock->adapters.size() > 1) {
      for (auto& entry : lock->adapters) {
        if (entry.key != venue) {
          report = entry.value->place_order(req);
          if (report != kj::none) {
            break;
          }
        }
      }
    }
    return report;
  }

  return kj::none;
}

kj::Maybe<ExecutionReport> OrderRouter::place_order(const PlaceOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  // Use orDefault to get venue or throw exception if not set
  return place_order(lock->default_venue.orDefault(veloz::common::Venue::Unknown), req);
}

kj::Maybe<ExecutionReport> OrderRouter::cancel_order(veloz::common::Venue venue,
                                                     const CancelOrderRequest& req) {
  auto lock = guarded_.lockExclusive();

  KJ_IF_SOME(adapter_ref, get_adapter(venue)) {
    return adapter_ref.cancel_order(req);
  }

  return kj::none;
}

kj::Maybe<ExchangeAdapter&> OrderRouter::get_adapter(veloz::common::Venue venue) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(adapter, lock->adapters.find(venue)) {
    return *adapter;
  }
  return kj::none;
}

kj::Array<veloz::common::Venue> OrderRouter::get_registered_venues() const {
  auto lock = guarded_.lockExclusive();
  auto builder = kj::heapArrayBuilder<veloz::common::Venue>(lock->adapters.size());
  for (const auto& entry : lock->adapters) {
    builder.add(entry.key);
  }
  return builder.finish();
}

void OrderRouter::set_order_timeout(kj::Duration timeout) {
  auto lock = guarded_.lockExclusive();
  lock->order_timeout = timeout;
}

kj::Duration OrderRouter::get_order_timeout() const {
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
