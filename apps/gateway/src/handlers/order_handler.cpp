#include "handlers/order_handler.h"

#include "audit/audit_logger.h"
#include "bridge/engine_bridge.h"
#include "veloz/oms/order_record.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <kj/debug.h>
#include <kj/time.h>

namespace veloz::gateway {

namespace {

// Simple JSON parsing helpers (minimal implementation for order params)
// In production, would use a proper JSON library like yyjson

kj::Maybe<kj::String> extractStringField(const kj::String& json, kj::StringPtr fieldName) {
  // Simple extraction: look for "fieldName":"value" or "fieldName": "value"
  kj::String searchPattern = kj::str("\"", fieldName, "\"");

  KJ_IF_SOME(pos, json.find(searchPattern)) {
    // Find colon after pattern
    kj::StringPtr afterPattern = json.slice(pos + searchPattern.size());
    KJ_IF_SOME(colonPos, afterPattern.findFirst(':')) {
      kj::StringPtr afterColon = afterPattern.slice(colonPos + 1);

      // Skip whitespace
      size_t start = 0;
      while (start < afterColon.size() && (afterColon[start] == ' ' || afterColon[start] == '\t')) {
        ++start;
      }

      if (start >= afterColon.size() || afterColon[start] != '"') {
        return kj::none;
      }
      ++start; // Skip opening quote

      // Find closing quote
      size_t end = start;
      while (end < afterColon.size() && afterColon[end] != '"') {
        if (afterColon[end] == '\\') {
          ++end; // Skip escaped character
        }
        ++end;
      }

      if (end >= afterColon.size()) {
        return kj::none;
      }

      return kj::heapString(afterColon.slice(start, end));
    }
  }

  return kj::none;
}

kj::Maybe<double> extractNumberField(const kj::String& json, kj::StringPtr fieldName) {
  // Simple extraction: look for "fieldName":number
  kj::String searchPattern = kj::str("\"", fieldName, "\"");

  KJ_IF_SOME(pos, json.find(searchPattern)) {
    // Find colon after pattern
    kj::StringPtr afterPattern = json.slice(pos + searchPattern.size());
    KJ_IF_SOME(colonPos, afterPattern.findFirst(':')) {
      kj::StringPtr afterColon = afterPattern.slice(colonPos + 1);

      // Skip whitespace
      size_t start = 0;
      while (start < afterColon.size() && (afterColon[start] == ' ' || afterColon[start] == '\t')) {
        ++start;
      }

      if (start >= afterColon.size())
        return kj::none;

      // Find end of number (until comma, brace, or whitespace)
      size_t end = start;
      while (end < afterColon.size() && afterColon[end] != ',' && afterColon[end] != '}' &&
             afterColon[end] != ' ' && afterColon[end] != '\n' && afterColon[end] != '\r' &&
             afterColon[end] != ']') {
        ++end;
      }

      if (end == start)
        return kj::none;

      // Parse number
      kj::String numStr = kj::heapString(afterColon.slice(start, end));
      char* endPtr = nullptr;
      double value = std::strtod(numStr.cStr(), &endPtr);

      if (endPtr == numStr.cStr() + numStr.size()) {
        return value;
      }
    }
  }

  return kj::none;
}

// Simple JSON escaping for output
kj::String escapeJson(kj::StringPtr str) {
  kj::Vector<char> result;
  for (char c : str) {
    switch (c) {
    case '"':
      result.add('\\');
      result.add('"');
      break;
    case '\\':
      result.add('\\');
      result.add('\\');
      break;
    case '\n':
      result.add('\\');
      result.add('n');
      break;
    case '\r':
      result.add('\\');
      result.add('r');
      break;
    case '\t':
      result.add('\\');
      result.add('t');
      break;
    default:
      result.add(c);
      break;
    }
  }
  result.add('\0');
  return kj::String(result.releaseAsArray());
}

// Get current timestamp in ISO8601 format
kj::String getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time));
  return kj::heapString(buffer);
}

// Counter for generating unique client order IDs
std::atomic<uint64_t> orderIdCounter{0};

} // namespace

OrderHandler::OrderHandler(bridge::EngineBridge* engineBridge, audit::AuditLogger* auditLogger)
    : engineBridge_(engineBridge), auditLogger_(auditLogger) {
  KJ_REQUIRE(engineBridge_ != nullptr, "EngineBridge cannot be null");
  KJ_REQUIRE(auditLogger_ != nullptr, "AuditLogger cannot be null");
}

