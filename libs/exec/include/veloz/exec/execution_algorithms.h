#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_coordinator.h"
#include "veloz/exec/order_api.h"
#include "veloz/exec/smart_order_router.h"

#include <cstdint>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Execution algorithm type
enum class AlgorithmType : std::uint8_t {
  TWAP = 0,  // Time-Weighted Average Price
  VWAP = 1,  // Volume-Weighted Average Price
  POV = 2,   // Percentage of Volume
  IS = 3,    // Implementation Shortfall
};

// Algorithm execution state
enum class AlgorithmState : std::uint8_t {
  Pending = 0,
  Running = 1,
  Paused = 2,
  Completed = 3,
  Cancelled = 4,
  Failed = 5,
};

// Child order from algorithm
struct ChildOrder {
  kj::String client_order_id;
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  double quantity{0.0};
  double price{0.0};
  OrderStatus status{OrderStatus::New};
  double filled_qty{0.0};
  double filled_price{0.0};
  std::int64_t created_at_ns{0};
  std::int64_t filled_at_ns{0};
};

// Algorithm execution progress
struct AlgorithmProgress {
  kj::String algo_id;
  AlgorithmType type{AlgorithmType::TWAP};
  AlgorithmState state{AlgorithmState::Pending};
  double target_quantity{0.0};
  double filled_quantity{0.0};
  double average_price{0.0};
  double progress_pct{0.0};
  std::size_t child_orders_sent{0};
  std::size_t child_orders_filled{0};
  std::int64_t start_time_ns{0};
  std::int64_t end_time_ns{0};
  kj::Duration elapsed{0 * kj::NANOSECONDS};
  kj::Duration remaining{0 * kj::NANOSECONDS};
};

// TWAP configuration
struct TwapConfig {
  kj::Duration duration{60 * kj::SECONDS};  // Total execution duration
  kj::Duration slice_interval{5 * kj::SECONDS};  // Time between slices
  double randomization{0.1};  // Random variation in timing (0-1)
  bool use_limit_orders{true};
  double limit_offset_bps{5.0};  // Basis points from mid for limit orders
  double min_slice_qty{0.0};  // Minimum quantity per slice
};

// VWAP configuration
struct VwapConfig {
  kj::Duration duration{60 * kj::SECONDS};
  kj::Duration slice_interval{5 * kj::SECONDS};
  kj::Vector<double> volume_profile;  // Historical volume distribution
  double participation_rate{0.1};  // Max participation rate
  bool use_limit_orders{true};
  double limit_offset_bps{5.0};
};

// Callback for algorithm progress updates
using AlgorithmCallback = kj::Function<void(const AlgorithmProgress&)>;

// Callback for child order fills
using ChildOrderCallback = kj::Function<void(const ChildOrder&)>;

// Base class for execution algorithms
class ExecutionAlgorithm {
public:
  virtual ~ExecutionAlgorithm() noexcept = default;

  // Start the algorithm
  virtual void start() = 0;

  // Pause execution
  virtual void pause() = 0;

  // Resume execution
  virtual void resume() = 0;

  // Cancel the algorithm
  virtual void cancel() = 0;

  // Get current progress
  [[nodiscard]] virtual AlgorithmProgress get_progress() const = 0;

  // Get child orders
  [[nodiscard]] virtual kj::Vector<ChildOrder> get_child_orders() const = 0;

  // Process a tick (for time-based algorithms)
  virtual void on_tick(std::int64_t current_time_ns) = 0;

  // Process market data update
  virtual void on_market_update(double bid, double ask, double volume) = 0;

  // Process child order fill
  virtual void on_fill(kj::StringPtr client_order_id, double qty, double price) = 0;
};

// TWAP execution algorithm
class TwapAlgorithm final : public ExecutionAlgorithm {
public:
  TwapAlgorithm(SmartOrderRouter& router, const veloz::common::SymbolId& symbol, OrderSide side,
                double quantity, const TwapConfig& config);
  ~TwapAlgorithm() noexcept override = default;

  void start() override;
  void pause() override;
  void resume() override;
  void cancel() override;

  [[nodiscard]] AlgorithmProgress get_progress() const override;
  [[nodiscard]] kj::Vector<ChildOrder> get_child_orders() const override;

  void on_tick(std::int64_t current_time_ns) override;
  void on_market_update(double bid, double ask, double volume) override;
  void on_fill(kj::StringPtr client_order_id, double qty, double price) override;

  // Set callbacks
  void set_progress_callback(AlgorithmCallback callback);
  void set_child_order_callback(ChildOrderCallback callback);

  // Get algorithm ID
  [[nodiscard]] kj::StringPtr algo_id() const;

private:
  void send_slice(std::int64_t current_time_ns);
  double calculate_slice_qty() const;

