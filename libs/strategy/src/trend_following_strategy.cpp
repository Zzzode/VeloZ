#include "veloz/strategy/trend_following_strategy.h"

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
} // namespace

TrendFollowingStrategy::TrendFollowingStrategy(const StrategyConfig& config)
    : BaseStrategy(config) {
  // Note: BaseStrategy moves config, so we must read from config_ member
  fast_period_ = static_cast<int>(get_param_or_default(config_.parameters, "fast_period"_kj, 10.0));
  slow_period_ = static_cast<int>(get_param_or_default(config_.parameters, "slow_period"_kj, 20.0));
  use_ema_ = get_param_or_default(config_.parameters, "use_ema"_kj, 1.0) > 0.5;
  position_size_multiplier_ = get_param_or_default(config_.parameters, "position_size"_kj, 1.0);
  use_atr_stop_ = get_param_or_default(config_.parameters, "use_atr_stop"_kj, 0.0) > 0.5;
  atr_period_ = static_cast<int>(get_param_or_default(config_.parameters, "atr_period"_kj, 14.0));
  atr_multiplier_ = get_param_or_default(config_.parameters, "atr_multiplier"_kj, 2.0);

  // Reserve space for price buffer (need slow_period + 1 prices)
  price_buffer_.reserve(static_cast<size_t>(slow_period_ + 1));

  // Reserve space for ATR buffers if using ATR-based stops
  if (use_atr_stop_) {
    high_buffer_.reserve(static_cast<size_t>(atr_period_ + 1));
    low_buffer_.reserve(static_cast<size_t>(atr_period_ + 1));
  }
}

StrategyType TrendFollowingStrategy::get_type() const {
  return StrategyType::TrendFollowing;
}

