#include "veloz/strategy/strategy.h"

#include <gtest/gtest.h> // Kept for gtest framework compatibility
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <memory> // Kept for std::shared_ptr (gtest compatibility)
#include <string> // Kept for gtest string comparisons
#include <vector> // Kept for gtest compatibility

namespace veloz::strategy::tests {

// Forward declaration of TestStrategy
class TestStrategy;

// Test strategy factory
// Note: std::shared_ptr kept for gtest compatibility and StrategyFactory API
class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override;
  kj::StringPtr get_strategy_type() const override;
};

// Strategy manager test class
class StrategyManagerTest : public ::testing::Test {
public:
  ~StrategyManagerTest() noexcept override = default;

protected:
  void SetUp() override {
    manager_ = kj::heap<StrategyManager>();
    // Register test strategy factory
    // Note: std::shared_ptr kept for gtest compatibility and StrategyManager API
    auto factory = std::make_shared<TestStrategyFactory>();
    manager_->register_strategy_factory(factory);
  }

  void TearDown() override {
    manager_ = nullptr;
  }

  kj::Own<StrategyManager> manager_;
};

// Test strategy implementation for testing
class TestStrategy : public IStrategy {
public:
  // Note: StrategyConfig contains kj::String which is not copyable, so we store a reference
  explicit TestStrategy(const StrategyConfig& config) : config_ref_(config) {}

  kj::StringPtr get_id() const override {
    return "test-strategy"_kj;
  }

  kj::StringPtr get_name() const override {
    return "Test Strategy"_kj;
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
    return StrategyState{.strategy_id = kj::heapString(get_id()),
                         .strategy_name = kj::heapString(get_name()),
                         .is_running = running_,
                         .pnl = 0.0,
                         .max_drawdown = 0.0,
                         .trade_count = 0,
                         .win_count = 0,
                         .lose_count = 0,
                         .win_rate = 0.0,
                         .profit_factor = 0.0};
  }

  kj::Vector<exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<exec::PlaceOrderRequest>();
  }

  void reset() override {
    initialized_ = false;
    running_ = false;
  }

  static kj::StringPtr get_strategy_type() {
    return "Custom"_kj; // Must match StrategyType::Custom lookup in StrategyManager
  }

  bool is_initialized() const {
    return initialized_;
  }

  bool is_running() const {
    return running_;
  }

private:
  const StrategyConfig& config_ref_; // Store reference since StrategyConfig is not copyable
  bool initialized_ = false;
  bool running_ = false;
};

// Test strategy factory implementation
// Note: std::shared_ptr kept for gtest compatibility and StrategyFactory API
std::shared_ptr<IStrategy> TestStrategyFactory::create_strategy(const StrategyConfig& config) {
  return std::make_shared<TestStrategy>(config);
}

kj::StringPtr TestStrategyFactory::get_strategy_type() const {
  return TestStrategy::get_strategy_type();
}

// Test strategy registration
TEST_F(StrategyManagerTest, TestStrategyRegistration) {
  // Verify that the test strategy factory is registered
  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  auto strategy = manager_->create_strategy(config);
  EXPECT_TRUE(strategy);
  EXPECT_EQ(std::string(strategy->get_name().cStr()), std::string(config.name.cStr()));
}

// Test strategy lifecycle management
TEST_F(StrategyManagerTest, TestStrategyLifecycle) {
  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  auto strategy = manager_->create_strategy(config);
  EXPECT_TRUE(strategy);

  // Test start and stop
  auto ids = manager_->get_all_strategy_ids();
  kj::StringPtr strategy_id = ids[0];
  EXPECT_TRUE(manager_->start_strategy(strategy_id));
  EXPECT_TRUE(manager_->stop_strategy(strategy_id));
}

// Test strategy state query
TEST_F(StrategyManagerTest, TestStrategyStateQuery) {
  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager_->create_strategy(config);

  // Get all strategy states
  auto states = manager_->get_all_strategy_states();
  EXPECT_EQ(states.size(), 1);
  EXPECT_EQ(std::string(states[0].strategy_name.cStr()), std::string(config.name.cStr()));
  EXPECT_FALSE(states[0].is_running);
}

// Test strategy event dispatch
TEST_F(StrategyManagerTest, TestStrategyEventDispatch) {
  StrategyConfig config;
  config.name = kj::str("Test Strategy");
  config.type = StrategyType::Custom;
  config.risk_per_trade = 0.01;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.1;
  config.symbols.add(kj::str("BTCUSDT"));

  manager_->create_strategy(config);

  // Create a test market event
  market::MarketEvent event;
  event.type = market::MarketEventType::Ticker;
  event.symbol = veloz::common::SymbolId{"BTCUSDT"};

  // Dispatch event (should not throw exception)
  manager_->on_market_event(event);
}

} // namespace veloz::strategy::tests

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
