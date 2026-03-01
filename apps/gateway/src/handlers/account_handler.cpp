#include "handlers/account_handler.h"

#include "audit/audit_logger.h"
#include "bridge/engine_bridge.h"
#include "veloz/core/json.h"

#include <kj/debug.h>
#include <kj/time.h>

namespace veloz::gateway {

AccountHandler::AccountHandler(bridge::EngineBridge& bridge, audit::AuditLogger& audit)
    : bridge_(bridge), audit_(audit) {}

kj::Promise<void> AccountHandler::handleGetAccount(RequestContext& ctx) {
  // Permission check - requires auth::Permission::ReadAccount
  bool has_permission = false;
  KJ_IF_SOME(auth, ctx.authInfo) {
    for (const auto& perm : auth.permissions) {
      if (perm == "read:account"_kj) {
        has_permission = true;
        break;
      }
    }
  }

  if (!has_permission) {
    return ctx.sendError(403, "Permission denied: read:account required"_kj);
  }

  try {
    // Get account state from engine bridge
    auto accountState = bridge_.get_account_state();

    // Build JSON response using JsonBuilder
    auto responseBuilder = core::JsonBuilder::object();
    responseBuilder.put("status", "success");
    responseBuilder.put_object("data", [&accountState](core::JsonBuilder& data) {
      data.put("total_equity", accountState.total_equity);
      data.put("available_balance", accountState.available_balance);
      data.put("unrealized_pnl", accountState.unrealized_pnl);
      data.put("open_position_count", static_cast<int64_t>(accountState.open_position_count));
      data.put("total_position_notional", accountState.total_position_notional);
      data.put("last_update_ns", static_cast<int64_t>(accountState.last_update_ns));

      data.put_object("balances", [&accountState](core::JsonBuilder& balances) {
        for (auto& entry : accountState.balances) {
          balances.put(entry.key.cStr(), entry.value);
        }
      });
    });

    kj::String json_body = responseBuilder.build();

    // Log audit event (non-blocking)
    kj::StringPtr user_id_str = "unknown"_kj;
    KJ_IF_SOME(auth, ctx.authInfo) {
      user_id_str = auth.user_id;
    }

    // Log audit event (non-blocking) - fire and forget
    (void)audit_
        .log(audit::AuditLogType::Access, kj::str("ACCOUNT_QUERY"), kj::str(user_id_str),
             kj::str(ctx.clientIP), kj::none)
        .eagerlyEvaluate(nullptr);

    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in account handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in account handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> AccountHandler::handleGetPositions(RequestContext& ctx) {
  // Permission check - requires "read:account"
  bool has_permission = false;
  KJ_IF_SOME(auth, ctx.authInfo) {
    for (const auto& perm : auth.permissions) {
      if (perm == "read:account"_kj) {
        has_permission = true;
        break;
      }
    }
  }

  if (!has_permission) {
    return ctx.sendError(403, "Permission denied: read:account required");
  }

  try {
    // Get all positions from engine bridge
    auto positions = bridge_.get_positions();

    // Build JSON array of positions
    auto builder = core::JsonBuilder::object();
    builder.put("status", "success");
    builder.put_array("data", [&positions](core::JsonBuilder& data) {
      for (const auto& position : positions) {
        data.add_object([&position](core::JsonBuilder& pos) {
          pos.put("symbol", position.symbol.cStr());
          pos.put("size", position.size);
          pos.put("avg_price", position.avg_price);
          pos.put("realized_pnl", position.realized_pnl);
          pos.put("unrealized_pnl", position.unrealized_pnl);

          // Convert side enum to string
          const char* side_str = "none";
          switch (position.side) {
          case oms::PositionSide::Long:
            side_str = "long";
            break;
          case oms::PositionSide::Short:
            side_str = "short";
            break;
          case oms::PositionSide::None:
            side_str = "none";
            break;
          }
          pos.put("side", side_str);
          pos.put("timestamp_ns", static_cast<int64_t>(position.timestamp_ns));
        });
      }
    });

    kj::String json_body = builder.build();

    // Log audit event (non-blocking)
    kj::StringPtr user_id_str = "unknown"_kj;
    KJ_IF_SOME(auth, ctx.authInfo) {
      user_id_str = auth.user_id;
    }

    // Log audit event (non-blocking) - fire and forget
    (void)audit_
        .log(audit::AuditLogType::Access, kj::str("POSITIONS_QUERY"), kj::str(user_id_str),
             kj::str(ctx.clientIP), kj::none)
        .eagerlyEvaluate(nullptr);

    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in positions handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in positions handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> AccountHandler::handleGetPosition(RequestContext& ctx) {
  // Permission check - requires "read:account"
  bool has_permission = false;
  KJ_IF_SOME(auth, ctx.authInfo) {
    for (const auto& perm : auth.permissions) {
      if (perm == "read:account"_kj) {
        has_permission = true;
        break;
      }
    }
  }

  if (!has_permission) {
    return ctx.sendError(403, "Permission denied: read:account required");
  }

  try {
    // Extract symbol from path parameters
    KJ_IF_SOME(symbol, ctx.path_params.find("symbol"_kj)) {
      // Get position from bridge
      auto maybePosition = bridge_.get_position(symbol);

      KJ_IF_SOME(position, maybePosition) {
        auto builder = core::JsonBuilder::object();
        builder.put("status", "success");
        builder.put_object("data", [&position](core::JsonBuilder& data) {
          data.put("symbol", position.symbol.cStr());
          data.put("size", position.size);
          data.put("avg_price", position.avg_price);
          data.put("realized_pnl", position.realized_pnl);
          data.put("unrealized_pnl", position.unrealized_pnl);

          const char* side_str = "none";
          switch (position.side) {
          case oms::PositionSide::Long:
            side_str = "long";
            break;
          case oms::PositionSide::Short:
            side_str = "short";
            break;
          case oms::PositionSide::None:
            side_str = "none";
            break;
          }
          data.put("side", side_str);
          data.put("timestamp_ns", static_cast<int64_t>(position.timestamp_ns));
        });

        kj::String json_body = builder.build();

        // Log audit event (non-blocking) - fire and forget
        kj::StringPtr user_id_str = "unknown"_kj;
        KJ_IF_SOME(auth, ctx.authInfo) {
          user_id_str = auth.user_id;
        }

        (void)audit_
            .log(audit::AuditLogType::Access, kj::str("POSITION_QUERY"), kj::str(user_id_str),
                 kj::str(ctx.clientIP), kj::none)
            .eagerlyEvaluate(nullptr);

        return ctx.sendJson(200, json_body);
      }
      else {
        return ctx.sendError(404, "Position not found");
      }
    }
    else {
      return ctx.sendError(400, "Missing symbol parameter");
    }
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in position handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in position handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

} // namespace veloz::gateway
