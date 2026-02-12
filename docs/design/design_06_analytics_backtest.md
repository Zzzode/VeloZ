# Backtesting System Design

## Overview

The backtesting system is one of the core components of the VeloZ quantitative trading framework. It allows developers to simulate the strategy execution process using historical market data, evaluate strategy performance, and optimize strategy parameters.

## Architecture Design

### System Architecture Diagram

```
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│ Historical Data  │     │ Backtest Engine    │     │ Backtest Result    │
│ Storage          │────▶│ Core               │────▶│ Analysis &          │
└─────────────────┘     └──────────────────┘     │  Visualization)      │
        ▲                   ▲                       └──────────────────┘
        │                   │                               ▲
        │                   │                               │
        │                   │                               │
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│ Data Source       │     │ Strategy           │     │ Parameter           │
│ Interface         │     │ Executor           │     │ Optimizer           │
│                  │     │                    │     │                    │
└─────────────────┘     └──────────────────┘     └──────────────────┘

                          ▲
                          │
                          │
                  ┌──────────────────┐
                  │ Backtest Report     │
                  │ Generation          │
                  │                    │
                  └──────────────────┘
```

### Core Components

#### 1. Historical Data Storage (Data Storage)

- **Function**: Store and manage historical market data
- **Data Formats**:
  - K-line data (Kline)
  - Tick-by-tick data (Trade)
  - Order book data (Order Book)
- **Storage Methods**:
  - Local file storage (CSV, Parquet, etc.)
  - Database storage (SQLite, PostgreSQL, etc.)
- **Interfaces**:
  - Data read interface
  - Data write interface
  - Data query interface

#### 2. Data Source Interface (Data Sources)

- **Function**: Retrieve historical data from various data sources
- **Supported Data Sources**:
  - Binance API (RESTful)
  - Local files
  - Other exchange APIs
- **Interfaces**:
  - Data download interface
  - Data parsing interface
  - Data conversion interface

#### 3. Backtest Engine Core (Backtest Engine)

- **Function**: Core engine for executing strategy backtesting
- **Core Modules**:
  - Time simulator
  - Market data replay engine
  - Order simulator
  - Account simulator
  - Event scheduler
- **Backtest Modes**:
  - Tick-by-tick backtesting
  - K-line backtesting
  - Order book backtesting

#### 4. Strategy Executor (Strategy Executor)

- **Function**: Execute strategy logic
- **Interfaces**:
  - Strategy initialization
  - Strategy execution
  - Strategy termination
- **Execution Environment**:
  - Simulated trading environment
  - Real trading environment

#### 5. Backtest Result Analysis (Analysis & Visualization)

- **Function**: Analyze backtesting results
- **Analysis Metrics**:
  - Return rate
  - Maximum drawdown
  - Sharpe ratio
  - Win rate
  - Profit/loss ratio
- **Visualization**:
  - Return curve
  - Drawdown curve
  - Trading signal distribution
  - Position distribution

#### 6. Parameter Optimizer (Parameter Optimization)

- **Function**: Optimize strategy parameters
- **Optimization Algorithms**:
  - Grid search
  - Genetic algorithm
  - Bayesian optimization
- **Interfaces**:
  - Parameter definition
  - Optimization target setting
  - Optimization result analysis

#### 7. Backtest Report Generation (Report Generation)

- **Function**: Generate backtesting reports
- **Report Formats**:
  - HTML report
  - PDF report
  - Charts
- **Report Content**:
  - Strategy information
  - Backtesting parameters
  - Backtesting results
  - Analysis charts
  - Optimization recommendations

## Data Models

### Backtest Configuration

```cpp
struct BacktestConfig {
    std::string strategy_name;
    std::string symbol;
    std::int64_t start_time;
    std::int64_t end_time;
    double initial_balance;
    double risk_per_trade;
    double max_position_size;
    std::map<std::string, double> strategy_parameters;
    std::string data_source;
    std::string data_type; // kline, trade, book
    std::string time_frame; // 1m, 5m, 1h, etc.
};
```

### Backtest Result

