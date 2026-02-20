# Backtest User Guide

This guide explains how to use the VeloZ backtesting system to test and optimize your trading strategies.

## Overview

The VeloZ backtest system allows you to:
- Test trading strategies against historical market data
- Analyze performance metrics (Sharpe ratio, drawdown, win rate, etc.)
- Optimize strategy parameters using Grid Search, Genetic Algorithm, Random Search, or Bayesian Optimization
- Generate comprehensive reports in HTML, JSON, CSV, and Markdown formats

### Performance

The backtest engine is optimized for high throughput:
- ~187,000 events/second on modern hardware
- 100K events processed in ~500ms
- Supports 1M+ events for comprehensive backtests

## Quick Start

### 1. Prepare Market Data

VeloZ supports multiple data sources:

**CSV Files** (Trade format)
```csv
timestamp,symbol,side,price,quantity
1704067200000,BTCUSDT,BUY,42000.50,0.001
1704067201000,BTCUSDT,SELL,42001.00,0.002
```

**CSV Files** (OHLCV format)
```csv
timestamp,open,high,low,close,volume
1704067200000,42000.0,42500.0,41800.0,42300.0,100.5
1704070800000,42300.0,42800.0,42100.0,42600.0,150.2
```

### 2. Configure the Backtest

```cpp
#include "veloz/backtest/backtest_engine.h"

using namespace veloz::backtest;

BacktestConfig config;
config.strategy_name = kj::str("ma_crossover");
config.symbol = kj::str("BTCUSDT");
config.start_time = 1704067200000;   // 2024-01-01 (milliseconds)
config.end_time = 1706745600000;     // 2024-02-01 (milliseconds)
config.initial_balance = 10000.0;
config.risk_per_trade = 0.02;        // 2% risk per trade
config.max_position_size = 1.0;
config.data_source = kj::str("csv");
config.data_type = kj::str("kline");
config.time_frame = kj::str("1h");

// Strategy-specific parameters
config.strategy_parameters.insert(kj::str("fast_period"), 10.0);
config.strategy_parameters.insert(kj::str("slow_period"), 20.0);
```

### 3. Run the Backtest

```cpp
#include "veloz/backtest/data_source.h"
#include "veloz/strategy/strategy.h"

// Create data source
auto data_source = DataSourceFactory::create_data_source("csv"_kj);
auto csv_source = static_cast<CSVDataSource*>(data_source.get());
csv_source->set_data_directory("/path/to/data"_kj);

// Create strategy
auto strategy = veloz::strategy::StrategyFactory::create("ma_crossover"_kj);

// Create and run engine
auto engine = kj::heap<BacktestEngine>();
engine->initialize(config);
engine->set_data_source(data_source.addRef());
engine->set_strategy(strategy.addRef());

// Add progress callback
engine->on_progress([](double progress) {
    KJ_LOG(INFO, "Progress: ", progress * 100, "%");
});

bool success = engine->run();
auto result = engine->get_result();
```

### 4. Analyze Results

```cpp
#include "veloz/backtest/reporter.h"

// Print summary
KJ_LOG(INFO, "Total Return: ", result.total_return * 100, "%");
KJ_LOG(INFO, "Sharpe Ratio: ", result.sharpe_ratio);
KJ_LOG(INFO, "Max Drawdown: ", result.max_drawdown * 100, "%");
KJ_LOG(INFO, "Win Rate: ", result.win_rate * 100, "%");
KJ_LOG(INFO, "Trade Count: ", result.trade_count);

// Generate reports
BacktestReporter reporter;
reporter.generate_report(result, "/path/to/report.html"_kj);
reporter.generate_report_format(result, "/path/to/report.json"_kj, ReportFormat::JSON);
```

---

## Data Sources

### CSV Data Source

Load historical data from CSV files with automatic format detection.

**Supported Formats:**

| Format | Required Columns |
|--------|-----------------|
| Trade | timestamp, symbol, side, price, quantity |
| OHLCV | timestamp, open, high, low, close, volume |
| Book | timestamp, bid_price, bid_qty, ask_price, ask_qty |

**Basic Usage:**

```cpp
auto data_source = DataSourceFactory::create_data_source("csv"_kj);
auto csv_source = static_cast<CSVDataSource*>(data_source.get());
csv_source->set_data_directory("/path/to/data"_kj);
```

**Advanced Configuration:**

