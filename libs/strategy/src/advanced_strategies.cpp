#include "veloz/strategy/advanced_strategies.h"

#include <cmath>
#include <functional>
#include <numeric>

namespace veloz::strategy {

namespace {
// Helper function to get parameter from map with kj::String keys
double get_param_or_default(const std::map<kj::String, double>& params, const char* key,
                            double default_value) {
  kj::String kj_key = kj::str(key);
  auto it = params.find(kj_key);
  if (it != params.end()) {
    return it->second;
  }
  return default_value;
}
} // anonymous namespace

// TechnicalIndicatorStrategy implementation
double TechnicalIndicatorStrategy::calculate_rsi(const std::vector<double>& prices,
                                                 int period) const {
  if (prices.size() < static_cast<size_t>(period + 1))
    return 50.0; // Default to neutral

  double gains = 0.0;
  double losses = 0.0;

  for (size_t i = 1; i <= static_cast<size_t>(period); ++i) {
    double change = prices[i] - prices[i - 1];
    if (change > 0) {
      gains += change;
    } else if (change < 0) {
      losses += std::abs(change);
    }
  }

  double average_gain = gains / period;
  double average_loss = losses / period;

  double rs = average_gain / average_loss;
  return 100.0 - (100.0 / (1.0 + rs));
}

double TechnicalIndicatorStrategy::calculate_macd(const std::vector<double>& prices, double& signal,
                                                  int fast_period, int slow_period,
                                                  int signal_period) const {
  double fast_ema = calculate_exponential_moving_average(prices, fast_period);
  double slow_ema = calculate_exponential_moving_average(prices, slow_period);
  double macd = fast_ema - slow_ema;

  // Calculate signal line (simple moving average of MACD for now)
  signal = macd; // Simplified for example

  return macd;
}

void TechnicalIndicatorStrategy::calculate_bollinger_bands(const std::vector<double>& prices,
                                                           double& upper, double& middle,
                                                           double& lower, int period,
                                                           double std_dev) const {
  middle = calculate_exponential_moving_average(prices, period);
  double sd = calculate_standard_deviation(prices);
  upper = middle + std_dev * sd;
  lower = middle - std_dev * sd;
}

void TechnicalIndicatorStrategy::calculate_stochastic_oscillator(const std::vector<double>& prices,
                                                                 double& k, double& d, int k_period,
                                                                 int d_period) const {
  if (prices.size() < static_cast<size_t>(k_period)) {
    k = 50.0;
    d = 50.0;
    return;
  }

  // Find min/max in the last k_period elements
  double min_price = prices[prices.size() - k_period];
  double max_price = prices[prices.size() - k_period];
  for (size_t i = prices.size() - k_period; i < prices.size(); ++i) {
    if (prices[i] < min_price)
      min_price = prices[i];
    if (prices[i] > max_price)
      max_price = prices[i];
  }

  double current_price = prices[prices.size() - 1];
  k = ((current_price - min_price) / (max_price - min_price)) * 100.0;
  d = k; // Simplified for example
}

double
TechnicalIndicatorStrategy::calculate_exponential_moving_average(const std::vector<double>& prices,
                                                                 int period) const {
  if (prices.size() == 0)
    return 0.0;

  double multiplier = 2.0 / (period + 1.0);
  double ema = prices[0];

  for (size_t i = 1; i < prices.size(); ++i) {
    ema = (prices[i] * multiplier) + (ema * (1 - multiplier));
  }

  return ema;
}

double
TechnicalIndicatorStrategy::calculate_standard_deviation(const std::vector<double>& prices) const {
  if (prices.size() == 0)
    return 0.0;

  double sum = 0.0;
  for (size_t i = 0; i < prices.size(); ++i) {
    sum += prices[i];
  }
  double mean = sum / prices.size();

  double sum_sq = 0.0;
  for (size_t i = 0; i < prices.size(); ++i) {
    double diff = prices[i] - mean;
    sum_sq += diff * diff;
  }

  return std::sqrt(sum_sq / prices.size());
}

// RsiStrategy implementation
RsiStrategy::RsiStrategy(const StrategyConfig& config)
    : TechnicalIndicatorStrategy(config),
      rsi_period_(get_param_or_default(config.parameters, "rsi_period", 14.0)),
      overbought_level_(get_param_or_default(config.parameters, "overbought_level", 70.0)),
      oversold_level_(get_param_or_default(config.parameters, "oversold_level", 30.0)) {}

StrategyType RsiStrategy::get_type() const {
  return StrategyType::Custom;
}

void RsiStrategy::on_event(const market::MarketEvent& event) {
  if (event.type == market::MarketEventType::Ticker) {
    if (std::holds_alternative<market::TradeData>(event.data)) {
      const auto& trade_data = std::get<market::TradeData>(event.data);
      recent_prices_.push_back(trade_data.price);
      // Remove oldest price if we have too many
      while (recent_prices_.size() > static_cast<size_t>(rsi_period_ + 1)) {
        recent_prices_.erase(recent_prices_.begin());
      }

      if (recent_prices_.size() >= static_cast<size_t>(rsi_period_ + 1)) {
        double rsi = calculate_rsi(recent_prices_, rsi_period_);

        if (rsi > overbought_level_ && current_position_.size() > 0) {
          // Generate sell signal
          signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                               .side = exec::OrderSide::Sell,
                                               .qty = current_position_.size(),
                                               .price = trade_data.price,
                                               .type = exec::OrderType::Market});
        } else if (rsi < oversold_level_ && current_position_.size() == 0) {
          // Generate buy signal
          kj::String pos_size_key = kj::str("position_size");
          double quantity =
              config_.max_position_size *
              (config_.parameters.count(pos_size_key) ? config_.parameters.at(pos_size_key) : 1.0);
          signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                               .side = exec::OrderSide::Buy,
                                               .qty = quantity,
                                               .price = trade_data.price,
                                               .type = exec::OrderType::Market});
        }
      }
    }
  }
}

void RsiStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> RsiStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr RsiStrategy::get_strategy_type() {
  return "RsiStrategy"_kj;
}

// MacdStrategy implementation
MacdStrategy::MacdStrategy(const StrategyConfig& config)
    : TechnicalIndicatorStrategy(config),
      fast_period_(static_cast<int>(get_param_or_default(config.parameters, "fast_period", 12.0))),
      slow_period_(static_cast<int>(get_param_or_default(config.parameters, "slow_period", 26.0))),
      signal_period_(
          static_cast<int>(get_param_or_default(config.parameters, "signal_period", 9.0))) {}

StrategyType MacdStrategy::get_type() const {
  return StrategyType::Custom;
}

void MacdStrategy::on_event(const market::MarketEvent& event) {
  if (event.type == market::MarketEventType::Ticker) {
    recent_prices_.push_back(std::get<market::TradeData>(event.data).price);
    int max_period = std::max({fast_period_, slow_period_, signal_period_});
    while (recent_prices_.size() > static_cast<size_t>(max_period + 1)) {
      recent_prices_.erase(recent_prices_.begin());
    }

    if (recent_prices_.size() >= static_cast<size_t>(max_period + 1)) {
      double signal;
      double macd =
          calculate_macd(recent_prices_, signal, fast_period_, slow_period_, signal_period_);

      // MACD crossover signal
      if (macd > signal && current_position_.size() == 0) {
        kj::String pos_size_key = kj::str("position_size");
        double quantity =
            config_.max_position_size *
            (config_.parameters.count(pos_size_key) ? config_.parameters.at(pos_size_key) : 1.0);
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Buy,
                                             .qty = quantity,
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      } else if (macd < signal && current_position_.size() > 0) {
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Sell,
                                             .qty = current_position_.size(),
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      }
    }
  }
}

void MacdStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> MacdStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr MacdStrategy::get_strategy_type() {
  return "MacdStrategy"_kj;
}

