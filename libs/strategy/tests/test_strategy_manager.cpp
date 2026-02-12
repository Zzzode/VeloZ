#include "veloz/strategy/strategy.h"

#include <gtest/gtest.h>

namespace veloz::strategy::tests {

// Forward declaration of TestStrategy
class TestStrategy;

// Test strategy factory
class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override;
  std::string get_strategy_type() const override;
};

// Strategy manager test class
class StrategyManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    manager_ = std::make_unique<StrategyManager>();
    // Register test strategy factory
    auto factory = std::make_shared<TestStrategyFactory>();
    manager_->register_strategy_factory(factory);
  }

  void TearDown() override {
    manager_.reset();
  }

  std::unique_ptr<StrategyManager> manager_;
};

// Test strategy implementation for testing
class TestStrategy : public IStrategy {
public:
  explicit TestStrategy(const StrategyConfig& config) : config_(config) {}

  std::string get_id() const override {
    return "test-strategy";
  }

  std::string get_name() const override {
    return "Test Strategy";
  }

  StrategyType get_type() const override {
    return StrategyType::Custom;
  }

  bool initialize(const StrategyConfig& config, core::Logger& logger) override {
    logger.info("Test strategy initialized");
    initialized_ = true;
    return true;
  }

  void on_start() override {
    running_ = true;
  }

  void on_stop() override {
    running_ = false;
  }

  void on_event(const market::MarketEvent& event) override {}
  void on_position_update(const oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {}

  StrategyState get_state() const override {
    return StrategyState{.strategy_id = get_id(),
                         .strategy_name = get_name(),
                         .is_running = running_,
                         .pnl = 0.0,
                         .max_drawdown = 0.0,
                         .trade_count = 0,
                         .win_count = 0,
                         .lose_count = 0,
                         .win_rate = 0.0,
                         .profit_factor = 0.0};
  }

  std::vector<exec::PlaceOrderRequest> get_signals() override {
    return {};
  }

  void reset() override {
    initialized_ = false;
    running_ = false;
  }

  static std::string get_strategy_type() {
    return "TestStrategy";
  }

  bool is_initialized() const {
    return initialized_;
  }

  bool is_running() const {
    return running_;
  }

private:
  StrategyConfig config_;
  bool initialized_ = false;
  bool running_ = false;
};

// Test strategy factory implementation
std::shared_ptr<IStrategy> TestStrategyFactory::create_strategy(const StrategyConfig& config) {
  return std::make_shared<TestStrategy>(config);
}

std::string TestStrategyFactory::get_strategy_type() const {
  return TestStrategy::get_strategy_type();
}

// Test strategy registration
TEST_F(StrategyManagerTest, TestStrategyRegistration) {
  // Verify that the test strategy factory is registered
  StrategyConfig config{.name = "Test Strategy",
                        .type = StrategyType::Custom,
                        .risk_per_trade = 0.01,
                        .max_position_size = 1.0,
                        .stop_loss = 0.05,
                        .take_profit = 0.1,
                        .symbols = {"BTCUSDT"},
                        .parameters = {}};

  auto strategy = manager_->create_strategy(config);
  EXPECT_TRUE(strategy);
  EXPECT_EQ(strategy->get_name(), config.name);
}

// Test strategy lifecycle management
TEST_F(StrategyManagerTest, TestStrategyLifecycle) {
  StrategyConfig config{.name = "Test Strategy",
                        .type = StrategyType::Custom,
                        .risk_per_trade = 0.01,
                        .max_position_size = 1.0,
                        .stop_loss = 0.05,
                        .take_profit = 0.1,
                        .symbols = {"BTCUSDT"},
                        .parameters = {}};

  auto strategy = manager_->create_strategy(config);
  EXPECT_TRUE(strategy);

  // Test start and stop
  std::string strategy_id = manager_->get_all_strategy_ids()[0];
  EXPECT_TRUE(manager_->start_strategy(strategy_id));
  EXPECT_TRUE(manager_->stop_strategy(strategy_id));
}

// Test strategy state query
TEST_F(StrategyManagerTest, TestStrategyStateQuery) {
  StrategyConfig config{.name = "Test Strategy",
                        .type = StrategyType::Custom,
                        .risk_per_trade = 0.01,
                        .max_position_size = 1.0,
                        .stop_loss = 0.05,
                        .take_profit = 0.1,
                        .symbols = {"BTCUSDT"},
                        .parameters = {}};

  manager_->create_strategy(config);

  // Get all strategy states
  auto states = manager_->get_all_strategy_states();
  EXPECT_EQ(states.size(), 1);
  EXPECT_EQ(states[0].strategy_name, config.name);
  EXPECT_FALSE(states[0].is_running);
}

// Test strategy event dispatch
TEST_F(StrategyManagerTest, TestStrategyEventDispatch) {
  StrategyConfig config{.name = "Test Strategy",
                        .type = StrategyType::Custom,
                        .risk_per_trade = 0.01,
                        .max_position_size = 1.0,
                        .stop_loss = 0.05,
                        .take_profit = 0.1,
                        .symbols = {"BTCUSDT"},
                        .parameters = {}};

  manager_->create_strategy(config);

  // Create a test market event
  market::MarketEvent event{
      .type = market::MarketEventType::Ticker,
      .symbol = "BTCUSDT",
  };

  // Dispatch event (should not throw exception)
  manager_->on_market_event(event);
}

} // namespace veloz::strategy::tests

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
