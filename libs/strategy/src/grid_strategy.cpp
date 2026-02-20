#include "veloz/strategy/grid_strategy.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace veloz::strategy {

namespace {
// Helper function to get parameter from kj::TreeMap with kj::String keys
double get_param_or_default(const kj::TreeMap<kj::String, double>& params, kj::StringPtr key,
                            double default_value) {
  KJ_IF_SOME(value, params.find(key)) {
    return value;
  }
  return default_value;
}

// Helper to get current timestamp in nanoseconds
int64_t get_current_time_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Helper to get current timestamp in milliseconds
int64_t get_current_time_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
} // namespace

GridStrategy::GridStrategy(const StrategyConfig& config) : BaseStrategy(config) {
  // Read parameters from config_ (BaseStrategy member) since config is moved
  upper_price_ = get_param_or_default(config_.parameters, "upper_price"_kj, 0.0);
  lower_price_ = get_param_or_default(config_.parameters, "lower_price"_kj, 0.0);
  grid_count_ = static_cast<int>(get_param_or_default(config_.parameters, "grid_count"_kj, 10.0));
  total_investment_ = get_param_or_default(config_.parameters, "total_investment"_kj, 1000.0);
  grid_mode_ = static_cast<int>(get_param_or_default(config_.parameters, "grid_mode"_kj, 0.0)) == 0
                   ? GridMode::Arithmetic
                   : GridMode::Geometric;
  take_profit_pct_ = get_param_or_default(config_.parameters, "take_profit_pct"_kj, 0.0);
  stop_loss_pct_ = get_param_or_default(config_.parameters, "stop_loss_pct"_kj, 0.0);
  trailing_up_ = get_param_or_default(config_.parameters, "trailing_up"_kj, 0.0) > 0.5;
  trailing_down_ = get_param_or_default(config_.parameters, "trailing_down"_kj, 0.0) > 0.5;
  rebalance_threshold_ = get_param_or_default(config_.parameters, "rebalance_threshold"_kj, 0.0);

  // Validate grid parameters and pre-allocate
  if (upper_price_ > 0 && lower_price_ > 0 && upper_price_ > lower_price_ && grid_count_ > 1) {
    grid_levels_.reserve(static_cast<size_t>(grid_count_));
  }
}

StrategyType GridStrategy::get_type() const {
  return StrategyType::Grid;
}

void GridStrategy::on_event(const market::MarketEvent& event) {
  if (!running_) {
    return;
  }

  auto start_time = get_current_time_ns();

  // Handle different event types to extract price
  double price = 0.0;

  if (event.type == market::MarketEventType::Ticker ||
      event.type == market::MarketEventType::BookTop ||
      event.type == market::MarketEventType::BookDelta) {
    // Use BookData for order book updates
    if (event.data.is<market::BookData>()) {
      const auto& book = event.data.get<market::BookData>();
      if (book.bids.size() > 0 && book.asks.size() > 0) {
        best_bid_ = book.bids[0].price;
        best_ask_ = book.asks[0].price;
        if (best_bid_ > 0 && best_ask_ > 0) {
          price = (best_bid_ + best_ask_) / 2.0;
        }
      }
    }
  } else if (event.type == market::MarketEventType::Trade) {
    // Use trade price
    if (event.data.is<market::TradeData>()) {
      const auto& trade = event.data.get<market::TradeData>();
      price = trade.price;
    }
  }

  if (price <= 0) {
    return;
  }

  // Update current price
  update_price(price);

  // Initialize grid if not done yet
  if (!grid_initialized_) {
    if (upper_price_ > 0 && lower_price_ > 0 && upper_price_ > lower_price_) {
      initialize_grid(price);
    } else {
      // Cannot initialize without valid price range
      return;
    }
  }

  // Check for simulated fills based on price movement
  check_and_process_fills();

  // Update unrealized PnL
  update_unrealized_pnl();

  // Check risk controls
  check_risk_controls();

  // Check if rebalancing is needed
  check_rebalance();

  // Update metrics
  metrics_.events_processed.fetch_add(1);
  metrics_.last_event_time_ns.store(static_cast<uint64_t>(event.ts_recv_ns));

  auto end_time = get_current_time_ns();
  uint64_t execution_time = static_cast<uint64_t>(end_time - start_time);
  metrics_.execution_time_ns.fetch_add(execution_time);

  uint64_t current_max = metrics_.max_execution_time_ns.load();
  while (execution_time > current_max &&
         !metrics_.max_execution_time_ns.compare_exchange_weak(current_max, execution_time)) {
  }
}

