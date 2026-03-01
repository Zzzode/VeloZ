#include "veloz/strategy/momentum_strategy.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <kj/vector.h>

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

MomentumStrategy::MomentumStrategy(const StrategyConfig& config) : BaseStrategy(config) {
  // Note: BaseStrategy moves config, so we must read from config_ member
  roc_period_ = static_cast<int>(get_param_or_default(config_.parameters, "roc_period"_kj, 14.0));
  rsi_period_ = static_cast<int>(get_param_or_default(config_.parameters, "rsi_period"_kj, 14.0));
  rsi_overbought_ = get_param_or_default(config_.parameters, "rsi_overbought"_kj, 70.0);
  rsi_oversold_ = get_param_or_default(config_.parameters, "rsi_oversold"_kj, 30.0);
  momentum_threshold_ = get_param_or_default(config_.parameters, "momentum_threshold"_kj, 0.02);
  position_size_ = get_param_or_default(config_.parameters, "position_size"_kj, 1.0);
  use_rsi_filter_ = get_param_or_default(config_.parameters, "use_rsi_filter"_kj, 1.0) > 0.5;
  allow_short_ = get_param_or_default(config_.parameters, "allow_short"_kj, 0.0) > 0.5;

  // Reserve space for price buffer (need enough history for ROC)
  // We need at least roc_period + 1 prices to calculate ROC
  price_buffer_.reserve(static_cast<size_t>(std::max(roc_period_, rsi_period_) + 5));
}

StrategyType MomentumStrategy::get_type() const {
  return StrategyType::Momentum;
}

