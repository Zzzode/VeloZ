/**
 * @file order_api.h
 * @brief Core interfaces and implementation for the order execution module
 *
 * This file contains the core interfaces and implementation for the order execution
 * module in the VeloZ quantitative trading framework, including order type definitions,
 * order request structures, execution reports, and order state management.
 *
 * The order execution system is one of the core components of the framework, responsible
 * for handling various types of order requests, communicating with trading venues, and
 * providing order state management and execution reporting functionality.
 */

#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace veloz::exec {

/**
 * @brief Order side enumeration
 *
 * Defines the buy/sell direction of orders.
 */
enum class OrderSide : std::uint8_t {
  Buy = 0, ///< Buy order
  Sell = 1 ///< Sell order
};

/**
 * @brief Order type enumeration
 *
 * Defines the order types, including market and limit orders.
 */
enum class OrderType : std::uint8_t {
  Market = 0, ///< Market order
  Limit = 1   ///< Limit order
};

/**
 * @brief Time in force enumeration
 *
 * Defines the order validity types, including GTC (Good Till Canceled),
 * IOC (Immediate or Cancel), FOK (Fill or Kill), and GTX (Good Till Crossing).
 */
enum class TimeInForce : std::uint8_t {
  GTC = 0, ///< Good Till Canceled
  IOC = 1, ///< Immediate or Cancel
  FOK = 2, ///< Fill or Kill
  GTX = 3  ///< Good Till Crossing
};

/**
 * @brief Place order request structure
 *
 * Contains all information required for placing an order, such as symbol,
 * side, type, quantity, price, etc.
 */
struct PlaceOrderRequest final {
  veloz::common::SymbolId symbol;    ///< Trading symbol ID
  OrderSide side{OrderSide::Buy};    ///< Order side (default: buy)
  OrderType type{OrderType::Limit};  ///< Order type (default: limit)
  TimeInForce tif{TimeInForce::GTC}; ///< Time in force (default: GTC)

  double qty{0.0};             ///< Order quantity
  std::optional<double> price; ///< Order price (optional for market orders)

  std::string client_order_id; ///< Client order ID (for unique identification)
  bool reduce_only{false};     ///< Reduce-only order (for futures trading only)
  bool post_only{false};       ///< Post-only order (for limit orders only)
};

/**
 * @brief Cancel order request structure
 *
 * Contains information required for canceling an order, such as symbol
 * and client order ID.
 */
struct CancelOrderRequest final {
  veloz::common::SymbolId symbol; ///< Trading symbol ID
  std::string client_order_id;    ///< Client order ID
};

/**
 * @brief Order status enumeration
 *
 * Defines the various states of an order throughout its entire lifecycle,
 * from creation to completion.
 */
enum class OrderStatus : std::uint8_t {
  New = 0,             ///< Order created
  Accepted = 1,        ///< Order accepted
  PartiallyFilled = 2, ///< Order partially filled
  Filled = 3,          ///< Order fully filled
  Canceled = 4,        ///< Order canceled
  Rejected = 5,        ///< Order rejected
  Expired = 6,         ///< Order expired
};

/**
 * @brief Execution report structure
 *
 * Contains detailed order execution report information, such as order status,
 * fill quantity, fill price, etc.
 */
struct ExecutionReport final {
  veloz::common::SymbolId symbol;       ///< Trading symbol ID
  std::string client_order_id;          ///< Client order ID
  std::string venue_order_id;           ///< Venue order ID
  OrderStatus status{OrderStatus::New}; ///< Order status

  double last_fill_qty{0.0};   ///< Last fill quantity
  double last_fill_price{0.0}; ///< Last fill price

  std::int64_t ts_exchange_ns{0}; ///< Exchange timestamp (nanoseconds)
  std::int64_t ts_recv_ns{0};     ///< Receive timestamp (nanoseconds)
};

} // namespace veloz::exec