void GridStrategy::on_timer(int64_t timestamp) {
  if (!running_) {
    return;
  }

  last_update_time_ = timestamp;

  // Periodic checks can be added here
  // For example, checking for stale orders or rebalancing
}

kj::Vector<exec::PlaceOrderRequest> GridStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

void GridStrategy::reset() {
  BaseStrategy::reset();

  // Reset grid state
  grid_levels_.clear();
  grid_initialized_ = false;
  initial_price_ = 0.0;
  grid_spacing_ = 0.0;
  order_quantity_ = 0.0;

  // Reset market state
  current_price_ = 0.0;
  best_bid_ = 0.0;
  best_ask_ = 0.0;
  last_update_time_ = 0;

  // Reset position tracking
  total_inventory_ = 0.0;
  avg_entry_price_ = 0.0;
  inventory_value_ = 0.0;

  // Reset PnL tracking
  realized_pnl_ = 0.0;
  unrealized_pnl_ = 0.0;
  total_fees_ = 0.0;
  total_trades_ = 0;

  // Reset order tracking
  active_buy_orders_ = 0;
  active_sell_orders_ = 0;

  signals_.clear();
  metrics_.reset();
}

kj::StringPtr GridStrategy::get_strategy_type() {
  return "GridStrategy"_kj;
}

bool GridStrategy::supports_hot_reload() const {
  return true;
}

bool GridStrategy::update_parameters(const kj::TreeMap<kj::String, double>& parameters) {
  // Only allow updating certain parameters at runtime
  // Grid bounds and count cannot be changed without reinitializing

  KJ_IF_SOME(value, parameters.find("take_profit_pct"_kj)) {
    take_profit_pct_ = value;
  }
  KJ_IF_SOME(value, parameters.find("stop_loss_pct"_kj)) {
    stop_loss_pct_ = value;
  }
  KJ_IF_SOME(value, parameters.find("trailing_up"_kj)) {
    trailing_up_ = value > 0.5;
  }
  KJ_IF_SOME(value, parameters.find("trailing_down"_kj)) {
    trailing_down_ = value > 0.5;
  }
  KJ_IF_SOME(value, parameters.find("rebalance_threshold"_kj)) {
    rebalance_threshold_ = value;
  }

  return true;
}

kj::Maybe<const StrategyMetrics&> GridStrategy::get_metrics() const {
  return metrics_;
}

void GridStrategy::initialize_grid(double current_price) {
  if (grid_initialized_) {
    return;
  }

  // Validate parameters
  if (upper_price_ <= lower_price_ || grid_count_ < 2) {
    return;
  }

  initial_price_ = current_price;
  current_price_ = current_price;

  // Calculate order quantity per level FIRST
  // Distribute investment across buy orders (roughly half the grid levels)
  int buy_levels = grid_count_ / 2;
  if (buy_levels > 0) {
    order_quantity_ = total_investment_ / (buy_levels * current_price);
  } else {
    order_quantity_ = total_investment_ / (grid_count_ * current_price);
  }

  // Calculate grid levels (uses order_quantity_)
  calculate_grid_levels();

  // Place initial orders
  place_initial_orders();

  grid_initialized_ = true;
}

