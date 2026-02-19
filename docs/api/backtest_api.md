# Backtest API Reference

This document describes the backtest system configuration and API for VeloZ.

## Overview

The VeloZ backtest system allows you to test trading strategies against historical market data. It supports multiple data sources, optimization algorithms, and comprehensive reporting.

## Backtest Configuration

### BacktestConfig Structure

| Field | Type | Description |
|-------|------|-------------|
| `strategy_name` | string | Name of the strategy to backtest |
| `symbol` | string | Trading symbol (e.g., "BTCUSDT") |
| `start_time` | int64 | Start timestamp in nanoseconds |
| `end_time` | int64 | End timestamp in nanoseconds |
| `initial_balance` | double | Starting balance for the backtest |
| `risk_per_trade` | double | Risk percentage per trade (0.0-1.0) |
| `max_position_size` | double | Maximum position size allowed |
| `strategy_parameters` | map | Strategy-specific parameters |
| `data_source` | string | Data source type (see Data Sources) |
| `data_type` | string | Data type: `"kline"`, `"trade"`, `"book"` |
| `time_frame` | string | Time frame: `"1m"`, `"5m"`, `"15m"`, `"1h"`, `"4h"`, `"1d"` |

### Example Configuration

```cpp
BacktestConfig config;
config.strategy_name = kj::str("ma_crossover");
config.symbol = kj::str("BTCUSDT");
config.start_time = 1704067200000000000;  // 2024-01-01 00:00:00 UTC
config.end_time = 1706745600000000000;    // 2024-02-01 00:00:00 UTC
config.initial_balance = 10000.0;
config.risk_per_trade = 0.02;  // 2% risk per trade
config.max_position_size = 1.0;
config.data_source = kj::str("csv");
config.data_type = kj::str("kline");
config.time_frame = kj::str("1h");
config.strategy_parameters["fast_period"] = 10.0;
config.strategy_parameters["slow_period"] = 20.0;
```

---

## Data Sources

### CSV Data Source

Load historical data from CSV files.

**Configuration**:
- `data_source`: `"csv"`
- File format: timestamp, open, high, low, close, volume

**Example CSV**:
```csv
1704067200000000000,42000.0,42500.0,41800.0,42300.0,100.5
1704070800000000000,42300.0,42800.0,42100.0,42600.0,150.2
```

### Synthetic Data Source

Generate synthetic market data for testing.

**Configuration**:
- `data_source`: `"synthetic"`
- Useful for strategy development and unit testing

### Binance API Data Source

Fetch historical data from Binance API.

**Configuration**:
- `data_source`: `"binance"`
- Requires network access to Binance API
- Supports all Binance trading pairs

---

## Optimization

### Grid Search Optimizer

Exhaustively searches all parameter combinations within specified ranges.

**Configuration**:
| Method | Description |
|--------|-------------|
| `set_parameter_ranges(ranges)` | Set min/max for each parameter |
| `set_optimization_target(target)` | Target metric: `"sharpe"`, `"return"`, `"win_rate"` |
| `set_max_iterations(n)` | Maximum number of backtests to run |
| `set_data_source(source)` | Set the data source for backtests |

**Example**:
```cpp
GridSearchOptimizer optimizer;
optimizer.initialize(config);

kj::TreeMap<kj::String, std::pair<double, double>> ranges;
ranges.insert(kj::str("fast_period"), {5.0, 20.0});
ranges.insert(kj::str("slow_period"), {15.0, 50.0});

optimizer.set_parameter_ranges(ranges);
optimizer.set_optimization_target("sharpe"_kj);
optimizer.set_max_iterations(100);
optimizer.set_data_source(data_source);

optimizer.optimize(strategy);

auto& best_params = optimizer.get_best_parameters();
```

### Genetic Algorithm Optimizer

Uses evolutionary algorithms to find optimal parameters efficiently.

**Configuration**:
| Method | Description |
|--------|-------------|
| `set_population_size(n)` | Number of individuals per generation |
| `set_mutation_rate(rate)` | Probability of mutation (0.0-1.0) |
| `set_crossover_rate(rate)` | Probability of crossover (0.0-1.0) |
| `set_elite_count(n)` | Number of elite individuals to preserve |
| `set_tournament_size(n)` | Tournament selection size |
| `set_convergence_params(threshold, generations)` | Early stopping criteria |