kj::Promise<void> OrderHandler::handleSubmitOrder(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::WriteOrders)) {
    return ctx.sendError(403, "Permission denied: write:orders required");
  }

  // Read request body
  return ctx.readBodyAsString().then([this, &ctx](kj::String body) -> kj::Promise<void> {
    // Parse order parameters
    auto params = parseOrderParams(body);

    KJ_IF_SOME(orderParams, params) {
      // Validate parameters
      kj::String error;
      if (!validateOrderParams(orderParams, error)) {
        return ctx.sendError(400, error);
      }

      // Generate or use provided client order ID
      kj::String clientOrderId;
      KJ_IF_SOME(id, orderParams.client_order_id) {
        clientOrderId = kj::mv(id);
      }
      else {
        clientOrderId = generateClientId();
      }

      // Submit order to engine
      auto side = orderParams.side == "BUY" ? "buy" : "sell";

      return engineBridge_
          ->place_order(side, orderParams.symbol, orderParams.qty, orderParams.price, clientOrderId)
          .then([this, &ctx, clientOrderId = kj::mv(clientOrderId),
                 orderParams = kj::mv(orderParams)]() mutable {
            // Log audit event
            kj::String userId;
            KJ_IF_SOME(auth, ctx.authInfo) {
              userId = kj::heapString(auth.user_id);
            }
            else {
              userId = kj::heapString("unknown");
            }
            (void)auditLogger_
                ->log(audit::AuditLogType::Order, kj::str("ORDER_SUBMIT"), kj::mv(userId),
                      kj::heapString(ctx.clientIP), kj::none)
                .eagerlyEvaluate(nullptr);

            // Return success response
            auto timestamp = getCurrentTimestamp();
            auto response = kj::str("{"
                                    "\"status\":\"success\","
                                    "\"data\":{"
                                    "\"client_order_id\":\"",
                                    escapeJson(clientOrderId),
                                    "\","
                                    "\"symbol\":\"",
                                    escapeJson(orderParams.symbol),
                                    "\","
                                    "\"side\":\"",
                                    escapeJson(orderParams.side),
                                    "\","
                                    "\"qty\":",
                                    orderParams.qty,
                                    ","
                                    "\"price\":",
                                    orderParams.price,
                                    ","
                                    "\"status\":\"new\","
                                    "\"created_at\":\"",
                                    timestamp,
                                    "\""
                                    "}"
                                    "}");

            return ctx.sendJson(200, response);
          });
    }
    else {
      return ctx.sendError(400, "Invalid order request: missing required fields");
    }
  });
}

kj::Promise<void> OrderHandler::handleListOrders(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::ReadOrders)) {
    return ctx.sendError(403, "Permission denied: read:orders required");
  }

  // Get all orders from bridge
  auto orders = engineBridge_->get_orders();

  // Build JSON response
  kj::Vector<kj::String> orderJsons;
  orderJsons.reserve(orders.size());

  for (const auto& order : orders) {
    orderJsons.add(formatOrderJson(order));
  }

  // Build array string
  kj::String response;
  if (orderJsons.empty()) {
    response = kj::str("{\"status\":\"success\",\"data\":[]}");
  } else {
    kj::Vector<kj::StringPtr> refs;
    for (const auto& json : orderJsons) {
      refs.add(json);
    }
    response = kj::str("{\"status\":\"success\",\"data\":[", kj::strArray(refs, ","), "]}");
  }

  return ctx.sendJson(200, response);
}

kj::Promise<void> OrderHandler::handleGetOrder(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::ReadOrders)) {
    return ctx.sendError(403, "Permission denied: read:orders required");
  }

  // Extract order ID from path parameters
  KJ_IF_SOME(orderId, ctx.path_params.find("id"_kj)) {
    // Get order from bridge
    auto maybeOrder = engineBridge_->get_order(orderId);

    KJ_IF_SOME(order, maybeOrder) {
      auto response = kj::str("{\"status\":\"success\",\"data\":", formatOrderJson(order), "}");
      return ctx.sendJson(200, response);
    }
    else {
      return ctx.sendError(404, "Order not found");
    }
  }
  else {
    return ctx.sendError(400, "Missing order ID");
  }
}

