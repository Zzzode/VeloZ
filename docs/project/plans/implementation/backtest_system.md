# VeloZ Quantitative Trading Framework - Backtest System Development Plan

## 1. Overview

The backtest system is an important component of the quantitative trading framework, allowing strategy developers to test their trading strategies on historical market data, evaluate strategy performance, and optimize strategy parameters. The backtest system needs to possess accuracy, efficiency, and usability.

## 2. Backtest System Architecture Design

### 2.0 Implementation Progress (Updated: 2026-02-11)

**Sprint 1 Completed** - Core MVP Functionality:

| Component | Status | Notes |
|-----------|--------|--------|
| CSV Data Reading | ✅ Implemented | Parses `timestamp,symbol,side,price,quantity` format with validation, time filtering, and error handling |
| CSV Synthetic Data Generation | ✅ Implemented | Deterministic generation using geometric Brownian motion with trend |
| Binance API Data Reading | ✅ Implemented | Kline endpoint with pagination, rate limiting; trade endpoint for recent data |
| Binance API Data Download | ✅ Implemented | Kline to CSV conversion with synthetic trade generation per candle |
| Order Execution Simulation | ✅ Implemented | Slippage (0.1%) and fee (0.1%) models, OMS integration, P&L tracking, equity updates |
| Backtest Engine | ✅ 95% | Core logic complete, fully functional with simulated execution |
| Backtest Analyzer | ✅ 100% | All metrics working (sharpe, drawdown, win rate, profit factor, profit/loss) |
| Backtest Reporter | ⚠️ 80% | HTML/JSON generation working, trade record iteration partially complete |
| Parameter Optimizer | ⚠️ 70% | Interface complete, grid search framework in place |
| Test Coverage | ✅ 100% | 6 test files with comprehensive coverage |

**Completed High Priority Items**:
- ✅ CSV data download functionality (synthetic data generation)
- ✅ Binance API data reading (kline and trade endpoints)
- ✅ Binance API data download (kline to CSV)
- ✅ Rate limiting and error handling for Binance API
- ✅ Data source factory pattern for extensibility

**Remaining Medium Priority Items**:
- Grid search optimization logic implementation
- Trade record iteration in HTML/JSON reports

---

### 2.1 System Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                          Backtest System Architecture                            │
├───────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                     User Interface Layer                            │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Strategy Editor        Backtest Configurator        Result Visualizer  │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                          ▲ ▲ ▲                                    │
│                          │ │ │                                    │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                     Backtest Control Layer                        │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Backtest Engine          Strategy Executor        Data Manager      │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                          ▲ ▲ ▲                                    │
│                          │ │ │                                    │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                     Core Function Layer                            │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Market Data Playback      Order Simulation          Trading Cost Calculation  │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Performance Calculation  Risk Metrics  Statistical Analysis          │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                          ▲ ▲ ▲ ▲                                    │
│                          │ │ │ │                                    │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                     Data Storage Layer                            │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Historical Market Data      Backtest Results Storage      Strategy Config Storage  │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                          ▲ ▲ ▲ ▲                                    │
│                          │ │ │ │                                    │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                     External Data Sources Layer                     │  │
│  ├─────────────────────────────────────────────────────────────┤  │
│  │  Exchange API         CSV Files          Database             │  │
│  └─────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
```

### 2.2 Core Components Design

#### 2.2.1 Backtest Engine

```cpp
// Backtest engine interface
class BacktestEngine {
public:
  struct BacktestResult {
    double total_return;
    double annualized_return;
    double max_drawdown;
    double sharpe_ratio;
    double sortino_ratio;
    std::vector<Trade> trades;
    std::vector<Position> positions;
    std::map<uint64_t, double> portfolio_value;
    std::map<uint64_t, double> drawdown;
  };

  virtual BacktestResult run(const Strategy& strategy,
                           const std::vector<MarketEvent>& market_data,
                           const BacktestConfig& config) = 0;
  virtual ~BacktestEngine() = default;
};

