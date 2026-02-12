#include "veloz/strategy/advanced_strategies.h"

#include <gtest/gtest.h>

class AdvancedStrategiesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a basic strategy configuration
    config_ = veloz::strategy::StrategyConfig{
        "TestStrategy",
        veloz::strategy::StrategyType::Custom,
        0.02,        // risk_per_trade
        0.1,         // max_position_size
        0.05,        // stop_loss
        0.1,         // take_profit
        {"BTCUSDT"}, // symbols
        {{"rsi_period", 14.0}, {"overbought_level", 70.0}, {"oversold_level", 30.0}} // parameters
    };
  }

  void TearDown() override {}

  veloz::strategy::StrategyConfig config_;
};

// Test RSI strategy creation and initialization
TEST_F(AdvancedStrategiesTest, RsiStrategyCreation) {
  veloz::strategy::RsiStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test RSI strategy calculation method
TEST_F(AdvancedStrategiesTest, RsiStrategyCalculation) {
  veloz::strategy::RsiStrategy strategy(config_);

  // Create some test price data
  std::vector<double> prices = {100, 101, 102, 103, 104, 105, 106, 107,
                                108, 109, 110, 111, 112, 113, 114};

  // Test RSI calculation
  auto rsi = strategy.calculate_rsi(prices, 14);
  EXPECT_GT(rsi, 0.0);
  EXPECT_LT(rsi, 100.0);
}

// Test MACD strategy creation and initialization
TEST_F(AdvancedStrategiesTest, MacdStrategyCreation) {
  veloz::strategy::MacdStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test MACD strategy calculation method
TEST_F(AdvancedStrategiesTest, MacdStrategyCalculation) {
  veloz::strategy::MacdStrategy strategy(config_);

  // Create some test price data
  std::vector<double> prices = {100, 101, 102, 103, 104, 105, 106, 107,
                                108, 109, 110, 111, 112, 113, 114};

  double signal;
  auto macd = strategy.calculate_macd(prices, signal);
  EXPECT_NE(macd, 0.0);
}

// Test Bollinger Bands strategy creation and initialization
TEST_F(AdvancedStrategiesTest, BollingerBandsStrategyCreation) {
  veloz::strategy::BollingerBandsStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test Bollinger Bands strategy calculation method
TEST_F(AdvancedStrategiesTest, BollingerBandsStrategyCalculation) {
  veloz::strategy::BollingerBandsStrategy strategy(config_);

  // Create some test price data
  std::vector<double> prices = {100, 101, 102, 103, 104, 105, 106, 107,
                                108, 109, 110, 111, 112, 113, 114};

  double upper, middle, lower;
  strategy.calculate_bollinger_bands(prices, upper, middle, lower);
  EXPECT_GT(upper, middle);
  EXPECT_GT(middle, lower);
}

// Test Stochastic Oscillator strategy creation and initialization
TEST_F(AdvancedStrategiesTest, StochasticOscillatorStrategyCreation) {
  veloz::strategy::StochasticOscillatorStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test Stochastic Oscillator strategy calculation method
TEST_F(AdvancedStrategiesTest, StochasticOscillatorStrategyCalculation) {
  veloz::strategy::StochasticOscillatorStrategy strategy(config_);

  // Create some test price data
  std::vector<double> prices = {100, 101, 102, 103, 104, 105, 106, 107,
                                108, 109, 110, 111, 112, 113, 114};

  double k, d;
  strategy.calculate_stochastic_oscillator(prices, k, d);
  EXPECT_GT(k, 0.0);
  EXPECT_LT(k, 100.0);
  EXPECT_GT(d, 0.0);
  EXPECT_LT(d, 100.0);
}

// Test Market Making strategy creation and initialization
TEST_F(AdvancedStrategiesTest, MarketMakingHFTStrategyCreation) {
  veloz::strategy::MarketMakingHFTStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test Cross-Exchange Arbitrage strategy creation and initialization
TEST_F(AdvancedStrategiesTest, CrossExchangeArbitrageStrategyCreation) {
  veloz::strategy::CrossExchangeArbitrageStrategy strategy(config_);
  EXPECT_EQ(strategy.get_name(), config_.name);
  EXPECT_TRUE(strategy.get_id().find("strat-") == 0);
}

// Test strategy portfolio management functionality
TEST_F(AdvancedStrategiesTest, StrategyPortfolioManagerTest) {
  veloz::strategy::StrategyPortfolioManager portfolio;

  // Create some strategy instances
  auto rsi_strategy = std::make_shared<veloz::strategy::RsiStrategy>(config_);
  auto macd_strategy = std::make_shared<veloz::strategy::MacdStrategy>(config_);
  auto bollinger_strategy = std::make_shared<veloz::strategy::BollingerBandsStrategy>(config_);

  // Add strategies to the portfolio
  portfolio.add_strategy(rsi_strategy, 0.4);
  portfolio.add_strategy(macd_strategy, 0.3);
  portfolio.add_strategy(bollinger_strategy, 0.3);

  // Test portfolio state
  auto state = portfolio.get_portfolio_state();
  EXPECT_EQ(state.strategy_id, "portfolio");
  EXPECT_EQ(state.strategy_name, "Portfolio");
  EXPECT_TRUE(state.is_running);

  // Test getting combined signals
  auto signals = portfolio.get_combined_signals();
  EXPECT_TRUE(signals.empty()); // Strategies not yet running, no signals
}

// Test strategy factory creation methods
TEST_F(AdvancedStrategiesTest, StrategyFactoriesTest) {
  veloz::strategy::RsiStrategyFactory rsi_factory;
  EXPECT_EQ(rsi_factory.get_strategy_type(), "RsiStrategy");

  veloz::strategy::MacdStrategyFactory macd_factory;
  EXPECT_EQ(macd_factory.get_strategy_type(), "MacdStrategy");

  veloz::strategy::BollingerBandsStrategyFactory bollinger_factory;
  EXPECT_EQ(bollinger_factory.get_strategy_type(), "BollingerBandsStrategy");

  veloz::strategy::StochasticOscillatorStrategyFactory stochastic_factory;
  EXPECT_EQ(stochastic_factory.get_strategy_type(), "StochasticOscillatorStrategy");

  veloz::strategy::MarketMakingHFTStrategyFactory hft_factory;
  EXPECT_EQ(hft_factory.get_strategy_type(), "MarketMakingHFTStrategy");

  veloz::strategy::CrossExchangeArbitrageStrategyFactory arbitrage_factory;
  EXPECT_EQ(arbitrage_factory.get_strategy_type(), "CrossExchangeArbitrageStrategy");
}
