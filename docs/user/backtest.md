# Backtest User Guide

This guide explains how to use the VeloZ backtesting system to test and optimize your trading strategies.

## Overview

The VeloZ backtest system allows you to:
- Test trading strategies against historical market data
- Analyze performance metrics (Sharpe ratio, drawdown, win rate, etc.)
- Optimize strategy parameters using Grid Search or Genetic Algorithm
- Generate HTML and JSON reports with equity curves and trade history

## Quick Start

### 1. Prepare Market Data

VeloZ supports multiple data sources:

**CSV Files**
```csv
timestamp,symbol,side,price,quantity
1704067200000000000,BTCUSDT,BUY,42000.50,0.001
1704067201000000000,BTCUSDT,SELL,42001.00,0.002
```

**Binance API**
```cpp
// Download historical kline data
auto source = DataSourceFactory::create("binance_kline");
source->configure({
    {"symbol", "BTCUSDT"},
    {"interval", "1m"},
    {"start_time", "1704067200000"},
    {"end_time", "1704153600000"}
});
auto data = source->fetch();
```

**Synthetic Data Generation**
```cpp
// Generate synthetic data for testing
auto source = DataSourceFactory::create("synthetic");
source->configure({
    {"symbol", "BTCUSDT"},
    {"initial_price", "42000.0"},
    {"volatility", "0.02"},
    {"trend", "0.0001"},
    {"num_events", "10000"}
});
auto data = source->fetch();
```

### 2. Create a Strategy

Implement the `Strategy` interface:

```cpp
#include <veloz/strategy/strategy.h>

class MyStrategy : public Strategy {
public:
    MyStrategy() : Strategy("my_strategy", "My Custom Strategy") {}

    std::vector<Signal> on_market_event(const MarketEvent& event) override {
        std::vector<Signal> signals;

        // Your strategy logic here
        if (should_buy(event)) {
            signals.push_back(Signal{
                .symbol = event.symbol,
                .side = Side::BUY,
                .quantity = 0.001,
                .price = event.price
            });
        }

        return signals;
    }

private:
    bool should_buy(const MarketEvent& event) {
        // Implement your buy logic
        return false;
    }
};
```

### 3. Configure the Backtest

```cpp
#include <veloz/backtest/backtest_engine.h>

BacktestConfig config;
config.initial_capital = 10000.0;      // Starting balance in quote currency
config.transaction_cost = 0.001;       // 0.1% transaction fee
config.slippage = 0.001;               // 0.1% slippage
config.start_time = 1704067200000000000;  // Start timestamp (ns)
config.end_time = 1704153600000000000;    // End timestamp (ns)
```

### 4. Run the Backtest

```cpp
BacktestEngine engine;
auto strategy = std::make_shared<MyStrategy>();
auto data_source = DataSourceFactory::create("csv");
data_source->load("market_data.csv");

auto result = engine.run(strategy, data_source, config);
```

### 5. Analyze Results

```cpp
#include <veloz/backtest/analyzer.h>
#include <veloz/backtest/reporter.h>

// Calculate performance metrics
BacktestAnalyzer analyzer;
auto metrics = analyzer.analyze(result);

std::cout << "Total Return: " << metrics.total_return * 100 << "%" << std::endl;
std::cout << "Sharpe Ratio: " << metrics.sharpe_ratio << std::endl;
std::cout << "Max Drawdown: " << metrics.max_drawdown * 100 << "%" << std::endl;
std::cout << "Win Rate: " << metrics.win_rate * 100 << "%" << std::endl;

// Generate reports
BacktestReporter reporter;
reporter.generate_html(result, metrics, "backtest_report.html");
reporter.generate_json(result, metrics, "backtest_report.json");
```

## Data Sources

### CSV Data Source

Load historical trade data from CSV files:

```cpp
auto source = DataSourceFactory::create("csv");
source->configure({
    {"file_path", "data/btcusdt_trades.csv"},
    {"symbol", "BTCUSDT"},
    {"start_time", "1704067200000000000"},  // Optional: filter by time
    {"end_time", "1704153600000000000"}     // Optional: filter by time
});
auto data = source->fetch();
```

**CSV Format Requirements:**
- Header row required
- Columns: `timestamp`, `symbol`, `side`, `price`, `quantity`
- Timestamp in nanoseconds (Unix epoch)
- Side: `BUY` or `SELL`

### Binance Kline Data Source

Fetch historical candlestick data from Binance:

