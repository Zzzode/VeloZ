# 回测系统设计

## 概述

回测系统是 VeloZ 量化交易框架的核心组件之一，它允许开发者在历史市场数据上模拟策略的执行过程，评估策略的性能，并优化策略参数。

## 架构设计

### 系统架构图

```
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│ 历史数据存储    │     │ 回测引擎核心      │     │ 回测结果分析     │
│ (Data Storage)  │────▶│ (Backtest Engine)│────▶│ (Analysis &      │
└─────────────────┘     └──────────────────┘     │  Visualization)  │
        ▲                   ▲                       └──────────────────┘
        │                   │                               ▲
        │                   │                               │
        │                   │                               │
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│ 数据源接口      │     │ 策略执行器        │     │ 参数优化器        │
│ (Data Sources)  │     │ (Strategy        │     │ (Parameter       │
│                 │     │  Executor)       │     │  Optimization)   │
└─────────────────┘     └──────────────────┘     └──────────────────┘

                          ▲
                          │
                          │
                  ┌──────────────────┐
                  │ 回测报告生成      │
                  │ (Report          │
                  │  Generation)     │
                  └──────────────────┘
```

### 核心组件

#### 1. 历史数据存储 (Data Storage)

- **功能**：存储和管理历史市场数据
- **数据格式**：
  - K线数据 (Kline)
  - 逐笔交易数据 (Trade)
  - 订单簿数据 (Order Book)
- **存储方式**：
  - 本地文件存储 (CSV、Parquet等)
  - 数据库存储 (SQLite、PostgreSQL等)
- **接口**：
  - 数据读取接口
  - 数据写入接口
  - 数据查询接口

#### 2. 数据源接口 (Data Sources)

- **功能**：从各种数据源获取历史数据
- **支持的数据源**：
  - Binance API (RESTful)
  - 本地文件
  - 其他交易所API
- **接口**：
  - 数据下载接口
  - 数据解析接口
  - 数据转换接口

#### 3. 回测引擎核心 (Backtest Engine)

- **功能**：执行策略回测的核心引擎
- **核心模块**：
  - 时间模拟器
  - 市场数据回放器
  - 订单模拟器
  - 账户模拟器
  - 事件调度器
- **回测模式**：
  - 逐笔回测
  - K线回测
  - 订单簿回测

#### 4. 策略执行器 (Strategy Executor)

- **功能**：执行策略逻辑
- **接口**：
  - 策略初始化
  - 策略执行
  - 策略结束
- **执行环境**：
  - 模拟交易环境
  - 真实交易环境

#### 5. 回测结果分析 (Analysis & Visualization)

- **功能**：分析回测结果
- **分析指标**：
  - 收益率
  - 最大回撤
  - 夏普比率
  - 胜率
  - 盈亏比
- **可视化**：
  - 收益率曲线
  - 回撤曲线
  - 交易信号分布
  - 持仓分布

#### 6. 参数优化器 (Parameter Optimization)

- **功能**：优化策略参数
- **优化算法**：
  - 网格搜索
  - 遗传算法
  - 贝叶斯优化
- **接口**：
  - 参数定义
  - 优化目标设置
  - 优化结果分析

#### 7. 回测报告生成 (Report Generation)

- **功能**：生成回测报告
- **报告格式**：
  - HTML报告
  - PDF报告
  - 图表
- **报告内容**：
  - 策略信息
  - 回测参数
  - 回测结果
  - 分析图表
  - 优化建议

## 数据模型

### 回测配置

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

### 回测结果

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

### 交易记录

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

### 权益曲线点

```cpp
struct EquityCurvePoint {
    std::int64_t timestamp;
    double equity;
    double cumulative_return;
};
```

### 回撤曲线点

```cpp
struct DrawdownPoint {
    std::int64_t timestamp;
    double drawdown;
};
```

## 接口设计

### 回测引擎接口

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

### 数据源接口

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

### 回测结果分析接口

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

### 参数优化接口

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

### 回测报告生成接口

```cpp
class IBacktestReporter {
public:
    virtual ~IBacktestReporter() = default;

    virtual bool generate_report(const BacktestResult& result, const std::string& output_path) = 0;
    virtual std::string generate_html_report(const BacktestResult& result) = 0;
    virtual std::string generate_json_report(const BacktestResult& result) = 0;
};
```

## 实现计划

### 第一阶段：基础架构

1. 创建回测系统的基础架构和核心组件
2. 实现数据存储和读取功能
3. 实现回测引擎的基础功能
4. 实现回测结果分析的基础功能

### 第二阶段：策略集成

1. 实现策略执行器
2. 实现策略参数优化功能
3. 实现回测报告生成功能

### 第三阶段：优化与完善

1. 优化回测引擎的性能
2. 增加更多的分析指标
3. 完善报告生成功能
4. 增加更多的数据源支持

## 技术选型

- **数据存储**：Parquet (高性能列式存储) + SQLite (本地数据库)
- **数据处理**：Apache Arrow (内存中数据处理)
- **可视化**：Plotly (交互式图表) + Matplotlib (静态图表)
- **优化算法**：Scipy (科学计算) + Optuna (超参数优化)
- **报告生成**：Jinja2 (模板引擎) + WeasyPrint (HTML到PDF转换)

## 代码组织

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

## 使用示例

```cpp
// 创建回测配置
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

// 创建策略
auto strategy = std::make_shared<TrendFollowingStrategy>(config.strategy_parameters);

// 创建数据源
auto data_source = std::make_shared<BinanceDataSource>();

// 创建回测引擎
auto backtest_engine = std::make_shared<BacktestEngine>();
backtest_engine->initialize(config);
backtest_engine->set_strategy(strategy);
backtest_engine->set_data_source(data_source);

// 设置进度回调
backtest_engine->on_progress([](double progress) {
    std::cout << "回测进度: " << progress * 100 << "%" << std::endl;
});

// 运行回测
backtest_engine->run();

// 获取回测结果
auto result = backtest_engine->get_result();

// 分析回测结果
auto analyzer = std::make_shared<BacktestAnalyzer>();
auto analyzed_result = analyzer->analyze(result.trades);

// 生成回测报告
auto reporter = std::make_shared<BacktestReporter>();
reporter->generate_report(analyzed_result, "backtest_report.html");

// 打印结果
std::cout << "最终余额: " << analyzed_result.final_balance << std::endl;
std::cout << "总收益率: " << analyzed_result.total_return * 100 << "%" << std::endl;
std::cout << "最大回撤: " << analyzed_result.max_drawdown * 100 << "%" << std::endl;
std::cout << "夏普比率: " << analyzed_result.sharpe_ratio << std::endl;
std::cout << "胜率: " << analyzed_result.win_rate * 100 << "%" << std::endl;
std::cout << "盈亏比: " << analyzed_result.profit_factor << std::endl;
```
