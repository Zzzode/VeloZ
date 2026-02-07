#pragma once

#include "veloz/exec/order_api.h"

#include <optional>
#include <string>

namespace veloz::exec {

class ExchangeAdapter {
public:
    virtual ~ExchangeAdapter() = default;

    // Place an order, returns execution report
    virtual std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) = 0;

    // Cancel an order, returns execution report
    virtual std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) = 0;

    // Connection management
    virtual bool is_connected() const = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;

    // Get adapter info
    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual const char* version() const = 0;
};

} // namespace veloz::exec
