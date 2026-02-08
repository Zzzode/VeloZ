/**
 * @file market_event.h
 * @brief 市场事件模块的核心接口和实现
 *
 * 该文件包含了 VeloZ 量化交易框架中市场事件模块的核心接口和实现，
 * 包括市场事件类型定义、市场数据结构以及事件处理相关功能。
 *
 * 市场事件系统是框架的核心组件之一，负责处理各种类型的市场数据，
 * 包括交易数据、订单簿数据、K线数据等，并提供了统一的事件处理接口。
 */

#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace veloz::market {

/**
 * @brief 市场事件类型枚举
 *
 * 定义了框架支持的各种市场事件类型，包括交易数据、订单簿数据、
 * K线数据、行情数据等。
 */
enum class MarketEventType : std::uint8_t {
  Unknown = 0,    ///< 未知事件类型
  Trade = 1,      ///< 交易数据事件
  BookTop = 2,    ///< 订单簿顶部数据事件
  BookDelta = 3,  ///< 订单簿增量数据事件
  Kline = 4,      ///< K线数据事件
  Ticker = 5,     ///< 行情数据事件
  FundingRate = 6, ///< 资金费率事件
  MarkPrice = 7,  ///< 标记价格事件
};

/**
 * @brief 交易数据结构
 *
 * 包含单笔交易的详细信息，如价格、数量、交易ID等。
 */
struct TradeData {
  double price{0.0};              ///< 交易价格
  double qty{0.0};                ///< 交易数量
  bool is_buyer_maker{false};     ///< 是否为买方主动成交
  std::int64_t trade_id{0};       ///< 交易ID
};

/**
 * @brief 订单簿价位数据结构
 *
 * 包含订单簿中单个价位的价格和数量信息。
 */
struct BookLevel {
  double price{0.0};              ///< 价位价格
  double qty{0.0};                ///< 价位数量
};

/**
 * @brief 订单簿数据结构
 *
 * 包含订单簿的完整或增量数据，包括买盘和卖盘价位信息。
 */
struct BookData {
  std::vector<BookLevel> bids;    ///< 买盘价位列表
  std::vector<BookLevel> asks;    ///< 卖盘价位列表
  std::int64_t sequence{0};       ///< 订单簿序列号
};

/**
 * @brief K线数据结构
 *
 * 包含单根 K 线的详细信息，如开盘价、收盘价、最高价、最低价等。
 */
struct KlineData {
  double open{0.0};               ///< 开盘价
  double high{0.0};               ///< 最高价
  double low{0.0};                ///< 最低价
  double close{0.0};              ///< 收盘价
  double volume{0.0};             ///< 成交量
  std::int64_t start_time{0};     ///< K线开始时间
  std::int64_t close_time{0};     ///< K线结束时间
};

/**
 * @brief 市场事件结构
 *
 * 包含市场事件的完整信息，包括事件类型、交易场所、交易品种、
 * 时间戳、事件数据等。
 */
struct MarketEvent final {
  MarketEventType type{MarketEventType::Unknown};  ///< 事件类型
  veloz::common::Venue venue{veloz::common::Venue::Unknown};  ///< 交易场所
  veloz::common::MarketKind market{veloz::common::MarketKind::Unknown};  ///< 市场类型
  veloz::common::SymbolId symbol{};  ///< 交易品种ID

  std::int64_t ts_exchange_ns{0};  ///< 交易所时间戳（纳秒）
  std::int64_t ts_recv_ns{0};      ///< 接收时间戳（纳秒）
  std::int64_t ts_pub_ns{0};       ///< 发布时间戳（纳秒）

  // Variant for typed access (optional, can parse from payload)
  std::variant<std::monostate, TradeData, BookData, KlineData> data;  ///< 事件数据

  // Raw JSON payload for backward compatibility
  std::string payload;  ///< 原始 JSON 负载

  // Helper: latency from exchange to publish
  /**
   * @brief 计算从交易所到发布的延迟（纳秒）
   * @return 延迟时间（纳秒）
   */
  [[nodiscard]] std::int64_t exchange_to_pub_ns() const {
    return ts_pub_ns - ts_exchange_ns;
  }

  // Helper: receive to publish latency
  /**
   * @brief 计算从接收到发布的延迟（纳秒）
   * @return 延迟时间（纳秒）
   */
  [[nodiscard]] std::int64_t recv_to_pub_ns() const {
    return ts_pub_ns - ts_recv_ns;
  }
};

} // namespace veloz::market