```cpp
struct BacktestResult {
    std::string strategy_name;
    std::string symbol;
    std::int64_t start_time;
    std::int64_t end_time;
    double initial_balance;
    double final_balance;
    double total_return;
    double max_drawdown;
    double sharpe_ratio;
    double win_rate;
    double profit_factor;
    int trade_count;
    int win_count;
    int lose_count;
    double avg_win;
    double avg_lose;
    std::vector<TradeRecord> trades;
    std::vector<EquityCurvePoint> equity_curve;
    std::vector<DrawdownPoint> drawdown_curve;
};
```

### Trade Record

```cpp
struct TradeRecord {
    std::int64_t timestamp;
    std::string symbol;
    std::string side; // buy or sell
    double price;
    double quantity;
    double fee;
    double pnl;
    std::string strategy_id;
};
```

### Equity Curve Point

```cpp
struct EquityCurvePoint {
    std::int64_t timestamp;
    double equity;
    double cumulative_return;
};
```

### Drawdown Curve Point

```cpp
struct DrawdownPoint {
    std::int64_t timestamp;
    double drawdown;
};
```

## Interface Design

### Backtest Engine Interface

```cpp
class IBacktestEngine {
public:
    virtual ~IBacktestEngine() = default;

    virtual bool initialize(const BacktestConfig& config) = 0;
    virtual bool run() = 0;
    virtual bool stop() = 0;
    virtual bool reset() = 0;

    virtual BacktestResult get_result() const = 0;
    virtual void set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
    virtual void set_data_source(const std::shared_ptr<IDataSource>& data_source) = 0;

    virtual void on_progress(std::function<void(double progress)> callback) = 0;
};
```

### Data Source Interface

```cpp
class IDataSource {
public:
    virtual ~IDataSource() = default;

    virtual bool connect() = 0;
    virtual bool disconnect() = 0;

    virtual std::vector<veloz::market::MarketEvent> get_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame
    ) = 0;

    virtual bool download_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame,
        const std::string& output_path
    ) = 0;
};
```

### Backtest Result Analysis Interface

```cpp
class IBacktestAnalyzer {
public:
    virtual ~IBacktestAnalyzer() = default;

    virtual std::shared_ptr<BacktestResult> analyze(const std::vector<TradeRecord>& trades) = 0;
    virtual std::vector<EquityCurvePoint> calculate_equity_curve(const std::vector<TradeRecord>& trades, double initial_balance) = 0;
    virtual std::vector<DrawdownPoint> calculate_drawdown(const std::vector<EquityCurvePoint>& equity_curve) = 0;
    virtual double calculate_sharpe_ratio(const std::vector<TradeRecord>& trades) = 0;
    virtual double calculate_max_drawdown(const std::vector<EquityCurvePoint>& equity_curve) = 0;
    virtual double calculate_win_rate(const std::vector<TradeRecord>& trades) = 0;
    virtual double calculate_profit_factor(const std::vector<TradeRecord>& trades) = 0;
};
```

### Parameter Optimization Interface

```cpp
class IParameterOptimizer {
public:
    virtual ~IParameterOptimizer() = default;

    virtual bool initialize(const BacktestConfig& config) = 0;
    virtual bool optimize(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
    virtual std::vector<BacktestResult> get_results() const = 0;
    virtual std::map<std::string, double> get_best_parameters() const = 0;

    virtual void set_parameter_ranges(const std::map<std::string, std::pair<double, double>>& ranges) = 0;
    virtual void set_optimization_target(const std::string& target) = 0; // "sharpe", "return", "win_rate"
    virtual void set_max_iterations(int iterations) = 0;
};
```

### Backtest Report Generation Interface

```cpp
class IBacktestReporter {
public:
    virtual ~IBacktestReporter() = default;

    virtual bool generate_report(const BacktestResult& result, const std::string& output_path) = 0;
    virtual std::string generate_html_report(const BacktestResult& result) = 0;
    virtual std::string generate_json_report(const BacktestResult& result) = 0;
};
```

## Implementation Plan

### Phase 1: Basic Infrastructure

1. Create basic architecture and core components for backtesting system
2. Implement data storage and read functionality
3. Implement basic functionality for backtesting engine
4. Implement basic functionality for backtesting result analysis

### Phase 2: Strategy Integration

1. Implement strategy executor
2. Implement strategy parameter optimization functionality
3. Implement backtesting report generation functionality

### Phase 3: Optimization and Improvement