```cpp
CSVParseOptions options;
options.format = CSVFormat::Auto;      // Auto-detect from header
options.delimiter = ',';
options.has_header = true;
options.skip_invalid_rows = true;
options.max_rows = 0;                  // 0 = unlimited
options.venue = veloz::common::Venue::Binance;
options.market = veloz::common::MarketKind::Spot;

csv_source->set_parse_options(options);
```

**Load from Specific File:**

```cpp
// Load with time filter
auto events = csv_source->load_file(
    "/path/to/data.csv"_kj,
    1704067200000,  // start_time (ms)
    1706745600000   // end_time (ms)
);

// Check parsing statistics
const auto& stats = csv_source->get_stats();
KJ_LOG(INFO, "Loaded ", stats.valid_rows, " events");
KJ_LOG(INFO, "Invalid rows: ", stats.invalid_rows);
KJ_LOG(INFO, "Parse time: ", stats.parse_time_ms, "ms");
```

**Stream Large Files (Memory Efficient):**

```cpp
int64_t events_processed = csv_source->stream_file(
    "/path/to/large_data.csv"_kj,
    [](veloz::market::MarketEvent& event) {
        // Process each event
        return true;  // Continue streaming (false to stop)
    },
    start_time,
    end_time
);
```

### Binance Historical Data Source

Download historical data directly from Binance API.

**Basic Download:**

```cpp
auto data_source = DataSourceFactory::create_data_source("binance"_kj);
auto binance_source = static_cast<BinanceDataSource*>(data_source.get());

bool success = binance_source->download_data(
    "BTCUSDT"_kj,
    1704067200000,  // start_time (ms)
    1706745600000,  // end_time (ms)
    "kline"_kj,
    "1h"_kj,
    "/path/to/output.csv"_kj
);
```

**Download with Progress:**

```cpp
binance_source->download_data_with_progress(
    "BTCUSDT"_kj,
    start_time,
    end_time,
    "kline"_kj,
    "1h"_kj,
    "/path/to/output.csv"_kj,
    [](const BinanceDownloadProgress& progress) {
        KJ_LOG(INFO, "Downloaded: ", progress.progress_fraction * 100, "%");
        KJ_LOG(INFO, "Records: ", progress.total_records);
    }
);
```

**Download Multiple Symbols:**

```cpp
kj::Vector<kj::String> symbols;
symbols.add(kj::str("BTCUSDT"));
symbols.add(kj::str("ETHUSDT"));
symbols.add(kj::str("BNBUSDT"));

int success_count = binance_source->download_multiple_symbols(
    symbols.asPtr(),
    start_time,
    end_time,
    "kline"_kj,
    "1h"_kj,
    "/path/to/output/"_kj
);
```

**Supported Time Frames:**
`1m`, `3m`, `5m`, `15m`, `30m`, `1h`, `2h`, `4h`, `6h`, `8h`, `12h`, `1d`, `3d`, `1w`, `1M`

---

## Parameter Optimization

### Grid Search Optimizer

Exhaustively searches all parameter combinations. Best for small parameter spaces.

```cpp
#include "veloz/backtest/optimizer.h"

auto optimizer = kj::heap<GridSearchOptimizer>();
optimizer->initialize(config);

// Define parameter ranges (min, max)
kj::TreeMap<kj::String, std::pair<double, double>> ranges;
ranges.insert(kj::str("fast_period"), {5.0, 20.0});
ranges.insert(kj::str("slow_period"), {20.0, 50.0});
ranges.insert(kj::str("threshold"), {0.01, 0.05});

optimizer->set_parameter_ranges(ranges);
optimizer->set_optimization_target("sharpe"_kj);
optimizer->set_max_iterations(100);
optimizer->set_data_source(data_source.addRef());

// Run optimization
optimizer->optimize(strategy.addRef());

// Get best parameters
auto& best_params = optimizer->get_best_parameters();
for (const auto& entry : best_params) {
    KJ_LOG(INFO, entry.key, " = ", entry.value);
}
```

### Genetic Algorithm Optimizer

Uses evolutionary optimization. Best for large parameter spaces.

```cpp
auto optimizer = kj::heap<GeneticAlgorithmOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);
optimizer->set_optimization_target("sharpe"_kj);

// GA-specific configuration
optimizer->set_population_size(50);
optimizer->set_mutation_rate(0.1);
optimizer->set_crossover_rate(0.8);
optimizer->set_elite_count(5);
optimizer->set_tournament_size(3);
optimizer->set_convergence_params(0.001, 10);  // threshold, generations

optimizer->set_data_source(data_source.addRef());
optimizer->optimize(strategy.addRef());
```