// BollingerBandsStrategy implementation
BollingerBandsStrategy::BollingerBandsStrategy(const StrategyConfig& config)
    : TechnicalIndicatorStrategy(config),
      period_(static_cast<int>(get_param_or_default(config.parameters, "period", 20.0))),
      std_dev_(get_param_or_default(config.parameters, "std_dev", 2.0)) {}

StrategyType BollingerBandsStrategy::get_type() const {
  return StrategyType::Custom;
}

void BollingerBandsStrategy::on_event(const market::MarketEvent& event) {
  if (event.type == market::MarketEventType::Ticker) {
    recent_prices_.push_back(std::get<market::TradeData>(event.data).price);
    while (recent_prices_.size() > static_cast<size_t>(period_ + 1)) {
      recent_prices_.erase(recent_prices_.begin());
    }

    if (recent_prices_.size() >= static_cast<size_t>(period_ + 1)) {
      double upper, middle, lower;
      calculate_bollinger_bands(recent_prices_, upper, middle, lower, period_, std_dev_);

      if (std::get<market::TradeData>(event.data).price <= lower && current_position_.size() == 0) {
        kj::String pos_size_key = kj::str("position_size");
        double quantity =
            config_.max_position_size *
            (config_.parameters.count(pos_size_key) ? config_.parameters.at(pos_size_key) : 1.0);
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Buy,
                                             .qty = quantity,
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      } else if (std::get<market::TradeData>(event.data).price >= upper &&
                 current_position_.size() > 0) {
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Sell,
                                             .qty = current_position_.size(),
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      }
    }
  }
}

void BollingerBandsStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> BollingerBandsStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr BollingerBandsStrategy::get_strategy_type() {
  return "BollingerBandsStrategy"_kj;
}

// StochasticOscillatorStrategy implementation
StochasticOscillatorStrategy::StochasticOscillatorStrategy(const StrategyConfig& config)
    : TechnicalIndicatorStrategy(config),
      k_period_(static_cast<int>(get_param_or_default(config.parameters, "k_period", 14.0))),
      d_period_(static_cast<int>(get_param_or_default(config.parameters, "d_period", 3.0))),
      overbought_level_(get_param_or_default(config.parameters, "overbought_level", 80.0)),
      oversold_level_(get_param_or_default(config.parameters, "oversold_level", 20.0)) {}

StrategyType StochasticOscillatorStrategy::get_type() const {
  return StrategyType::Custom;
}

void StochasticOscillatorStrategy::on_event(const market::MarketEvent& event) {
  if (event.type == market::MarketEventType::Ticker) {
    recent_prices_.push_back(std::get<market::TradeData>(event.data).price);
    while (recent_prices_.size() > static_cast<size_t>(k_period_ + 1)) {
      recent_prices_.erase(recent_prices_.begin());
    }

    if (recent_prices_.size() >= static_cast<size_t>(k_period_ + 1)) {
      double k, d;
      calculate_stochastic_oscillator(recent_prices_, k, d, k_period_, d_period_);

      if (k < oversold_level_ && d < oversold_level_ && current_position_.size() == 0) {
        kj::String pos_size_key = kj::str("position_size");
        double quantity =
            config_.max_position_size *
            (config_.parameters.count(pos_size_key) ? config_.parameters.at(pos_size_key) : 1.0);
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Buy,
                                             .qty = quantity,
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      } else if (k > overbought_level_ && d > overbought_level_ && current_position_.size() > 0) {
        signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                             .side = exec::OrderSide::Sell,
                                             .qty = current_position_.size(),
                                             .price = std::get<market::TradeData>(event.data).price,
                                             .type = exec::OrderType::Market});
      }
    }
  }
}

void StochasticOscillatorStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> StochasticOscillatorStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr StochasticOscillatorStrategy::get_strategy_type() {
  return "StochasticOscillatorStrategy"_kj;
}