kj::Promise<void> OrderHandler::handleCancelOrder(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::WriteCancel)) {
    return ctx.sendError(403, "Permission denied: write:cancel required");
  }

  // Extract order ID from path parameters
  KJ_IF_SOME(orderId, ctx.path_params.find("id"_kj)) {
    // Submit cancel request
    return engineBridge_->cancel_order(orderId).then(
        [this, &ctx, orderId = kj::heapString(orderId)]() {
          // Log audit event
          kj::String userId;
          KJ_IF_SOME(auth, ctx.authInfo) {
            userId = kj::heapString(auth.user_id);
          }
          else {
            userId = kj::heapString("unknown");
          }
          (void)auditLogger_
              ->log(audit::AuditLogType::Order, kj::str("ORDER_CANCEL"), kj::mv(userId),
                    kj::heapString(ctx.clientIP), kj::none)
              .eagerlyEvaluate(nullptr);

          auto timestamp = getCurrentTimestamp();
          auto response = kj::str("{"
                                  "\"status\":\"success\","
                                  "\"data\":{"
                                  "\"client_order_id\":\"",
                                  escapeJson(orderId),
                                  "\","
                                  "\"status\":\"cancel_requested\","
                                  "\"cancelled_at\":\"",
                                  timestamp,
                                  "\""
                                  "}"
                                  "}");

          return ctx.sendJson(200, response);
        });
  }
  else {
    return ctx.sendError(400, "Missing order ID");
  }
}

kj::Promise<void> OrderHandler::handleBulkCancel(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::WriteCancel)) {
    return ctx.sendError(403, "Permission denied: write:cancel required");
  }

  // Read request body
  return ctx.readBodyAsString().then([this, &ctx](kj::String body) -> kj::Promise<void> {
    // Parse array of order IDs from JSON
    // Expecting: {"order_ids": ["id1", "id2", ...]}

    kj::Vector<kj::String> orderIds;

    // Simple parsing for order_ids array
    KJ_IF_SOME(arrayStart, body.find("\"order_ids\"")) {
      KJ_IF_SOME(bracketPos, body.slice(arrayStart).findFirst('[')) {
        size_t start = arrayStart + bracketPos + 1;
        kj::StringPtr arrayContent = body.slice(start);

        // Find each quoted string
        size_t pos = 0;
        while (pos < arrayContent.size()) {
          KJ_IF_SOME(quotePos, arrayContent.slice(pos).findFirst('"')) {
            size_t strStart = pos + quotePos + 1;
            KJ_IF_SOME(endQuote, arrayContent.slice(strStart).findFirst('"')) {
              size_t strEnd = strStart + endQuote;
              orderIds.add(kj::heapString(arrayContent.slice(strStart, strEnd)));
              pos = strEnd + 1;
            }
            else {
              break;
            }
          }
          else {
            break;
          }
        }
      }
    }

    if (orderIds.empty()) {
      return ctx.sendError(400, "No order IDs provided");
    }

    // Cancel each order - collect promises into an array for joinPromises
    kj::Vector<kj::Promise<void>> cancelPromises;
    for (const auto& orderId : orderIds) {
      cancelPromises.add(engineBridge_->cancel_order(orderId));
    }

    // Convert to array for joinPromises
    auto promisesArray = cancelPromises.releaseAsArray();

    // Wait for all cancellations
    return kj::joinPromises(kj::mv(promisesArray))
        .then([this, &ctx, orderIds = kj::mv(orderIds)]() {
          // Log audit event
          kj::String userId;
          KJ_IF_SOME(auth, ctx.authInfo) {
            userId = kj::heapString(auth.user_id);
          }
          else {
            userId = kj::heapString("unknown");
          }
          (void)auditLogger_
              ->log(audit::AuditLogType::Order, kj::str("ORDER_BULK_CANCEL"), kj::mv(userId),
                    kj::heapString(ctx.clientIP), kj::none)
              .eagerlyEvaluate(nullptr);

          // Build response
          auto timestamp = getCurrentTimestamp();
          auto response = kj::str("{"
                                  "\"status\":\"success\","
                                  "\"data\":{"
                                  "\"cancelled_count\":",
                                  orderIds.size(),
                                  ","
                                  "\"cancelled_at\":\"",
                                  timestamp,
                                  "\""
                                  "}"
                                  "}");

          return ctx.sendJson(200, response);
        });
  });
}