### Random Search Optimizer

Samples random parameter combinations. Often competitive with grid search for high-dimensional spaces.

```cpp
auto optimizer = kj::heap<RandomSearchOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);
optimizer->set_max_iterations(1000);

// Progress callback
optimizer->set_progress_callback([](const OptimizationProgress& progress) {
    KJ_LOG(INFO, "Iteration ", progress.current_iteration, "/", progress.total_iterations);
    KJ_LOG(INFO, "Best fitness: ", progress.best_fitness);
});

optimizer->set_data_source(data_source.addRef());
optimizer->optimize(strategy.addRef());

// Get top 10 results
auto ranked = optimizer->get_ranked_results(10);
for (const auto& r : ranked) {
    KJ_LOG(INFO, "Rank ", r.rank, ": fitness=", r.fitness);
}
```

### Bayesian Optimizer

Uses probabilistic model to guide search. Most sample-efficient for expensive evaluations.

```cpp
auto optimizer = kj::heap<BayesianOptimizer>();
optimizer->initialize(config);
optimizer->set_parameter_ranges(ranges);

// Bayesian-specific configuration
optimizer->set_initial_samples(10);           // Random samples before model
optimizer->set_acquisition_function("ei"_kj); // "ei", "ucb", "pi"
optimizer->set_exploration_params(2.0, 0.01); // kappa, xi

optimizer->set_progress_callback([](const OptimizationProgress& progress) {
    KJ_LOG(INFO, "Best fitness: ", progress.best_fitness);
});

optimizer->set_data_source(data_source.addRef());
optimizer->optimize(strategy.addRef());
```

### Optimization Targets

| Target | Description |
|--------|-------------|
| `sharpe` | Sharpe ratio (risk-adjusted return) |
| `return` | Total return |
| `win_rate` | Percentage of winning trades |

---

## Performance Metrics

The backtest result includes these metrics:

| Metric | Description |
|--------|-------------|
| `total_return` | Total percentage return |
| `max_drawdown` | Maximum peak-to-trough decline |
| `sharpe_ratio` | Risk-adjusted return |
| `win_rate` | Percentage of profitable trades |
| `profit_factor` | Gross profit / gross loss |
| `trade_count` | Total number of trades |
| `win_count` | Number of winning trades |
| `lose_count` | Number of losing trades |
| `avg_win` | Average profit on winning trades |
| `avg_lose` | Average loss on losing trades |

### Extended Risk Metrics

```cpp
auto metrics = BacktestReporter::calculate_extended_metrics(result);

KJ_LOG(INFO, "Sortino ratio: ", metrics.sortino_ratio);
KJ_LOG(INFO, "Calmar ratio: ", metrics.calmar_ratio);
KJ_LOG(INFO, "Omega ratio: ", metrics.omega_ratio);
KJ_LOG(INFO, "VaR (95%): ", metrics.value_at_risk_95);
KJ_LOG(INFO, "Expected shortfall: ", metrics.expected_shortfall_95);
KJ_LOG(INFO, "Skewness: ", metrics.skewness);
KJ_LOG(INFO, "Kurtosis: ", metrics.kurtosis);
```

### Trade Analysis

```cpp
auto analysis = BacktestReporter::analyze_trades(result);

KJ_LOG(INFO, "Best trade: ", analysis.best_trade_pnl);
KJ_LOG(INFO, "Worst trade: ", analysis.worst_trade_pnl);
KJ_LOG(INFO, "Max consecutive wins: ", analysis.max_consecutive_wins);
KJ_LOG(INFO, "Max consecutive losses: ", analysis.max_consecutive_losses);
KJ_LOG(INFO, "Avg trade duration: ", analysis.avg_trade_duration_ms, "ms");
```

### Monthly Returns

```cpp
auto monthly = BacktestReporter::calculate_monthly_returns(result);

for (const auto& m : monthly) {
    KJ_LOG(INFO, m.year, "-", m.month, ": ", m.return_pct, "% (",
           m.trade_count, " trades, max DD: ", m.max_drawdown, "%)");
}
```

---

## Report Generation

### Report Formats

| Format | Description |
|--------|-------------|
| HTML | Comprehensive report with styling and tables |
| JSON | Machine-readable format for UI integration |
| CSV | Trade list for spreadsheet analysis |
| Markdown | Documentation-friendly format |

### Generate Reports

