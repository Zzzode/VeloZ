/**
 * @file grid_strategy.h
 * @brief Grid Trading Strategy implementation
 *
 * This file implements a grid trading strategy that places buy and sell orders
 * at predetermined price levels (grid lines) to profit from price oscillations
 * within a defined range.
 */

#pragma once

#include "veloz/strategy/strategy.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::strategy {

/**
 * @brief Grid level state
 *
 * Tracks the state of each grid level including price, order status, and fill info.
 */
struct GridLevel {
  double price{0.0};          ///< Grid level price
  double quantity{0.0};       ///< Order quantity at this level
  bool has_buy_order{false};  ///< Whether a buy order is placed at this level
  bool has_sell_order{false}; ///< Whether a sell order is placed at this level
  bool buy_filled{false};     ///< Whether buy order was filled
  bool sell_filled{false};    ///< Whether sell order was filled
  int buy_fill_count{0};      ///< Number of times buy was filled
  int sell_fill_count{0};     ///< Number of times sell was filled
  double realized_pnl{0.0};   ///< Realized PnL from this level
};

/**
 * @brief Grid configuration mode
 */
enum class GridMode {
  Arithmetic, ///< Equal price spacing between levels
  Geometric   ///< Equal percentage spacing between levels
};

/**
 * @brief Grid Trading Strategy
 *
 * This strategy implements a grid trading approach:
 * - Creates a grid of price levels between upper and lower bounds
 * - Places buy orders below current price and sell orders above
 * - When a buy order fills, places a sell order one grid level above
 * - When a sell order fills, places a buy order one grid level below
 * - Profits from price oscillations within the grid range
 *
 * Trading logic:
 * - Initialize grid levels based on price range and number of grids
 * - Place initial orders: buys below current price, sells above
 * - On buy fill: mark level as bought, place sell at level above
 * - On sell fill: mark level as sold, place buy at level below
 * - Track inventory and PnL per grid level
 *
 * Risk management:
 * - Maximum position size limit
 * - Stop-loss if price breaks out of grid range
 * - Take-profit on total grid profit
 * - Grid rebalancing when price moves significantly
 *
 * Configurable parameters:
 * - upper_price: Upper bound of grid range (required)
 * - lower_price: Lower bound of grid range (required)
 * - grid_count: Number of grid levels (default: 10)
 * - total_investment: Total capital to deploy (default: 1000.0)
 * - grid_mode: 0=Arithmetic, 1=Geometric (default: 0)
 * - take_profit_pct: Take profit percentage (default: 0.0 = disabled)
 * - stop_loss_pct: Stop loss percentage (default: 0.0 = disabled)
 * - trailing_up: Enable trailing upper bound (default: false)
 * - trailing_down: Enable trailing lower bound (default: false)
 * - rebalance_threshold: Price deviation to trigger rebalance (default: 0.0 = disabled)
 */
class GridStrategy : public BaseStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit GridStrategy(const StrategyConfig& config);

  /**
   * @brief Virtual destructor
   */
  ~GridStrategy() noexcept override = default;

  /**
   * @brief Get strategy type
   * @return StrategyType::Grid
   */
  StrategyType get_type() const override;

  /**
   * @brief Handle market event
   * @param event Market event object
   */
  void on_event(const market::MarketEvent& event) override;

  /**
   * @brief Handle timer event
   * @param timestamp Timestamp in milliseconds
   */
  void on_timer(int64_t timestamp) override;

  /**
   * @brief Get trading signals
   * @return List of trading signals
   */
  kj::Vector<exec::PlaceOrderRequest> get_signals() override;

  /**
   * @brief Reset strategy state
   */
  void reset() override;

  /**
   * @brief Get strategy type name (static)
   * @return Strategy type name string
   */
  static kj::StringPtr get_strategy_type();

  /**
   * @brief Check if strategy supports hot-reload of parameters
   * @return true (supports hot-reload)
   */
  bool supports_hot_reload() const override;

  /**
   * @brief Update strategy parameters at runtime
   * @param parameters New parameter values
   * @return Whether update was successful
   */
  bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) override;

  /**
   * @brief Get strategy performance metrics
   * @return Reference to metrics
   */
  kj::Maybe<const StrategyMetrics&> get_metrics() const override;

  // Public getters for testing
  double get_current_price() const {
    return current_price_;
  }
  double get_upper_price() const {
    return upper_price_;
  }
  double get_lower_price() const {
    return lower_price_;
  }
  int get_grid_count() const {
    return grid_count_;
  }
  double get_grid_spacing() const {
    return grid_spacing_;
  }
  double get_total_inventory() const {
    return total_inventory_;
  }
  double get_realized_pnl() const {
    return realized_pnl_;
  }
  double get_unrealized_pnl() const {
    return unrealized_pnl_;
  }
  size_t get_active_buy_orders() const {
    return active_buy_orders_;
  }
  size_t get_active_sell_orders() const {
    return active_sell_orders_;
  }
  const kj::Vector<GridLevel>& get_grid_levels() const {
    return grid_levels_;
  }
  bool is_grid_initialized() const {
    return grid_initialized_;
  }