kj::Maybe<OrderParams> OrderHandler::parseOrderParams(const kj::String& body) {
  OrderParams params;
  params.price = 0.0; // Default to market order

  // Extract required fields
  KJ_IF_SOME(side, extractStringField(body, "side")) {
    params.side = kj::mv(side);
    // Normalize to uppercase
    if (params.side == "buy")
      params.side = kj::heapString("BUY");
    if (params.side == "sell")
      params.side = kj::heapString("SELL");
  }
  else {
    return kj::none;
  }

  KJ_IF_SOME(symbol, extractStringField(body, "symbol")) {
    params.symbol = kj::mv(symbol);
  }
  else {
    return kj::none;
  }

  KJ_IF_SOME(qty, extractNumberField(body, "qty")) {
    params.qty = qty;
  }
  else {
    return kj::none;
  }

  // Optional fields
  KJ_IF_SOME(price, extractNumberField(body, "price")) {
    params.price = price;
  }

  params.client_order_id = extractStringField(body, "client_order_id");

  return params;
}

bool OrderHandler::validateOrderParams(const OrderParams& params, kj::String& error) {
  // Validate side
  if (params.side != "BUY" && params.side != "SELL") {
    error = kj::str("Invalid order side: must be BUY or SELL");
    return false;
  }

  // Validate symbol
  if (params.symbol.size() == 0) {
    error = kj::str("Symbol cannot be empty");
    return false;
  }

  // Validate quantity
  if (params.qty <= 0.0) {
    error = kj::str("Order quantity must be positive");
    return false;
  }

  if (!std::isfinite(params.qty)) {
    error = kj::str("Order quantity must be a finite number");
    return false;
  }

  // Validate price
  if (params.price < 0.0) {
    error = kj::str("Order price cannot be negative");
    return false;
  }

  if (!std::isfinite(params.price)) {
    error = kj::str("Order price must be a finite number");
    return false;
  }

  return true;
}

kj::String OrderHandler::formatOrderJson(const oms::OrderState& order) {
  kj::String priceStr;
  KJ_IF_SOME(p, order.limit_price) {
    priceStr = kj::str(p);
  }
  else {
    priceStr = kj::heapString("null");
  }

  kj::String qtyStr;
  KJ_IF_SOME(q, order.order_qty) {
    qtyStr = kj::str(q);
  }
  else {
    qtyStr = kj::heapString("null");
  }

  return kj::str("{"
                 "\"client_order_id\":\"",
                 escapeJson(order.client_order_id),
                 "\","
                 "\"symbol\":\"",
                 escapeJson(order.symbol),
                 "\","
                 "\"side\":\"",
                 escapeJson(order.side),
                 "\","
                 "\"qty\":",
                 qtyStr,
                 ","
                 "\"price\":",
                 priceStr,
                 ","
                 "\"executed_qty\":",
                 order.executed_qty,
                 ","
                 "\"avg_price\":",
                 order.avg_price,
                 ","
                 "\"status\":\"",
                 escapeJson(order.status),
                 "\","
                 "\"venue_order_id\":\"",
                 escapeJson(order.venue_order_id),
                 "\","
                 "\"reason\":\"",
                 escapeJson(order.reason),
                 "\","
                 "\"last_update_ns\":",
                 order.last_ts_ns,
                 ","
                 "\"created_ns\":",
                 order.created_ts_ns, "}");
}

kj::String OrderHandler::generateClientId() {
  uint64_t id = orderIdCounter.fetch_add(1, std::memory_order_relaxed);
  auto now = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  return kj::str("veloz_", ns, "_", id);
}

bool OrderHandler::checkPermission(RequestContext& ctx, auth::Permission permission) {
  KJ_IF_SOME(auth, ctx.authInfo) {
    auto perm_name = auth::RbacManager::permission_name(permission);
    for (const auto& perm : auth.permissions) {
      if (perm == perm_name) {
        return true;
      }
    }
  }
  return false;
}

bool OrderHandler::checkPermission(RequestContext& ctx, kj::StringPtr permission) {
  KJ_IF_SOME(auth, ctx.authInfo) {
    for (const auto& perm : auth.permissions) {
      if (perm == permission) {
        return true;
      }
    }
  }
  return false;
}

} // namespace veloz::gateway
