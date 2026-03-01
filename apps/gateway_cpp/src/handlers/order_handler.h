#pragma once

#include "auth/rbac.h"
#include "request_context.h"

#include <kj/async.h>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/vector.h>

// Include OrderState definition instead of forward declaration
#include "veloz/oms/order_record.h"

namespace veloz::gateway {

// Forward declarations
namespace bridge {
class EngineBridge;
}
namespace audit {
class AuditLogger;
}

/**
 * Order parameters for submission.
 */
struct OrderParams {
  kj::String side;                       // "BUY" or "SELL"
  kj::String symbol;                     // e.g., "BTCUSDT"
  double qty{0.0};                       // Order quantity
  double price{0.0};                     // Limit price (0 for market orders)
  kj::Maybe<kj::String> client_order_id; // Optional custom ID
};

/**
 * Order handler.
 *
 * Handles order submission and management.
 * Endpoints:
 * - POST /api/orders - Submit new order
 * - GET /api/orders - List all orders
 * - GET /api/orders/{id} - Get order details
 * - DELETE /api/orders/{id} - Cancel order
 * - POST /api/cancel - Bulk cancellation
 */
class OrderHandler {
public:
  explicit OrderHandler(bridge::EngineBridge* engineBridge, audit::AuditLogger* auditLogger);

  // POST /api/orders - Submit new order
  kj::Promise<void> handleSubmitOrder(RequestContext& ctx);

  // GET /api/orders - List all orders
  kj::Promise<void> handleListOrders(RequestContext& ctx);

  // GET /api/orders/{id} - Get order details
  kj::Promise<void> handleGetOrder(RequestContext& ctx);

  // DELETE /api/orders/{id} - Cancel order
  kj::Promise<void> handleCancelOrder(RequestContext& ctx);

  // POST /api/cancel - Bulk cancellation
  kj::Promise<void> handleBulkCancel(RequestContext& ctx);

private:
  bridge::EngineBridge* engineBridge_;
  audit::AuditLogger* auditLogger_;

  // Parse order submission request body
  kj::Maybe<OrderParams> parseOrderParams(const kj::String& body);

  // Validate order parameters
  bool validateOrderParams(const OrderParams& params, kj::String& error);

  // Format order state as JSON response
  kj::String formatOrderJson(const oms::OrderState& order);

  // Generate unique client order ID
  kj::String generateClientId();

  // Check if user has required permission
  bool checkPermission(RequestContext& ctx, kj::StringPtr permission);
  bool checkPermission(RequestContext& ctx, auth::Permission permission);
};

} // namespace veloz::gateway
