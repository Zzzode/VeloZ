#include "veloz/strategy/market_making_strategy.h"

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

MarketMakingStrategy::MarketMakingStrategy(const StrategyConfig& config) : BaseStrategy(config) {
  // Note: BaseStrategy moves config, so we must read from config_ member
  base_spread_ = get_param_or_default(config_.parameters, "base_spread"_kj, 0.001);
  order_size_ = get_param_or_default(config_.parameters, "order_size"_kj, 0.1);
  max_inventory_ = get_param_or_default(config_.parameters, "max_inventory"_kj, 10.0);
  inventory_skew_factor_ =
      get_param_or_default(config_.parameters, "inventory_skew_factor"_kj, 0.5);
  quote_refresh_interval_ms_ = static_cast<int64_t>(
      get_param_or_default(config_.parameters, "quote_refresh_interval_ms"_kj, 1000.0));
  min_spread_ = get_param_or_default(config_.parameters, "min_spread"_kj, 0.0005);
  max_spread_ = get_param_or_default(config_.parameters, "max_spread"_kj, 0.01);
  volatility_adjustment_ =
      get_param_or_default(config_.parameters, "volatility_adjustment"_kj, 1.0) > 0.5;

  // Reserve space for price history
  price_history_.reserve(volatility_window_ + 1);
}

StrategyType MarketMakingStrategy::get_type() const {
  return StrategyType::MarketMaking;
}