**Example**:
```cpp
GeneticAlgorithmOptimizer optimizer;
optimizer.initialize(config);
optimizer.set_population_size(50);
optimizer.set_mutation_rate(0.1);
optimizer.set_crossover_rate(0.8);
optimizer.set_elite_count(5);
optimizer.set_tournament_size(3);
optimizer.set_convergence_params(0.001, 10);

optimizer.optimize(strategy);
```

---

## Backtest Results

### BacktestResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `strategy_name` | string | Strategy name |
| `symbol` | string | Trading symbol |
| `start_time` | int64 | Backtest start time |
| `end_time` | int64 | Backtest end time |
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
| `timestamp` | int64 | Trade execution time |
| `symbol` | string | Trading symbol |
| `side` | string | Trade side: `"buy"` or `"sell"` |
| `price` | double | Execution price |
| `quantity` | double | Trade quantity |
| `fee` | double | Trading fee |
| `pnl` | double | Profit/loss for this trade |
| `strategy_id` | string | Strategy identifier |

---

## Report Generation

### HTML Report

Generate a comprehensive HTML report with styling.

```cpp
Reporter reporter;
reporter.generate_html_report(result, "/path/to/report.html");
```

**Report Contents**:
- Summary metrics table
- Detailed metrics table
- Trade history table
- Equity curve chart (placeholder)
- Drawdown chart (placeholder)

### JSON Report

Generate a machine-readable JSON report.

```cpp
Reporter reporter;
reporter.generate_json_report(result, "/path/to/report.json");
```

**JSON Structure**:
```json
{
  "strategy_name": "ma_crossover",
  "symbol": "BTCUSDT",
  "start_time": 1704067200000000000,
  "end_time": 1706745600000000000,
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
      "timestamp": 1704067200000000000,
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
    {"timestamp": 1704067200000000000, "equity": 10000.0, "cumulative_return": 0.0}
  ],
  "drawdown_curve": [
    {"timestamp": 1704067200000000000, "drawdown": 0.0}
  ]
}
```

---

## Built-in Strategies

### MA Crossover Strategy

Moving average crossover strategy.

**Parameters**:
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fast_period` | double | 10.0 | Fast MA period |
| `slow_period` | double | 20.0 | Slow MA period |

### RSI Strategy

Relative Strength Index strategy.

**Parameters**:
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `period` | double | 14.0 | RSI calculation period |
| `overbought` | double | 70.0 | Overbought threshold |
| `oversold` | double | 30.0 | Oversold threshold |

### Bollinger Bands Strategy

Bollinger Bands mean reversion strategy.

**Parameters**:
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `period` | double | 20.0 | SMA period |
| `std_dev` | double | 2.0 | Standard deviation multiplier |

---

## Usage Example

```cpp
#include "veloz/backtest/backtest_engine.h"
#include "veloz/backtest/data_source.h"
#include "veloz/backtest/optimizer.h"
#include "veloz/backtest/reporter.h"
#include "veloz/strategy/strategy.h"

using namespace veloz::backtest;
using namespace veloz::strategy;

int main() {
    // Create configuration
    BacktestConfig config;
    config.strategy_name = kj::str("ma_crossover");
    config.symbol = kj::str("BTCUSDT");
    config.start_time = 1704067200000000000;
    config.end_time = 1706745600000000000;
    config.initial_balance = 10000.0;
    config.risk_per_trade = 0.02;
    config.max_position_size = 1.0;
    config.data_source = kj::str("csv");
    config.data_type = kj::str("kline");
    config.time_frame = kj::str("1h");

    // Create data source
    auto data_source = DataSourceFactory::create("csv"_kj, "/path/to/data.csv"_kj);

    // Create strategy
    auto strategy = StrategyFactory::create("ma_crossover"_kj);

    // Run backtest
    BacktestEngine engine;
    engine.initialize(config);
    engine.set_data_source(kj::mv(data_source));
    engine.set_strategy(kj::mv(strategy));
    engine.run();

    // Get results
    auto result = engine.get_result();

    // Generate reports
    Reporter reporter;
    reporter.generate_html_report(result, "/path/to/report.html");
    reporter.generate_json_report(result, "/path/to/report.json");

    return 0;
}
```

---

## Related

- [HTTP API Reference](http_api.md) - REST API endpoints
- [SSE API](sse_api.md) - Server-Sent Events stream
- [Engine Protocol](engine_protocol.md) - Engine stdio protocol
