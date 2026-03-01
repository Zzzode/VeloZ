# Backtest API Reference

This document describes the backtest system configuration and API for VeloZ.

## Overview

The VeloZ backtest system allows you to test trading strategies against historical market data. It supports:

- **Multiple data sources**: CSV files, Binance historical data
- **Event-driven simulation**: Priority-based event queue with virtual clock
- **Optimization algorithms**: Grid search, genetic algorithm, random search, Bayesian optimization
- **Comprehensive reporting**: HTML, JSON, CSV, Markdown formats
- **Progress streaming**: Real-time progress callbacks for UI integration

### Performance

The backtest engine is optimized for high throughput:
- ~187,000 events/second on modern hardware
- 100K events processed in ~500ms
- Supports 1M+ events for comprehensive backtests

---

## Backtest Configuration

### BacktestConfig Structure

| Field | Type | Description |
|-------|------|-------------|
| `strategy_name` | string | Name of the strategy to backtest |
| `symbol` | string | Trading symbol (e.g., "BTCUSDT") |
| `start_time` | int64 | Start timestamp in milliseconds |
| `end_time` | int64 | End timestamp in milliseconds |
| `initial_balance` | double | Starting balance for the backtest |
| `risk_per_trade` | double | Risk percentage per trade (0.0-1.0) |
| `max_position_size` | double | Maximum position size allowed |
| `strategy_parameters` | map | Strategy-specific parameters |
| `data_source` | string | Data source type: `"csv"`, `"binance"` |
| `data_type` | string | Data type: `"kline"`, `"trade"`, `"book"` |
| `time_frame` | string | Time frame: `"1m"`, `"5m"`, `"15m"`, `"1h"`, `"4h"`, `"1d"` |

### Example Configuration

```cpp
BacktestConfig config;
config.strategy_name = kj::str("ma_crossover");
config.symbol = kj::str("BTCUSDT");
config.start_time = 1704067200000;   // 2024-01-01 00:00:00 UTC (ms)
config.end_time = 1706745600000;     // 2024-02-01 00:00:00 UTC (ms)
config.initial_balance = 10000.0;
config.risk_per_trade = 0.02;        // 2% risk per trade
config.max_position_size = 1.0;
config.data_source = kj::str("csv");
config.data_type = kj::str("kline");
config.time_frame = kj::str("1h");
config.strategy_parameters.insert(kj::str("fast_period"), 10.0);
config.strategy_parameters.insert(kj::str("slow_period"), 20.0);
```

---

## Data Sources

### IDataSource Interface

All data sources implement the `IDataSource` interface:

```cpp
class IDataSource : public kj::Refcounted {
public:
  virtual bool connect() = 0;
  virtual bool disconnect() = 0;
  virtual kj::Vector<veloz::market::MarketEvent>
  get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
           kj::StringPtr data_type, kj::StringPtr time_frame) = 0;
  virtual bool download_data(kj::StringPtr symbol, std::int64_t start_time,
                             std::int64_t end_time, kj::StringPtr data_type,
                             kj::StringPtr time_frame, kj::StringPtr output_path) = 0;
};
```

### CSV Data Source

Load historical data from CSV files.

**Configuration**:
- `data_source`: `"csv"`

**Supported Formats**:

| Format | Columns |
|--------|---------|
| Trade | timestamp, symbol, side, price, quantity |
| OHLCV | timestamp, open, high, low, close, volume |
| Book | timestamp, bid_price, bid_qty, ask_price, ask_qty |

**CSV Parse Options**:

```cpp
CSVParseOptions options;
options.format = CSVFormat::Auto;      // Auto-detect from header
options.delimiter = ',';
options.has_header = true;
options.skip_invalid_rows = true;
options.max_rows = 0;                  // 0 = unlimited
options.venue = veloz::common::Venue::Binance;
options.market = veloz::common::MarketKind::Spot;
```

**Example Usage**:

```cpp
auto csv_source = kj::rc<CSVDataSource>();
csv_source->set_data_directory("/path/to/data"_kj);

// Load from file with time filter
auto events = csv_source->load_file("/path/to/data.csv"_kj, start_time, end_time);

// Stream with callback (memory efficient)
csv_source->stream_file("/path/to/data.csv"_kj, [](veloz::market::MarketEvent& event) {
  // Process event
  return true; // Continue streaming
});

// Get parsing statistics
const auto& stats = csv_source->get_stats();
KJ_LOG(INFO, "Loaded ", stats.valid_rows, " events in ", stats.parse_time_ms, "ms");
```

### Binance Historical Data Source

