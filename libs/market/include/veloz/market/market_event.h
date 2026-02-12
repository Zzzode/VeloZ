/**
 * @file market_event.h
 * @brief Core interfaces and implementations for the market event module
 *
 * This file contains the core interfaces and implementations for the market event module
 * in the VeloZ quantitative trading framework, including market event type definitions,
 * market data structures, and event handling related functionality.
 *
 * The market event system is one of the core components of the framework, responsible for
 * handling various types of market data including trade data, order book data, candlestick data,
 * and providing a unified event handling interface.
 */

#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace veloz::market {

/**
 * @brief Market event type enumeration
 *
 * Defines various market event types supported by the framework, including trade data,
 * order book data, candlestick data, ticker data, etc.
 */
enum class MarketEventType : std::uint8_t {
  Unknown = 0,     ///< Unknown event type
  Trade = 1,       ///< Trade data event
  BookTop = 2,     ///< Order book top data event
  BookDelta = 3,   ///< Order book delta data event
  Kline = 4,       ///< Candlestick data event
  Ticker = 5,      ///< Ticker data event
  FundingRate = 6, ///< Funding rate event
  MarkPrice = 7,   ///< Mark price event
};

/**
 * @brief Trade data structure
 *
 * Contains detailed information about a single trade, such as price, quantity, trade ID, etc.
 */
struct TradeData {
  double price{0.0};          ///< Trade price
  double qty{0.0};            ///< Trade quantity
  bool is_buyer_maker{false}; ///< Whether buyer is maker
  std::int64_t trade_id{0};   ///< Trade ID

  auto operator<=>(const TradeData&) const = default; // C++20 spaceship
};

/**
 * @brief Order book level data structure
 *
 * Contains price and quantity information for a single level in the order book.
 */
struct BookLevel {
  double price{0.0}; ///< Level price
  double qty{0.0};   ///< Level quantity

  auto operator<=>(const BookLevel&) const = default;
};

/**
 * @brief Order book data structure
 *
 * Contains complete or incremental order book data, including bid and ask levels.
 */
struct BookData {
  std::vector<BookLevel> bids; ///< Bid levels list
  std::vector<BookLevel> asks; ///< Ask levels list
  std::int64_t sequence{0};    ///< Order book sequence number

  auto operator<=>(const BookData&) const = default;
};

/**
 * @brief Candlestick data structure
 *
 * Contains detailed information about a single candlestick, such as open, close, high, low prices,
 * etc.
 */
struct KlineData {
  double open{0.0};           ///< Open price
  double high{0.0};           ///< High price
  double low{0.0};            ///< Low price
  double close{0.0};          ///< Close price
  double volume{0.0};         ///< Volume
  std::int64_t start_time{0}; ///< Candlestick start time
  std::int64_t close_time{0}; ///< Candlestick close time

  auto operator<=>(const KlineData&) const = default;
};

/**
 * @brief Market event structure
 *
 * Contains complete information about a market event, including event type, venue, symbol,
 * timestamps, event data, etc.
 */
struct MarketEvent final {
  MarketEventType type{MarketEventType::Unknown};                       ///< Event type
  veloz::common::Venue venue{veloz::common::Venue::Unknown};            ///< Trading venue
  veloz::common::MarketKind market{veloz::common::MarketKind::Unknown}; ///< Market type
  veloz::common::SymbolId symbol{};                                     ///< Trading symbol ID

  std::int64_t ts_exchange_ns{0}; ///< Exchange timestamp (nanoseconds)
  std::int64_t ts_recv_ns{0};     ///< Receive timestamp (nanoseconds)
  std::int64_t ts_pub_ns{0};      ///< Publish timestamp (nanoseconds)

  // Variant for typed access (optional, can parse from payload)
  std::variant<std::monostate, TradeData, BookData, KlineData> data; ///< Event data

  // Raw JSON payload for backward compatibility
  std::string payload; ///< Raw JSON payload

  // Helper: latency from exchange to publish
  /**
   * @brief Calculate latency from exchange to publish (nanoseconds)
   * @return Latency time (nanoseconds)
   */
  [[nodiscard]] std::int64_t exchange_to_pub_ns() const {
    if (ts_pub_ns == 0 || ts_exchange_ns == 0)
      return 0;
    if (ts_pub_ns < ts_exchange_ns)
      return 0; // Clock skew protection
    return ts_pub_ns - ts_exchange_ns;
  }

  // Helper: receive to publish latency
  /**
   * @brief Calculate latency from receive to publish (nanoseconds)
   * @return Latency time (nanoseconds)
   */
  [[nodiscard]] std::int64_t recv_to_pub_ns() const {
    if (ts_pub_ns == 0 || ts_recv_ns == 0)
      return 0;
    if (ts_pub_ns < ts_recv_ns)
      return 0; // Clock skew protection
    return ts_pub_ns - ts_recv_ns;
  }
};

} // namespace veloz::market
