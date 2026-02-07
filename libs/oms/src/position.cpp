#include "veloz/oms/position.h"
#include <cmath>

namespace veloz::oms {

Position::Position(veloz::common::SymbolId symbol)
    : symbol_(std::move(symbol)) {}

const veloz::common::SymbolId& Position::symbol() const {
  return symbol_;
}

double Position::size() const {
  return size_;
}

double Position::avg_price() const {
  return avg_price_;
}

PositionSide Position::side() const {
  if (std::abs(size_) < 1e-9) return PositionSide::None;
  return size_ > 0 ? PositionSide::Long : PositionSide::Short;
}

double Position::realized_pnl() const {
  return realized_pnl_;
}

double Position::unrealized_pnl(double current_price) const {
  if (std::abs(size_) < 1e-9) return 0.0;

  if (size_ > 0) {
    // Long: (current - avg) * size
    return (current_price - avg_price_) * size_;
  } else {
    // Short: (avg - current) * |size|
    return (avg_price_ - current_price) * (-size_);
  }
}

void Position::apply_fill(veloz::exec::OrderSide side, double qty, double price) {
  if (side == veloz::exec::OrderSide::Buy) {
    // Buy: add to long or reduce short
    if (size_ < 0) {
      // Reducing short position
      double close_qty = std::min(qty, -size_);
      realized_pnl_ += close_qty * (avg_price_ - price);
      size_ += close_qty;
      qty -= close_qty;
    }

    if (qty > 0) {
      // Opening or adding to long
      if (size_ + qty > 0) {
        avg_price_ = (avg_price_ * size_ + price * qty) / (size_ + qty);
      } else {
        avg_price_ = price;
      }
      size_ += qty;
    }
  } else {
    // Sell: add to short or reduce long
    if (size_ > 0) {
      // Reducing long position
      double close_qty = std::min(qty, size_);
      realized_pnl_ += close_qty * (price - avg_price_);
      size_ -= close_qty;
      qty -= close_qty;
    }

    if (qty > 0) {
      // Opening or adding to short
      if (size_ - qty < 0) {
        avg_price_ = (avg_price_ * (-size_) + price * qty) / ((-size_) + qty);
      } else {
        avg_price_ = price;
      }
      size_ -= qty;
    }
  }

  // Reset if position is closed
  if (std::abs(size_) < 1e-9) {
    size_ = 0.0;
    avg_price_ = 0.0;
  }
}

void Position::reset() {
  size_ = 0.0;
  avg_price_ = 0.0;
  realized_pnl_ = 0.0;
}

} // namespace veloz::oms
