#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declarations
class EngineBridge;
class AuditLogger;

/**
 * Order handler for managing order state queries.
 *
 * Handles endpoints:
 * - GET /api/orders_state - Get all order states
 * - GET /api/order_state - Get order state by client_order_id
 */
class OrderStateHandler {
public:
  explicit OrderStateHandler(EngineBridge* engineBridge, AuditLogger* auditLogger);

  /**
   * Handle GET /api/orders_state
   *
   * Returns JSON array of all order states.
   */
  kj::Promise<void> handleOrdersState(RequestContext& ctx);

  /**
   * Handle GET /api/order_state
   *
   * Returns JSON object with order state for specified client_order_id.
   * Query parameter: client_order_id
   */
  kj::Promise<void> handleOrderState(RequestContext& ctx);

private:
  EngineBridge* engineBridge_;
  AuditLogger* auditLogger_;
};

/**
 * Account handler for account state queries.
 *
 * Handles endpoint:
 * - GET /api/account - Get account state and balances
 */
class AccountHandler {
public:
  explicit AccountHandler(EngineBridge* engineBridge, AuditLogger* auditLogger);

  /**
   * Handle GET /api/account
   *
   * Returns JSON object with account state:
   * - total_equity
   * - available_balance
   * - unrealized_pnl
   * - balances (symbol -> amount)
   */
  kj::Promise<void> handleAccountState(RequestContext& ctx);

private:
  EngineBridge* engineBridge_;
  AuditLogger* auditLogger_;
};

/**
 * Config handler for gateway configuration.
 *
 * Handles endpoints:
 * - GET /api/config - Get gateway configuration
 * - POST /api/config - Update configuration
 */
class ConfigHandler {
public:
  explicit ConfigHandler(EngineBridge* engineBridge);

  /**
   * Handle GET /api/config
   *
   * Returns JSON object with configuration:
   * - engine_mode
   * - market_source
   * - market_symbol
   * - execution_mode
   * - auth_enabled
   */
  kj::Promise<void> handleGetConfig(RequestContext& ctx);

  /**
   * Handle POST /api/config
   *
   * Accepts JSON configuration and updates gateway config.
   */
  kj::Promise<void> handleUpdateConfig(RequestContext& ctx);

private:
  EngineBridge* engineBridge_;
};

} // namespace veloz::gateway