```cpp
BacktestReporter reporter;

// Configure report options
ReportConfig report_config;
report_config.include_equity_curve = true;
report_config.include_drawdown_curve = true;
report_config.include_trade_list = true;
report_config.include_monthly_returns = true;
report_config.include_trade_analysis = true;
report_config.include_risk_metrics = true;
report_config.title = kj::str("MA Crossover Backtest");
report_config.author = kj::str("VeloZ");

reporter.set_config(report_config);

// Generate different formats
reporter.generate_report(result, "/path/to/report.html"_kj);
reporter.generate_report_format(result, "/path/to/report.json"_kj, ReportFormat::JSON);
reporter.generate_report_format(result, "/path/to/report.md"_kj, ReportFormat::Markdown);

// Export data for charting
reporter.export_equity_curve_csv(result, "/path/to/equity.csv"_kj);
reporter.export_drawdown_curve_csv(result, "/path/to/drawdown.csv"_kj);
```

### Compare Multiple Results

```cpp
kj::Vector<BacktestResult> results;
results.add(kj::mv(result1));
results.add(kj::mv(result2));
results.add(kj::mv(result3));

reporter.generate_comparison_report(results.asPtr(), "/path/to/comparison.html"_kj);
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
  "trades": [...],
  "equity_curve": [...],
  "drawdown_curve": [...]
}
```

---

## Built-in Strategies

VeloZ includes several built-in strategies for testing:

### Trend Following Strategy

```cpp
auto strategy = veloz::strategy::StrategyFactory::create("trend_following"_kj);

// Parameters
config.strategy_parameters.insert(kj::str("ema_fast_period"), 12.0);
config.strategy_parameters.insert(kj::str("ema_slow_period"), 26.0);
config.strategy_parameters.insert(kj::str("atr_period"), 14.0);
config.strategy_parameters.insert(kj::str("atr_multiplier"), 2.0);
```

### Mean Reversion Strategy

```cpp
auto strategy = veloz::strategy::StrategyFactory::create("mean_reversion"_kj);

// Parameters
config.strategy_parameters.insert(kj::str("bb_period"), 20.0);
config.strategy_parameters.insert(kj::str("bb_std_dev"), 2.0);
config.strategy_parameters.insert(kj::str("rsi_period"), 14.0);
config.strategy_parameters.insert(kj::str("rsi_oversold"), 30.0);
config.strategy_parameters.insert(kj::str("rsi_overbought"), 70.0);
```

### Momentum Strategy

```cpp
auto strategy = veloz::strategy::StrategyFactory::create("momentum"_kj);

// Parameters
config.strategy_parameters.insert(kj::str("rsi_period"), 14.0);
config.strategy_parameters.insert(kj::str("ema_period"), 20.0);
config.strategy_parameters.insert(kj::str("momentum_threshold"), 0.02);
```

### Market Making Strategy

```cpp
auto strategy = veloz::strategy::StrategyFactory::create("market_making"_kj);

// Parameters
config.strategy_parameters.insert(kj::str("spread_bps"), 10.0);
config.strategy_parameters.insert(kj::str("order_size"), 0.01);
config.strategy_parameters.insert(kj::str("max_position"), 1.0);
config.strategy_parameters.insert(kj::str("inventory_skew"), 0.5);
```

### Grid Strategy

```cpp
auto strategy = veloz::strategy::StrategyFactory::create("grid"_kj);

// Parameters
config.strategy_parameters.insert(kj::str("upper_price"), 45000.0);
config.strategy_parameters.insert(kj::str("lower_price"), 40000.0);
config.strategy_parameters.insert(kj::str("grid_count"), 10.0);
config.strategy_parameters.insert(kj::str("grid_mode"), 0.0);  // 0=arithmetic, 1=geometric
config.strategy_parameters.insert(kj::str("order_size"), 0.01);
```

---

## Best Practices

### Avoid Overfitting

1. **Use out-of-sample testing**: Split data into training (70%) and testing (30%) sets
2. **Walk-forward optimization**: Re-optimize periodically on rolling windows
3. **Keep strategies simple**: Fewer parameters = less overfitting risk
4. **Test on multiple time periods**: Ensure robustness across market conditions

### Realistic Simulation

1. **Include transaction costs**: Set realistic fee rates (0.1% for spot)
2. **Model slippage**: Account for execution price differences
3. **Consider market impact**: Large orders may move the market
4. **Use appropriate data frequency**: Match your actual trading frequency

### Performance Optimization

