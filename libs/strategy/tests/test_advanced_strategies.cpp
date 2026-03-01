#include "kj/test.h"
#include "veloz/strategy/advanced_strategies.h"

namespace {

using namespace veloz::strategy;

KJ_TEST("RsiStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));
  config.parameters.insert(kj::str("rsi_period"), 14.0);
  config.parameters.insert(kj::str("overbought_level"), 70.0);
  config.parameters.insert(kj::str("oversold_level"), 30.0);

  RsiStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("RsiStrategy: Calculation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  RsiStrategy strategy(config);

  // Use kj::Array for KJ-compatible API
  kj::Array<double> prices = kj::heapArray<double>(
      {100, 102, 101, 103, 102, 104, 103, 105, 104, 106, 105, 107, 106, 108, 107});

  auto rsi = strategy.calculate_rsi(prices, 14);
  KJ_EXPECT(rsi > 0.0);
  KJ_EXPECT(rsi <= 100.0);
}

KJ_TEST("MacdStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  MacdStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("MacdStrategy: Calculation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  MacdStrategy strategy(config);

  kj::Array<double> prices = kj::heapArray<double>(
      {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114});

  double signal;
  auto macd = strategy.calculate_macd(prices, signal);
  KJ_EXPECT(macd != 0.0);
}

KJ_TEST("BollingerBandsStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  BollingerBandsStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("BollingerBandsStrategy: Calculation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  BollingerBandsStrategy strategy(config);

  kj::Array<double> prices = kj::heapArray<double>(
      {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114});

  double upper, middle, lower;
  strategy.calculate_bollinger_bands(prices, upper, middle, lower);
  KJ_EXPECT(upper > middle);
  KJ_EXPECT(middle > lower);
}

KJ_TEST("StochasticOscillatorStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  StochasticOscillatorStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("StochasticOscillatorStrategy: Calculation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  StochasticOscillatorStrategy strategy(config);

  kj::Array<double> prices = kj::heapArray<double>(
      {100, 105, 102, 110, 104, 114, 103, 108, 106, 112, 101, 109, 105, 111, 107});

  double k, d;
  strategy.calculate_stochastic_oscillator(prices, k, d);
  KJ_EXPECT(k >= 0.0);
  KJ_EXPECT(k <= 100.0);
  KJ_EXPECT(d >= 0.0);
  KJ_EXPECT(d <= 100.0);
}

KJ_TEST("MarketMakingHFTStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  MarketMakingHFTStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("CrossExchangeArbitrageStrategy: Creation") {
  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  CrossExchangeArbitrageStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TestStrategy");
  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TestStrategy_"));
}

KJ_TEST("StrategyPortfolioManager: Basic functionality") {
  StrategyPortfolioManager portfolio;

  StrategyConfig config;
  config.name = kj::str("TestStrategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  kj::Rc<IStrategy> rsi_strategy = kj::rc<RsiStrategy>(config);
  kj::Rc<IStrategy> macd_strategy = kj::rc<MacdStrategy>(config);
  kj::Rc<IStrategy> bollinger_strategy = kj::rc<BollingerBandsStrategy>(config);

  portfolio.add_strategy(kj::mv(rsi_strategy), 0.4);
  portfolio.add_strategy(kj::mv(macd_strategy), 0.3);
  portfolio.add_strategy(kj::mv(bollinger_strategy), 0.3);

  auto state = portfolio.get_portfolio_state();
  KJ_EXPECT(state.strategy_id == "portfolio");
  KJ_EXPECT(state.strategy_name == "Portfolio");
  KJ_EXPECT(state.is_running == true);

  auto signals = portfolio.get_combined_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("StrategyFactories: Type names") {
  RsiStrategyFactory rsi_factory;
  KJ_EXPECT(rsi_factory.get_strategy_type() == "RsiStrategy");

  MacdStrategyFactory macd_factory;
  KJ_EXPECT(macd_factory.get_strategy_type() == "MacdStrategy");

  BollingerBandsStrategyFactory bollinger_factory;
  KJ_EXPECT(bollinger_factory.get_strategy_type() == "BollingerBandsStrategy");

  StochasticOscillatorStrategyFactory stochastic_factory;
  KJ_EXPECT(stochastic_factory.get_strategy_type() == "StochasticOscillatorStrategy");

  MarketMakingHFTStrategyFactory hft_factory;
  KJ_EXPECT(hft_factory.get_strategy_type() == "MarketMakingHFTStrategy");

  CrossExchangeArbitrageStrategyFactory arbitrage_factory;
  KJ_EXPECT(arbitrage_factory.get_strategy_type() == "CrossExchangeArbitrageStrategy");
}

} // namespace