// Backtest engine implementation
class BacktestEngineImpl : public BacktestEngine {
public:
  BacktestResult run(const Strategy& strategy,
                   const std::vector<MarketEvent>& market_data,
                   const BacktestConfig& config) override;

private:
  void initialize_backtest(const BacktestConfig& config);
  void process_market_event(const MarketEvent& event, const Strategy& strategy);
  void execute_order(const PlaceOrderRequest& order);
  void update_portfolio(uint64_t timestamp);
  BacktestResult calculate_results();

  std::shared_ptr<OrderBook> order_book_;
  std::shared_ptr<OrderExecutor> order_executor_;
  std::shared_ptr<Portfolio> portfolio_;
  std::shared_ptr<PositionManager> position_manager_;
};
```

#### 2.2.2 Data Playback Device

```cpp
// Data playback device interface
class DataReplayer {
public:
  using DataCallback = std::function<void(const MarketEvent&)>;

  virtual bool load_data(const std::string& source) = 0;
  virtual bool start_replay(uint64_t start_time = 0, uint64_t end_time = UINT64_MAX) = 0;
  virtual bool pause_replay() = 0;
  virtual bool resume_replay() = 0;
  virtual bool stop_replay() = 0;
  virtual bool is_replaying() const = 0;
  virtual void set_callback(DataCallback callback) = 0;
  virtual ~DataReplayer() = default;
};

// CSV data playback device
class CsvDataReplayer : public DataReplayer {
public:
  bool load_data(const std::string& filename) override;
  bool start_replay(uint64_t start_time = 0, uint64_t end_time = UINT64_MAX) override;
  bool pause_replay() override;
  bool resume_replay() override;
  bool stop_replay() override;
  bool is_replaying() const override;
  void set_callback(DataCallback callback) override;

private:
  std::vector<MarketEvent> events_;
  size_t current_index_;
  DataCallback callback_;
  bool is_replaying_;
  bool is_paused_;
  std::thread replay_thread_;
  std::condition_variable cv_;
  std::mutex mutex_;
};
```

#### 2.2.3 Order Simulator

```cpp
// Order simulator interface
class OrderSimulator {
public:
  virtual std::optional<ExecutionReport> place_order(const PlaceOrderRequest& order) = 0;
  virtual std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& order) = 0;
  virtual std::optional<OrderStatus> get_order_status(const std::string& order_id) = 0;
  virtual void on_market_data(const MarketEvent& event) = 0;
  virtual ~OrderSimulator() = default;
};

// Order simulator implementation
class OrderSimulatorImpl : public OrderSimulator {
public:
  OrderSimulatorImpl(std::shared_ptr<OrderBook> order_book);
  std::optional<ExecutionReport> place_order(const PlaceOrderRequest& order) override;
  std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& order) override;
  std::optional<OrderStatus> get_order_status(const std::string& order_id) override;
  void on_market_data(const MarketEvent& event) override;

private:
  std::shared_ptr<OrderBook> order_book_;
  std::unordered_map<std::string, Order> orders_;
  std::mutex mutex_;
};
```

#### 2.2.4 Trading Cost Calculator

```cpp
// Trading cost calculator interface
class TransactionCostCalculator {
public:
  virtual double calculate_cost(const Order& order, double price) = 0;
  virtual ~TransactionCostCalculator() = default;
};

// Fixed rate cost calculator
class FixedRateCostCalculator : public TransactionCostCalculator {
public:
  FixedRateCostCalculator(double fee_rate);
  double calculate_cost(const Order& order, double price) override;

private:
  double fee_rate_;
};

// Tiered rate cost calculator
class TieredRateCostCalculator : public TransactionCostCalculator {
public:
  TieredRateCostCalculator(const std::vector<std::pair<double, double>>& tiers);
  double calculate_cost(const Order& order, double price) override;

private:
  std::vector<std::pair<double, double>> tiers_;
};
```

## 3. Backtest Configuration and Parameterization

### 3.1 Backtest Configuration

```cpp
// Backtest configuration
struct BacktestConfig {
  uint64_t start_time;
  uint64_t end_time;
  double initial_capital;
  double transaction_cost;
  double slippage;
  std::vector<Symbol> symbols;
  std::string data_source;
  std::string output_path;
  bool save_results;
};