  struct TwapState {
    kj::String algo_id;
    AlgorithmState state{AlgorithmState::Pending};
    veloz::common::SymbolId symbol;
    OrderSide side{OrderSide::Buy};
    double target_qty{0.0};
    double filled_qty{0.0};
    double total_value{0.0};  // For average price calculation
    TwapConfig config;

    std::int64_t start_time_ns{0};
    std::int64_t last_slice_time_ns{0};
    std::size_t slices_sent{0};
    std::size_t total_slices{0};

    kj::Vector<ChildOrder> child_orders;

    double current_bid{0.0};
    double current_ask{0.0};

    kj::Maybe<AlgorithmCallback> progress_callback;
    kj::Maybe<ChildOrderCallback> child_order_callback;
  };

  SmartOrderRouter& router_;
  kj::MutexGuarded<TwapState> guarded_;
};

// VWAP execution algorithm
class VwapAlgorithm final : public ExecutionAlgorithm {
public:
  VwapAlgorithm(SmartOrderRouter& router, const veloz::common::SymbolId& symbol, OrderSide side,
                double quantity, const VwapConfig& config);
  ~VwapAlgorithm() noexcept override = default;

  void start() override;
  void pause() override;
  void resume() override;
  void cancel() override;

  [[nodiscard]] AlgorithmProgress get_progress() const override;
  [[nodiscard]] kj::Vector<ChildOrder> get_child_orders() const override;

  void on_tick(std::int64_t current_time_ns) override;
  void on_market_update(double bid, double ask, double volume) override;
  void on_fill(kj::StringPtr client_order_id, double qty, double price) override;

  // Set callbacks
  void set_progress_callback(AlgorithmCallback callback);
  void set_child_order_callback(ChildOrderCallback callback);

  // Get algorithm ID
  [[nodiscard]] kj::StringPtr algo_id() const;

private:
  void send_slice(std::int64_t current_time_ns);
  double calculate_slice_qty(std::size_t slice_index) const;

  struct VwapState {
    kj::String algo_id;
    AlgorithmState state{AlgorithmState::Pending};
    veloz::common::SymbolId symbol;
    OrderSide side{OrderSide::Buy};
    double target_qty{0.0};
    double filled_qty{0.0};
    double total_value{0.0};
    VwapConfig config;

    std::int64_t start_time_ns{0};
    std::int64_t last_slice_time_ns{0};
    std::size_t slices_sent{0};
    std::size_t total_slices{0};

    kj::Vector<ChildOrder> child_orders;

    double current_bid{0.0};
    double current_ask{0.0};
    double cumulative_volume{0.0};

    kj::Maybe<AlgorithmCallback> progress_callback;
    kj::Maybe<ChildOrderCallback> child_order_callback;
  };

  SmartOrderRouter& router_;
  kj::MutexGuarded<VwapState> guarded_;
};

// Algorithm manager for running multiple algorithms
class AlgorithmManager final {
public:
  explicit AlgorithmManager(SmartOrderRouter& router);

  // Create and start a TWAP algorithm
  kj::String start_twap(const veloz::common::SymbolId& symbol, OrderSide side, double quantity,
                        const TwapConfig& config);

  // Create and start a VWAP algorithm
  kj::String start_vwap(const veloz::common::SymbolId& symbol, OrderSide side, double quantity,
                        const VwapConfig& config);

  // Control algorithms
  void pause(kj::StringPtr algo_id);
  void resume(kj::StringPtr algo_id);
  void cancel(kj::StringPtr algo_id);

  // Get algorithm progress
  [[nodiscard]] kj::Maybe<AlgorithmProgress> get_progress(kj::StringPtr algo_id) const;

  // Get all running algorithms
  [[nodiscard]] kj::Vector<AlgorithmProgress> get_all_progress() const;

  // Process time tick for all algorithms
  void on_tick(std::int64_t current_time_ns);

  // Process market data for a symbol
  void on_market_update(const veloz::common::SymbolId& symbol, double bid, double ask,
                        double volume);

  // Process fill for a child order
  void on_fill(kj::StringPtr client_order_id, double qty, double price);

  // Set global callbacks
  void set_progress_callback(AlgorithmCallback callback);
  void set_child_order_callback(ChildOrderCallback callback);

  // Cleanup completed algorithms
  void cleanup_completed();

private:
  struct ManagerState {
    kj::HashMap<kj::String, kj::Own<ExecutionAlgorithm>> algorithms;
    kj::HashMap<kj::String, veloz::common::SymbolId> algo_symbols;  // algo_id -> symbol
    kj::HashMap<kj::String, kj::String> child_to_algo;  // child_order_id -> algo_id
    kj::Maybe<AlgorithmCallback> progress_callback;
    kj::Maybe<ChildOrderCallback> child_order_callback;
    std::size_t algo_counter{0};
  };

  SmartOrderRouter& router_;
  kj::MutexGuarded<ManagerState> guarded_;
};

} // namespace veloz::exec