void MarketMakingStrategy::on_event(const market::MarketEvent& event) {
  if (!running_) {
    return;
  }

  auto start_time = get_current_time_ns();

  // Handle different event types
  if (event.type == market::MarketEventType::Ticker) {
    // Ticker provides best bid/ask - ideal for market making
    // Using BookData for ticker-like data since TickerData doesn't exist in OneOf
    if (event.data.is<market::BookData>()) {
      const auto& book = event.data.get<market::BookData>();
      if (book.bids.size() > 0 && book.asks.size() > 0) {
        double bid_price = book.bids[0].price;
        double ask_price = book.asks[0].price;
        if (bid_price > 0 && ask_price > 0) {
          update_mid_price(bid_price, ask_price);
          update_volatility(mid_price_);
        }
      }
    }
  } else if (event.type == market::MarketEventType::Trade) {
    // Use trade price if no ticker available
    if (event.data.is<market::TradeData>()) {
      const auto& trade = event.data.get<market::TradeData>();
      update_mid_price_from_trade(trade.price);
      update_volatility(trade.price);

      // Check if this is a fill for our orders (simplified - in production would use order IDs)
      // For now, we'll handle fills via on_position_update
    }
  } else if (event.type == market::MarketEventType::BookTop ||
             event.type == market::MarketEventType::BookDelta) {
    // Order book depth - use top of book for mid price
    if (event.data.is<market::BookData>()) {
      const auto& book = event.data.get<market::BookData>();
      if (book.bids.size() > 0 && book.asks.size() > 0) {
        double bid_price = book.bids[0].price;
        double ask_price = book.asks[0].price;
        if (bid_price > 0 && ask_price > 0) {
          update_mid_price(bid_price, ask_price);
          update_volatility(mid_price_);
        }
      }
    }
  }

  // Generate or refresh quotes if we have a valid mid price
  if (mid_price_ > 0) {
    int64_t current_time = get_current_time_ms();
    if (!quotes_active_ || should_refresh_quotes(current_time)) {
      generate_quotes();
      last_quote_time_ = current_time;
    }
  }

  // Update unrealized PnL
  update_unrealized_pnl();

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

void MarketMakingStrategy::on_timer(int64_t timestamp) {
  if (!running_) {
    return;
  }

  // Refresh quotes on timer if needed
  if (mid_price_ > 0 && should_refresh_quotes(timestamp)) {
    generate_quotes();
    last_quote_time_ = timestamp;
  }
}

kj::Vector<exec::PlaceOrderRequest> MarketMakingStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

void MarketMakingStrategy::reset() {
  BaseStrategy::reset();

  mid_price_ = 0.0;
  best_bid_ = 0.0;
  best_ask_ = 0.0;
  last_trade_price_ = 0.0;
  current_spread_ = 0.0;

  price_history_.clear();
  volatility_ = 0.0;

  bid_price_ = 0.0;
  ask_price_ = 0.0;
  bid_size_ = 0.0;
  ask_size_ = 0.0;
  quotes_active_ = false;
  last_quote_time_ = 0;

  inventory_ = 0.0;
  inventory_value_ = 0.0;
  avg_entry_price_ = 0.0;

  realized_pnl_ = 0.0;
  unrealized_pnl_ = 0.0;
  total_volume_ = 0.0;
  fills_count_ = 0;

  signals_.clear();
  metrics_.reset();
}

kj::StringPtr MarketMakingStrategy::get_strategy_type() {
  return "MarketMakingStrategy"_kj;
}

bool MarketMakingStrategy::supports_hot_reload() const {
  return true;
}

bool MarketMakingStrategy::update_parameters(const kj::TreeMap<kj::String, double>& parameters) {
  KJ_IF_SOME(value, parameters.find("base_spread"_kj)) {
    base_spread_ = value;
  }
  KJ_IF_SOME(value, parameters.find("order_size"_kj)) {
    order_size_ = value;
  }
  KJ_IF_SOME(value, parameters.find("max_inventory"_kj)) {
    max_inventory_ = value;
  }
  KJ_IF_SOME(value, parameters.find("inventory_skew_factor"_kj)) {
    inventory_skew_factor_ = value;
  }
  KJ_IF_SOME(value, parameters.find("quote_refresh_interval_ms"_kj)) {
    quote_refresh_interval_ms_ = static_cast<int64_t>(value);
  }
  KJ_IF_SOME(value, parameters.find("min_spread"_kj)) {
    min_spread_ = value;
  }
  KJ_IF_SOME(value, parameters.find("max_spread"_kj)) {
    max_spread_ = value;
  }
  KJ_IF_SOME(value, parameters.find("volatility_adjustment"_kj)) {
    volatility_adjustment_ = value > 0.5;
  }
  return true;
}

kj::Maybe<const StrategyMetrics&> MarketMakingStrategy::get_metrics() const {
  return metrics_;
}

void MarketMakingStrategy::update_mid_price(double bid, double ask) {
  best_bid_ = bid;
  best_ask_ = ask;
  mid_price_ = (bid + ask) / 2.0;
}

void MarketMakingStrategy::update_mid_price_from_trade(double price) {
  last_trade_price_ = price;
  // Only use trade price if we don't have bid/ask
  if (mid_price_ <= 0 || best_bid_ <= 0 || best_ask_ <= 0) {
    mid_price_ = price;
  }
}

void MarketMakingStrategy::update_volatility(double price) {
  // Add price to history
  if (price_history_.size() >= volatility_window_) {
    // Remove oldest price (shift left)
    for (size_t i = 0; i < price_history_.size() - 1; ++i) {
      price_history_[i] = price_history_[i + 1];
    }
    price_history_[price_history_.size() - 1] = price;
  } else {
    price_history_.add(price);
  }

  // Calculate volatility as standard deviation of returns
  if (price_history_.size() >= 2) {
    kj::Vector<double> returns;
    for (size_t i = 1; i < price_history_.size(); ++i) {
      if (price_history_[i - 1] > 0) {
        double ret = (price_history_[i] - price_history_[i - 1]) / price_history_[i - 1];
        returns.add(ret);
      }
    }

    if (returns.size() > 0) {
      // Calculate mean return
      double sum = 0.0;
      for (size_t i = 0; i < returns.size(); ++i) {
        sum += returns[i];
      }
      double mean = sum / static_cast<double>(returns.size());

      // Calculate variance
      double var_sum = 0.0;
      for (size_t i = 0; i < returns.size(); ++i) {
        double diff = returns[i] - mean;
        var_sum += diff * diff;
      }
      volatility_ = std::sqrt(var_sum / static_cast<double>(returns.size()));
    }
  }
}

double MarketMakingStrategy::calculate_spread() const {
  double spread = base_spread_;

  // Adjust spread based on volatility
  if (volatility_adjustment_ && volatility_ > 0) {
    // Higher volatility = wider spread
    // Multiply by volatility factor (e.g., if volatility is 1%, add 1% to spread)
    spread += volatility_ * 2.0; // 2x volatility adjustment
  }

  // Adjust spread based on inventory (inventory skew)
  double skew = calculate_inventory_skew();
  // Wider spread when inventory is high (either direction)
  spread += std::abs(skew) * base_spread_;

  // Clamp to min/max
  spread = std::max(min_spread_, std::min(max_spread_, spread));

  return spread;
}

double MarketMakingStrategy::calculate_inventory_skew() const {
  if (max_inventory_ <= 0) {
    return 0.0;
  }

  // Skew is inventory as fraction of max, scaled by skew factor
  // Positive inventory (long) -> negative skew (lower bid, higher ask to reduce long)
  // Negative inventory (short) -> positive skew (higher bid, lower ask to reduce short)
  return -(inventory_ / max_inventory_) * inventory_skew_factor_;
}

void MarketMakingStrategy::generate_quotes() {
  if (mid_price_ <= 0) {
    return;
  }

  // Check inventory limits
  bool can_buy = inventory_ < max_inventory_;
  bool can_sell = inventory_ > -max_inventory_;

  // Calculate spread and skew
  current_spread_ = calculate_spread();
  double half_spread = current_spread_ * mid_price_ / 2.0;
  double skew = calculate_inventory_skew();
  double skew_adjustment = skew * mid_price_ * base_spread_;

  // Calculate quote prices with skew
  // Skew shifts both quotes in the same direction
  bid_price_ = mid_price_ - half_spread + skew_adjustment;
  ask_price_ = mid_price_ + half_spread + skew_adjustment;

  // Ensure bid < ask
  if (bid_price_ >= ask_price_) {
    double mid = (bid_price_ + ask_price_) / 2.0;
    bid_price_ = mid - min_spread_ * mid_price_ / 2.0;
    ask_price_ = mid + min_spread_ * mid_price_ / 2.0;
  }

  // Calculate order sizes (reduce size when approaching inventory limits)
  double inventory_ratio = std::abs(inventory_) / max_inventory_;
  double size_reduction = std::max(0.0, 1.0 - inventory_ratio);

  bid_size_ = can_buy ? order_size_ * (inventory_ < 0 ? 1.0 : size_reduction) : 0.0;
  ask_size_ = can_sell ? order_size_ * (inventory_ > 0 ? 1.0 : size_reduction) : 0.0;

  // Get symbol from config
  kj::StringPtr symbol = "BTCUSDT"_kj;
  if (config_.symbols.size() > 0) {
    symbol = config_.symbols[0];
  }

  // Generate bid order
  if (bid_size_ > 0) {
    exec::PlaceOrderRequest bid_order;
    bid_order.symbol = symbol;
    bid_order.side = exec::OrderSide::Buy;
    bid_order.qty = bid_size_;
    bid_order.price = bid_price_;
    bid_order.type = exec::OrderType::Limit;
    bid_order.tif = exec::TimeInForce::GTC;
    bid_order.strategy_id = kj::str(get_id());
    signals_.add(kj::mv(bid_order));
    metrics_.signals_generated.fetch_add(1);
  }

  // Generate ask order
  if (ask_size_ > 0) {
    exec::PlaceOrderRequest ask_order;
    ask_order.symbol = symbol;
    ask_order.side = exec::OrderSide::Sell;
    ask_order.qty = ask_size_;
    ask_order.price = ask_price_;
    ask_order.type = exec::OrderType::Limit;
    ask_order.tif = exec::TimeInForce::GTC;
    ask_order.strategy_id = kj::str(get_id());
    signals_.add(kj::mv(ask_order));
    metrics_.signals_generated.fetch_add(1);
  }

  quotes_active_ = (bid_size_ > 0 || ask_size_ > 0);
}

void MarketMakingStrategy::cancel_quotes() {
  // In a real implementation, we would track order IDs and cancel them
  // For now, just mark quotes as inactive
  quotes_active_ = false;
  bid_price_ = 0.0;
  ask_price_ = 0.0;
  bid_size_ = 0.0;
  ask_size_ = 0.0;
}

void MarketMakingStrategy::handle_fill(double price, double qty, exec::OrderSide side) {
  fills_count_++;
  total_volume_ += qty * price;

  if (side == exec::OrderSide::Buy) {
    // Bought - increase inventory
    double old_inventory = inventory_;
    double old_value = inventory_value_;

    inventory_ += qty;
    inventory_value_ += qty * price;

    // Update average entry price
    if (inventory_ > 0) {
      avg_entry_price_ = inventory_value_ / inventory_;
    }

    // If we were short and now less short or flat, realize some PnL
    if (old_inventory < 0 && inventory_ >= old_inventory) {
      double closed_qty = std::min(qty, -old_inventory);
      realized_pnl_ += closed_qty * (avg_entry_price_ - price);
    }
  } else {
    // Sold - decrease inventory
    double old_inventory = inventory_;

    inventory_ -= qty;
    inventory_value_ -= qty * price;

    // Update average entry price
    if (inventory_ < 0) {
      avg_entry_price_ = -inventory_value_ / (-inventory_);
    }

    // If we were long and now less long or flat, realize some PnL
    if (old_inventory > 0 && inventory_ <= old_inventory) {
      double closed_qty = std::min(qty, old_inventory);
      realized_pnl_ += closed_qty * (price - avg_entry_price_);
    }
  }

  // Update trade statistics
  trade_count_++;
  current_pnl_ = realized_pnl_ + unrealized_pnl_;
}

void MarketMakingStrategy::update_unrealized_pnl() {
  if (mid_price_ <= 0 || inventory_ == 0) {
    unrealized_pnl_ = 0.0;
    return;
  }

  if (inventory_ > 0) {
    // Long position - profit if price went up
    unrealized_pnl_ = inventory_ * (mid_price_ - avg_entry_price_);
  } else {
    // Short position - profit if price went down
    unrealized_pnl_ = (-inventory_) * (avg_entry_price_ - mid_price_);
  }

  current_pnl_ = realized_pnl_ + unrealized_pnl_;

  // Update max drawdown
  if (current_pnl_ < 0 && std::abs(current_pnl_) > max_drawdown_) {
    max_drawdown_ = std::abs(current_pnl_);
  }
}

bool MarketMakingStrategy::should_refresh_quotes(int64_t current_time) const {
  return (current_time - last_quote_time_) >= quote_refresh_interval_ms_;
}

bool MarketMakingStrategy::is_price_stale(double new_mid) const {
  if (mid_price_ <= 0) {
    return true;
  }

  // Consider price stale if it moved more than half the spread
  double price_change = std::abs(new_mid - mid_price_) / mid_price_;
  return price_change > (current_spread_ / 2.0);
}

// Factory implementation
kj::StringPtr MarketMakingStrategyFactory::get_strategy_type() const {
  return MarketMakingStrategy::get_strategy_type();
}

} // namespace veloz::strategy