Fetch historical data from Binance API.

**Configuration**:
- `data_source`: `"binance"`
- Requires network access to Binance API
- Supports all Binance trading pairs

**Download Options**:

```cpp
BinanceDownloadOptions options;
options.parallel_download = true;      // Enable parallel downloading
options.max_parallel_requests = 4;     // Maximum concurrent requests
options.validate_data = true;          // Validate downloaded data
options.compress_output = false;       // Compress output file (gzip)
options.append_to_existing = false;    // Append to existing file
options.output_format = kj::str("csv"); // "csv" or "parquet"
```

**Example Usage**:

```cpp
auto binance_source = kj::rc<BinanceDataSource>();

// Download with progress callback
binance_source->download_data_with_progress(
    "BTCUSDT"_kj,
    1704067200000,  // start_time (ms)
    1706745600000,  // end_time (ms)
    "kline"_kj,
    "1h"_kj,
    "/path/to/output.csv"_kj,
    [](const BinanceDownloadProgress& progress) {
      KJ_LOG(INFO, "Progress: ", progress.progress_fraction * 100, "%");
    });

// Download multiple symbols
kj::Vector<kj::String> symbols;
symbols.add(kj::str("BTCUSDT"));
symbols.add(kj::str("ETHUSDT"));
int success_count = binance_source->download_multiple_symbols(
    symbols.asPtr(), start_time, end_time, "kline"_kj, "1h"_kj, "/path/to/output/"_kj);

// Validate downloaded data
auto errors = BinanceDataSource::validate_downloaded_data("/path/to/data.csv"_kj);
if (errors.size() > 0) {
  KJ_LOG(ERROR, "Validation errors: ", errors.size());
}
```

### Data Source Factory

Create data sources using the factory:

```cpp
auto data_source = DataSourceFactory::create_data_source("csv"_kj);
// or
auto data_source = DataSourceFactory::create_data_source("binance"_kj);
```

---

## Backtest Engine

### Engine States

| State | Description |
|-------|-------------|
| `Idle` | Not initialized or reset |
| `Initialized` | Initialized but not running |
| `Running` | Currently running |
| `Paused` | Paused (can be resumed) |
| `Completed` | Completed successfully |
| `Stopped` | Stopped by user |
| `Error` | Error occurred |

### Event Priority System

Events are processed in priority order:

| Priority | Use Case |
|----------|----------|
| `Critical` | Risk events, stop loss |
| `High` | Order fills |
| `Normal` | Market data |
| `Low` | Cleanup, logging |

### Virtual Clock

The backtest engine uses a virtual clock for time simulation:

```cpp
const auto& clock = engine.get_clock();

// Query time
int64_t current_ns = clock.now_ns();
int64_t current_ms = clock.now_ms();

// Query progress
double progress = clock.progress();        // 0.0 to 1.0
int64_t elapsed = clock.elapsed_ns();
int64_t remaining = clock.remaining_ns();
```

### Progress Callbacks

**Simple Progress Callback**:

```cpp
engine.on_progress([](double progress) {
  KJ_LOG(INFO, "Progress: ", progress * 100, "%");
});
```

**Detailed Progress Callback**:

```cpp
engine.on_progress_detailed([](const BacktestProgress& progress) {
  KJ_LOG(INFO, "Events: ", progress.events_processed, "/", progress.total_events,
         " (", progress.events_per_second, " events/sec)");
});
```

**State Change Callback**:

```cpp
engine.on_state_change([](BacktestState old_state, BacktestState new_state) {
  KJ_LOG(INFO, "State: ", to_string(old_state), " -> ", to_string(new_state));
});
```

### Running a Backtest

```cpp
#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

using namespace veloz::backtest;
using namespace veloz::strategy;

// Create and configure engine
auto engine = kj::heap<BacktestEngine>();
engine->initialize(config);

// Set strategy (kj::Rc for reference counting)
auto strategy = StrategyFactory::create("ma_crossover"_kj);
engine->set_strategy(strategy.addRef());

// Set data source
auto data_source = DataSourceFactory::create_data_source("csv"_kj);
engine->set_data_source(data_source.addRef());

// Run backtest
bool success = engine->run();

// Get results
auto result = engine->get_result();
```

### Pause/Resume Support

```cpp
// Pause the backtest
engine->pause();

// Resume later
engine->resume();

// Stop completely
engine->stop();

// Reset for new run
engine->reset();
```

### Step-by-Step Debugging

```cpp
// Process one event at a time
while (engine->step()) {
  auto& clock = engine->get_clock();
  KJ_LOG(INFO, "Time: ", clock.now_ms(), " Pending: ", engine->pending_events());
}
```