void TrendFollowingStrategy::on_event(const market::MarketEvent& event) {
  if (!running_) {
    return;
  }

  auto start_time = get_current_time_ns();

  // Handle different event types
  if (event.type == market::MarketEventType::Ticker ||
      event.type == market::MarketEventType::Trade) {
    double price = 0.0;
    double high = 0.0;
    double low = 0.0;

    if (event.data.is<market::TradeData>()) {
      const auto& trade_data = event.data.get<market::TradeData>();
      price = trade_data.price;
      high = price;
      low = price;
    } else {
      // Skip if no valid price data
      return;
    }

    // Add price to buffer
    add_price_to_buffer(price);

    // Add high/low for ATR calculation
    if (use_atr_stop_) {
      add_high_low_to_buffer(high, low);
    }

    // Check stop-loss and take-profit if in position
    if (in_position_) {
      check_stop_loss_take_profit(price);
    }

    // Calculate MAs if we have enough data
    size_t required_size = static_cast<size_t>(slow_period_ + 1);
    if (price_buffer_.size() >= required_size) {
      kj::Array<double> ordered_prices = get_ordered_prices();
      kj::ArrayPtr<const double> prices_ptr = ordered_prices;

      double fast_ma = use_ema_ ? calculate_ema(prices_ptr, fast_period_)
                                : calculate_sma(prices_ptr, fast_period_);
      double slow_ma = use_ema_ ? calculate_ema(prices_ptr, slow_period_)
                                : calculate_sma(prices_ptr, slow_period_);

      // Calculate ATR if enabled
      if (use_atr_stop_ && high_buffer_.size() >= static_cast<size_t>(atr_period_)) {
        // For simplicity, use price buffer as close prices
        current_atr_ =
            calculate_atr(kj::ArrayPtr<const double>(high_buffer_.begin(), high_buffer_.size()),
                          kj::ArrayPtr<const double>(low_buffer_.begin(), low_buffer_.size()),
                          prices_ptr, atr_period_);
        atr_initialized_ = true;
      }

      // Check for crossover signals
      if (ma_initialized_) {
        // Golden cross: fast MA crosses above slow MA -> BUY signal
        if (prev_fast_ma_ <= prev_slow_ma_ && fast_ma > slow_ma) {
          if (!in_position_) {
            generate_entry_signal(price, exec::OrderSide::Buy);
            metrics_.signals_generated.fetch_add(1);
          }
        }
        // Death cross: fast MA crosses below slow MA -> SELL signal
        else if (prev_fast_ma_ >= prev_slow_ma_ && fast_ma < slow_ma) {
          if (in_position_ && position_side_ == exec::OrderSide::Buy) {
            generate_exit_signal(price);
            metrics_.signals_generated.fetch_add(1);
          } else if (!in_position_) {
            // Optional: short selling on death cross
            // generate_entry_signal(price, exec::OrderSide::Sell);
          }
        }
      }

      // Update previous MA values
      prev_fast_ma_ = fast_ma;
      prev_slow_ma_ = slow_ma;
      ma_initialized_ = true;
    }

    metrics_.events_processed.fetch_add(1);
    metrics_.last_event_time_ns.store(static_cast<uint64_t>(event.ts_recv_ns));
  } else if (event.type == market::MarketEventType::Kline) {
    // Handle kline data for more accurate OHLC
    if (event.data.is<market::KlineData>()) {
      const auto& kline = event.data.get<market::KlineData>();

      // Use close price for MA calculation
      add_price_to_buffer(kline.close);

      // Use actual high/low for ATR
      if (use_atr_stop_) {
        add_high_low_to_buffer(kline.high, kline.low);
      }

      // Same MA crossover logic as above
      size_t required_size = static_cast<size_t>(slow_period_ + 1);
      if (price_buffer_.size() >= required_size) {
        kj::Array<double> ordered_prices = get_ordered_prices();
        kj::ArrayPtr<const double> prices_ptr = ordered_prices;

        double fast_ma = use_ema_ ? calculate_ema(prices_ptr, fast_period_)
                                  : calculate_sma(prices_ptr, fast_period_);
        double slow_ma = use_ema_ ? calculate_ema(prices_ptr, slow_period_)
                                  : calculate_sma(prices_ptr, slow_period_);

        if (ma_initialized_) {
          if (prev_fast_ma_ <= prev_slow_ma_ && fast_ma > slow_ma && !in_position_) {
            generate_entry_signal(kline.close, exec::OrderSide::Buy);
            metrics_.signals_generated.fetch_add(1);
          } else if (prev_fast_ma_ >= prev_slow_ma_ && fast_ma < slow_ma && in_position_ &&
                     position_side_ == exec::OrderSide::Buy) {
            generate_exit_signal(kline.close);
            metrics_.signals_generated.fetch_add(1);
          }
        }

        prev_fast_ma_ = fast_ma;
        prev_slow_ma_ = slow_ma;
        ma_initialized_ = true;
      }

      metrics_.events_processed.fetch_add(1);
    }
  }

  // Update execution time metrics
  auto end_time = get_current_time_ns();
  uint64_t execution_time = static_cast<uint64_t>(end_time - start_time);
  metrics_.execution_time_ns.fetch_add(execution_time);

  // Update max execution time
  uint64_t current_max = metrics_.max_execution_time_ns.load();
  while (execution_time > current_max &&
         !metrics_.max_execution_time_ns.compare_exchange_weak(current_max, execution_time)) {
    // Retry until successful
  }
}

void TrendFollowingStrategy::on_timer(int64_t timestamp) {
  (void)timestamp;
  // Timer-based logic can be added here if needed
  // For example, periodic position review or trailing stop updates
}

kj::Vector<exec::PlaceOrderRequest> TrendFollowingStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

void TrendFollowingStrategy::reset() {
  BaseStrategy::reset();

  price_buffer_.clear();
  buffer_start_ = 0;
  high_buffer_.clear();
  low_buffer_.clear();

  prev_fast_ma_ = 0.0;
  prev_slow_ma_ = 0.0;
  ma_initialized_ = false;

  current_atr_ = 0.0;
  atr_initialized_ = false;

  entry_price_ = 0.0;
  stop_loss_price_ = 0.0;
  take_profit_price_ = 0.0;
  position_size_ = 0.0;
  position_avg_price_ = 0.0;
  in_position_ = false;

  signals_.clear();
  metrics_.reset();
}

kj::StringPtr TrendFollowingStrategy::get_strategy_type() {
  return "TrendFollowingStrategy"_kj;
}

bool TrendFollowingStrategy::supports_hot_reload() const {
  return true;
}