// Backtest configuration management
class BacktestConfigManager {
public:
  static BacktestConfigManager& instance();

  bool load_config(const std::string& filename);
  bool save_config(const std::string& filename);
  BacktestConfig get_config() const;
  void set_config(const BacktestConfig& config);

private:
  BacktestConfig config_;
};
```

### 3.2 Strategy Parameter Optimization

```cpp
// Parameter optimization objective function
using OptimizationObjective = std::function<double(const std::vector<double>&, const BacktestConfig&)>;

// Parameter optimization constraint
struct ParameterConstraint {
  double min_value;
  double max_value;
  double step;
};

// Parameter optimization result
struct OptimizationResult {
  std::vector<double> best_parameters;
  double best_score;
  std::vector<std::vector<double>> all_parameters;
  std::vector<double> all_scores;
};

// Parameter optimizer interface
class ParameterOptimizer {
public:
  virtual OptimizationResult optimize(const OptimizationObjective& objective,
                                    const std::vector<ParameterConstraint>& constraints,
                                    const BacktestConfig& config) = 0;
  virtual ~ParameterOptimizer() = default;
};

// Grid search optimizer
class GridSearchOptimizer : public ParameterOptimizer {
public:
  OptimizationResult optimize(const OptimizationObjective& objective,
                            const std::vector<ParameterConstraint>& constraints,
                            const BacktestConfig& config) override;
};

// Genetic algorithm optimizer
class GeneticAlgorithmOptimizer : public ParameterOptimizer {
public:
  OptimizationResult optimize(const OptimizationObjective& objective,
                            const std::vector<ParameterConstraint>& constraints,
                            const BacktestConfig& config) override;
};
```

## 4. Backtest Results Analysis

### 4.1 Performance Metrics Calculation

```cpp
// Backtest performance metrics
struct PerformanceMetrics {
  double total_return;
  double annualized_return;
  double max_drawdown;
  double sharpe_ratio;
  double sortino_ratio;
  double win_rate;
  double profit_factor;
  double average_trade;
  double expectancy;
  int number_of_trades;
};

// Performance metrics calculator
class PerformanceMetricsCalculator {
public:
  PerformanceMetrics calculate(const BacktestResult& result);
};
```

### 4.2 Risk Metrics Calculation

```cpp
// Risk metrics
struct RiskMetrics {
  double value_at_risk;
  double conditional_value_at_risk;
  double var_95;
  double var_99;
  double cvar_95;
  double cvar_99;
};

// Risk metrics calculator
class RiskMetricsCalculator {
public:
  RiskMetrics calculate(const BacktestResult& result);
};
```

### 4.3 Visualization Interface

```cpp
// Visualization interface
class BacktestVisualizer {
public:
  virtual bool plot_portfolio_value(const BacktestResult& result, const std::string& filename) = 0;
  virtual bool plot_drawdown(const BacktestResult& result, const std::string& filename) = 0;
  virtual bool plot_trades(const BacktestResult& result, const std::string& filename) = 0;
  virtual bool plot_performance_metrics(const PerformanceMetrics& metrics, const std::string& filename) = 0;
  virtual bool plot_risk_metrics(const RiskMetrics& metrics, const std::string& filename) = 0;
  virtual ~BacktestVisualizer() = default;
};

