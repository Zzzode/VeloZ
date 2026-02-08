#include "veloz/strategy/strategy.h"

namespace veloz::strategy {

class TestStrategy : public IStrategy {
public:
    explicit TestStrategy(const StrategyConfig& config)
        : config_(config), strategy_id_("test-strategy-" + config.name) {}

    std::string get_id() const override {
        return strategy_id_;
    }

    std::string get_name() const override {
        return config_.name;
    }

    StrategyType get_type() const override {
        return StrategyType::Custom;
    }

    bool initialize(const StrategyConfig& config, core::Logger& logger) override {
        logger.info("Test strategy {} initialized", config_.name);
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
        return StrategyState{
            .strategy_id = get_id(),
            .strategy_name = get_name(),
            .is_running = running_,
            .pnl = 0.0,
            .max_drawdown = 0.0,
            .trade_count = 0,
            .win_count = 0,
            .lose_count = 0,
            .win_rate = 0.0,
            .profit_factor = 0.0
        };
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
    std::string strategy_id_;
    bool initialized_ = false;
    bool running_ = false;
};

class TestStrategyFactory : public IStrategyFactory {
public:
    std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override {
        return std::make_shared<TestStrategy>(config);
    }

    std::string get_strategy_type() const override {
        return TestStrategy::get_strategy_type();
    }
};

} // namespace veloz::strategy
