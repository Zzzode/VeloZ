#include "handlers/market_handler.h"

#include "bridge/engine_bridge.h"
#include "veloz/core/json.h"

#include <kj/async-io.h>
#include <kj/debug.h>

namespace veloz::gateway {

// ============================================================================
// MarketHandler Implementation
// ============================================================================

MarketHandler::MarketHandler(bridge::EngineBridge* engineBridge) : engineBridge_(engineBridge) {
  KJ_REQUIRE(engineBridge_ != nullptr, "EngineBridge cannot be null");
}

kj::Promise<void> MarketHandler::handleGetMarket(RequestContext& ctx) {
  try {
    // Parse query parameters
    kj::StringPtr symbol = "BTCUSDT"_kj; // Default symbol

    // Parse symbol from query string
    // Format: "?symbol=BTCUSDT"
    if (ctx.queryString.size() > 0) {
      auto params = ctx.queryString.slice(1); // Skip leading '?'
      KJ_IF_SOME(eq_pos, params.findFirst('=')) {
        auto key = params.slice(0, eq_pos);
        if (key == "symbol"_kj) {
          symbol = params.slice(eq_pos + 1);
        }
      }
    }

    // Get market snapshot from engine bridge
    auto snapshot = engineBridge_->get_market_snapshot(symbol);

    // Build JSON response using JsonBuilder
    auto builder = core::JsonBuilder::object();

    builder.put("symbol", snapshot.symbol.cStr());

    // Handle bid/ask prices
    KJ_IF_SOME(bid_price, snapshot.best_bid_price) {
      builder.put("best_bid_price", bid_price);
    }
    KJ_IF_SOME(bid_qty, snapshot.best_bid_qty) {
      builder.put("best_bid_qty", bid_qty);
    }
    KJ_IF_SOME(ask_price, snapshot.best_ask_price) {
      builder.put("best_ask_price", ask_price);
    }
    KJ_IF_SOME(ask_qty, snapshot.best_ask_qty) {
      builder.put("best_ask_qty", ask_qty);
    }

    builder.put("last_price", snapshot.last_price);
    builder.put("volume_24h", snapshot.volume_24h);
    builder.put("last_trade_id", static_cast<int64_t>(snapshot.last_trade_id));
    builder.put("last_update_ns", static_cast<int64_t>(snapshot.last_update_ns));
    builder.put("exchange_ts_ns", static_cast<int64_t>(snapshot.exchange_ts_ns));

    kj::String json_body = builder.build();

    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in market handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in market handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

} // namespace veloz::gateway
