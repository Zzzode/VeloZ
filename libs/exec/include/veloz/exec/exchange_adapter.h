#pragma once

#include "veloz/exec/order_api.h"

#include <kj/common.h>
#include <kj/memory.h>

namespace veloz::exec {

class ExchangeAdapter {
public:
  virtual ~ExchangeAdapter() noexcept = default;

  // Place an order, returns execution report
  virtual kj::Maybe<ExecutionReport> place_order(const PlaceOrderRequest& req) = 0;

  // Cancel an order, returns execution report
  virtual kj::Maybe<ExecutionReport> cancel_order(const CancelOrderRequest& req) = 0;

  // Connection management
  virtual bool is_connected() const = 0;
  virtual void connect() = 0;
  virtual void disconnect() = 0;

  // Get adapter info
  [[nodiscard]] virtual const char* name() const = 0;
  [[nodiscard]] virtual const char* version() const = 0;
};

} // namespace veloz::exec