---

## Parameter Optimization

### Optimization Algorithms

| Algorithm | Description | Best For |
|-----------|-------------|----------|
| `GridSearch` | Exhaustive search of all combinations | Small parameter spaces |
| `GeneticAlgorithm` | Evolutionary optimization | Large parameter spaces |
| `RandomSearch` | Random sampling | High-dimensional spaces |
| `BayesianOptimization` | Probabilistic model-guided search | Expensive evaluations |

### Grid Search Optimizer

```cpp
auto optimizer = kj::heap<GridSearchOptimizer>();
optimizer->initialize(config);

// Set parameter ranges
kj::TreeMap<kj::String, std::pair<double, double>> ranges;
ranges.insert(kj::str("fast_period"), {5.0, 20.0});
ranges.insert(kj::str("slow_period"), {15.0, 50.0});
optimizer->set_parameter_ranges(ranges);

// Configure optimization
optimizer->set_optimization_target("sharpe"_kj);  // "sharpe", "return", "win_rate"
optimizer->set_max_iterations(100);
optimizer->set_data_source(data_source.addRef());

// Run optimization
optimizer->optimize(strategy.addRef());

// Get results
auto& best_params = optimizer->get_best_parameters();
auto results = optimizer->get_results();
```

### Genetic Algorithm Optimizer

```cpp
auto optimizer = kj::heap<GeneticAlgorithmOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);

// GA-specific configuration
optimizer->set_population_size(50);
optimizer->set_mutation_rate(0.1);
optimizer->set_crossover_rate(0.8);
optimizer->set_elite_count(5);
optimizer->set_tournament_size(3);
optimizer->set_convergence_params(0.001, 10);  // threshold, generations

optimizer->optimize(strategy.addRef());
```

### Random Search Optimizer

```cpp
auto optimizer = kj::heap<RandomSearchOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);
optimizer->set_max_iterations(1000);

// Progress callback
optimizer->set_progress_callback([](const OptimizationProgress& progress) {
  KJ_LOG(INFO, "Iteration ", progress.current_iteration, "/", progress.total_iterations,
         " Best: ", progress.best_fitness);
});

optimizer->optimize(strategy.addRef());

// Get ranked results
auto ranked = optimizer->get_ranked_results(10);  // Top 10
```

### Bayesian Optimizer

```cpp
auto optimizer = kj::heap<BayesianOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);

// Bayesian-specific configuration
optimizer->set_initial_samples(10);              // Random samples before model
optimizer->set_acquisition_function("ei"_kj);    // "ei", "ucb", "pi"
optimizer->set_exploration_params(2.0, 0.01);    // kappa, xi

optimizer->set_progress_callback([](const OptimizationProgress& progress) {
  KJ_LOG(INFO, "Best fitness: ", progress.best_fitness);
});

optimizer->optimize(strategy.addRef());

// Get model predictions
auto [mean, std] = optimizer->predict(test_params);
```

### Optimizer Factory

```cpp
auto optimizer = OptimizerFactory::create(OptimizationAlgorithm::GeneticAlgorithm);
```

### Optimization Progress Structure

```cpp
struct OptimizationProgress {
  int current_iteration;
  int total_iterations;
  double best_fitness;
  double current_fitness;
  double progress_fraction;
  kj::String status;
  kj::TreeMap<kj::String, double> current_parameters;
  kj::TreeMap<kj::String, double> best_parameters;
};
```

### Ranked Result Structure

```cpp
struct RankedResult {
  int rank;
  double fitness;
  kj::TreeMap<kj::String, double> parameters;
  BacktestResult result;
};
```

---

## Backtest Results

### BacktestResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `strategy_name` | string | Strategy name |
| `symbol` | string | Trading symbol |
| `start_time` | int64 | Backtest start time (ms) |
| `end_time` | int64 | Backtest end time (ms) |
| `initial_balance` | double | Starting balance |
| `final_balance` | double | Ending balance |
| `total_return` | double | Total return percentage |
| `max_drawdown` | double | Maximum drawdown percentage |
| `sharpe_ratio` | double | Sharpe ratio |
| `win_rate` | double | Win rate (0.0-1.0) |
| `profit_factor` | double | Profit factor (gross profit / gross loss) |
| `trade_count` | int | Total number of trades |
| `win_count` | int | Number of winning trades |
| `lose_count` | int | Number of losing trades |
| `avg_win` | double | Average winning trade |
| `avg_lose` | double | Average losing trade |
| `trades` | vector | List of trade records |
| `equity_curve` | vector | Equity curve data points |
| `drawdown_curve` | vector | Drawdown curve data points |