1. **Use streaming for large files**: Memory-efficient processing
2. **Limit parameter ranges**: Reduce search space
3. **Use Genetic Algorithm for large spaces**: More efficient than grid search
4. **Use Bayesian optimization for expensive evaluations**: Most sample-efficient

---

## Complete Example: Optimization Workflow

```cpp
#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/backtest/reporter.h"
#include "veloz/strategy/strategy.h"

using namespace veloz::backtest;
using namespace veloz::strategy;

int main() {
    // 1. Configure backtest
    BacktestConfig config;
    config.strategy_name = kj::str("trend_following");
    config.symbol = kj::str("BTCUSDT");
    config.start_time = 1704067200000;
    config.end_time = 1706745600000;
    config.initial_balance = 10000.0;
    config.risk_per_trade = 0.02;
    config.max_position_size = 1.0;
    config.data_source = kj::str("csv");
    config.data_type = kj::str("kline");
    config.time_frame = kj::str("1h");

    // 2. Load data
    auto data_source = DataSourceFactory::create_data_source("csv"_kj);
    auto csv_source = static_cast<CSVDataSource*>(data_source.get());
    csv_source->set_data_directory("/path/to/data"_kj);

    // 3. Optimize parameters
    auto optimizer = kj::heap<GeneticAlgorithmOptimizer>();
    optimizer->initialize(config);

    kj::TreeMap<kj::String, std::pair<double, double>> ranges;
    ranges.insert(kj::str("ema_fast_period"), {8.0, 20.0});
    ranges.insert(kj::str("ema_slow_period"), {20.0, 50.0});
    ranges.insert(kj::str("atr_multiplier"), {1.5, 3.0});

    optimizer->set_parameter_ranges(ranges);
    optimizer->set_optimization_target("sharpe"_kj);
    optimizer->set_population_size(30);
    optimizer->set_max_iterations(50);
    optimizer->set_data_source(data_source.addRef());

    auto strategy = StrategyFactory::create("trend_following"_kj);
    optimizer->optimize(strategy.addRef());

    // 4. Get best parameters
    auto& best_params = optimizer->get_best_parameters();
    KJ_LOG(INFO, "Best parameters found:");
    for (const auto& entry : best_params) {
        KJ_LOG(INFO, "  ", entry.key, " = ", entry.value);
        config.strategy_parameters.insert(kj::str(entry.key), entry.value);
    }

    // 5. Run final backtest with best parameters
    auto final_strategy = StrategyFactory::create("trend_following"_kj);
    auto engine = kj::heap<BacktestEngine>();
    engine->initialize(config);
    engine->set_data_source(data_source.addRef());
    engine->set_strategy(final_strategy.addRef());

    engine->on_progress_detailed([](const BacktestProgress& p) {
        KJ_LOG(INFO, "Progress: ", p.progress_fraction * 100, "% (",
               p.events_per_second, " events/sec)");
    });

    bool success = engine->run();
    auto result = engine->get_result();

    // 6. Generate reports
    BacktestReporter reporter;
    reporter.generate_report(result, "/path/to/final_report.html"_kj);
    reporter.generate_report_format(result, "/path/to/final_report.json"_kj, ReportFormat::JSON);

    KJ_LOG(INFO, "Optimization complete!");
    KJ_LOG(INFO, "Total Return: ", result.total_return * 100, "%");
    KJ_LOG(INFO, "Sharpe Ratio: ", result.sharpe_ratio);
    KJ_LOG(INFO, "Max Drawdown: ", result.max_drawdown * 100, "%");

    return 0;
}
```

---

## Troubleshooting

### "No data loaded" error

Ensure your CSV file:
- Has the correct header row
- Uses millisecond timestamps (not nanoseconds)
- Contains data within the specified time range

### Sharpe ratio is 0 or NaN

This can happen when:
- There are no trades (strategy never triggers)
- All trades have the same return (zero variance)
- The backtest period is too short

### Optimization takes too long

Try:
- Reducing parameter ranges
- Using Genetic Algorithm or Random Search instead of Grid Search
- Limiting max_iterations
- Using Bayesian optimization for expensive evaluations

### Memory issues with large datasets

- Use `stream_file()` instead of `load_file()` for large CSVs
- Process data in time chunks
- Reduce equity curve sampling frequency

---

## API Reference

For complete API documentation, see:
- [Backtest API Reference](../../api/backtest_api.md)
- Header files in `libs/backtest/include/veloz/backtest/`
