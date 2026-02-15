#include "veloz/strategy/advanced_strategies.h"

#include <gtest/gtest.h> // gtest framework
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <vector> // gtest compatibility

class AdvancedStrategiesTest : public ::testing::Test {
public:
  ~AdvancedStrategiesTest() noexcept override = default;

protected:
  void SetUp() override {
    // Create a basic strategy configuration
    config_.name = kj::str("TestStrategy");
    config_.type = veloz::strategy::StrategyType::Custom;
    config_.risk_per_trade = 0.02;
    config_.max_position_size = 0.1;
    config_.stop_loss = 0.05;
    config_.take_profit = 0.1;
    config_.symbols.add(kj::str("BTCUSDT"));
    // kj::TreeMap uses insert() instead of operator[]
    config_.parameters.insert(kj::str("rsi_period"), 14.0);
    config_.parameters.insert(kj::str("overbought_level"), 70.0);
    config_.parameters.insert(kj::str("oversold_level"), 30.0);
  }

  void TearDown() override {}

  veloz::strategy::StrategyConfig config_;
};

// Test RSI strategy creation and initialization
TEST_F(AdvancedStrategiesTest, RsiStrategyCreation) {
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::RsiStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
}

// Test RSI strategy calculation method
TEST_F(AdvancedStrategiesTest, RsiStrategyCalculation) {
  veloz::strategy::RsiStrategy strategy(config_);

  // Create test price data with both gains and losses for meaningful RSI
  std::vector<double> prices = {100, 102, 101, 103, 102, 104, 103, 105,
                                104, 106, 105, 107, 106, 108, 107};

  // Test RSI calculation
  auto rsi = strategy.calculate_rsi(prices, 14);
  EXPECT_GT(rsi, 0.0);
  EXPECT_LE(rsi, 100.0); // RSI can be exactly 100 in edge cases
}

// Test MACD strategy creation and initialization
TEST_F(AdvancedStrategiesTest, MacdStrategyCreation) {
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::MacdStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
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
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::BollingerBandsStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
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
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::StochasticOscillatorStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
}

// Test Stochastic Oscillator strategy calculation method
TEST_F(AdvancedStrategiesTest, StochasticOscillatorStrategyCalculation) {
  veloz::strategy::StochasticOscillatorStrategy strategy(config_);

  // Create test price data with variation for meaningful stochastic values
  // Current price (107) is in the middle of the range [100, 114]
  std::vector<double> prices = {100, 105, 102, 110, 104, 114, 103, 108,
                                106, 112, 101, 109, 105, 111, 107};

  double k, d;
  strategy.calculate_stochastic_oscillator(prices, k, d);
  EXPECT_GE(k, 0.0);
  EXPECT_LE(k, 100.0);
  EXPECT_GE(d, 0.0);
  EXPECT_LE(d, 100.0);
}

// Test Market Making strategy creation and initialization
TEST_F(AdvancedStrategiesTest, MarketMakingHFTStrategyCreation) {
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::MarketMakingHFTStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
}

// Test Cross-Exchange Arbitrage strategy creation and initialization
TEST_F(AdvancedStrategiesTest, CrossExchangeArbitrageStrategyCreation) {
  // Note: BaseStrategy moves config_, so save expected name before construction
  std::string expected_name = "TestStrategy";
  veloz::strategy::CrossExchangeArbitrageStrategy strategy(config_);
  EXPECT_EQ(std::string(strategy.get_name().cStr()), expected_name);
  // ID format is "{name}_{counter}" per generate_strategy_id()
  EXPECT_TRUE(std::string(strategy.get_id().cStr()).find("TestStrategy_") == 0);
}

// Test strategy portfolio management functionality
TEST_F(AdvancedStrategiesTest, StrategyPortfolioManagerTest) {
  veloz::strategy::StrategyPortfolioManager portfolio;

  // Create some strategy instances using kj::rc
  kj::Rc<veloz::strategy::IStrategy> rsi_strategy = kj::rc<veloz::strategy::RsiStrategy>(config_);
  kj::Rc<veloz::strategy::IStrategy> macd_strategy = kj::rc<veloz::strategy::MacdStrategy>(config_);
  kj::Rc<veloz::strategy::IStrategy> bollinger_strategy = kj::rc<veloz::strategy::BollingerBandsStrategy>(config_);

  // Add strategies to the portfolio
  portfolio.add_strategy(kj::mv(rsi_strategy), 0.4);
  portfolio.add_strategy(kj::mv(macd_strategy), 0.3);
  portfolio.add_strategy(kj::mv(bollinger_strategy), 0.3);

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
