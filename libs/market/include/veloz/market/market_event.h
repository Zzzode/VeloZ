#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace veloz::market {

enum class MarketEventType : std::uint8_t {
  Unknown = 0,
  Trade = 1,
  BookTop = 2,
  BookDelta = 3,
  Kline = 4,
  Ticker = 5,
  FundingRate = 6,
  MarkPrice = 7,
};

struct TradeData {
  double price{0.0};
  double qty{0.0};
  bool is_buyer_maker{false};
  std::int64_t trade_id{0};
};

struct BookLevel {
  double price{0.0};
  double qty{0.0};
};

struct BookData {
  std::vector<BookLevel> bids;
  std::vector<BookLevel> asks;
  std::int64_t sequence{0};
};

struct KlineData {
  double open{0.0};
  double high{0.0};
  double low{0.0};
  double close{0.0};
  double volume{0.0};
  std::int64_t start_time{0};
  std::int64_t close_time{0};
};

struct MarketEvent final {
  MarketEventType type{MarketEventType::Unknown};
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  veloz::common::MarketKind market{veloz::common::MarketKind::Unknown};
  veloz::common::SymbolId symbol{};

  std::int64_t ts_exchange_ns{0};
  std::int64_t ts_recv_ns{0};
  std::int64_t ts_pub_ns{0};

  // Variant for typed access (optional, can parse from payload)
  std::variant<std::monostate, TradeData, BookData, KlineData> data;

  // Raw JSON payload for backward compatibility
  std::string payload;

  // Helper: latency from exchange to publish
  [[nodiscard]] std::int64_t exchange_to_pub_ns() const {
    return ts_pub_ns - ts_exchange_ns;
  }

  // Helper: receive to publish latency
  [[nodiscard]] std::int64_t recv_to_pub_ns() const {
    return ts_pub_ns - ts_recv_ns;
  }
};

} // namespace veloz::market
