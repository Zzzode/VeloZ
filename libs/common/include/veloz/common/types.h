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

inline std::string to_string(Venue venue) {
    switch (venue) {
        case Venue::Binance: return "Binance";
        case Venue::Okx: return "OKX";
        case Venue::Bybit: return "Bybit";
        default: return "Unknown";
    }
}

inline Venue from_string(const std::string& str) {
    if (str == "Binance" || str == "binance") return Venue::Binance;
    if (str == "OKX" || str == "Okx" || str == "okx") return Venue::Okx;
    if (str == "Bybit" || str == "bybit") return Venue::Bybit;
    return Venue::Unknown;
}

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

  bool operator<(const SymbolId& other) const {
    return value < other.value;
  }

  bool operator==(const SymbolId& other) const {
    return value == other.value;
  }
};

} // namespace veloz::common
