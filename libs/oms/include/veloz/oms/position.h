#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"

#include <cstdint>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::oms {

/**
 * @brief Position side enumeration
 */
enum class PositionSide : std::uint8_t {
  None = 0,
  Long = 1,
  Short = 2,
};

/**
 * @brief Cost basis calculation method
 */
enum class CostBasisMethod : std::uint8_t {
  WeightedAverage = 0, // Current default - weighted average cost
  FIFO = 1,            // First-In-First-Out
};

/**
 * @brief Position lot for FIFO tracking
 *
 * Represents a single entry lot with quantity, price, and metadata.
 */
struct PositionLot {
  double quantity{0.0};
  double price{0.0};
  int64_t timestamp_ns{0};
  kj::String order_id; // Originating order

  PositionLot() = default;
  PositionLot(double qty, double px, int64_t ts, kj::String oid)
      : quantity(qty), price(px), timestamp_ns(ts), order_id(kj::mv(oid)) {}

  // Move-only due to kj::String
  PositionLot(PositionLot&&) noexcept = default;
  PositionLot& operator=(PositionLot&&) noexcept = default;
  PositionLot(const PositionLot&) = delete;
  PositionLot& operator=(const PositionLot&) = delete;
};

/**
 * @brief Position snapshot for point-in-time state
 */
struct PositionSnapshot {
  kj::String symbol;
  double size{0.0};
  double avg_price{0.0};
  double realized_pnl{0.0};
  double unrealized_pnl{0.0};
  PositionSide side{PositionSide::None};
  int64_t timestamp_ns{0};
};

/**
 * @brief Position class with FIFO and weighted average cost basis support
 */
class Position final {
public:
  Position() = default;
  explicit Position(veloz::common::SymbolId symbol);

  // Copy constructor (needed since SymbolId contains move-only kj::String)
  Position(const Position& other);
  Position& operator=(const Position& other);
  // Move constructor
  Position(Position&& other) noexcept = default;
  Position& operator=(Position&& other) noexcept = default;

  // Basic accessors
  [[nodiscard]] const veloz::common::SymbolId& symbol() const;
  [[nodiscard]] double size() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] PositionSide side() const;

  // PnL calculations
  [[nodiscard]] double realized_pnl() const;
  [[nodiscard]] double unrealized_pnl(double current_price) const;
  [[nodiscard]] double total_pnl(double current_price) const;

  // Cost basis configuration
  void set_cost_basis_method(CostBasisMethod method);
  [[nodiscard]] CostBasisMethod cost_basis_method() const;

  // Apply a fill to update position
  void apply_fill(veloz::exec::OrderSide side, double qty, double price);

  // Apply fill from ExecutionReport directly
  void apply_execution_report(const veloz::exec::ExecutionReport& report,
                              veloz::exec::OrderSide side);

  // Position lot access (for FIFO tracking)
  [[nodiscard]] const kj::Vector<PositionLot>& lots() const;
  [[nodiscard]] size_t lot_count() const;

  // Position snapshot
  [[nodiscard]] PositionSnapshot snapshot(double current_price) const;

  // Reset position (e.g., after reconciliation)
  void reset();

  // Check if position is flat (zero size)
  [[nodiscard]] bool is_flat() const;

  // Notional value at given price
  [[nodiscard]] double notional_value(double current_price) const;

private:
  // Apply fill using weighted average method
  void apply_fill_weighted_average(veloz::exec::OrderSide side, double qty, double price);

  // Apply fill using FIFO method
  void apply_fill_fifo(veloz::exec::OrderSide side, double qty, double price, int64_t timestamp_ns,
                       kj::StringPtr order_id);

  veloz::common::SymbolId symbol_;
  double size_{0.0};         // Positive = long, Negative = short
  double avg_price_{0.0};    // Average entry price (weighted average mode)
  double realized_pnl_{0.0}; // Cumulative realized PnL

  CostBasisMethod cost_basis_method_{CostBasisMethod::WeightedAverage};
  kj::Vector<PositionLot> lots_; // For FIFO tracking
};

/**
 * @brief Position manager for multi-symbol position tracking
 *
 * Thread-safe position management with callbacks for position updates.
 * Uses kj::MutexGuarded for thread safety per architect recommendation.
 */
class PositionManager final {
public:
  using PositionUpdateCallback = kj::Function<void(const Position&)>;

  PositionManager() = default;
  ~PositionManager() = default;

  // Non-copyable, non-movable (owns mutex)
  PositionManager(const PositionManager&) = delete;
  PositionManager& operator=(const PositionManager&) = delete;
  PositionManager(PositionManager&&) = delete;
  PositionManager& operator=(PositionManager&&) = delete;

  // Position access
  [[nodiscard]] kj::Maybe<const Position&> get_position(kj::StringPtr symbol) const;
  [[nodiscard]] Position& get_or_create_position(const veloz::common::SymbolId& symbol);

  // Apply execution report to update position
  void apply_execution_report(const veloz::exec::ExecutionReport& report,
                              veloz::exec::OrderSide side);

  // Aggregation
  [[nodiscard]] double total_unrealized_pnl(const kj::HashMap<kj::String, double>& prices) const;
  [[nodiscard]] double total_realized_pnl() const;
  [[nodiscard]] double total_notional(const kj::HashMap<kj::String, double>& prices) const;

  // Iteration
  void for_each_position(kj::Function<void(const Position&)> callback) const;

  // Get all position snapshots
  [[nodiscard]] kj::Vector<PositionSnapshot>
  get_all_snapshots(const kj::HashMap<kj::String, double>& prices) const;

  // Position count
  [[nodiscard]] size_t position_count() const;

  // Clear all positions
  void clear();

  // Reconciliation
  void reconcile_with_exchange(kj::ArrayPtr<const Position> exchange_positions);

  // Callbacks for position updates
  void set_position_update_callback(PositionUpdateCallback callback);

  // Configuration
  void set_default_cost_basis_method(CostBasisMethod method);

private:
  // Thread-safe state (architect recommendation)
  struct State {
    kj::HashMap<kj::String, Position> positions;
    kj::Maybe<PositionUpdateCallback> on_position_update;
    CostBasisMethod default_cost_basis_method{CostBasisMethod::WeightedAverage};
  };
  mutable kj::MutexGuarded<State> state_;

  // Notify callback of position update (called with lock held)
  void notify_position_update(const Position& position, State& state);
};

} // namespace veloz::oms
