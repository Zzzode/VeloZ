#include "veloz/oms/position.h"

#include <chrono>
#include <cmath>
#include <kj/debug.h>

namespace veloz::oms {

namespace {
constexpr double EPSILON = 1e-9;

int64_t current_timestamp_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
} // namespace

// ============================================================================
// Position Implementation
// ============================================================================

Position::Position(veloz::common::SymbolId symbol) : symbol_(kj::mv(symbol)) {}

Position::Position(const Position& other)
    : symbol_(veloz::common::SymbolId{kj::str(other.symbol_.value)}), size_(other.size_),
      avg_price_(other.avg_price_), realized_pnl_(other.realized_pnl_),
      cost_basis_method_(other.cost_basis_method_) {
  // Copy lots (PositionLot is move-only, so we need to recreate)
  for (const auto& lot : other.lots_) {
    lots_.add(PositionLot{lot.quantity, lot.price, lot.timestamp_ns, kj::str(lot.order_id)});
  }
}

Position& Position::operator=(const Position& other) {
  if (this != &other) {
    symbol_ = veloz::common::SymbolId{kj::str(other.symbol_.value)};
    size_ = other.size_;
    avg_price_ = other.avg_price_;
    realized_pnl_ = other.realized_pnl_;
    cost_basis_method_ = other.cost_basis_method_;

    lots_.clear();
    for (const auto& lot : other.lots_) {
      lots_.add(PositionLot{lot.quantity, lot.price, lot.timestamp_ns, kj::str(lot.order_id)});
    }
  }
  return *this;
}

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
  if (std::abs(size_) < EPSILON)
    return PositionSide::None;
  return size_ > 0 ? PositionSide::Long : PositionSide::Short;
}

double Position::realized_pnl() const {
  return realized_pnl_;
}

double Position::unrealized_pnl(double current_price) const {
  if (std::abs(size_) < EPSILON)
    return 0.0;

  if (size_ > 0) {
    // Long: (current - avg) * size
    return (current_price - avg_price_) * size_;
  } else {
    // Short: (avg - current) * |size|
    return (avg_price_ - current_price) * (-size_);
  }
}

double Position::total_pnl(double current_price) const {
  return realized_pnl_ + unrealized_pnl(current_price);
}

void Position::set_cost_basis_method(CostBasisMethod method) {
  cost_basis_method_ = method;
}

CostBasisMethod Position::cost_basis_method() const {
  return cost_basis_method_;
}

void Position::apply_fill(veloz::exec::OrderSide side, double qty, double price) {
  if (cost_basis_method_ == CostBasisMethod::FIFO) {
    apply_fill_fifo(side, qty, price, current_timestamp_ns(), ""_kj);
  } else {
    apply_fill_weighted_average(side, qty, price);
  }
}

void Position::apply_execution_report(const veloz::exec::ExecutionReport& report,
                                      veloz::exec::OrderSide side) {
  if (report.last_fill_qty <= 0.0) {
    return; // No fill to apply
  }

  if (cost_basis_method_ == CostBasisMethod::FIFO) {
    apply_fill_fifo(side, report.last_fill_qty, report.last_fill_price, report.ts_recv_ns,
                    report.client_order_id);
  } else {
    apply_fill_weighted_average(side, report.last_fill_qty, report.last_fill_price);
  }
}

const kj::Vector<PositionLot>& Position::lots() const {
  return lots_;
}

size_t Position::lot_count() const {
  return lots_.size();
}

PositionSnapshot Position::snapshot(double current_price) const {
  PositionSnapshot snap;
  snap.symbol = kj::str(symbol_.value);
  snap.size = size_;
  snap.avg_price = avg_price_;
  snap.realized_pnl = realized_pnl_;
  snap.unrealized_pnl = unrealized_pnl(current_price);
  snap.side = side();
  snap.timestamp_ns = current_timestamp_ns();
  return snap;
}

void Position::reset() {
  size_ = 0.0;
  avg_price_ = 0.0;
  realized_pnl_ = 0.0;
  lots_.clear();
}

bool Position::is_flat() const {
  return std::abs(size_) < EPSILON;
}

double Position::notional_value(double current_price) const {
  return std::abs(size_) * current_price;
}