// Matplotlib visualizer implementation
class MatplotlibVisualizer : public BacktestVisualizer {
public:
  bool plot_portfolio_value(const BacktestResult& result, const std::string& filename) override;
  bool plot_drawdown(const BacktestResult& result, const std::string& filename) override;
  bool plot_trades(const BacktestResult& result, const std::string& filename) override;
  bool plot_performance_metrics(const PerformanceMetrics& metrics, const std::string& filename) override;
  bool plot_risk_metrics(const RiskMetrics& metrics, const std::string& filename) override;
};
```

## 5. Backtest System Implementation Plan

### 5.1 Phase Division

1. **Phase 1 (2 weeks)**: Backtest engine core functionality development
2. **Phase 2 (2 weeks)**: Data playback and order simulation
3. **Phase 3 (1 week)**: Trading cost calculation
4. **Phase 4 (1 week)**: Performance metrics calculation
5. **Phase 5 (1 week)**: Risk metrics calculation
6. **Phase 6 (2 weeks)**: Parameter optimization and visualization
7. **Phase 7 (1 week)**: Testing and optimization

### 5.2 Resource Allocation

- **Backtest Engine**: 1 developer
- **Data Playback and Order Simulation**: 1 developer
- **Cost and Metrics Calculation**: 1 developer
- **Parameter Optimization and Visualization**: 1 developer
- **Testing and Optimization**: 1 developer

### 5.3 Milestones

| Milestone | Estimated Time | Deliverable |
|----------|--------------|------------|
| Backtest Engine Complete | Week 2 | Basic backtest functionality |
| Data Playback and Order Simulation Complete | Week 4 | CSV data playback and order simulation |
| Trading Cost Calculation Complete | Week 5 | Fixed rate and tiered rate cost calculation |
| Performance Metrics Calculation Complete | Week 6 | Basic performance and risk metrics |
| Parameter Optimization and Visualization Complete | Week 8 | Parameter optimization and basic visualization |
| Backtest System Complete | Week 9 | Complete backtest system |

## 6. Testing and Verification

### 6.1 Unit Testing

- Backtest engine unit tests
- Data playback device unit tests
- Order simulator unit tests
- Cost calculator unit tests

### 6.2 Integration Testing

- Backtest flow integration tests
- Strategy execution integration tests
- Results calculation integration tests

### 6.3 Accuracy Testing

- Backtest result verification
- Trading cost calculation verification
- Performance metrics calculation verification

### 6.4 Performance Testing

- Backtest speed testing
- Memory usage testing
- Large dataset testing

## 7. Technology Selection

### 7.1 C++ Libraries

- **Data Processing**: pugixml, nlohmann/json
- **Math Calculation**: Eigen, Boost.Math
- **Testing Framework**: KJ Test (from Cap'n Proto)

### 7.2 Python Libraries

- **Data Analysis**: pandas, numpy
- **Visualization**: matplotlib, plotly
- **Optimization Algorithms**: scipy.optimize

### 7.3 Data Storage

- **CSV Files**: Direct read and write
- **Database**: PostgreSQL, SQLite
- **File Format**: HDF5, Parquet

## 8. Risk Assessment

### 8.1 Technical Risks

1. **Backtest Accuracy**: Difference between backtest results and real market performance
2. **Performance Bottlenecks**: Performance issues during backtesting large datasets
3. **Data Quality**: Completeness and accuracy of historical data

### 8.2 Strategy Risks

1. **Overfitting**: Strategy performs well in backtesting but poorly in live trading
2. **Data Leakage**: Use of future data in backtesting
3. **Parameter Optimization Bias**: Bias issues during optimization process

### 8.3 Operational Risks

1. **Configuration Errors**: Errors in backtest parameter configuration
2. **Data Selection Errors**: Selection of inappropriate historical data
3. **Results Interpretation Errors**: Incorrect interpretation of backtest results

## 9. Optimization Strategies

### 9.1 Performance Optimization

1. **Parallel Computing**: Use multi-threading or GPU acceleration
2. **Memory Optimization**: Reduce memory usage
3. **Algorithm Optimization**: Optimize computation-intensive operations

### 9.2 Accuracy Optimization

1. **Order Simulation Optimization**: More precise order execution simulation
2. **Cost Calculation Optimization**: More accurate trading cost calculation
3. **Market Impact Simulation**: Simulate order impact on market prices

### 9.3 Usability Optimization

1. **User Interface Optimization**: Simplify strategy development and backtesting process
2. **Documentation Enhancement**: Provide detailed usage documentation
3. **Example Strategies**: Provide various types of example strategies

## 10. Summary

The backtest system is one of the core components of the quantitative trading framework, providing a safe and efficient testing environment for strategy developers. Through detailed architecture design and implementation planning, we will develop a functional, high-performance backtest system that supports strategy development, parameter optimization, and performance evaluation.

This plan will be completed in 9 weeks, with each phase focusing on the development of a core functionality, ensuring system quality and accuracy through rigorous testing processes.