1. Optimize backtesting engine performance
2. Add more analysis metrics
3. Improve report generation functionality
4. Add more data source support

## Technology Selection

- **Data Storage**: Parquet (high-performance columnar storage) + SQLite (local database)
- **Data Processing**: Apache Arrow (in-memory data processing)
- **Visualization**: Plotly (interactive charts) + Matplotlib (static charts)
- **Optimization Algorithms**: Scipy (scientific computing) + Optuna (hyperparameter optimization)
- **Report Generation**: Jinja2 (template engine) + WeasyPrint (HTML to PDF conversion)

## Code Organization

```
libs/backtest/
├── include/veloz/backtest/
│   ├── backtest_engine.h
│   ├── data_source.h
│   ├── data_storage.h
│   ├── analyzer.h
│   ├── optimizer.h
│   ├── reporter.h
│   ├── config.h
│   └── types.h
├── src/
│   ├── backtest_engine.cpp
│   ├── data_source.cpp
│   ├── data_storage.cpp
│   ├── analyzer.cpp
│   ├── optimizer.cpp
│   └── reporter.cpp
└── tests/
    ├── test_backtest_engine.cpp
    ├── test_data_source.cpp
    ├── test_analyzer.cpp
    ├── test_optimizer.cpp
    └── test_reporter.cpp
```

## Usage Example

```cpp
// Create backtesting configuration
BacktestConfig config;
config.strategy_name = "TrendFollowing";
config.symbol = "BTCUSDT";
config.start_time = 1609459200000; // 2021-01-01
config.end_time = 1640995200000; // 2021-12-31
config.initial_balance = 10000.0;
config.risk_per_trade = 0.02;
config.max_position_size = 0.1;
config.strategy_parameters = {{"lookback_period", 20}, {"stop_loss", 0.02}, {"take_profit", 0.05}};
config.data_source = "binance";
config.data_type = "kline";
config.time_frame = "1h";

// Create strategy
auto strategy = std::make_shared<TrendFollowingStrategy>(config.strategy_parameters);

// Create data source
auto data_source = std::make_shared<BinanceDataSource>();

// Create backtesting engine
auto backtest_engine = std::make_shared<BacktestEngine>();
backtest_engine->initialize(config);
backtest_engine->set_strategy(strategy);
backtest_engine->set_data_source(data_source);

// Set progress callback
backtest_engine->on_progress([](double progress) {
    std::cout << "Backtesting progress: " << progress * 100 << "%" << std::endl;
});

// Run backtest
backtest_engine->run();

// Get backtesting results
auto result = backtest_engine->get_result();

// Analyze backtesting results
auto analyzer = std::make_shared<BacktestAnalyzer>();
auto analyzed_result = analyzer->analyze(result.trades);

// Generate backtesting report
auto reporter = std::make_shared<BacktestReporter>();
reporter->generate_report(analyzed_result, "backtest_report.html");

// Print results
std::cout << "Final balance: " << analyzed_result.final_balance << std::endl;
std::cout << "Total return: " << analyzed_result.total_return * 100 << "%" << std::endl;
std::cout << "Maximum drawdown: " << analyzed_result.max_drawdown * 100 << "%" << std::endl;
std::cout << "Sharpe ratio: " << analyzed_result.sharpe_ratio << std::endl;
std::cout << "Win rate: " << analyzed_result.win_rate * 100 << "%" << std::endl;
std::cout << "Profit/loss ratio: " << analyzed_result.profit_factor << std::endl;
```

## Implementation Status Update (Feb 2026)

### Recently Implemented Features

#### Grid Search Optimization ✅
**Implementation**: Complete implementation in `libs/backtest/src/optimizer.cpp`
- Full parameter space iteration with configurable step sizes (default 10 steps)
- Automatic parameter combination generation using recursion
- Support for multiple parameter ranges
- Limit optimization via `max_iterations` parameter
- Multiple optimization targets: sharpe, return, win_rate
- Result ranking and best parameter selection
- Comprehensive statistics collection

**Key Design Decisions**:
1. **Recursive combination generation**: Simple to implement, efficient for moderate parameter counts
2. **Default 10-step approach**: Balance between coverage and performance
3. **Optimization target selection**: Allows flexible strategy evaluation
4. **Result collection**: Store all optimization results for post-analysis

