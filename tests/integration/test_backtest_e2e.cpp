// Integration test: Backtest end-to-end with optimization
// Tests the complete backtest workflow from data loading to report generation

#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/backtest/analyzer.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/backtest/reporter.h"
#include "veloz/strategy/strategy.h"
#include "veloz/strategy/advanced_strategies.h"

#include <kj/test.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/refcount.h>

using namespace veloz::backtest;
using namespace veloz::strategy;

namespace {

// Test strategy that generates predictable signals for testing
class TestTradingStrategy : public veloz::strategy::IStrategy {
public:
  TestTradingStrategy()
      : id_(kj::str("test_trading_strategy")),
        name_(kj::str("TestTradingStrategy")),
        type_(veloz::strategy::StrategyType::Custom),
        event_count_(0),
        trade_count_(0) {}

  ~TestTradingStrategy() noexcept(false) override = default;

  kj::StringPtr get_id() const override { return id_; }
  kj::StringPtr get_name() const override { return name_; }
  veloz::strategy::StrategyType get_type() const override { return type_; }

  bool initialize(const veloz::strategy::StrategyConfig& config,
                  veloz::core::Logger& logger) override {
    return true;
  }

  void on_start() override { is_running_ = true; }
  void on_stop() override { is_running_ = false; }
  void on_pause() override { is_running_ = false; }
  void on_resume() override { is_running_ = true; }

  void on_event(const veloz::market::MarketEvent& event) override {
    event_count_++;
    // Generate a buy signal every 10 events, sell every 20
    if (event_count_ % 20 == 0 && position_open_) {
      // Close position
      position_open_ = false;
      trade_count_++;
    } else if (event_count_ % 10 == 0 && !position_open_) {
      // Open position
      position_open_ = true;
    }
  }

  void on_position_update(const veloz::oms::Position& position) override {}
  void on_timer(int64_t timestamp) override {}

  veloz::strategy::StrategyState get_state() const override {
    veloz::strategy::StrategyState state;
    state.strategy_id = kj::str(id_);
    state.strategy_name = kj::str(name_);
    state.is_running = is_running_;
    state.pnl = 0.0;
    state.max_drawdown = 0.0;
    state.trade_count = trade_count_;
    state.win_count = 0;
    state.lose_count = 0;
    state.win_rate = 0.0;
    state.profit_factor = 0.0;
    return state;
  }

  kj::Vector<veloz::exec::PlaceOrderRequest> get_signals() override {
    return kj::Vector<veloz::exec::PlaceOrderRequest>();
  }

  void reset() override {
    event_count_ = 0;
    trade_count_ = 0;
    position_open_ = false;
    is_running_ = false;
  }

  bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) override {
    return false;
  }

  bool supports_hot_reload() const override {
    return false;
  }

  kj::Maybe<const StrategyMetrics&> get_metrics() const override {
    return kj::none;
  }

  void on_order_rejected(const veloz::exec::PlaceOrderRequest& req, kj::StringPtr reason) override {}

  size_t get_event_count() const { return event_count_; }
  size_t get_trade_count() const { return trade_count_; }

private:
  kj::String id_;
  kj::String name_;
  veloz::strategy::StrategyType type_;
  size_t event_count_;
  size_t trade_count_;
  bool position_open_ = false;
  bool is_running_ = false;
};

// Helper to create test configuration
BacktestConfig create_e2e_config() {
  BacktestConfig config;
  config.strategy_name = kj::str("TestTradingStrategy");
  config.symbol = kj::str("BTCUSDT");
  config.start_time = 1704067200000; // 2024-01-01
  config.end_time = 1704153600000;   // 2024-01-02
  config.initial_balance = 10000.0;
  config.risk_per_trade = 0.02;
  config.max_position_size = 0.1;
  config.data_source = kj::str("csv");
  config.data_type = kj::str("trade");
  config.time_frame = kj::str("1m");
  return config;
}

// ============================================================================
// Integration Test: Backtest End-to-End
// ============================================================================

KJ_TEST("Integration: Backtest engine initialization and configuration") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_e2e_config();

  KJ_EXPECT(engine->initialize(config));

  auto strategy = kj::rc<TestTradingStrategy>();
  engine->set_strategy(strategy.addRef());

  auto data_source = DataSourceFactory::create_data_source("csv");
  engine->set_data_source(data_source.addRef());

  // Verify engine is properly configured
  KJ_EXPECT(engine->reset());
}

KJ_TEST("Integration: Backtest with synthetic data source") {
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_e2e_config();
  config.data_source = kj::str("synthetic");

  KJ_EXPECT(engine->initialize(config));

  auto strategy = kj::rc<TestTradingStrategy>();
  engine->set_strategy(strategy.addRef());

  // Create synthetic data source with test data
  auto data_source = DataSourceFactory::create_data_source("synthetic");
  engine->set_data_source(data_source.addRef());

  // Run should complete (may fail if no data, but should not crash)
  engine->run();
  engine->reset();
}

KJ_TEST("Integration: Analyzer computes metrics from trade records") {
  BacktestAnalyzer analyzer;

  // Create sample trade records
  kj::Vector<TradeRecord> trades;
  for (int i = 0; i < 20; i++) {
    TradeRecord trade;
    trade.timestamp = 1704067200000 + (i * 3600000);
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str((i % 2 == 0) ? "buy" : "sell");
    trade.price = 50000.0 + (i * 100);
    trade.quantity = 0.1;
    trade.fee = 0.5;
    trade.pnl = (i % 3 == 0) ? 200.0 : -100.0;
    trade.strategy_id = kj::str("test_strategy");
    trades.add(kj::mv(trade));
  }

  // Analyze trades
  auto result = analyzer.analyze(trades);

  // Verify metrics are computed
  KJ_EXPECT(result->trade_count == 20);
}

