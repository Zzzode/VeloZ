#include "veloz/strategy/strategy.h"

#include <cmath>
#include <kj/vector.h>

namespace veloz::strategy {

namespace {

template <typename T> class RingBuffer {
public:
  explicit RingBuffer(size_t capacity) : buffer_(kj::heapArray<T>(capacity)), capacity_(capacity) {}

  void add(T value) {
    buffer_[tail_] = value;
    tail_ = (tail_ + 1) % capacity_;
    if (size_ < capacity_) {
      size_++;
    } else {
      head_ = (head_ + 1) % capacity_;
    }
  }

  T operator[](size_t index) const {
    KJ_IREQUIRE(index < size_);
    return buffer_[(head_ + index) % capacity_];
  }

  size_t size() const {
    return size_;
  }
  bool full() const {
    return size_ == capacity_;
  }

  T front() const {
    KJ_IREQUIRE(size_ > 0);
    return buffer_[head_];
  }

private:
  kj::Array<T> buffer_;
  size_t capacity_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
};

} // namespace

// Trend following strategy implementation
class TrendFollowingStrategy : public BaseStrategy {
public:
  explicit TrendFollowingStrategy(const StrategyConfig& config)
      : BaseStrategy(config), recent_prices_(20) {}
  ~TrendFollowingStrategy() noexcept override = default;

  StrategyType get_type() const override {
    return StrategyType::TrendFollowing;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple trend following logic using moving average crossover
    if (event.type == market::MarketEventType::Ticker) {
      // Track recent prices for moving average calculation
      if (event.data.is<market::TradeData>()) {
        const auto& trade_data = event.data.get<market::TradeData>();
        recent_prices_.add(trade_data.price);

        if (recent_prices_.full()) {
          double ma = calculate_moving_average(recent_prices_);
          // If price crosses above MA, buy; if crosses below, sell
          if (trade_data.price > ma && last_price_ <= ma) {
            // Generate buy signal
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1, // Fixed quantity for example
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (trade_data.price < ma && last_price_ >= ma) {
            // Generate sell signal
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Sell,
                                        .qty = 0.1, // Fixed quantity for example
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          }
        }
        last_price_ = trade_data.price;
      }
    }
  }

  void on_timer(int64_t timestamp) override {
    // Timer-based logic for trend following (e.g., periodic rebalancing)
  }

  kj::Vector<exec::PlaceOrderRequest> get_signals() override {
    kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
    signals_ = kj::Vector<exec::PlaceOrderRequest>();
    return result;
  }

  static kj::StringPtr get_strategy_type() {
    return "TrendFollowing";
  }

private:
  double calculate_moving_average(const RingBuffer<double>& prices) const {
    double sum = 0.0;
    for (size_t i = 0; i < prices.size(); ++i) {
      sum += prices[i];
    }
    return sum / prices.size();
  }

  RingBuffer<double> recent_prices_;
  double last_price_ = 0.0;
  kj::Vector<exec::PlaceOrderRequest> signals_;
};

// Mean reversion strategy implementation
class MeanReversionStrategy : public BaseStrategy {
public:
  explicit MeanReversionStrategy(const StrategyConfig& config)
      : BaseStrategy(config), recent_prices_(20) {}
  ~MeanReversionStrategy() noexcept override = default;