bool TrendFollowingStrategy::update_parameters(const kj::TreeMap<kj::String, double>& parameters) {
  // Update parameters that can be changed at runtime
  KJ_IF_SOME(value, parameters.find("position_size"_kj)) {
    position_size_multiplier_ = value;
  }
  KJ_IF_SOME(value, parameters.find("atr_multiplier"_kj)) {
    atr_multiplier_ = value;
  }
  // Note: Changing MA periods would require buffer resize, which is more complex
  // For now, only allow safe parameter updates
  return true;
}

kj::Maybe<const StrategyMetrics&> TrendFollowingStrategy::get_metrics() const {
  return metrics_;
}

double TrendFollowingStrategy::calculate_sma(kj::ArrayPtr<const double> prices, int period) const {
  if (prices.size() < static_cast<size_t>(period)) {
    return 0.0;
  }

  double sum = 0.0;
  size_t start = prices.size() - static_cast<size_t>(period);
  for (size_t i = start; i < prices.size(); ++i) {
    sum += prices[i];
  }
  return sum / period;
}

double TrendFollowingStrategy::calculate_ema(kj::ArrayPtr<const double> prices, int period) const {
  if (prices.size() == 0) {
    return 0.0;
  }

  double multiplier = 2.0 / (period + 1.0);
  double ema = prices[0];

  for (size_t i = 1; i < prices.size(); ++i) {
    ema = (prices[i] * multiplier) + (ema * (1.0 - multiplier));
  }

  return ema;
}

double TrendFollowingStrategy::calculate_atr(kj::ArrayPtr<const double> highs,
                                             kj::ArrayPtr<const double> lows,
                                             kj::ArrayPtr<const double> closes, int period) const {
  if (highs.size() < static_cast<size_t>(period) || lows.size() < static_cast<size_t>(period) ||
      closes.size() < static_cast<size_t>(period + 1)) {
    return 0.0;
  }

  double atr_sum = 0.0;
  size_t start = closes.size() - static_cast<size_t>(period);

  for (size_t i = start; i < closes.size(); ++i) {
    size_t hl_idx = i - (closes.size() - highs.size());
    if (hl_idx >= highs.size())
      continue;

    double high = highs[hl_idx];
    double low = lows[hl_idx];
    double prev_close = (i > 0) ? closes[i - 1] : closes[i];

    // True Range = max(high - low, |high - prev_close|, |low - prev_close|)
    double tr1 = high - low;
    double tr2 = std::abs(high - prev_close);
    double tr3 = std::abs(low - prev_close);
    double true_range = std::max({tr1, tr2, tr3});

    atr_sum += true_range;
  }

  return atr_sum / period;
}

kj::Array<double> TrendFollowingStrategy::get_ordered_prices() const {
  if (price_buffer_.size() == 0) {
    return kj::Array<double>();
  }

  kj::Vector<double> ordered;
  ordered.reserve(price_buffer_.size());

  // Copy from buffer_start_ to end
  for (size_t i = buffer_start_; i < price_buffer_.size(); ++i) {
    ordered.add(price_buffer_[i]);
  }

  // Copy from 0 to buffer_start_ (wrap-around)
  for (size_t i = 0; i < buffer_start_; ++i) {
    ordered.add(price_buffer_[i]);
  }

  return ordered.releaseAsArray();
}

void TrendFollowingStrategy::add_price_to_buffer(double price) {
  size_t max_size = static_cast<size_t>(slow_period_ + 1);
  if (price_buffer_.size() < max_size) {
    price_buffer_.add(price);
  } else {
    // Overwrite oldest price (circular buffer)
    price_buffer_[buffer_start_] = price;
    buffer_start_ = (buffer_start_ + 1) % max_size;
  }
}

void TrendFollowingStrategy::add_high_low_to_buffer(double high, double low) {
  size_t max_size = static_cast<size_t>(atr_period_ + 1);
  if (high_buffer_.size() < max_size) {
    high_buffer_.add(high);
    low_buffer_.add(low);
  } else {
    // Simple append for now (not circular, just keep last N values)
    // For production, implement proper circular buffer
    high_buffer_.add(high);
    low_buffer_.add(low);
    if (high_buffer_.size() > max_size * 2) {
      // Trim old values
      kj::Vector<double> new_highs;
      kj::Vector<double> new_lows;
      for (size_t i = high_buffer_.size() - max_size; i < high_buffer_.size(); ++i) {
        new_highs.add(high_buffer_[i]);
        new_lows.add(low_buffer_[i]);
      }
      high_buffer_ = kj::mv(new_highs);
      low_buffer_ = kj::mv(new_lows);
    }
  }
}

