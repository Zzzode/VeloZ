#include <iostream>
#include <memory>

#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/backtest/analyzer.h"
#include "veloz/backtest/reporter.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/strategy/strategy.h"

class SimpleMovingAverageStrategy : public veloz::strategy::IStrategy {
public:
    SimpleMovingAverageStrategy() : id_("sma_strategy"), name_("SimpleMovingAverage"), type_(veloz::strategy::StrategyType::TrendFollowing) {}

    std::string get_id() const override { return id_; }
    std::string get_name() const override { return name_; }
    veloz::strategy::StrategyType get_type() const override { return type_; }

    bool initialize(const veloz::strategy::StrategyConfig& config, veloz::core::Logger& logger) override {
        logger_ = logger;
        logger_.info("Initializing SimpleMovingAverageStrategy");
        return true;
    }

    void on_start() override {
        logger_.info("SimpleMovingAverageStrategy started");
    }

    void on_stop() override {
        logger_.info("SimpleMovingAverageStrategy stopped");
    }

    void on_event(const veloz::market::MarketEvent& event) override {
        // TODO: Implement strategy logic
    }

    void on_position_update(const veloz::oms::Position& position) override {
        // TODO: Implement position update handling
    }

    void on_timer(int64_t timestamp) override {
        // TODO: Implement timer handling
    }

    veloz::strategy::StrategyState get_state() const override {
        veloz::strategy::StrategyState state;
        state.strategy_id = id_;
        state.strategy_name = name_;
        state.is_running = true;
        state.pnl = 0.0;
        state.max_drawdown = 0.0;
        state.trade_count = 0;
        state.win_count = 0;
        state.lose_count = 0;
        state.win_rate = 0.0;
        state.profit_factor = 0.0;
        return state;
    }

    std::vector<veloz::exec::PlaceOrderRequest> get_signals() override {
        return {};
    }

    void reset() override {}

private:
    std::string id_;
    std::string name_;
    veloz::strategy::StrategyType type_;
    veloz::core::Logger logger_;
};

int main() {
    // Create strategy
    auto strategy = std::make_shared<SimpleMovingAverageStrategy>();

    // Create data source (CSV source for now)
    auto data_source = veloz::backtest::DataSourceFactory::create_data_source("csv");

    // Create backtest engine
    auto backtest_engine = std::make_shared<veloz::backtest::BacktestEngine>();

    // Configure backtest
    veloz::backtest::BacktestConfig config;
    config.strategy_name = "SimpleMovingAverage";
    config.symbol = "BTCUSDT";
    config.start_time = 1609459200000; // 2021-01-01
    config.end_time = 1640995200000; // 2021-12-31
    config.initial_balance = 10000.0;
    config.risk_per_trade = 0.02;
    config.max_position_size = 0.1;
    config.strategy_parameters = {
        {"short_window", 5.0},
        {"long_window", 20.0},
        {"stop_loss", 0.02},
        {"take_profit", 0.05}
    };
    config.data_source = "csv";
    config.data_type = "kline";
    config.time_frame = "1h";

    // Initialize engine
    if (!backtest_engine->initialize(config)) {
        std::cerr << "Failed to initialize backtest engine" << std::endl;
        return 1;
    }

    // Set strategy and data source
    backtest_engine->set_strategy(strategy);
    backtest_engine->set_data_source(data_source);

    // Set progress callback
    backtest_engine->on_progress([](double progress) {
        std::cout << "Progress: " << static_cast<int>(progress * 100) << "%" << std::endl;
    });

    // Run backtest
    if (!backtest_engine->run()) {
        std::cerr << "Backtest failed" << std::endl;
        return 1;
    }

    // Get results
    auto result = backtest_engine->get_result();

    // Analyze results
    auto analyzer = std::make_unique<veloz::backtest::BacktestAnalyzer>();
    auto analyzed_result = analyzer->analyze(result.trades);

    // Generate report
    auto reporter = std::make_unique<veloz::backtest::BacktestReporter>();
    reporter->generate_report(result, "backtest_report.html");

    // Print results
    std::cout << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "       Backtest Results" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "Strategy Name: " << result.strategy_name << std::endl;
    std::cout << "Trading Pair: " << result.symbol << std::endl;
    std::cout << "Initial Balance: $" << result.initial_balance << std::endl;
    std::cout << "Final Balance: $" << result.final_balance << std::endl;
    std::cout << "Total Return: " << result.total_return * 100 << "%" << std::endl;
    std::cout << "Max Drawdown: " << result.max_drawdown * 100 << "%" << std::endl;
    std::cout << "Sharpe Ratio: " << result.sharpe_ratio << std::endl;
    std::cout << "Win Rate: " << result.win_rate * 100 << "%" << std::endl;
    std::cout << "Profit Factor: " << result.profit_factor << std::endl;
    std::cout << "Total Trades: " << result.trade_count << std::endl;
    std::cout << "Winning Trades: " << result.win_count << std::endl;
    std::cout << "Losing Trades: " << result.lose_count << std::endl;
    std::cout << "Average Win: $" << result.avg_win << std::endl;
    std::cout << "Average Loss: $" << result.avg_lose << std::endl;

    // Parameter optimization
    std::cout << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "    Parameter Optimization" << std::endl;
    std::cout << "==============================" << std::endl;

    auto optimizer = std::make_unique<veloz::backtest::GridSearchOptimizer>();
    optimizer->initialize(config);

    // Define parameter ranges
    std::map<std::string, std::pair<double, double>> parameter_ranges = {
        {"short_window", {5, 20}},
        {"long_window", {20, 60}},
        {"stop_loss", {0.01, 0.05}},
        {"take_profit", {0.02, 0.10}}
    };

    optimizer->set_parameter_ranges(parameter_ranges);
    optimizer->set_optimization_target("sharpe");
    optimizer->set_max_iterations(10);

    if (optimizer->optimize(strategy)) {
        auto optimization_results = optimizer->get_results();
        auto best_parameters = optimizer->get_best_parameters();

        std::cout << "Optimization Results Count: " << optimization_results.size() << std::endl;
        std::cout << "Best Parameter Combination:" << std::endl;

        for (const auto& [name, value] : best_parameters) {
            std::cout << "  " << name << ": " << value << std::endl;
        }
    } else {
        std::cout << "Parameter optimization failed" << std::endl;
    }

    return 0;
}
