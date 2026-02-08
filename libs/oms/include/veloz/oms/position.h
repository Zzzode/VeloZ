#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"

#include <cstdint>

namespace veloz::oms {

enum class PositionSide : std::uint8_t {
  None = 0,
  Long = 1,
  Short = 2,
};

class Position final {
public:
  Position() = default;
  explicit Position(veloz::common::SymbolId symbol);

  [[nodiscard]] const veloz::common::SymbolId& symbol() const;
  [[nodiscard]] double size() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] PositionSide side() const;

  [[nodiscard]] double realized_pnl() const;
  [[nodiscard]] double unrealized_pnl(double current_price) const;

  // Apply a fill to update position
  void apply_fill(veloz::exec::OrderSide side, double qty, double price);

  // Reset position (e.g., after reconciliation)
  void reset();

private:
  veloz::common::SymbolId symbol_;
  double size_{0.0};         // Positive = long, Negative = short
  double avg_price_{0.0};    // Average entry price
  double realized_pnl_{0.0}; // Cumulative realized PnL
};

} // namespace veloz::oms
