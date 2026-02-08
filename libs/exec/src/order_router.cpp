#include "veloz/exec/order_router.h"
#include <stdexcept>

namespace veloz::exec {

void OrderRouter::register_adapter(veloz::common::Venue venue,
                                   std::shared_ptr<ExchangeAdapter> adapter) {
    if (!adapter) {
        throw std::invalid_argument("Adapter cannot be null");
    }
    std::scoped_lock lock(mu_);
    adapters_[venue] = adapter;
}

void OrderRouter::unregister_adapter(veloz::common::Venue venue) {
    std::scoped_lock lock(mu_);
    adapters_.erase(venue);
}

void OrderRouter::set_default_venue(veloz::common::Venue venue) {
    std::scoped_lock lock(mu_);
    default_venue_ = venue;
}

bool OrderRouter::has_adapter(veloz::common::Venue venue) const {
    std::scoped_lock lock(mu_);
    return adapters_.contains(venue);
}

std::optional<ExecutionReport> OrderRouter::place_order(veloz::common::Venue venue,
                                                           const PlaceOrderRequest& req) {
    std::scoped_lock lock(mu_);

    auto adapter = get_adapter(venue);
    if (!adapter) {
        return std::nullopt;
    }

    auto report = adapter->place_order(req);

    // If failover is enabled and primary venue fails, try other venues
    if (!report && failover_enabled_ && adapters_.size() > 1) {
        for (const auto& [other_venue, other_adapter] : adapters_) {
            if (other_venue != venue) {
                report = other_adapter->place_order(req);
                if (report) {
                    break;
                }
            }
        }
    }

    return report;
}

std::optional<ExecutionReport> OrderRouter::place_order(const PlaceOrderRequest& req) {
    std::scoped_lock lock(mu_);

    if (!default_venue_) {
        return std::nullopt;
    }

    return place_order(*default_venue_, req);
}

std::optional<ExecutionReport> OrderRouter::cancel_order(veloz::common::Venue venue,
                                                            const CancelOrderRequest& req) {
    std::scoped_lock lock(mu_);

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

std::vector<veloz::common::Venue> OrderRouter::get_registered_venues() const {
    std::scoped_lock lock(mu_);

    std::vector<veloz::common::Venue> venues;
    for (const auto& [venue, _] : adapters_) {
        venues.push_back(venue);
    }
    return venues;
}

void OrderRouter::set_order_timeout(std::chrono::milliseconds timeout) {
    std::scoped_lock lock(mu_);
    order_timeout_ = timeout;
}

std::chrono::milliseconds OrderRouter::get_order_timeout() const {
    std::scoped_lock lock(mu_);
    return order_timeout_;
}

void OrderRouter::set_failover_enabled(bool enabled) {
    std::scoped_lock lock(mu_);
    failover_enabled_ = enabled;
}

bool OrderRouter::is_failover_enabled() const {
    std::scoped_lock lock(mu_);
    return failover_enabled_;
}

} // namespace veloz::exec