void MomentumStrategy::on_event(const market::MarketEvent& event) {
  if (!running_) {
    return;
  }

  auto start_time = get_current_time_ns();

  double price = 0.0;

  // Handle different event types
  if (event.type == market::MarketEventType::Trade) {
    if (event.data.is<market::TradeData>()) {
      const auto& trade_data = event.data.get<market::TradeData>();
      price = trade_data.price;
    }
  } else if (event.type == market::MarketEventType::BookTop) {
    if (event.data.is<market::BookData>()) {
      const auto& book = event.data.get<market::BookData>();
      if (book.bids.size() > 0 && book.asks.size() > 0) {
        price = (book.bids[0].price + book.asks[0].price) / 2.0;
      }
    }
  } else if (event.type == market::MarketEventType::Kline) {
    if (event.data.is<market::KlineData>()) {
      const auto& kline = event.data.get<market::KlineData>();
      price = kline.close;
    }
  }

  if (price > 0.0) {
    last_price_ = price;

    // Add price to buffer and update indicators
    add_price(price);

    // Check stop-loss and take-profit if in position
    if (in_position_) {
      check_stop_loss_take_profit(price);
    }

    // Check for new signals if indicators are ready
    if (indicators_ready_) {
      if (in_position_) {
        check_exit_signals(price);
      } else {
        check_entry_signals(price);
      }
    }
  }

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

void MomentumStrategy::on_timer(int64_t timestamp) {
  // Timer logic if needed
}

kj::Vector<exec::PlaceOrderRequest> MomentumStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

void MomentumStrategy::reset() {
  BaseStrategy::reset();

  price_buffer_.clear();
  current_roc_ = 0.0;
  current_rsi_ = 50.0;
  last_price_ = 0.0;
  indicators_ready_ = false;

  avg_gain_ = 0.0;
  avg_loss_ = 0.0;
  rsi_initialized_ = false;

  entry_price_ = 0.0;
  stop_loss_price_ = 0.0;
  take_profit_price_ = 0.0;
  position_qty_ = 0.0;
  in_position_ = false;
  position_side_ = exec::OrderSide::Buy;

  signals_.clear();
  metrics_.reset();
}

kj::StringPtr MomentumStrategy::get_strategy_type() {
  return "MomentumStrategy"_kj;
}

bool MomentumStrategy::supports_hot_reload() const {
  return true;
}

bool MomentumStrategy::update_parameters(const kj::TreeMap<kj::String, double>& parameters) {
  KJ_IF_SOME(value, parameters.find("roc_period"_kj)) {
    roc_period_ = static_cast<int>(value);
  }
  KJ_IF_SOME(value, parameters.find("rsi_period"_kj)) {
    rsi_period_ = static_cast<int>(value);
    // Reset RSI calculation if period changes
    rsi_initialized_ = false;
  }
  KJ_IF_SOME(value, parameters.find("rsi_overbought"_kj)) {
    rsi_overbought_ = value;
  }
  KJ_IF_SOME(value, parameters.find("rsi_oversold"_kj)) {
    rsi_oversold_ = value;
  }
  KJ_IF_SOME(value, parameters.find("momentum_threshold"_kj)) {
    momentum_threshold_ = value;
  }
  KJ_IF_SOME(value, parameters.find("position_size"_kj)) {
    position_size_ = value;
  }
  KJ_IF_SOME(value, parameters.find("use_rsi_filter"_kj)) {
    use_rsi_filter_ = value > 0.5;
  }
  KJ_IF_SOME(value, parameters.find("allow_short"_kj)) {
    allow_short_ = value > 0.5;
  }
  return true;
}

kj::Maybe<const StrategyMetrics&> MomentumStrategy::get_metrics() const {
  return metrics_;
}

void MomentumStrategy::add_price(double price) {
  // Add to price buffer
  price_buffer_.add(price);

  // Maintain buffer size
  size_t max_size = static_cast<size_t>(std::max(roc_period_, rsi_period_) + 5);
  if (price_buffer_.size() > max_size) {
    // Ideally we would use a circular buffer, but for simplicity we'll just remove from front
    // Since kj::Vector doesn't have removeFront efficiently, we'll implement a shift
    // In production, use a RingBuffer or similar structure
    for (size_t i = 0; i < price_buffer_.size() - 1; ++i) {
      price_buffer_[i] = price_buffer_[i + 1];
    }
    price_buffer_.resize(price_buffer_.size() - 1);
  }

  // Calculate indicators
  current_roc_ = calculate_roc();
  current_rsi_ = calculate_rsi();

  // Check if we have enough data for indicators
  indicators_ready_ = price_buffer_.size() >= static_cast<size_t>(roc_period_) &&
                      price_buffer_.size() >= static_cast<size_t>(rsi_period_);
}

double MomentumStrategy::calculate_roc() const {
  if (price_buffer_.size() <= static_cast<size_t>(roc_period_)) {
    return 0.0;
  }

  double current = price_buffer_[price_buffer_.size() - 1];
  double past = price_buffer_[price_buffer_.size() - 1 - roc_period_];

  if (past <= 0) {
    return 0.0;
  }

  return (current - past) / past;
}

double MomentumStrategy::calculate_rsi() {
  if (price_buffer_.size() < 2) {
    return 50.0;
  }

  double current = price_buffer_[price_buffer_.size() - 1];
  double prev = price_buffer_[price_buffer_.size() - 2];
  double change = current - prev;

  double gain = change > 0 ? change : 0.0;
  double loss = change < 0 ? std::abs(change) : 0.0;

  if (!rsi_initialized_) {
    // Initial RSI calculation using simple average
    // We need at least rsi_period samples to initialize properly
    // For now, we use Wilder's smoothing from the start
    if (price_buffer_.size() <= static_cast<size_t>(rsi_period_)) {
      // Not enough data yet, accumulate for simple average
      avg_gain_ += gain;
      avg_loss_ += loss;

      if (price_buffer_.size() == static_cast<size_t>(rsi_period_)) {
        avg_gain_ /= rsi_period_;
        avg_loss_ /= rsi_period_;
        rsi_initialized_ = true;
      }
      return 50.0;
    }
  }

  // Wilder's Smoothing Method
  avg_gain_ = ((avg_gain_ * (rsi_period_ - 1)) + gain) / rsi_period_;
  avg_loss_ = ((avg_loss_ * (rsi_period_ - 1)) + loss) / rsi_period_;

  if (avg_loss_ == 0) {
    return 100.0;
  }

  double rs = avg_gain_ / avg_loss_;
  return 100.0 - (100.0 / (1.0 + rs));
}

void MomentumStrategy::check_entry_signals(double price) {
  // Buy Signal:
  // 1. Momentum (ROC) > threshold
  // 2. RSI not overbought (if filter enabled)
  if (current_roc_ > momentum_threshold_) {
    bool rsi_condition = !use_rsi_filter_ || (current_rsi_ < rsi_overbought_);
    if (rsi_condition) {
      generate_entry_signal(price, exec::OrderSide::Buy);
      return;
    }
  }

  // Sell Signal (Short):
  // 1. Momentum (ROC) < -threshold
  // 2. RSI not oversold (if filter enabled)
  // 3. Shorting allowed
  if (allow_short_ && current_roc_ < -momentum_threshold_) {
    bool rsi_condition = !use_rsi_filter_ || (current_rsi_ > rsi_oversold_);
    if (rsi_condition) {
      generate_entry_signal(price, exec::OrderSide::Sell);
      return;
    }
  }
}

void MomentumStrategy::check_exit_signals(double price) {
  if (position_side_ == exec::OrderSide::Buy) {
    // Exit Long:
    // 1. RSI becomes overbought
    // 2. Momentum turns negative
    if (current_rsi_ > rsi_overbought_ || current_roc_ < 0) {
      generate_exit_signal(price);
    }
  } else {
    // Exit Short:
    // 1. RSI becomes oversold
    // 2. Momentum turns positive
    if (current_rsi_ < rsi_oversold_ || current_roc_ > 0) {
      generate_exit_signal(price);
    }
  }
}

void MomentumStrategy::generate_entry_signal(double price, exec::OrderSide side) {
  // Check cooldown or other constraints here if needed

  double qty = calculate_position_size(std::abs(current_roc_));

  // Get symbol from config
  kj::StringPtr symbol = "BTCUSDT"_kj;
  if (config_.symbols.size() > 0) {
    symbol = config_.symbols[0];
  }

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  order.side = side;
  order.qty = qty;
  order.price = price; // Market order conceptual, or limit at current price
  order.type = exec::OrderType::Market;
  order.tif = exec::TimeInForce::GTC;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));
  metrics_.signals_generated.fetch_add(1);

  // Update position state (assuming immediate fill for simplicity in this strategy logic)
  // In a real system, we'd wait for fill confirmation
  in_position_ = true;
  position_side_ = side;
  entry_price_ = price;
  position_qty_ = qty;

  // Set initial stops
  // 2% stop loss, 4% take profit (example)
  if (side == exec::OrderSide::Buy) {
    stop_loss_price_ = price * 0.98;
    take_profit_price_ = price * 1.04;
  } else {
    stop_loss_price_ = price * 1.02;
    take_profit_price_ = price * 0.96;
  }
}

