#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/analyzer.h"
#include "veloz/core/logger.h"
#include "veloz/oms/position.h"

namespace veloz::backtest {

struct BacktestEngine::Impl {
    BacktestConfig config;
    std::shared_ptr<veloz::strategy::IStrategy> strategy;
    std::shared_ptr<IDataSource> data_source;
    BacktestResult result;
    bool is_running;
    bool is_initialized;
    std::function<void(double)> progress_callback;
    std::shared_ptr<veloz::core::Logger> logger;

    Impl() : is_running(false), is_initialized(false) {
        logger = std::make_shared<veloz::core::Logger>(std::cout);
    }
};

BacktestEngine::BacktestEngine() : impl_(std::make_unique<Impl>()) {}

BacktestEngine::~BacktestEngine() {}

bool BacktestEngine::initialize(const BacktestConfig& config) {
    impl_->logger->info(std::format("Initializing backtest engine"));
    impl_->config = config;
    impl_->result = BacktestResult();
    impl_->is_running = false;
    impl_->is_initialized = true;
    return true;
}

bool BacktestEngine::run() {
    if (!impl_->is_initialized) {
        impl_->logger->error(std::format("Backtest engine not initialized"));
        return false;
    }

    if (!impl_->strategy) {
        impl_->logger->error(std::format("No strategy set"));
        return false;
    }

    if (!impl_->data_source) {
        impl_->logger->error(std::format("No data source set"));
        return false;
    }

    impl_->logger->info(std::format("Starting backtest"));
    impl_->is_running = true;

    try {
        // Connect to data source
        if (!impl_->data_source->connect()) {
            impl_->logger->error(std::format("Failed to connect to data source"));
            impl_->is_running = false;
            return false;
        }

        // Get historical data
        auto market_events = impl_->data_source->get_data(
            impl_->config.symbol,
            impl_->config.start_time,
            impl_->config.end_time,
            impl_->config.data_type,
            impl_->config.time_frame
        );

        // Initialize strategy
        veloz::strategy::StrategyConfig strategy_config;
        strategy_config.name = impl_->config.strategy_name;
        strategy_config.type = veloz::strategy::StrategyType::Custom;
        strategy_config.risk_per_trade = impl_->config.risk_per_trade;
        strategy_config.max_position_size = impl_->config.max_position_size;
        strategy_config.symbols = {impl_->config.symbol};
        strategy_config.parameters = impl_->config.strategy_parameters;

        if (!impl_->strategy->initialize(strategy_config, *impl_->logger)) {
            impl_->logger->error(std::format("Failed to initialize strategy"));
            impl_->data_source->disconnect();
            impl_->is_running = false;
            return false;
        }

        // Run backtest simulation
        double progress = 0.0;
        const double progress_step = 1.0 / market_events.size();

        for (size_t i = 0; i < market_events.size(); ++i) {
            if (!impl_->is_running) {
                break;
            }

            const auto& event = market_events[i];
            impl_->strategy->on_event(event);

            // Get signals
            auto signals = impl_->strategy->get_signals();

            // TODO: Simulate order execution

            // Update progress
            progress += progress_step;
            if (impl_->progress_callback) {
                impl_->progress_callback(std::min(progress, 1.0));
            }
        }

        // Stop strategy
        impl_->strategy->on_stop();

        // Disconnect from data source
        impl_->data_source->disconnect();

        // Calculate backtest result
        auto analyzer = BacktestAnalyzer();
        auto analyzed_result = analyzer.analyze(impl_->result.trades);
        impl_->result = *analyzed_result;

        impl_->is_running = false;
        impl_->logger->info(std::format("Backtest completed"));

        return true;
    } catch (const std::exception& e) {
        impl_->logger->error(std::format("Backtest failed: {}", e.what()));
        impl_->is_running = false;

        if (impl_->data_source) {
            impl_->data_source->disconnect();
        }

        return false;
    }
}

bool BacktestEngine::stop() {
    if (!impl_->is_running) {
        impl_->logger->warn(std::format("Backtest not running"));
        return false;
    }

    impl_->logger->info(std::format("Stopping backtest"));
    impl_->is_running = false;
    return true;
}

bool BacktestEngine::reset() {
    impl_->logger->info(std::format("Resetting backtest engine"));
    impl_->config = BacktestConfig();
    impl_->strategy.reset();
    impl_->data_source.reset();
    impl_->result = BacktestResult();
    impl_->is_running = false;
    impl_->is_initialized = false;
    return true;
}

BacktestResult BacktestEngine::get_result() const {
    return impl_->result;
}

void BacktestEngine::set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) {
    impl_->strategy = strategy;
}

void BacktestEngine::set_data_source(const std::shared_ptr<IDataSource>& data_source) {
    impl_->data_source = data_source;
}

void BacktestEngine::on_progress(std::function<void(double progress)> callback) {
    impl_->progress_callback = callback;
}

} // namespace veloz::backtest