// MarketMakingHFTStrategy implementation
MarketMakingHFTStrategy::MarketMakingHFTStrategy(const StrategyConfig& config)
    : BaseStrategy(config), spread_(get_param_or_default(config.parameters, "spread", 0.001)),
      order_size_(get_param_or_default(config.parameters, "order_size", 0.01)),
      refresh_rate_ms_(
          static_cast<int>(get_param_or_default(config.parameters, "refresh_rate_ms", 100.0))),
      last_order_time_(0) {}

StrategyType MarketMakingHFTStrategy::get_type() const {
  return StrategyType::MarketMaking;
}

void MarketMakingHFTStrategy::on_event(const market::MarketEvent& event) {
  if (event.type == market::MarketEventType::Ticker) {
    int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    if (last_order_time_ == 0 || (current_time - last_order_time_) > refresh_rate_ms_) {
      double mid_price = std::get<market::TradeData>(event.data).price;
      double ask_price = mid_price * (1 + spread_);
      double bid_price = mid_price * (1 - spread_);

      signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                           .side = exec::OrderSide::Buy,
                                           .qty = order_size_,
                                           .price = bid_price,
                                           .type = exec::OrderType::Limit});

      signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                           .side = exec::OrderSide::Sell,
                                           .qty = order_size_,
                                           .price = ask_price,
                                           .type = exec::OrderType::Limit});

      last_order_time_ = current_time;
    }
  }
}

void MarketMakingHFTStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> MarketMakingHFTStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr MarketMakingHFTStrategy::get_strategy_type() {
  return "MarketMakingHFTStrategy"_kj;
}

// CrossExchangeArbitrageStrategy implementation
CrossExchangeArbitrageStrategy::CrossExchangeArbitrageStrategy(const StrategyConfig& config)
    : BaseStrategy(config),
      min_profit_(get_param_or_default(config.parameters, "min_profit", 0.001)),
      max_slippage_(get_param_or_default(config.parameters, "max_slippage", 0.0005)) {}

StrategyType CrossExchangeArbitrageStrategy::get_type() const {
  return StrategyType::Arbitrage;
}

void CrossExchangeArbitrageStrategy::on_event(const market::MarketEvent& event) {
  // Store prices by venue
  prices_by_venue_[event.venue] = std::get<market::TradeData>(event.data).price;

  // Check for arbitrage opportunity when we have prices from at least two venues
  if (prices_by_venue_.size() > 1) {
    double best_bid = 0.0;
    double best_ask = std::numeric_limits<double>::max();
    veloz::common::Venue best_bid_venue, best_ask_venue;

    for (const auto& [venue, price] : prices_by_venue_) {
      if (price > best_bid) {
        best_bid = price;
        best_bid_venue = venue;
      }

      if (price < best_ask) {
        best_ask = price;
        best_ask_venue = venue;
      }
    }

    // Check if spread is large enough for profitable arbitrage
    double spread = best_bid - best_ask;
    if (spread > min_profit_) {
      // Generate sell signal on best bid venue
      signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                           .side = exec::OrderSide::Sell,
                                           .qty = config_.max_position_size,
                                           .price = best_bid,
                                           .type = exec::OrderType::Limit});

      // Generate buy signal on best ask venue
      signals_.add(exec::PlaceOrderRequest{.symbol = "BTCUSDT",
                                           .side = exec::OrderSide::Buy,
                                           .qty = config_.max_position_size,
                                           .price = best_ask,
                                           .type = exec::OrderType::Limit});
    }
  }
}

void CrossExchangeArbitrageStrategy::on_timer(int64_t timestamp) {}

kj::Vector<exec::PlaceOrderRequest> CrossExchangeArbitrageStrategy::get_signals() {
  kj::Vector<exec::PlaceOrderRequest> result = kj::mv(signals_);
  signals_ = kj::Vector<exec::PlaceOrderRequest>();
  return result;
}

kj::StringPtr CrossExchangeArbitrageStrategy::get_strategy_type() {
  return "CrossExchangeArbitrageStrategy"_kj;
}