```cpp
auto source = DataSourceFactory::create("binance_kline");
source->configure({
    {"symbol", "BTCUSDT"},
    {"interval", "1m"},           // 1m, 5m, 15m, 1h, 4h, 1d
    {"start_time", "1704067200000"},  // Milliseconds
    {"end_time", "1704153600000"},
    {"limit", "1000"}             // Max 1000 per request
});
auto data = source->fetch();
```

**Supported Intervals:** `1m`, `3m`, `5m`, `15m`, `30m`, `1h`, `2h`, `4h`, `6h`, `8h`, `12h`, `1d`, `3d`, `1w`, `1M`

### Synthetic Data Source

Generate synthetic market data for testing:

```cpp
auto source = DataSourceFactory::create("synthetic");
source->configure({
    {"symbol", "BTCUSDT"},
    {"initial_price", "42000.0"},
    {"volatility", "0.02"},       // Daily volatility
    {"trend", "0.0001"},          // Daily trend (positive = upward)
    {"num_events", "10000"},
    {"seed", "12345"}             // Optional: for reproducibility
});
auto data = source->fetch();
```

## Parameter Optimization

### Grid Search Optimizer

Exhaustively search through parameter combinations:

```cpp
#include <veloz/backtest/optimizer.h>

GridSearchOptimizer optimizer;

// Define parameter ranges
std::vector<ParameterRange> ranges = {
    {"fast_period", 5.0, 20.0, 5.0},    // min, max, step
    {"slow_period", 20.0, 50.0, 10.0},
    {"threshold", 0.01, 0.05, 0.01}
};

// Configure optimizer
OptimizerConfig opt_config;
opt_config.target = "sharpe";           // Optimize for Sharpe ratio
opt_config.max_iterations = 1000;       // Limit iterations

// Run optimization
auto result = optimizer.optimize(
    strategy_factory,   // Function that creates strategy with params
    data_source,
    backtest_config,
    ranges,
    opt_config
);

std::cout << "Best parameters:" << std::endl;
for (const auto& [name, value] : result.best_parameters) {
    std::cout << "  " << name << ": " << value << std::endl;
}
std::cout << "Best Sharpe: " << result.best_score << std::endl;
```

### Genetic Algorithm Optimizer

Use evolutionary optimization for large parameter spaces:

```cpp
GeneticAlgorithmOptimizer optimizer;

// Configure GA parameters
GAConfig ga_config;
ga_config.population_size = 50;
ga_config.generations = 100;
ga_config.mutation_rate = 0.1;
ga_config.crossover_rate = 0.8;
ga_config.elite_count = 5;
ga_config.tournament_size = 3;

// Run optimization
auto result = optimizer.optimize(
    strategy_factory,
    data_source,
    backtest_config,
    ranges,
    ga_config
);
```

**Optimization Targets:**
- `sharpe` - Sharpe ratio (risk-adjusted return)
- `return` - Total return
- `win_rate` - Percentage of winning trades
- `profit_factor` - Gross profit / gross loss

## Performance Metrics

The analyzer calculates these metrics:

| Metric | Description |
|--------|-------------|
| `total_return` | Total percentage return |
| `annualized_return` | Annualized return rate |
| `max_drawdown` | Maximum peak-to-trough decline |
| `sharpe_ratio` | Risk-adjusted return (excess return / volatility) |
| `sortino_ratio` | Downside risk-adjusted return |
| `win_rate` | Percentage of profitable trades |
| `profit_factor` | Gross profit / gross loss |
| `avg_win` | Average profit on winning trades |
| `avg_loss` | Average loss on losing trades |
| `trade_count` | Total number of trades |
| `win_count` | Number of winning trades |
| `loss_count` | Number of losing trades |

## Reports

### HTML Report

The HTML report includes:
- Summary metrics table
- Interactive equity curve chart (Chart.js)
- Drawdown chart
- Trade markers (buy/sell indicators)
- Complete trade history table

```cpp
reporter.generate_html(result, metrics, "report.html");
```

### JSON Report

The JSON report provides machine-readable output:

```json
{
  "summary": {
    "initial_balance": 10000.0,
    "final_balance": 11500.0,
    "total_return": 0.15,
    "max_drawdown": 0.05,
    "sharpe_ratio": 1.8,
    "win_rate": 0.55
  },
  "equity_curve": [
    {"timestamp": 1704067200000000000, "value": 10000.0},
    {"timestamp": 1704067260000000000, "value": 10050.0}
  ],
  "trades": [
    {
      "timestamp": 1704067200000000000,
      "symbol": "BTCUSDT",
      "side": "BUY",
      "quantity": 0.001,
      "price": 42000.0,
      "pnl": 50.0
    }
  ]
}
```

## Built-in Strategies

VeloZ includes several built-in strategies for testing:

### Moving Average Crossover