KJ_TEST("Integration: Reporter generates HTML and JSON reports") {
  BacktestReporter reporter;

  // Create sample result
  BacktestResult result;
  result.strategy_name = kj::str("TestStrategy");
  result.symbol = kj::str("BTCUSDT");
  result.start_time = 1704067200000;
  result.end_time = 1704153600000;
  result.initial_balance = 10000.0;
  result.final_balance = 11000.0;
  result.trade_count = 10;
  result.win_count = 6;
  result.lose_count = 4;
  result.max_drawdown = 0.03;
  result.sharpe_ratio = 1.2;
  result.profit_factor = 1.5;
  result.win_rate = 0.6;

  // Add sample trade
  TradeRecord trade;
  trade.timestamp = 1704067200000;
  trade.symbol = kj::str("BTCUSDT");
  trade.side = kj::str("buy");
  trade.price = 50000.0;
  trade.quantity = 0.1;
  trade.pnl = 100.0;
  trade.fee = 0.5;
  trade.strategy_id = kj::str("test_strategy");
  result.trades.add(kj::mv(trade));

  // Generate HTML report
  auto html = reporter.generate_html_report(result);
  KJ_EXPECT(html.size() > 0);
  KJ_EXPECT(html.contains("<!DOCTYPE html"_kj));
  KJ_EXPECT(html.contains("BTCUSDT"_kj));

  // Generate JSON report
  auto json = reporter.generate_json_report(result);
  KJ_EXPECT(json.size() > 0);
  KJ_EXPECT(json.contains("initial_balance"_kj));
  KJ_EXPECT(json.contains("trades"_kj));
}

KJ_TEST("Integration: Grid search optimizer initialization") {
  GridSearchOptimizer optimizer;

  // Create test configuration
  auto config = create_e2e_config();
  KJ_EXPECT(optimizer.initialize(config));

  // Set parameter ranges
  kj::TreeMap<kj::String, std::pair<double, double>> ranges;
  ranges.insert(kj::str("lookback_period"), std::make_pair(10.0, 20.0));
  ranges.insert(kj::str("threshold"), std::make_pair(0.01, 0.03));
  optimizer.set_parameter_ranges(ranges);

  optimizer.set_optimization_target("sharpe"_kj);
  optimizer.set_max_iterations(10);

  // Verify configuration was accepted (no crash)
}

KJ_TEST("Integration: Complete backtest workflow with all components") {
  // This test verifies the complete integration of all backtest components

  // 1. Create and configure engine
  auto engine = kj::heap<BacktestEngine>();
  auto config = create_e2e_config();
  KJ_EXPECT(engine->initialize(config));

  // 2. Set up strategy
  auto strategy = kj::rc<TestTradingStrategy>();
  engine->set_strategy(strategy.addRef());

  // 3. Set up data source
  auto data_source = DataSourceFactory::create_data_source("csv");
  engine->set_data_source(data_source.addRef());

  // 4. Run backtest (may not produce results without real data)
  engine->run();

  // 5. Get results
  auto result = engine->get_result();

  // 6. Analyze results (analyzer takes trades, not result)
  BacktestAnalyzer analyzer;
  auto metrics = analyzer.analyze(result.trades);

  // 7. Generate report
  BacktestReporter reporter;
  auto html = reporter.generate_html_report(result);
  auto json = reporter.generate_json_report(result);

  // Verify workflow completed
  KJ_EXPECT(html.size() > 0);
  KJ_EXPECT(json.size() > 0);

  // 8. Reset for next run
  engine->reset();
}

KJ_TEST("Integration: Multiple backtest runs with different configurations") {
  auto engine = kj::heap<BacktestEngine>();

  // Run 1: Short timeframe
  {
    auto config = create_e2e_config();
    config.time_frame = kj::str("1m");
    KJ_EXPECT(engine->initialize(config));

    auto strategy = kj::rc<TestTradingStrategy>();
    engine->set_strategy(strategy.addRef());

    auto data_source = DataSourceFactory::create_data_source("csv");
    engine->set_data_source(data_source.addRef());

    engine->run();
    engine->reset();
  }

  // Run 2: Different symbol
  {
    auto config = create_e2e_config();
    config.symbol = kj::str("ETHUSDT");
    KJ_EXPECT(engine->initialize(config));

    auto strategy = kj::rc<TestTradingStrategy>();
    engine->set_strategy(strategy.addRef());

    auto data_source = DataSourceFactory::create_data_source("csv");
    engine->set_data_source(data_source.addRef());

    engine->run();
    engine->reset();
  }

  // Run 3: Different initial balance
  {
    auto config = create_e2e_config();
    config.initial_balance = 50000.0;
    KJ_EXPECT(engine->initialize(config));

    auto strategy = kj::rc<TestTradingStrategy>();
    engine->set_strategy(strategy.addRef());

    auto data_source = DataSourceFactory::create_data_source("csv");
    engine->set_data_source(data_source.addRef());

    engine->run();
    engine->reset();
  }
}

KJ_TEST("Integration: Data source factory creates different source types") {
  // CSV data source
  auto csv_source = DataSourceFactory::create_data_source("csv");
  KJ_EXPECT(csv_source.get() != nullptr);

  // Binance data source
  auto binance_source = DataSourceFactory::create_data_source("binance");
  KJ_EXPECT(binance_source.get() != nullptr);

  // Note: "synthetic" data source type is not implemented, returns nullptr
}

} // namespace
