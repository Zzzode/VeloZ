#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <string>

namespace veloz::market {

enum class MarketEventType : std::uint8_t {
  Unknown = 0,
  Trade = 1,
  BookTop = 2,
  Kline = 3,
};

struct MarketEvent final {
  MarketEventType type{MarketEventType::Unknown};
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  veloz::common::MarketKind market{veloz::common::MarketKind::Unknown};
  veloz::common::SymbolId symbol{};

  std::int64_t ts_exchange_ns{0};
  std::int64_t ts_recv_ns{0};
  std::int64_t ts_pub_ns{0};

  std::string payload;
};

} // namespace veloz::market