### TradeRecord Structure

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | int64 | Trade execution time (ms) |
| `symbol` | string | Trading symbol |
| `side` | string | Trade side: `"buy"` or `"sell"` |
| `price` | double | Execution price |
| `quantity` | double | Trade quantity |
| `fee` | double | Trading fee |
| `pnl` | double | Profit/loss for this trade |
| `strategy_id` | string | Strategy identifier |

### EquityCurvePoint Structure

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | int64 | Time (ms) |
| `equity` | double | Account equity |
| `cumulative_return` | double | Cumulative return percentage |

### DrawdownPoint Structure

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | int64 | Time (ms) |
| `drawdown` | double | Drawdown percentage |

---

## Report Generation

### Report Formats

| Format | Description |
|--------|-------------|
| `HTML` | Comprehensive HTML report with styling |
| `JSON` | Machine-readable JSON format |
| `CSV` | Trade list in CSV format |
| `Markdown` | Markdown report for documentation |

### Report Configuration

```cpp
ReportConfig config;
config.include_equity_curve = true;
config.include_drawdown_curve = true;
config.include_trade_list = true;
config.include_monthly_returns = true;
config.include_trade_analysis = true;
config.include_risk_metrics = true;
config.title = kj::str("My Backtest Report");
config.description = kj::str("MA Crossover Strategy Backtest");
config.author = kj::str("VeloZ");
```

### Generating Reports

```cpp
BacktestReporter reporter;
reporter.set_config(config);

// Generate HTML report
kj::String html = reporter.generate_html_report(result);

// Generate JSON report
kj::String json = reporter.generate_json_report(result);

// Generate Markdown report
kj::String markdown = reporter.generate_markdown_report(result);

// Generate CSV trade list
kj::String csv = reporter.generate_csv_trades(result);

// Save to file
reporter.generate_report(result, "/path/to/report.html"_kj);
reporter.generate_report_format(result, "/path/to/report.json"_kj, ReportFormat::JSON);

// Export curves to CSV
reporter.export_equity_curve_csv(result, "/path/to/equity.csv"_kj);
reporter.export_drawdown_curve_csv(result, "/path/to/drawdown.csv"_kj);
```

### Comparison Reports

Compare multiple backtest results:

```cpp
kj::Vector<BacktestResult> results;
results.add(kj::mv(result1));
results.add(kj::mv(result2));
results.add(kj::mv(result3));

reporter.generate_comparison_report(results.asPtr(), "/path/to/comparison.html"_kj);
```

### Extended Analytics

**Monthly Returns**:

```cpp
auto monthly = BacktestReporter::calculate_monthly_returns(result);
for (const auto& m : monthly) {
  KJ_LOG(INFO, m.year, "-", m.month, ": ", m.return_pct, "%");
}
```

**Trade Analysis**:

```cpp
auto analysis = BacktestReporter::analyze_trades(result);
KJ_LOG(INFO, "Best trade: ", analysis.best_trade_pnl);
KJ_LOG(INFO, "Worst trade: ", analysis.worst_trade_pnl);
KJ_LOG(INFO, "Max consecutive wins: ", analysis.max_consecutive_wins);
KJ_LOG(INFO, "Avg trade duration: ", analysis.avg_trade_duration_ms, "ms");
```

**Extended Risk Metrics**:

```cpp
auto metrics = BacktestReporter::calculate_extended_metrics(result);
KJ_LOG(INFO, "Sortino ratio: ", metrics.sortino_ratio);
KJ_LOG(INFO, "Calmar ratio: ", metrics.calmar_ratio);
KJ_LOG(INFO, "VaR (95%): ", metrics.value_at_risk_95);
KJ_LOG(INFO, "Expected shortfall: ", metrics.expected_shortfall_95);
```

### JSON Report Structure

```json
{
  "strategy_name": "ma_crossover",
  "symbol": "BTCUSDT",
  "start_time": 1704067200000,
  "end_time": 1706745600000,
  "initial_balance": 10000.0,
  "final_balance": 12500.0,
  "total_return": 0.25,
  "max_drawdown": 0.08,
  "sharpe_ratio": 1.5,
  "win_rate": 0.55,
  "profit_factor": 1.8,
  "trade_count": 50,
  "win_count": 28,
  "lose_count": 22,
  "avg_win": 150.0,
  "avg_lose": -80.0,
  "trades": [
    {
      "timestamp": 1704067200000,
      "symbol": "BTCUSDT",
      "side": "buy",
      "price": 42000.0,
      "quantity": 0.1,
      "fee": 4.2,
      "pnl": 0.0,
      "strategy_id": "ma_crossover_1"
    }
  ],
  "equity_curve": [
    {"timestamp": 1704067200000, "equity": 10000.0, "cumulative_return": 0.0}
  ],
  "drawdown_curve": [
    {"timestamp": 1704067200000, "drawdown": 0.0}
  ]
}
```

