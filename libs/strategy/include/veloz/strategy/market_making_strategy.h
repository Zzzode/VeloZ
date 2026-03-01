/**
 * @file market_making_strategy.h
 * @brief Market Making Strategy implementation
 *
 * This file implements a market making strategy that provides liquidity
 * by placing bid and ask orders around the mid-price, managing inventory
 * risk, and capturing the bid-ask spread.
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
 * @brief Market Making Strategy
 *
 * This strategy implements a market making approach:
 * - Continuously quotes bid and ask prices around mid-price
 * - Manages inventory to avoid directional exposure
 * - Adjusts spreads based on volatility and inventory
 * - Implements risk controls for position limits
 *
 * Trading logic:
 * - Calculate mid-price from best bid/ask or recent trades
 * - Place bid at mid_price - half_spread
 * - Place ask at mid_price + half_spread
 * - Adjust spread based on inventory skew (wider when inventory is high)
 * - Cancel and replace quotes when price moves significantly
 *
 * Configurable parameters:
 * - base_spread: Base spread as percentage of price (default: 0.001 = 0.1%)
 * - order_size: Size of each quote order (default: 0.1)
 * - max_inventory: Maximum inventory position (default: 10.0)
 * - inventory_skew_factor: How much to skew quotes based on inventory (default: 0.5)
 * - quote_refresh_interval_ms: How often to refresh quotes (default: 1000)
 * - min_spread: Minimum spread as percentage (default: 0.0005 = 0.05%)
 * - max_spread: Maximum spread as percentage (default: 0.01 = 1%)
 * - volatility_adjustment: Adjust spread based on volatility (default: true)
 */
class MarketMakingStrategy : public BaseStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit MarketMakingStrategy(const StrategyConfig& config);

  /**
   * @brief Virtual destructor
   */
  ~MarketMakingStrategy() noexcept override = default;

  /**
   * @brief Get strategy type
   * @return StrategyType::MarketMaking
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
  double get_mid_price() const {
    return mid_price_;
  }
  double get_current_spread() const {
    return current_spread_;
  }
  double get_inventory() const {
    return inventory_;
  }
  double get_bid_price() const {
    return bid_price_;
  }
  double get_ask_price() const {
    return ask_price_;
  }

private:
  // Market state
  double mid_price_{0.0};
  double best_bid_{0.0};
  double best_ask_{0.0};
  double last_trade_price_{0.0};
  double current_spread_{0.0};

  // Volatility tracking
  kj::Vector<double> price_history_;
  double volatility_{0.0};
  size_t volatility_window_{20};

  // Quote state
  double bid_price_{0.0};
  double ask_price_{0.0};
  double bid_size_{0.0};
  double ask_size_{0.0};
  bool quotes_active_{false};
  int64_t last_quote_time_{0};

  // Inventory management
  double inventory_{0.0};       // Current inventory (positive = long, negative = short)
  double inventory_value_{0.0}; // Total value of inventory
  double avg_entry_price_{0.0}; // Average entry price for inventory

  // PnL tracking
  double realized_pnl_{0.0};
  double unrealized_pnl_{0.0};
  double total_volume_{0.0};
  int fills_count_{0};

  // Strategy parameters
  double base_spread_;
  double order_size_;
  double max_inventory_;
  double inventory_skew_factor_;
  int64_t quote_refresh_interval_ms_;
  double min_spread_;
  double max_spread_;
  bool volatility_adjustment_;

  // Pending signals
  kj::Vector<exec::PlaceOrderRequest> signals_;

  // Performance metrics
  StrategyMetrics metrics_;

  // Helper methods
  void update_mid_price(double bid, double ask);
  void update_mid_price_from_trade(double price);
  void update_volatility(double price);
  double calculate_spread() const;
  double calculate_inventory_skew() const;
  void generate_quotes();
  void cancel_quotes();
  void handle_fill(double price, double qty, exec::OrderSide side);
  void update_unrealized_pnl();
  bool should_refresh_quotes(int64_t current_time) const;
  bool is_price_stale(double new_mid) const;
};

/**
 * @brief Strategy factory for Market Making Strategy
 */
class MarketMakingStrategyFactory : public StrategyFactory<MarketMakingStrategy> {
public:
  kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