void MomentumStrategy::generate_exit_signal(double price) {
  // Get symbol from config
  kj::StringPtr symbol = "BTCUSDT"_kj;
  if (config_.symbols.size() > 0) {
    symbol = config_.symbols[0];
  }

  exec::PlaceOrderRequest order;
  order.symbol = symbol;
  // Close position by taking opposite side
  order.side =
      (position_side_ == exec::OrderSide::Buy) ? exec::OrderSide::Sell : exec::OrderSide::Buy;
  order.qty = position_qty_;
  order.price = price;
  order.type = exec::OrderType::Market;
  order.tif = exec::TimeInForce::GTC;
  order.strategy_id = kj::str(get_id());

  signals_.add(kj::mv(order));
  metrics_.signals_generated.fetch_add(1);

  // Reset position state
  in_position_ = false;
  position_qty_ = 0.0;
  entry_price_ = 0.0;
}

double MomentumStrategy::calculate_position_size(double momentum_strength) const {
  // Base size * momentum factor (stronger momentum -> larger size, capped at 2x)
  double factor = 1.0 + std::min(1.0, momentum_strength * 10.0);
  return position_size_ * factor;
}

void MomentumStrategy::check_stop_loss_take_profit(double current_price) {
  if (position_side_ == exec::OrderSide::Buy) {
    if (current_price <= stop_loss_price_ || current_price >= take_profit_price_) {
      generate_exit_signal(current_price);
    }
  } else {
    if (current_price >= stop_loss_price_ || current_price <= take_profit_price_) {
      generate_exit_signal(current_price);
    }
  }
}

// Factory implementation
kj::StringPtr MomentumStrategyFactory::get_strategy_type() const {
  return MomentumStrategy::get_strategy_type();
}

} // namespace veloz::strategy
