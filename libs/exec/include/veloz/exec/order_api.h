/**
 * @file order_api.h
 * @brief 订单执行模块的核心接口和实现
 *
 * 该文件包含了 VeloZ 量化交易框架中订单执行模块的核心接口和实现，
 * 包括订单类型定义、订单请求结构、执行报告和订单状态管理等。
 *
 * 订单执行系统是框架的核心组件之一，负责处理各种类型的订单请求，
 * 与交易场所进行通信，并提供订单状态管理和执行报告功能。
 */

#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace veloz::exec {

/**
 * @brief 订单方向枚举
 *
 * 定义了订单的买卖方向。
 */
enum class OrderSide : std::uint8_t {
  Buy = 0,   ///< 买入订单
  Sell = 1   ///< 卖出订单
};

/**
 * @brief 订单类型枚举
 *
 * 定义了订单的类型，包括市价单和限价单。
 */
enum class OrderType : std::uint8_t {
  Market = 0,  ///< 市价单
  Limit = 1    ///< 限价单
};

/**
 * @brief 订单有效期枚举
 *
 * 定义了订单的有效期类型，包括 GTC（一直有效）、IOC（立即成交或取消）、
 * FOK（全部成交否则取消）和 GTX（成交不可取消部分）。
 */
enum class TimeInForce : std::uint8_t {
  GTC = 0,  ///< 一直有效（Good Till Canceled）
  IOC = 1,  ///< 立即成交或取消（Immediate or Cancel）
  FOK = 2,  ///< 全部成交否则取消（Fill or Kill）
  GTX = 3   ///< 成交不可取消部分（Good Till Crossing）
};

/**
 * @brief 下单请求结构
 *
 * 包含下单所需的所有信息，如交易品种、方向、类型、数量、价格等。
 */
struct PlaceOrderRequest final {
  veloz::common::SymbolId symbol;        ///< 交易品种ID
  OrderSide side{OrderSide::Buy};       ///< 订单方向（默认买入）
  OrderType type{OrderType::Limit};     ///< 订单类型（默认限价单）
  TimeInForce tif{TimeInForce::GTC};    ///< 订单有效期（默认一直有效）

  double qty{0.0};                      ///< 订单数量
  std::optional<double> price;          ///< 订单价格（市价单可选）

  std::string client_order_id;          ///< 客户端订单ID（用于唯一标识订单）
  bool reduce_only{false};              ///< 是否为减仓订单（仅在合约交易中使用）
  bool post_only{false};                ///< 是否为只挂单（仅在限价单中使用）
};

/**
 * @brief 取消订单请求结构
 *
 * 包含取消订单所需的信息，如交易品种和客户端订单ID。
 */
struct CancelOrderRequest final {
  veloz::common::SymbolId symbol;        ///< 交易品种ID
  std::string client_order_id;          ///< 客户端订单ID
};

/**
 * @brief 订单状态枚举
 *
 * 定义了订单的各种状态，从新建到完成的整个生命周期。
 */
enum class OrderStatus : std::uint8_t {
  New = 0,              ///< 订单新建
  Accepted = 1,         ///< 订单已接受
  PartiallyFilled = 2,  ///< 订单部分成交
  Filled = 3,           ///< 订单全部成交
  Canceled = 4,         ///< 订单已取消
  Rejected = 5,         ///< 订单已拒绝
  Expired = 6,          ///< 订单已过期
};

/**
 * @brief 执行报告结构
 *
 * 包含订单执行的详细报告信息，如订单状态、成交数量、成交价格等。
 */
struct ExecutionReport final {
  veloz::common::SymbolId symbol;        ///< 交易品种ID
  std::string client_order_id;          ///< 客户端订单ID
  std::string venue_order_id;           ///< 交易场所订单ID
  OrderStatus status{OrderStatus::New}; ///< 订单状态

  double last_fill_qty{0.0};            ///< 最后一次成交数量
  double last_fill_price{0.0};          ///< 最后一次成交价格

  std::int64_t ts_exchange_ns{0};      ///< 交易所时间戳（纳秒）
  std::int64_t ts_recv_ns{0};          ///< 接收时间戳（纳秒）
};

} // namespace veloz::exec