// StrategyPortfolioManager implementation
void StrategyPortfolioManager::add_strategy(const std::shared_ptr<IStrategy>& strategy,
                                            double weight) {
  strategies_[std::string(strategy->get_id().cStr())] = {strategy, weight};
}

void StrategyPortfolioManager::remove_strategy(kj::StringPtr strategy_id) {
  strategies_.erase(std::string(strategy_id.cStr()));
}

void StrategyPortfolioManager::update_strategy_weight(kj::StringPtr strategy_id, double weight) {
  auto it = strategies_.find(std::string(strategy_id.cStr()));
  if (it != strategies_.end()) {
    it->second.weight = weight;
  }
}

kj::Vector<exec::PlaceOrderRequest> StrategyPortfolioManager::get_combined_signals() {
  kj::Vector<exec::PlaceOrderRequest> combined_signals;

  for (auto& [strategy_id, sw] : strategies_) {
    auto signals = sw.strategy->get_signals();
    // Adjust signal quantity based on strategy weight
    for (size_t i = 0; i < signals.size(); ++i) {
      signals[i].qty *= sw.weight;
      combined_signals.add(kj::mv(signals[i]));
    }
  }

  return combined_signals;
}

StrategyState StrategyPortfolioManager::get_portfolio_state() const {
  StrategyState portfolio_state;
  portfolio_state.strategy_id = kj::str("portfolio");
  portfolio_state.strategy_name = kj::str("Portfolio");
  portfolio_state.is_running = true;
  portfolio_state.trade_count = 0;
  portfolio_state.win_count = 0;
  portfolio_state.lose_count = 0;
  portfolio_state.total_pnl = 0.0;

  for (const auto& [strategy_id, sw] : strategies_) {
    auto state = sw.strategy->get_state();
    portfolio_state.pnl += state.pnl * sw.weight;
    portfolio_state.trade_count += state.trade_count;
    portfolio_state.win_count += state.win_count;
    portfolio_state.lose_count += state.lose_count;
  }

  portfolio_state.win_rate =
      portfolio_state.trade_count > 0
          ? static_cast<double>(portfolio_state.win_count) / portfolio_state.trade_count
          : 0.0;

  return portfolio_state;
}

void StrategyPortfolioManager::set_risk_model(
    kj::Function<double(const kj::Vector<double>&)> risk_model) {
  risk_model_ = kj::mv(risk_model);
}

void StrategyPortfolioManager::rebalance_portfolio() {
  KJ_IF_MAYBE (risk_model, risk_model_) {
    // Calculate risk for each strategy (simplified)
    kj::Vector<double> risks;
    for (const auto& [strategy_id, sw] : strategies_) {
      risks.add(1.0); // Simplified, assuming each strategy has equal risk
    }

    double risk = (*risk_model)(risks);
    (void)risk; // Risk value used for future weight adjustments
    // Adjust weights based on risk (simplified)
    double new_weight = 1.0 / strategies_.size();
    for (auto& [strategy_id, sw] : strategies_) {
      sw.weight = new_weight;
    }
  }
}

// Strategy factory implementations
kj::StringPtr RsiStrategyFactory::get_strategy_type() const {
  return RsiStrategy::get_strategy_type();
}

kj::StringPtr MacdStrategyFactory::get_strategy_type() const {
  return MacdStrategy::get_strategy_type();
}

kj::StringPtr BollingerBandsStrategyFactory::get_strategy_type() const {
  return BollingerBandsStrategy::get_strategy_type();
}

kj::StringPtr StochasticOscillatorStrategyFactory::get_strategy_type() const {
  return StochasticOscillatorStrategy::get_strategy_type();
}

kj::StringPtr MarketMakingHFTStrategyFactory::get_strategy_type() const {
  return MarketMakingHFTStrategy::get_strategy_type();
}

kj::StringPtr CrossExchangeArbitrageStrategyFactory::get_strategy_type() const {
  return CrossExchangeArbitrageStrategy::get_strategy_type();
}

} // namespace veloz::strategy
