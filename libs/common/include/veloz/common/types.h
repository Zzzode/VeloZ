#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>

namespace veloz::common {

enum class Venue : std::uint8_t {
  Unknown = 0,
  Binance = 1,
  Okx = 2,
  Bybit = 3,
};

inline kj::StringPtr to_string(Venue venue) {
  switch (venue) {
  case Venue::Binance:
    return "Binance"_kj;
  case Venue::Okx:
    return "OKX"_kj;
  case Venue::Bybit:
    return "Bybit"_kj;
  default:
    return "Unknown"_kj;
  }
}

inline Venue from_string(kj::StringPtr str) {
  if (str == "Binance" || str == "binance")
    return Venue::Binance;
  if (str == "OKX" || str == "Okx" || str == "okx")
    return Venue::Okx;
  if (str == "Bybit" || str == "bybit")
    return Venue::Bybit;
  return Venue::Unknown;
}

enum class MarketKind : std::uint8_t {
  Unknown = 0,
  Spot = 1,
  LinearPerp = 2,
  InversePerp = 3,
};

struct SymbolId final {
  kj::String value;

  SymbolId() = default;
  SymbolId(kj::StringPtr v) : value(kj::heapString(v)) {}
  explicit SymbolId(kj::String&& v) : value(kj::mv(v)) {}

  // Copy constructor (needed since kj::String is move-only)
  SymbolId(const SymbolId& other) : value(kj::heapString(other.value)) {}
  SymbolId& operator=(const SymbolId& other) {
    value = kj::heapString(other.value);
    return *this;
  }
  // Move constructor
  SymbolId(SymbolId&& other) noexcept = default;
  SymbolId& operator=(SymbolId&& other) noexcept = default;

  SymbolId& operator=(kj::StringPtr v) {
    value = kj::heapString(v);
    return *this;
  }

  SymbolId& operator=(kj::String&& v) {
    value = kj::mv(v);
    return *this;
  }

  bool operator<(const SymbolId& other) const {
    return value < other.value;
  }

  bool operator==(const SymbolId& other) const {
    return value == other.value;
  }

  // Allow comparison with kj::StringPtr (string literals)
  bool operator==(kj::StringPtr str) const {
    return value == str;
  }
};

// Symmetric operator for kj::StringPtr == SymbolId
inline bool operator==(kj::StringPtr str, const SymbolId& symbol) {
  return symbol == str;
}

} // namespace veloz::common