void Position::apply_fill_weighted_average(veloz::exec::OrderSide side, double qty, double price) {
  if (side == veloz::exec::OrderSide::Buy) {
    // Buy: add to long or reduce short
    if (size_ < 0) {
      // Reducing short position
      double close_qty = std::fmin(qty, -size_);
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
      double close_qty = std::fmin(qty, size_);
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
  if (std::abs(size_) < EPSILON) {
    size_ = 0.0;
    avg_price_ = 0.0;
  }
}

void Position::apply_fill_fifo(veloz::exec::OrderSide side, double qty, double price,
                               int64_t timestamp_ns, kj::StringPtr order_id) {
  bool is_buy = (side == veloz::exec::OrderSide::Buy);
  bool is_long = (size_ > 0);
  bool is_short = (size_ < 0);

  // Case 1: Opening or adding to position (same direction)
  if ((is_buy && !is_short) || (!is_buy && !is_long)) {
    // Add new lot
    lots_.add(PositionLot{qty, price, timestamp_ns, kj::str(order_id)});

    // Update size
    if (is_buy) {
      size_ += qty;
    } else {
      size_ -= qty;
    }

    // Recalculate average price from lots
    double total_qty = 0.0;
    double total_value = 0.0;
    for (const auto& lot : lots_) {
      total_qty += lot.quantity;
      total_value += lot.quantity * lot.price;
    }
    if (total_qty > EPSILON) {
      avg_price_ = total_value / total_qty;
    }
    return;
  }

  // Case 2: Reducing or closing position (opposite direction)
  double remaining_qty = qty;

  while (remaining_qty > EPSILON && !lots_.empty()) {
    auto& oldest_lot = lots_.front();

    if (oldest_lot.quantity <= remaining_qty) {
      // Close entire lot
      double lot_qty = oldest_lot.quantity;
      double lot_price = oldest_lot.price;

      // Calculate realized PnL for this lot
      if (is_long) {
        // Closing long: sell price - buy price
        realized_pnl_ += lot_qty * (price - lot_price);
      } else {
        // Closing short: buy price - sell price (short sold at lot_price, buying back at price)
        realized_pnl_ += lot_qty * (lot_price - price);
      }

      remaining_qty -= lot_qty;

      // Remove the lot (shift all elements)
      kj::Vector<PositionLot> new_lots;
      for (size_t i = 1; i < lots_.size(); ++i) {
        new_lots.add(kj::mv(lots_[i]));
      }
      lots_ = kj::mv(new_lots);
    } else {
      // Partial close of oldest lot
      double close_qty = remaining_qty;
      double lot_price = oldest_lot.price;

      // Calculate realized PnL
      if (is_long) {
        realized_pnl_ += close_qty * (price - lot_price);
      } else {
        realized_pnl_ += close_qty * (lot_price - price);
      }

      oldest_lot.quantity -= close_qty;
      remaining_qty = 0.0;
    }
  }

  // Update size
  if (is_buy) {
    size_ += qty;
  } else {
    size_ -= qty;
  }

  // If we still have remaining qty, we're flipping position
  if (remaining_qty > EPSILON) {
    // Add new lot for the flipped position
    lots_.add(PositionLot{remaining_qty, price, timestamp_ns, kj::str(order_id)});
    avg_price_ = price;
  } else if (lots_.empty()) {
    // Position closed
    avg_price_ = 0.0;
  } else {
    // Recalculate average price from remaining lots
    double total_qty = 0.0;
    double total_value = 0.0;
    for (const auto& lot : lots_) {
      total_qty += lot.quantity;
      total_value += lot.quantity * lot.price;
    }
    if (total_qty > EPSILON) {
      avg_price_ = total_value / total_qty;
    }
  }

  // Normalize near-zero size
  if (std::abs(size_) < EPSILON) {
    size_ = 0.0;
    avg_price_ = 0.0;
    lots_.clear();
  }
}

// ============================================================================
// PositionManager Implementation
// ============================================================================

kj::Maybe<const Position&> PositionManager::get_position(kj::StringPtr symbol) const {
  auto lock = state_.lockShared();
  KJ_IF_SOME(pos, lock->positions.find(symbol)) {
    return pos;
  }
  return kj::none;
}

Position& PositionManager::get_or_create_position(const veloz::common::SymbolId& symbol) {
  auto lock = state_.lockExclusive();

  KJ_IF_SOME(pos, lock->positions.find(symbol.value)) {
    return pos;
  }

  // Create new position
  Position new_pos(veloz::common::SymbolId{kj::str(symbol.value)});
  new_pos.set_cost_basis_method(lock->default_cost_basis_method);

  auto key = kj::str(symbol.value);
  lock->positions.insert(kj::mv(key), kj::mv(new_pos));

  // Return reference to inserted position
  return KJ_REQUIRE_NONNULL(lock->positions.find(symbol.value));
}

void PositionManager::apply_execution_report(const veloz::exec::ExecutionReport& report,
                                             veloz::exec::OrderSide side) {
  if (report.last_fill_qty <= 0.0) {
    return;
  }

  auto lock = state_.lockExclusive();

  // Get or create position
  Position* pos_ptr = nullptr;
  KJ_IF_SOME(pos, lock->positions.find(report.symbol.value)) {
    pos_ptr = &pos;
  }
  else {
    Position new_pos(veloz::common::SymbolId{kj::str(report.symbol.value)});
    new_pos.set_cost_basis_method(lock->default_cost_basis_method);

    auto key = kj::str(report.symbol.value);
    lock->positions.insert(kj::mv(key), kj::mv(new_pos));
    pos_ptr = &KJ_REQUIRE_NONNULL(lock->positions.find(report.symbol.value));
  }

  // Apply the fill
  pos_ptr->apply_execution_report(report, side);

  // Notify callback
  notify_position_update(*pos_ptr, *lock);
}

double PositionManager::total_unrealized_pnl(const kj::HashMap<kj::String, double>& prices) const {
  auto lock = state_.lockShared();
  double total = 0.0;

  for (auto& entry : lock->positions) {
    KJ_IF_SOME(price, prices.find(entry.key)) {
      total += entry.value.unrealized_pnl(price);
    }
  }

  return total;
}

double PositionManager::total_realized_pnl() const {
  auto lock = state_.lockShared();
  double total = 0.0;

  for (auto& entry : lock->positions) {
    total += entry.value.realized_pnl();
  }

  return total;
}

double PositionManager::total_notional(const kj::HashMap<kj::String, double>& prices) const {
  auto lock = state_.lockShared();
  double total = 0.0;

  for (auto& entry : lock->positions) {
    KJ_IF_SOME(price, prices.find(entry.key)) {
      total += entry.value.notional_value(price);
    }
  }

  return total;
}

void PositionManager::for_each_position(kj::Function<void(const Position&)> callback) const {
  auto lock = state_.lockShared();

  for (auto& entry : lock->positions) {
    callback(entry.value);
  }
}

kj::Vector<PositionSnapshot>
PositionManager::get_all_snapshots(const kj::HashMap<kj::String, double>& prices) const {
  auto lock = state_.lockShared();
  kj::Vector<PositionSnapshot> snapshots;

  for (auto& entry : lock->positions) {
    double price = 0.0;
    KJ_IF_SOME(p, prices.find(entry.key)) {
      price = p;
    }
    snapshots.add(entry.value.snapshot(price));
  }

  return snapshots;
}

size_t PositionManager::position_count() const {
  auto lock = state_.lockShared();
  return lock->positions.size();
}

void PositionManager::clear() {
  auto lock = state_.lockExclusive();
  lock->positions.clear();
}

void PositionManager::reconcile_with_exchange(kj::ArrayPtr<const Position> exchange_positions) {
  auto lock = state_.lockExclusive();

  // Clear existing positions
  lock->positions.clear();

  // Add exchange positions
  for (const auto& pos : exchange_positions) {
    auto key = kj::str(pos.symbol().value);
    Position new_pos(pos); // Copy
    lock->positions.insert(kj::mv(key), kj::mv(new_pos));
  }
}

void PositionManager::set_position_update_callback(PositionUpdateCallback callback) {
  auto lock = state_.lockExclusive();
  lock->on_position_update = kj::mv(callback);
}

void PositionManager::set_default_cost_basis_method(CostBasisMethod method) {
  auto lock = state_.lockExclusive();
  lock->default_cost_basis_method = method;
}

void PositionManager::notify_position_update(const Position& position, State& state) {
  KJ_IF_SOME(callback, state.on_position_update) {
    callback(position);
  }
}

} // namespace veloz::oms
