#include "veloz/strategy/strategy.h"

#include <cmath>
#include <deque>

namespace veloz::strategy {

// Trend following strategy implementation
class TrendFollowingStrategy : public BaseStrategy {
public:
  explicit TrendFollowingStrategy(const StrategyConfig& config) : BaseStrategy(config) {}

  StrategyType get_type() const override {
    return StrategyType::TrendFollowing;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple trend following logic using moving average crossover
    if (event.type == market::MarketEventType::Ticker) {
      // Track recent prices for moving average calculation
      if (std::holds_alternative<market::TradeData>(event.data)) {
        const auto& trade_data = std::get<market::TradeData>(event.data);
        recent_prices_.push_back(trade_data.price);
        if (recent_prices_.size() > 20) { // Simple 20-period moving average
          recent_prices_.pop_front();
        }

        if (recent_prices_.size() == 20) {
          double ma = calculate_moving_average(recent_prices_);
          // If price crosses above MA, buy; if crosses below, sell
          if (trade_data.price > ma && last_price_ <= ma) {
            // Generate buy signal
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1, // Fixed quantity for example
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (trade_data.price < ma && last_price_ >= ma) {
            // Generate sell signal
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
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

  std::vector<exec::PlaceOrderRequest> get_signals() override {
    std::vector<exec::PlaceOrderRequest> result = signals_;
    signals_.clear();
    return result;
  }

  static std::string get_strategy_type() {
    return "TrendFollowing";
  }

private:
  double calculate_moving_average(const std::deque<double>& prices) const {
    double sum = 0.0;
    for (double price : prices) {
      sum += price;
    }
    return sum / prices.size();
  }

  std::deque<double> recent_prices_;
  double last_price_ = 0.0;
  std::vector<exec::PlaceOrderRequest> signals_;
};

// Mean reversion strategy implementation
class MeanReversionStrategy : public BaseStrategy {
public:
  explicit MeanReversionStrategy(const StrategyConfig& config) : BaseStrategy(config) {}

  StrategyType get_type() const override {
    return StrategyType::MeanReversion;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple mean reversion logic using Bollinger Bands
    if (event.type == market::MarketEventType::Ticker) {
      if (std::holds_alternative<market::TradeData>(event.data)) {
        const auto& trade_data = std::get<market::TradeData>(event.data);
        recent_prices_.push_back(trade_data.price);
        if (recent_prices_.size() > 20) { // 20-period Bollinger Bands
          recent_prices_.pop_front();
        }

        if (recent_prices_.size() == 20) {
          double ma = calculate_moving_average(recent_prices_);
          double std_dev = calculate_standard_deviation(recent_prices_, ma);
          double upper_band = ma + 2 * std_dev;
          double lower_band = ma - 2 * std_dev;

          // Buy when price touches lower band, sell when touches upper band
          if (trade_data.price <= lower_band) {
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (trade_data.price >= upper_band) {
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
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

  std::vector<exec::PlaceOrderRequest> get_signals() override {
    std::vector<exec::PlaceOrderRequest> result = signals_;
    signals_.clear();
    return result;
  }

  static std::string get_strategy_type() {
    return "MeanReversion";
  }

private:
  double calculate_moving_average(const std::deque<double>& prices) const {
    double sum = 0.0;
    for (double price : prices) {
      sum += price;
    }
    return sum / prices.size();
  }

  double calculate_standard_deviation(const std::deque<double>& prices, double mean) const {
    double sum = 0.0;
    for (double price : prices) {
      sum += std::pow(price - mean, 2);
    }
    double variance = sum / prices.size();
    return std::sqrt(variance);
  }

  std::deque<double> recent_prices_;
  std::vector<exec::PlaceOrderRequest> signals_;
};

// Momentum strategy implementation
class MomentumStrategy : public BaseStrategy {
public:
  explicit MomentumStrategy(const StrategyConfig& config) : BaseStrategy(config) {}

  StrategyType get_type() const override {
    return StrategyType::Momentum;
  }

  void on_event(const market::MarketEvent& event) override {
    // Simple momentum strategy using price change over time
    if (event.type == market::MarketEventType::Ticker) {
      if (std::holds_alternative<market::TradeData>(event.data)) {
        const auto& trade_data = std::get<market::TradeData>(event.data);
        recent_prices_.push_back(trade_data.price);
        if (recent_prices_.size() > 10) { // 10-period momentum
          recent_prices_.pop_front();
        }

        if (recent_prices_.size() == 10) {
          double momentum = trade_data.price - recent_prices_.front();
          // Buy if positive momentum, sell if negative momentum
          if (momentum > 0.0) {
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
                                        .side = exec::OrderSide::Buy,
                                        .qty = 0.1,
                                        .price = trade_data.price,
                                        .type = exec::OrderType::Market});
          } else if (momentum < 0.0) {
            signals_.push_back(
                exec::PlaceOrderRequest{.symbol = "BTCUSDT", // Default symbol for example
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

  std::vector<exec::PlaceOrderRequest> get_signals() override {
    std::vector<exec::PlaceOrderRequest> result = signals_;
    signals_.clear();
    return result;
  }

  static std::string get_strategy_type() {
    return "Momentum";
  }

private:
  std::deque<double> recent_prices_;
  std::vector<exec::PlaceOrderRequest> signals_;
};

// Test strategy implementation (for testing purposes)
class TestStrategy : public BaseStrategy {
public:
  explicit TestStrategy(const StrategyConfig& config) : BaseStrategy(config) {}

  StrategyType get_type() const override {
    return StrategyType::Custom;
  }

  void on_event(const market::MarketEvent& event) override {}
  void on_timer(int64_t timestamp) override {}

  std::vector<exec::PlaceOrderRequest> get_signals() override {
    return {};
  }

  static std::string get_strategy_type() {
    return "TestStrategy";
  }
};

// Strategy factories
class TrendFollowingStrategyFactory : public StrategyFactory<TrendFollowingStrategy> {
public:
  std::string get_strategy_type() const override {
    return TrendFollowingStrategy::get_strategy_type();
  }
};

class MeanReversionStrategyFactory : public StrategyFactory<MeanReversionStrategy> {
public:
  std::string get_strategy_type() const override {
    return MeanReversionStrategy::get_strategy_type();
  }
};

class MomentumStrategyFactory : public StrategyFactory<MomentumStrategy> {
public:
  std::string get_strategy_type() const override {
    return MomentumStrategy::get_strategy_type();
  }
};

class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  std::string get_strategy_type() const override {
    return TestStrategy::get_strategy_type();
  }
};

} // namespace veloz::strategy
