#include "veloz/exec/order_router.h"
#include <stdexcept>

namespace veloz::exec {

void OrderRouter::register_adapter(veloz::common::Venue venue,
                                   std::shared_ptr<ExchangeAdapter> adapter) {
    if (!adapter) {
        throw std::invalid_argument("Adapter cannot be null");
    }
    adapters_[venue] = adapter;
}

void OrderRouter::unregister_adapter(veloz::common::Venue venue) {
    adapters_.erase(venue);
}

void OrderRouter::set_default_venue(veloz::common::Venue venue) {
    default_venue_ = venue;
}

bool OrderRouter::has_adapter(veloz::common::Venue venue) const {
    return adapters_.contains(venue);
}

std::optional<ExecutionReport> OrderRouter::place_order(veloz::common::Venue venue,
                                                           const PlaceOrderRequest& req) {
    auto adapter = get_adapter(venue);
    if (!adapter) {
        return std::nullopt;
    }
    return adapter->place_order(req);
}

std::optional<ExecutionReport> OrderRouter::place_order(const PlaceOrderRequest& req) {
    if (!default_venue_) {
        return std::nullopt;
    }
    return place_order(*default_venue_, req);
}

std::optional<ExecutionReport> OrderRouter::cancel_order(veloz::common::Venue venue,
                                                            const CancelOrderRequest& req) {
    auto adapter = get_adapter(venue);
    if (!adapter) {
        return std::nullopt;
    }
    return adapter->cancel_order(req);
}

std::shared_ptr<ExchangeAdapter> OrderRouter::get_adapter(veloz::common::Venue venue) const {
    auto it = adapters_.find(venue);
    if (it == adapters_.end()) {
        return nullptr;
    }
    return it->second;
}

} // namespace veloz::exec