void GridStrategy::calculate_grid_levels() {
  grid_levels_.clear();

  if (grid_mode_ == GridMode::Arithmetic) {
    // Equal price spacing
    grid_spacing_ = (upper_price_ - lower_price_) / (grid_count_ - 1);

    for (int i = 0; i < grid_count_; ++i) {
      GridLevel level;
      level.price = calculate_arithmetic_price(i);
      level.quantity = order_quantity_;
      grid_levels_.add(kj::mv(level));
    }
  } else {
    // Geometric (equal percentage) spacing
    double ratio = std::pow(upper_price_ / lower_price_, 1.0 / (grid_count_ - 1));
    grid_spacing_ = ratio; // Store ratio for geometric mode

    for (int i = 0; i < grid_count_; ++i) {
      GridLevel level;
      level.price = calculate_geometric_price(i);
      level.quantity = order_quantity_;
      grid_levels_.add(kj::mv(level));
    }
  }
}

void GridStrategy::place_initial_orders() {
  if (grid_levels_.size() == 0) {
    return;
  }

  // Find current price level
  int current_level = find_current_level();

  kj::StringPtr symbol = get_symbol();

  // Place buy orders below current price
  for (int i = 0; i < current_level && i < static_cast<int>(grid_levels_.size()); ++i) {
    place_buy_order(static_cast<size_t>(i));
  }

  // Place sell orders above current price
  for (int i = current_level + 1; i < static_cast<int>(grid_levels_.size()); ++i) {
    place_sell_order(static_cast<size_t>(i));
  }
}

void GridStrategy::update_price(double price) {
  current_price_ = price;
  last_update_time_ = get_current_time_ms();
}

void GridStrategy::check_and_process_fills() {
  if (!grid_initialized_ || grid_levels_.size() == 0) {
    return;
  }

  // Simulate fills based on price movement
  // In a real implementation, fills would come from the exchange

  for (size_t i = 0; i < grid_levels_.size(); ++i) {
    GridLevel& level = grid_levels_[i];

    // Check buy order fill (price dropped to or below level)
    if (level.has_buy_order && !level.buy_filled) {
      if (current_price_ <= level.price) {
        handle_buy_fill(i, level.price, level.quantity);
      }
    }

    // Check sell order fill (price rose to or above level)
    if (level.has_sell_order && !level.sell_filled) {
      if (current_price_ >= level.price) {
        handle_sell_fill(i, level.price, level.quantity);
      }
    }
  }
}

void GridStrategy::handle_buy_fill(size_t level_index, double fill_price, double fill_qty) {
  if (level_index >= grid_levels_.size()) {
    return;
  }

  GridLevel& level = grid_levels_[level_index];

  // Mark as filled
  level.buy_filled = true;
  level.has_buy_order = false;
  level.buy_fill_count++;
  active_buy_orders_--;

  // Update inventory
  double old_inventory = total_inventory_;
  total_inventory_ += fill_qty;
  inventory_value_ += fill_qty * fill_price;

  // Update average entry price
  if (total_inventory_ > 0) {
    avg_entry_price_ = inventory_value_ / total_inventory_;
  }

  // Track trade
  total_trades_++;
  trade_count_++;

  // Place sell order at the level above (if exists)
  if (level_index + 1 < grid_levels_.size()) {
    place_sell_order(level_index + 1);
    // Reset the sell_filled flag for the level above so it can be filled again
    grid_levels_[level_index + 1].sell_filled = false;
  }

  // Reset buy_filled so this level can be bought again after selling
  // (This happens when the corresponding sell fills)

  metrics_.signals_generated.fetch_add(1);
}