---

## Event Streaming for Progress

### BacktestProgress Structure

```cpp
struct BacktestProgress {
  double progress_fraction;      // 0.0 to 1.0
  int64_t events_processed;      // Number of events processed
  int64_t total_events;          // Total number of events
  int64_t current_time_ns;       // Current simulated time
  int64_t elapsed_real_ns;       // Real elapsed time
  double events_per_second;      // Processing rate
  BacktestState state;           // Current engine state
  kj::String message;            // Optional status message
};
```

### Streaming Progress to UI

```cpp
engine.on_progress_detailed([&websocket](const BacktestProgress& progress) {
  // Send progress update via WebSocket or SSE
  auto json = kj::str(
    "{\"type\":\"backtest_progress\","
    "\"progress\":", progress.progress_fraction, ","
    "\"events_processed\":", progress.events_processed, ","
    "\"total_events\":", progress.total_events, ","
    "\"events_per_second\":", progress.events_per_second, ","
    "\"state\":\"", to_string(progress.state), "\"}"
  );
  websocket.send(json);
});
```

---

## Complete Usage Example

```cpp
#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/backtest/reporter.h"
#include "veloz/strategy/strategy.h"

using namespace veloz::backtest;
using namespace veloz::strategy;

int main() {
    // 1. Create configuration
    BacktestConfig config;
    config.strategy_name = kj::str("ma_crossover");
    config.symbol = kj::str("BTCUSDT");
    config.start_time = 1704067200000;   // 2024-01-01
    config.end_time = 1706745600000;     // 2024-02-01
    config.initial_balance = 10000.0;
    config.risk_per_trade = 0.02;
    config.max_position_size = 1.0;
    config.data_source = kj::str("csv");
    config.data_type = kj::str("kline");
    config.time_frame = kj::str("1h");

    // 2. Create data source
    auto data_source = DataSourceFactory::create_data_source("csv"_kj);
    auto csv_source = kj::downcast<CSVDataSource>(data_source.get());
    csv_source->set_data_directory("/path/to/data"_kj);

    // 3. Create strategy
    auto strategy = StrategyFactory::create("ma_crossover"_kj);

    // 4. Run single backtest
    auto engine = kj::heap<BacktestEngine>();
    engine->initialize(config);
    engine->set_data_source(data_source.addRef());
    engine->set_strategy(strategy.addRef());

    engine->on_progress_detailed([](const BacktestProgress& p) {
        KJ_LOG(INFO, "Progress: ", p.progress_fraction * 100, "% ",
               "(", p.events_per_second, " events/sec)");
    });

    bool success = engine->run();
    auto result = engine->get_result();

    // 5. Generate reports
    BacktestReporter reporter;
    reporter.generate_report(result, "/path/to/report.html"_kj);
    reporter.generate_report_format(result, "/path/to/report.json"_kj, ReportFormat::JSON);

    // 6. Run parameter optimization
    auto optimizer = kj::heap<GeneticAlgorithmOptimizer>();
    optimizer->initialize(config);

    kj::TreeMap<kj::String, std::pair<double, double>> ranges;
    ranges.insert(kj::str("fast_period"), {5.0, 20.0});
    ranges.insert(kj::str("slow_period"), {15.0, 50.0});
    optimizer->set_parameter_ranges(ranges);
    optimizer->set_optimization_target("sharpe"_kj);
    optimizer->set_population_size(50);
    optimizer->set_data_source(data_source.addRef());

    optimizer->optimize(strategy.addRef());

    auto& best_params = optimizer->get_best_parameters();
    KJ_LOG(INFO, "Best parameters found:");
    for (const auto& entry : best_params) {
        KJ_LOG(INFO, "  ", entry.key, " = ", entry.value);
    }

    return 0;
}
```

---

## Related Documentation

- [HTTP API Reference](http_api.md) - REST API endpoints
- [SSE API](sse_api.md) - Server-Sent Events stream
- [Engine Protocol](engine_protocol.md) - Engine stdio protocol
- [KJ Library Usage](../kj/library_usage_guide.md) - KJ type patterns