```cpp
auto strategy = StrategyFactory::create("ma_crossover");
strategy->set_parameter("fast_period", 10);
strategy->set_parameter("slow_period", 20);
```

### RSI Strategy

```cpp
auto strategy = StrategyFactory::create("rsi");
strategy->set_parameter("period", 14);
strategy->set_parameter("overbought", 70);
strategy->set_parameter("oversold", 30);
```

### Bollinger Bands Strategy

```cpp
auto strategy = StrategyFactory::create("bollinger");
strategy->set_parameter("period", 20);
strategy->set_parameter("std_dev", 2.0);
```

## Best Practices

### Avoid Overfitting

1. **Use out-of-sample testing**: Split data into training and testing sets
2. **Walk-forward optimization**: Re-optimize periodically on rolling windows
3. **Keep strategies simple**: Fewer parameters = less overfitting risk
4. **Test on multiple time periods**: Ensure robustness across market conditions

### Realistic Simulation

1. **Include transaction costs**: Set realistic fee rates (0.1% for spot)
2. **Model slippage**: Account for execution price differences
3. **Consider market impact**: Large orders may move the market
4. **Use appropriate data frequency**: Match your actual trading frequency

### Performance Optimization

1. **Use synthetic data for development**: Faster iteration
2. **Limit parameter ranges**: Reduce search space
3. **Use Genetic Algorithm for large spaces**: More efficient than grid search
4. **Cache intermediate results**: Avoid redundant calculations

## Example: Complete Backtest Workflow

```cpp
#include <veloz/backtest/backtest_engine.h>
#include <veloz/backtest/analyzer.h>
#include <veloz/backtest/reporter.h>
#include <veloz/backtest/optimizer.h>
#include <veloz/strategy/advanced_strategies.h>

int main() {
    // 1. Load data
    auto data_source = DataSourceFactory::create("csv");
    data_source->load("btcusdt_2024.csv");

    // 2. Configure backtest
    BacktestConfig config;
    config.initial_capital = 10000.0;
    config.transaction_cost = 0.001;
    config.slippage = 0.001;

    // 3. Optimize parameters
    GridSearchOptimizer optimizer;
    std::vector<ParameterRange> ranges = {
        {"fast_period", 5.0, 15.0, 2.0},
        {"slow_period", 20.0, 40.0, 5.0}
    };

    auto opt_result = optimizer.optimize(
        [](const std::map<std::string, double>& params) {
            auto strategy = std::make_shared<MACrossoverStrategy>();
            strategy->set_parameter("fast_period", params.at("fast_period"));
            strategy->set_parameter("slow_period", params.at("slow_period"));
            return strategy;
        },
        data_source,
        config,
        ranges,
        {.target = "sharpe", .max_iterations = 100}
    );

    // 4. Run final backtest with best parameters
    auto best_strategy = std::make_shared<MACrossoverStrategy>();
    best_strategy->set_parameter("fast_period", opt_result.best_parameters["fast_period"]);
    best_strategy->set_parameter("slow_period", opt_result.best_parameters["slow_period"]);

    BacktestEngine engine;
    auto result = engine.run(best_strategy, data_source, config);

    // 5. Analyze and report
    BacktestAnalyzer analyzer;
    auto metrics = analyzer.analyze(result);

    BacktestReporter reporter;
    reporter.generate_html(result, metrics, "final_report.html");
    reporter.generate_json(result, metrics, "final_report.json");

    std::cout << "Optimization complete!" << std::endl;
    std::cout << "Best Sharpe: " << opt_result.best_score << std::endl;
    std::cout << "Report saved to final_report.html" << std::endl;

    return 0;
}
```

## Troubleshooting

### "No data loaded" error

Ensure your CSV file:
- Has the correct header row
- Uses the correct timestamp format (nanoseconds)
- Contains data within the specified time range

### Sharpe ratio is 0 or NaN

This can happen when:
- There are no trades (strategy never triggers)
- All trades have the same return (zero variance)
- The backtest period is too short

### Optimization takes too long

Try:
- Reducing parameter ranges
- Increasing step sizes
- Using Genetic Algorithm instead of Grid Search
- Limiting max_iterations

### Memory issues with large datasets

- Process data in chunks
- Use streaming data sources
- Reduce the number of stored equity curve points

## API Reference

See the header files for complete API documentation:
- `libs/backtest/include/veloz/backtest/backtest_engine.h`
- `libs/backtest/include/veloz/backtest/analyzer.h`
- `libs/backtest/include/veloz/backtest/reporter.h`
- `libs/backtest/include/veloz/backtest/optimizer.h`
- `libs/backtest/include/veloz/backtest/data_source.h`