void TrendFollowingStrategy::check_stop_loss_take_profit(double current_price) {
  if (!in_position_) {
    return;
  }

  bool should_exit = false;

  if (position_side_ == exec::OrderSide::Buy) {
    // Long position: stop-loss below entry, take-profit above entry
    if (current_price <= stop_loss_price_) {
      should_exit = true;
      lose_count_++;
      total_loss_ += (entry_price_ - current_price) * position_size_;
    } else if (current_price >= take_profit_price_) {
      should_exit = true;
      win_count_++;
      total_profit_ += (current_price - entry_price_) * position_size_;
    }
  } else {
    // Short position: stop-loss above entry, take-profit below entry
    if (current_price >= stop_loss_price_) {
      should_exit = true;
      lose_count_++;
      total_loss_ += (current_price - entry_price_) * position_size_;
    } else if (current_price <= take_profit_price_) {
      should_exit = true;
      win_count_++;
      total_profit_ += (entry_price_ - current_price) * position_size_;
    }
  }

  if (should_exit) {
    generate_exit_signal(current_price);
    trade_count_++;
  }
}

// Track position size internally since Position class uses getter methods
double TrendFollowingStrategy::get_position_size() const {
  return position_size_;
}

void TrendFollowingStrategy::generate_entry_signal(double price, exec::OrderSide side) {
  double qty = calculate_position_size();
  if (qty <= 0) {
    return;
  }

  // Get symbol from config (use first symbol if available)
  kj::StringPtr symbol = "BTCUSDT"_kj;
  if (config_.symbols.size() > 0) {
    symbol = config_.symbols[0];
  }

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  order.side = side;
  order.qty = qty;
  order.price = price;
  order.type = exec::OrderType::Market;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));

  // Update position state
  in_position_ = true;
  position_side_ = side;
  entry_price_ = price;
  position_size_ = qty;
  position_avg_price_ = price;

  // Calculate stop-loss and take-profit
  double stop_distance = 0.0;
  if (use_atr_stop_ && atr_initialized_ && current_atr_ > 0) {
    stop_distance = current_atr_ * atr_multiplier_;
  } else {
    // Use config stop-loss as percentage
    stop_distance = price * config_.stop_loss;
  }

  double profit_distance = price * config_.take_profit;

  if (side == exec::OrderSide::Buy) {
    stop_loss_price_ = price - stop_distance;
    take_profit_price_ = price + profit_distance;
  } else {
    stop_loss_price_ = price + stop_distance;
    take_profit_price_ = price - profit_distance;
  }
}

void TrendFollowingStrategy::generate_exit_signal(double price) {
  if (!in_position_) {
    return;
  }

  // Get symbol from config
  kj::StringPtr symbol = "BTCUSDT"_kj;
  if (config_.symbols.size() > 0) {
    symbol = config_.symbols[0];
  }

  // Exit signal is opposite of entry
  exec::OrderSide exit_side =
      (position_side_ == exec::OrderSide::Buy) ? exec::OrderSide::Sell : exec::OrderSide::Buy;

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  order.side = exit_side;
  order.qty = position_size_;
  order.price = price;
  order.type = exec::OrderType::Market;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));

  // Calculate PnL
  double pnl = 0.0;
  if (position_side_ == exec::OrderSide::Buy) {
    pnl = (price - entry_price_) * position_size_;
  } else {
    pnl = (entry_price_ - price) * position_size_;
  }
  current_pnl_ += pnl;

  // Reset position state
  in_position_ = false;
  entry_price_ = 0.0;
  stop_loss_price_ = 0.0;
  take_profit_price_ = 0.0;
  position_size_ = 0.0;
  position_avg_price_ = 0.0;
  trade_count_++;
}

double TrendFollowingStrategy::calculate_position_size() const {
  // Position sizing based on risk per trade and max position size
  double base_size = config_.max_position_size * config_.risk_per_trade;
  return base_size * position_size_multiplier_;
}

// Factory implementation
kj::StringPtr TrendFollowingStrategyFactory::get_strategy_type() const {
  return TrendFollowingStrategy::get_strategy_type();
}

} // namespace veloz::strategy