private:
  // Grid configuration
  double upper_price_{0.0};         ///< Upper bound of grid
  double lower_price_{0.0};         ///< Lower bound of grid
  int grid_count_{10};              ///< Number of grid levels
  double total_investment_{1000.0}; ///< Total capital to deploy
  GridMode grid_mode_{GridMode::Arithmetic};
  double take_profit_pct_{0.0};     ///< Take profit percentage (0 = disabled)
  double stop_loss_pct_{0.0};       ///< Stop loss percentage (0 = disabled)
  bool trailing_up_{false};         ///< Enable trailing upper bound
  bool trailing_down_{false};       ///< Enable trailing lower bound
  double rebalance_threshold_{0.0}; ///< Rebalance threshold (0 = disabled)

  // Grid state
  kj::Vector<GridLevel> grid_levels_; ///< Grid level states
  double grid_spacing_{0.0};          ///< Price spacing between levels
  double order_quantity_{0.0};        ///< Quantity per grid order
  bool grid_initialized_{false};      ///< Whether grid has been initialized
  double initial_price_{0.0};         ///< Price when grid was initialized

  // Market state
  double current_price_{0.0};   ///< Current market price
  double best_bid_{0.0};        ///< Best bid price
  double best_ask_{0.0};        ///< Best ask price
  int64_t last_update_time_{0}; ///< Last price update timestamp

  // Position tracking
  double total_inventory_{0.0}; ///< Total inventory held
  double avg_entry_price_{0.0}; ///< Average entry price
  double inventory_value_{0.0}; ///< Total inventory value

  // PnL tracking
  double realized_pnl_{0.0};   ///< Total realized PnL
  double unrealized_pnl_{0.0}; ///< Current unrealized PnL
  double total_fees_{0.0};     ///< Total fees paid
  int total_trades_{0};        ///< Total number of trades

  // Order tracking
  size_t active_buy_orders_{0};  ///< Number of active buy orders
  size_t active_sell_orders_{0}; ///< Number of active sell orders

  // Pending signals
  kj::Vector<exec::PlaceOrderRequest> signals_;

  // Performance metrics
  StrategyMetrics metrics_;

  // Helper methods
  void initialize_grid(double current_price);
  void calculate_grid_levels();
  void place_initial_orders();
  void update_price(double price);
  void check_and_process_fills();
  void handle_buy_fill(size_t level_index, double fill_price, double fill_qty);
  void handle_sell_fill(size_t level_index, double fill_price, double fill_qty);
  void place_buy_order(size_t level_index);
  void place_sell_order(size_t level_index);
  void cancel_all_orders();
  void update_unrealized_pnl();
  void check_risk_controls();
  void check_rebalance();
  int find_current_level() const;
  double calculate_geometric_price(int level_index) const;
  double calculate_arithmetic_price(int level_index) const;
  kj::StringPtr get_symbol() const;
};

/**
 * @brief Strategy factory for Grid Strategy
 */
class GridStrategyFactory : public StrategyFactory<GridStrategy> {
public:
  kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