void GridStrategy::handle_sell_fill(size_t level_index, double fill_price, double fill_qty) {
  if (level_index >= grid_levels_.size()) {
    return;
  }

  GridLevel& level = grid_levels_[level_index];

  // Mark as filled
  level.sell_filled = true;
  level.has_sell_order = false;
  level.sell_fill_count++;
  active_sell_orders_--;

  // Calculate realized PnL for this trade
  double trade_pnl = 0.0;
  if (total_inventory_ > 0 && avg_entry_price_ > 0) {
    trade_pnl = fill_qty * (fill_price - avg_entry_price_);
  }
  realized_pnl_ += trade_pnl;
  level.realized_pnl += trade_pnl;

  // Update inventory
  total_inventory_ -= fill_qty;
  inventory_value_ -= fill_qty * fill_price;

  // Recalculate average entry price
  if (total_inventory_ > 0) {
    avg_entry_price_ = inventory_value_ / total_inventory_;
  } else {
    avg_entry_price_ = 0.0;
    inventory_value_ = 0.0;
  }

  // Track trade
  total_trades_++;
  trade_count_++;

  // Track win/loss
  if (trade_pnl > 0) {
    win_count_++;
    total_profit_ += trade_pnl;
  } else if (trade_pnl < 0) {
    lose_count_++;
    total_loss_ += std::abs(trade_pnl);
  }

  // Place buy order at the level below (if exists)
  if (level_index > 0) {
    place_buy_order(level_index - 1);
    // Reset the buy_filled flag for the level below so it can be filled again
    grid_levels_[level_index - 1].buy_filled = false;
  }

  metrics_.signals_generated.fetch_add(1);
}

void GridStrategy::place_buy_order(size_t level_index) {
  if (level_index >= grid_levels_.size()) {
    return;
  }

  GridLevel& level = grid_levels_[level_index];

  // Don't place if already has an order
  if (level.has_buy_order) {
    return;
  }

  // Check position limits
  if (total_inventory_ + level.quantity > config_.max_position_size &&
      config_.max_position_size > 0) {
    return;
  }

  kj::StringPtr symbol = get_symbol();

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  order.side = exec::OrderSide::Buy;
  order.qty = level.quantity;
  order.price = level.price;
  order.type = exec::OrderType::Limit;
  order.tif = exec::TimeInForce::GTC;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));

  level.has_buy_order = true;
  active_buy_orders_++;
}

void GridStrategy::place_sell_order(size_t level_index) {
  if (level_index >= grid_levels_.size()) {
    return;
  }

  GridLevel& level = grid_levels_[level_index];

  // Don't place if already has an order
  if (level.has_sell_order) {
    return;
  }

  // Check if we have inventory to sell
  if (total_inventory_ <= 0) {
    return;
  }

  kj::StringPtr symbol = get_symbol();

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  order.side = exec::OrderSide::Sell;
  order.qty = std::min(level.quantity, total_inventory_);
  order.price = level.price;
  order.type = exec::OrderType::Limit;
  order.tif = exec::TimeInForce::GTC;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));

  level.has_sell_order = true;
  active_sell_orders_++;
}

void GridStrategy::cancel_all_orders() {
  // In a real implementation, we would send cancel requests
  // For now, just reset order flags
  for (size_t i = 0; i < grid_levels_.size(); ++i) {
    grid_levels_[i].has_buy_order = false;
    grid_levels_[i].has_sell_order = false;
  }
  active_buy_orders_ = 0;
  active_sell_orders_ = 0;
}

void GridStrategy::update_unrealized_pnl() {
  if (total_inventory_ == 0 || current_price_ <= 0) {
    unrealized_pnl_ = 0.0;
    return;
  }

  unrealized_pnl_ = total_inventory_ * (current_price_ - avg_entry_price_);
  current_pnl_ = realized_pnl_ + unrealized_pnl_;

  // Update max drawdown
  if (current_pnl_ < 0 && std::abs(current_pnl_) > max_drawdown_) {
    max_drawdown_ = std::abs(current_pnl_);
  }
}

