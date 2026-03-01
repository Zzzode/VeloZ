#pragma once

#include "request_context.h"

#include <kj/string.h>

namespace veloz::gateway {

// Forward declarations
namespace bridge {
class EngineBridge;
}
namespace audit {
class AuditLogger;
}

/**
 * Account handler.
 *
 * Handles account state queries (/api/account).
 * Endpoints:
 * - GET /api/account - Get account state (requires ReadAccount permission)
 * - GET /api/account/positions - Get all positions (requires ReadAccount permission)
 * - GET /api/account/positions/{symbol} - Get position for specific symbol (requires ReadAccount
 * permission)
 *
 * Performance target: <50μs
 */
class AccountHandler {
public:
  /**
   * @brief Construct AccountHandler with dependencies
   *
   * @param bridge EngineBridge for account state queries
   * @param audit Audit logger for access logging
   */
  explicit AccountHandler(bridge::EngineBridge& bridge, audit::AuditLogger& audit);

  /**
   * @brief Handle GET /api/account
   *
   * Returns current account state including balances.
   * Requires ReadAccount permission.
   *
   * Response format:
   * {
   *   "status": "success",
   *   "data": {
   *     "total_equity": 50000.0,
   *     "available_balance": 45000.0,
   *     "unrealized_pnl": 500.0,
   *     "open_position_count": 2,
   *     "total_position_notional": 10000.0,
   *     "last_update_ns": 1709030400000000000,
   *     "balances": {
   *       "BTC": 1.5,
   *       "USDT": 45000.0
   *     }
   *   }
   * }
   *
   * @param ctx Request context with auth info
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleGetAccount(RequestContext& ctx);

  /**
   * @brief Handle GET /api/account/positions
   *
   * Returns all positions.
   * Requires ReadAccount permission.
   *
   * @param ctx Request context with auth info
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleGetPositions(RequestContext& ctx);

  /**
   * @brief Handle GET /api/account/positions/{symbol}
   *
   * Returns position for a specific symbol.
   * Requires ReadAccount permission.
   *
   * @param ctx Request context with auth info
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleGetPosition(RequestContext& ctx);

private:
  bridge::EngineBridge& bridge_;
  audit::AuditLogger& audit_;
};

} // namespace veloz::gateway