  StrategyType get_type() const override {
    return StrategyType::MeanReversion;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple mean reversion logic using Bollinger Bands
    if (event.type == market::MarketEventType::Ticker) {
      if (event.data.is<market::TradeData>()) {
        const auto& trade_data = event.data.get<market::TradeData>();
        recent_prices_.add(trade_data.price);

        if (recent_prices_.full()) {
          double ma = calculate_moving_average(recent_prices_);
          double std_dev = calculate_standard_deviation(recent_prices_, ma);
          double upper_band = ma + 2 * std_dev;
          double lower_band = ma - 2 * std_dev;

          // Buy when price touches lower band, sell when touches upper band
          if (trade_data.price <= lower_band) {
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (trade_data.price >= upper_band) {
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Sell,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          }
        }
      }
    }
  }

  void on_timer(int64_t timestamp) override {
    // Timer-based logic for mean reversion
  }

  kj::Vector<exec::PlaceOrderRequest> get_signals() override {
    kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
    signals_ = kj::Vector<exec::PlaceOrderRequest>();
    return result;
  }

  static kj::StringPtr get_strategy_type() {
    return "MeanReversion";
  }

private:
  double calculate_moving_average(const RingBuffer<double>& prices) const {
    double sum = 0.0;
    for (size_t i = 0; i < prices.size(); ++i) {
      sum += prices[i];
    }
    return sum / prices.size();
  }

  double calculate_standard_deviation(const RingBuffer<double>& prices, double mean) const {
    double sum = 0.0;
    for (size_t i = 0; i < prices.size(); ++i) {
      sum += std::pow(prices[i] - mean, 2);
    }
    double variance = sum / prices.size();
    return std::sqrt(variance);
  }

  RingBuffer<double> recent_prices_;
  kj::Vector<exec::PlaceOrderRequest> signals_;
};

// Momentum strategy implementation
class MomentumStrategy : public BaseStrategy {
public:
  explicit MomentumStrategy(const StrategyConfig& config)
      : BaseStrategy(config), recent_prices_(10) {}
  ~MomentumStrategy() noexcept override = default;

  StrategyType get_type() const override {
    return StrategyType::Momentum;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple momentum strategy using price change over time
    if (event.type == market::MarketEventType::Ticker) {
      if (event.data.is<market::TradeData>()) {
        const auto& trade_data = event.data.get<market::TradeData>();
        recent_prices_.add(trade_data.price);

        if (recent_prices_.full()) {
          double momentum = trade_data.price - recent_prices_.front();
          // Buy if positive momentum, sell if negative momentum
          if (momentum > 0.0) {
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (momentum < 0.0) {
            signals_.add(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT"_kj, // Default symbol for example
                                        .side = exec::OrderSide::Sell,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          }
        }
      }
    }
  }

  void on_timer(int64_t timestamp) override {
    // Timer-based logic for momentum strategy
  }

  kj::Vector<exec::PlaceOrderRequest> get_signals() override {
    kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
    signals_ = kj::Vector<exec::PlaceOrderRequest>();
    return result;
  }

  static kj::StringPtr get_strategy_type() {
    return "Momentum";
  }

private:
  RingBuffer<double> recent_prices_;
  kj::Vector<exec::PlaceOrderRequest> signals_;
};

// Test strategy implementation (for testing purposes)
class TestStrategy : public BaseStrategy {
public:
  explicit TestStrategy(const StrategyConfig& config) : BaseStrategy(config) {}
  ~TestStrategy() noexcept override = default;

  StrategyType get_type() const override {
    return StrategyType::Custom;
  }

  void on_event(const market::MarketEvent& event) override {}
  void on_timer(int64_t timestamp) override {}

  kj::Vector<exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<exec::PlaceOrderRequest>();
  }

  static kj::StringPtr get_strategy_type() {
    return "TestStrategy"_kj;
  }
};

// Strategy factories
class TrendFollowingStrategyFactory : public StrategyFactory<TrendFollowingStrategy> {
public:
  kj::StringPtr get_strategy_type() const override {
    return TrendFollowingStrategy::get_strategy_type();
  }
};

class MeanReversionStrategyFactory : public StrategyFactory<MeanReversionStrategy> {
public:
  kj::StringPtr get_strategy_type() const override {
    return MeanReversionStrategy::get_strategy_type();
  }
};

class MomentumStrategyFactory : public StrategyFactory<MomentumStrategy> {
public:
  kj::StringPtr get_strategy_type() const override {
    return MomentumStrategy::get_strategy_type();
  }
};

class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  kj::StringPtr get_strategy_type() const override {
    return TestStrategy::get_strategy_type();
  }
};

} // namespace veloz::strategy
