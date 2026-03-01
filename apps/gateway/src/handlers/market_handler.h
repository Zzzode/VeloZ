#pragma once

#include "request_context.h"

#include <kj/async-io.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declarations
namespace bridge {
class EngineBridge;
}

/**
 * Market data handler.
 *
 * Handles market data queries (/api/market).
 */
class MarketHandler {
public:
  explicit MarketHandler(bridge::EngineBridge* engineBridge);

  kj::Promise<void> handleGetMarket(RequestContext& ctx);

private:
  bridge::EngineBridge* engineBridge_;
};

} // namespace veloz::gateway
