#pragma once

#include <cstdint>
#include <string>

namespace veloz::common {

enum class Venue : std::uint8_t {
  Unknown = 0,
  Binance = 1,
  Okx = 2,
  Bybit = 3,
};

enum class MarketKind : std::uint8_t {
  Unknown = 0,
  Spot = 1,
  LinearPerp = 2,
  InversePerp = 3,
};

struct SymbolId final {
  std::string value;

  SymbolId() = default;
  SymbolId(const char* v) : value(v) {}
  explicit SymbolId(std::string v) : value(std::move(v)) {}

  SymbolId& operator=(const std::string& v) {
    value = v;
    return *this;
  }

  SymbolId& operator=(const char* v) {
    value = v;
    return *this;
  }
};

} // namespace veloz::common