void GridStrategy::check_risk_controls() {
  double total_pnl = realized_pnl_ + unrealized_pnl_;

  // Check take profit
  if (take_profit_pct_ > 0) {
    double take_profit_amount = total_investment_ * take_profit_pct_;
    if (total_pnl >= take_profit_amount) {
      // Close all positions and cancel orders
      cancel_all_orders();
      // Generate market sell order to close position
      if (total_inventory_ > 0) {
        kj::StringPtr symbol = get_symbol();
        exec::PlaceOrderRequest close_order;
        close_order.symbol = symbol;
        close_order.side = exec::OrderSide::Sell;
        close_order.qty = total_inventory_;
        close_order.type = exec::OrderType::Market;
        close_order.tif = exec::TimeInForce::IOC;
        close_order.strategy_id = kj::str(get_id());
        signals_.add(kj::mv(close_order));
      }
      running_ = false;
      return;
    }
  }

  // Check stop loss
  if (stop_loss_pct_ > 0) {
    double stop_loss_amount = total_investment_ * stop_loss_pct_;
    if (total_pnl <= -stop_loss_amount) {
      // Close all positions and cancel orders
      cancel_all_orders();
      // Generate market sell order to close position
      if (total_inventory_ > 0) {
        kj::StringPtr symbol = get_symbol();
        exec::PlaceOrderRequest close_order;
        close_order.symbol = symbol;
        close_order.side = exec::OrderSide::Sell;
        close_order.qty = total_inventory_;
        close_order.type = exec::OrderType::Market;
        close_order.tif = exec::TimeInForce::IOC;
        close_order.strategy_id = kj::str(get_id());
        signals_.add(kj::mv(close_order));
      }
      running_ = false;
      return;
    }
  }

  // Check if price is outside grid range
  if (current_price_ > upper_price_ || current_price_ < lower_price_) {
    // Price has broken out of grid range
    // Could implement trailing grid here
    if (trailing_up_ && current_price_ > upper_price_) {
      // Shift grid up
      double shift = current_price_ - upper_price_;
      upper_price_ = current_price_;
      lower_price_ += shift;
      // Recalculate and reinitialize grid
      grid_initialized_ = false;
      cancel_all_orders();
      initialize_grid(current_price_);
    } else if (trailing_down_ && current_price_ < lower_price_) {
      // Shift grid down
      double shift = lower_price_ - current_price_;
      lower_price_ = current_price_;
      upper_price_ -= shift;
      // Recalculate and reinitialize grid
      grid_initialized_ = false;
      cancel_all_orders();
      initialize_grid(current_price_);
    }
  }
}

void GridStrategy::check_rebalance() {
  if (rebalance_threshold_ <= 0 || !grid_initialized_) {
    return;
  }

  // Check if price has deviated significantly from initial price
  double deviation = std::abs(current_price_ - initial_price_) / initial_price_;

  if (deviation >= rebalance_threshold_) {
    // Rebalance the grid around current price
    cancel_all_orders();
    grid_initialized_ = false;

    // Recenter the grid
    double range = upper_price_ - lower_price_;
    upper_price_ = current_price_ + range / 2.0;
    lower_price_ = current_price_ - range / 2.0;

    initialize_grid(current_price_);
  }
}

int GridStrategy::find_current_level() const {
  if (grid_levels_.size() == 0) {
    return 0;
  }

  // Find the grid level closest to current price
  for (size_t i = 0; i < grid_levels_.size(); ++i) {
    if (current_price_ < grid_levels_[i].price) {
      return static_cast<int>(i);
    }
  }

  return static_cast<int>(grid_levels_.size() - 1);
}

double GridStrategy::calculate_geometric_price(int level_index) const {
  // grid_spacing_ stores the ratio for geometric mode
  return lower_price_ * std::pow(grid_spacing_, level_index);
}

double GridStrategy::calculate_arithmetic_price(int level_index) const {
  return lower_price_ + grid_spacing_ * level_index;
}

kj::StringPtr GridStrategy::get_symbol() const {
  if (config_.symbols.size() > 0) {
    return config_.symbols[0];
  }
  return "BTCUSDT"_kj;
}

// Factory implementation
kj::StringPtr GridStrategyFactory::get_strategy_type() const {
  return GridStrategy::get_strategy_type();
}

} // namespace veloz::strategy
