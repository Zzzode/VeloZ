#include "veloz/strategy/mean_reversion_strategy.h"

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

MeanReversionStrategy::MeanReversionStrategy(const StrategyConfig& config) : BaseStrategy(config) {
  // Note: BaseStrategy moves config, so we must read from config_ member
  lookback_period_ =
      static_cast<int>(get_param_or_default(config_.parameters, "lookback_period"_kj, 20.0));
  entry_threshold_ = get_param_or_default(config_.parameters, "entry_threshold"_kj, 2.0);
  exit_threshold_ = get_param_or_default(config_.parameters, "exit_threshold"_kj, 0.5);
  position_size_multiplier_ = get_param_or_default(config_.parameters, "position_size"_kj, 1.0);
  enable_short_ = get_param_or_default(config_.parameters, "enable_short"_kj, 0.0) > 0.5;

  // Reserve space for price buffer
  price_buffer_.reserve(static_cast<size_t>(lookback_period_ + 1));
}

StrategyType MeanReversionStrategy::get_type() const {
  return StrategyType::MeanReversion;
}

void MeanReversionStrategy::on_event(const market::MarketEvent& event) {
  if (!running_) {
    return;
  }

  auto start_time = get_current_time_ns();

  // Handle different event types
  if (event.type == market::MarketEventType::Ticker ||
      event.type == market::MarketEventType::Trade) {
    double price = 0.0;

    if (event.data.is<market::TradeData>()) {
      const auto& trade_data = event.data.get<market::TradeData>();
      price = trade_data.price;
    } else {
      return;
    }

    // Add price to buffer
    add_price_to_buffer(price);

    // Update statistics if we have enough data
    if (price_buffer_.size() >= static_cast<size_t>(lookback_period_)) {
      update_statistics();

      // Check for trading signals
      if (in_position_) {
        check_exit_signals(price);
      } else {
        check_entry_signals(price);
      }
    }

    metrics_.events_processed.fetch_add(1);
    metrics_.last_event_time_ns.store(static_cast<uint64_t>(event.ts_recv_ns));
  } else if (event.type == market::MarketEventType::Kline) {
    // Handle kline data
    if (event.data.is<market::KlineData>()) {
      const auto& kline = event.data.get<market::KlineData>();

      // Use close price for mean reversion calculation
      add_price_to_buffer(kline.close);

      if (price_buffer_.size() >= static_cast<size_t>(lookback_period_)) {
        update_statistics();

        if (in_position_) {
          check_exit_signals(kline.close);
        } else {
          check_entry_signals(kline.close);
        }
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
  }
}

void MeanReversionStrategy::on_timer(int64_t timestamp) {
  (void)timestamp;
  // Timer-based logic can be added here if needed
}

kj::Vector<exec::PlaceOrderRequest> MeanReversionStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

void MeanReversionStrategy::reset() {
  BaseStrategy::reset();

  price_buffer_.clear();
  buffer_start_ = 0;

  current_mean_ = 0.0;
  current_std_dev_ = 0.0;
  current_zscore_ = 0.0;
  stats_initialized_ = false;

  entry_price_ = 0.0;
  entry_zscore_ = 0.0;
  stop_loss_price_ = 0.0;
  take_profit_price_ = 0.0;
  position_size_ = 0.0;
  in_position_ = false;

  signals_.clear();
  metrics_.reset();
}

kj::StringPtr MeanReversionStrategy::get_strategy_type() {
  return "MeanReversionStrategy"_kj;
}

bool MeanReversionStrategy::supports_hot_reload() const {
  return true;
}

bool MeanReversionStrategy::update_parameters(const kj::TreeMap<kj::String, double>& parameters) {
  // Update parameters that can be changed at runtime
  KJ_IF_SOME(value, parameters.find("position_size"_kj)) {
    position_size_multiplier_ = value;
  }
  KJ_IF_SOME(value, parameters.find("entry_threshold"_kj)) {
    entry_threshold_ = value;
  }
  KJ_IF_SOME(value, parameters.find("exit_threshold"_kj)) {
    exit_threshold_ = value;
  }
  KJ_IF_SOME(value, parameters.find("enable_short"_kj)) {
    enable_short_ = value > 0.5;
  }
  return true;
}

kj::Maybe<const StrategyMetrics&> MeanReversionStrategy::get_metrics() const {
  return metrics_;
}

double MeanReversionStrategy::calculate_mean(kj::ArrayPtr<const double> prices) const {
  if (prices.size() == 0) {
    return 0.0;
  }

  double sum = 0.0;
  for (size_t i = 0; i < prices.size(); ++i) {
    sum += prices[i];
  }
  return sum / static_cast<double>(prices.size());
}

double MeanReversionStrategy::calculate_std_dev(kj::ArrayPtr<const double> prices,
                                                double mean) const {
  if (prices.size() < 2) {
    return 0.0;
  }

  double sum_sq = 0.0;
  for (size_t i = 0; i < prices.size(); ++i) {
    double diff = prices[i] - mean;
    sum_sq += diff * diff;
  }

  // Use sample standard deviation (N-1)
  return std::sqrt(sum_sq / static_cast<double>(prices.size() - 1));
}

double MeanReversionStrategy::calculate_zscore(double price, double mean, double std_dev) const {
  if (std_dev <= 0.0) {
    return 0.0;
  }
  return (price - mean) / std_dev;
}

kj::Array<double> MeanReversionStrategy::get_ordered_prices() const {
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

void MeanReversionStrategy::add_price_to_buffer(double price) {
  size_t max_size = static_cast<size_t>(lookback_period_);
  if (price_buffer_.size() < max_size) {
    price_buffer_.add(price);
  } else {
    // Overwrite oldest price (circular buffer)
    price_buffer_[buffer_start_] = price;
    buffer_start_ = (buffer_start_ + 1) % max_size;
  }
}

void MeanReversionStrategy::update_statistics() {
  kj::Array<double> ordered_prices = get_ordered_prices();
  kj::ArrayPtr<const double> prices_ptr = ordered_prices;

  current_mean_ = calculate_mean(prices_ptr);
  current_std_dev_ = calculate_std_dev(prices_ptr, current_mean_);

  // Calculate Z-score for the most recent price
  if (ordered_prices.size() > 0) {
    double latest_price = ordered_prices[ordered_prices.size() - 1];
    current_zscore_ = calculate_zscore(latest_price, current_mean_, current_std_dev_);
  }

  stats_initialized_ = true;
}

void MeanReversionStrategy::check_entry_signals(double price) {
  if (!stats_initialized_ || current_std_dev_ <= 0.0) {
    return;
  }

  // Recalculate Z-score for current price
  double zscore = calculate_zscore(price, current_mean_, current_std_dev_);

  // Oversold condition: Z-score below negative threshold -> BUY
  if (zscore < -entry_threshold_) {
    generate_entry_signal(price, exec::OrderSide::Buy);
    entry_zscore_ = zscore;
    metrics_.signals_generated.fetch_add(1);
  }
  // Overbought condition: Z-score above positive threshold -> SELL (if short enabled)
  else if (enable_short_ && zscore > entry_threshold_) {
    generate_entry_signal(price, exec::OrderSide::Sell);
    entry_zscore_ = zscore;
    metrics_.signals_generated.fetch_add(1);
  }
}

void MeanReversionStrategy::check_exit_signals(double price) {
  if (!in_position_) {
    return;
  }

  // Check stop-loss and take-profit first
  bool should_exit = false;

  if (position_side_ == exec::OrderSide::Buy) {
    // Long position
    if (price <= stop_loss_price_) {
      should_exit = true;
      lose_count_++;
      total_loss_ += (entry_price_ - price) * position_size_;
    } else if (price >= take_profit_price_) {
      should_exit = true;
      win_count_++;
      total_profit_ += (price - entry_price_) * position_size_;
    }
  } else {
    // Short position
    if (price >= stop_loss_price_) {
      should_exit = true;
      lose_count_++;
      total_loss_ += (price - entry_price_) * position_size_;
    } else if (price <= take_profit_price_) {
      should_exit = true;
      win_count_++;
      total_profit_ += (entry_price_ - price) * position_size_;
    }
  }

  // Check mean reversion exit (price returned to mean)
  if (!should_exit && stats_initialized_ && current_std_dev_ > 0.0) {
    double zscore = calculate_zscore(price, current_mean_, current_std_dev_);

    if (position_side_ == exec::OrderSide::Buy) {
      // Exit long when Z-score rises above exit threshold (price reverted to mean)
      if (zscore > -exit_threshold_) {
        should_exit = true;
        if (price > entry_price_) {
          win_count_++;
          total_profit_ += (price - entry_price_) * position_size_;
        } else {
          lose_count_++;
          total_loss_ += (entry_price_ - price) * position_size_;
        }
      }
    } else {
      // Exit short when Z-score falls below exit threshold
      if (zscore < exit_threshold_) {
        should_exit = true;
        if (price < entry_price_) {
          win_count_++;
          total_profit_ += (entry_price_ - price) * position_size_;
        } else {
          lose_count_++;
          total_loss_ += (price - entry_price_) * position_size_;
        }
      }
    }
  }

  if (should_exit) {
    generate_exit_signal(price);
    trade_count_++;
    metrics_.signals_generated.fetch_add(1);
  }
}

void MeanReversionStrategy::generate_entry_signal(double price, exec::OrderSide side) {
  double qty = calculate_position_size();
  if (qty <= 0) {
    return;
  }

  // Get symbol from config
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

  // Calculate stop-loss and take-profit based on standard deviation
  // Stop-loss: 3 standard deviations from entry (wider than normal due to mean reversion nature)
  // Take-profit: when price returns to mean (handled by exit logic)
  double stop_distance = current_std_dev_ * 3.0;
  if (stop_distance < price * config_.stop_loss) {
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

void MeanReversionStrategy::generate_exit_signal(double price) {
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
  entry_zscore_ = 0.0;
  stop_loss_price_ = 0.0;
  take_profit_price_ = 0.0;
  position_size_ = 0.0;
}

double MeanReversionStrategy::calculate_position_size() const {
  // Position sizing based on risk per trade and max position size
  double base_size = config_.max_position_size * config_.risk_per_trade;
  return base_size * position_size_multiplier_;
}

// Factory implementation
kj::StringPtr MeanReversionStrategyFactory::get_strategy_type() const {
  return MeanReversionStrategy::get_strategy_type();
}

} // namespace veloz::strategy
