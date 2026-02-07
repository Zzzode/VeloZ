#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace veloz::exec {

class OrderRouter final {
public:
    OrderRouter() = default;

    // Register an adapter for a venue
    void register_adapter(veloz::common::Venue venue, std::shared_ptr<ExchangeAdapter> adapter);

    // Unregister an adapter
    void unregister_adapter(veloz::common::Venue venue);

    // Set default venue (when not specified in order request)
    void set_default_venue(veloz::common::Venue venue);

    // Check if adapter exists for venue
    [[nodiscard]] bool has_adapter(veloz::common::Venue venue) const;

    // Route order to specific venue or default
    std::optional<ExecutionReport> place_order(veloz::common::Venue venue,
                                                const PlaceOrderRequest& req);

    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req);

    // Route cancel to specific venue
    std::optional<ExecutionReport> cancel_order(veloz::common::Venue venue,
                                                 const CancelOrderRequest& req);

    // Get adapter for venue
    [[nodiscard]] std::shared_ptr<ExchangeAdapter> get_adapter(veloz::common::Venue venue) const;

private:
    std::unordered_map<veloz::common::Venue, std::shared_ptr<ExchangeAdapter>> adapters_;
    std::optional<veloz::common::Venue> default_venue_;
};

} // namespace veloz::exec