**Performance Characteristics**:
- Time complexity: O(n^p) where n = parameters, p = steps per parameter
- Memory usage: Stores all results in memory
- Thread-safe: Uses mutex for result collection

**Usage Example**:
```cpp
veloz::backtest::GridSearchOptimizer optimizer;
optimizer.initialize(config);

// Set parameter ranges
std::map<std::string, std::pair<double, double>> ranges;
ranges["lookback_period"] = {10.0, 50.0};
ranges["stop_loss"] = {0.01, 0.05};
ranges["take_profit"] = {0.02, 0.10};
optimizer.set_parameter_ranges(ranges);

// Set optimization target
optimizer.set_optimization_target("sharpe"); // or "return", "win_rate"

// Limit iterations
optimizer.set_max_iterations(100);

// Run optimization
optimizer.optimize(strategy);
auto best_params = optimizer.get_best_parameters();
auto all_results = optimizer.get_results();
```

#### Trade Records in Reports ✅
**Implementation**: Enhanced HTML and JSON report generation in `libs/backtest/src/reporter.cpp`

**HTML Report Enhancements**:
- Trade history table section added
- Columns: Time, Symbol, Side, Price, Quantity, Fee, P&L
- P&L color coding (positive/negative classes)
- Detailed trade-by-trade breakdown

**JSON Report Enhancements**:
- Trades array with full trade details
- Each trade includes: timestamp, symbol, side, price, quantity, fee, pnl, strategy_id
- Proper JSON array formatting

**Design Decisions**:
1. **Comprehensive trade tracking**: Include all trade data for audit trails
2. **Visual P&L indication**: Color coding for quick analysis
3. **Structured data**: Consistent format across HTML and JSON

**Report Structure**:
```
HTML Report:
┌─────────────────────────┐
│ Backtest Summary        │
├─────────────────────────┤
│ Performance Metrics      │
├─────────────────────────┤
│ Trade History           │
│ Time │ Symbol │ Side │ Price │ Qty │ Fee  │ P&L   │
├─────────────────────────┤
│ [timestamp]...[data] │
└─────────────────────────┘

JSON Report:
{
  "strategy": "...",
  "symbol": "...",
  "metrics": {
    "total_return": ...,
    "sharpe_ratio": ...
  },
  "trades": [
    {
      "timestamp": ...,
      "symbol": "...",
      "side": "buy",
      "price": 50000.0,
      "quantity": 0.5,
      "fee": 0.001,
      "pnl": 100.0,
      "strategy_id": "test_strategy"
    },
    ...
  ]
}
```

### Implementation Progress

| Component | Status | Completion |
|-----------|--------|------------|
| Grid Search Optimizer | ✅ Complete | 100% |
| Trade Records (HTML) | ✅ Complete | 100% |
| Trade Records (JSON) | ✅ Complete | 100% |
| Backtest Report Generation | ✅ Functional | 90% |
| Parameter Optimization | ✅ Complete | 100% |

### Testing Status

- **Optimizer Tests**: Enhanced with 8+ new test cases covering:
  - Single parameter optimization
  - Multi-parameter optimization
  - Max iterations limiting
  - Different optimization targets
  - Result clearing verification

- **Reporter Tests**: Enhanced with 8+ new test cases covering:
  - Trade history table presence
  - Trade data field completeness
  - P&L color coding
  - Empty trade history handling
  - Large trade history (1000+ trades)

All tests are ready for execution once build system is resolved.

### Future Enhancements

#### High Priority
1. **Multi-objective optimization**: Combine Sharpe + return + win_rate into weighted score
2. **Parallel optimization**: Utilize multi-core processors for faster grid search
3. **Incremental optimization**: Start with previous best parameters
4. **Advanced algorithms**: Implement Bayesian optimization, genetic algorithms

#### Medium Priority
1. **Advanced trade analysis**: Trade clustering, pattern detection
2. **Interactive reports**: HTML reports with collapsible sections, sorting
3. **Real-time charts**: Dynamic chart rendering for strategy development

### Code Quality

- **Type safety**: Extensive use of templates and `std::optional`
- **Error handling**: Comprehensive exception handling and logging
- **Documentation**: Inline documentation in source files
- **Test coverage**: Growing test suite for critical paths

