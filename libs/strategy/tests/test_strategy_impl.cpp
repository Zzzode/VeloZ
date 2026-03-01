#include "veloz/strategy/strategy.h"

#include <kj/vector.h>

namespace veloz::strategy {

// Test strategy implementation (for testing purposes)
// This strategy does nothing but can be used for basic connectivity testing
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

class TestStrategyFactory : public StrategyFactory<TestStrategy> {
public:
  kj::StringPtr get_strategy_type() const override {
    return TestStrategy::get_strategy_type();
  }
};

} // namespace veloz::strategy
