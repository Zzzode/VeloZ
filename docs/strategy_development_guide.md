# VeloZ 策略开发指南

## 1. 概述

VeloZ 是一个面向加密货币市场的工业级量化交易框架，提供统一的回测/模拟/实盘交易运行时模型和可演进的工程架构。本指南旨在帮助开发者快速上手 VeloZ 策略开发，包括策略架构、开发流程、调试和部署等方面。

## 2. 策略架构

### 2.1 系统架构

VeloZ 采用分层架构设计，策略层位于顶层，与核心引擎通过事件驱动方式通信。

```
┌─────────────────────────────────────────────────────────────┐
│                     策略开发层 (Python SDK)                 │
├─────────────────────────────────────────────────────────────┤
│                     策略运行时 (C++23)                      │
├─────────────────────────────────────────────────────────────┤
│ 市场数据系统 │ 交易执行系统 │ 风险管理系统 │ 分析回测系统   │
├─────────────────────────────────────────────────────────────┤
│                     核心引擎 (C++23)                        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 策略接口

所有策略都必须实现 `IStrategy` 接口，该接口定义了策略的生命周期管理、事件处理和信号生成方法。

```cpp
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // 策略信息获取
    virtual std::string get_id() const = 0;
    virtual std::string get_name() const = 0;
    virtual StrategyType get_type() const = 0;

    // 策略生命周期方法
    virtual bool initialize(const StrategyConfig& config, core::Logger& logger) = 0;
    virtual void on_start() = 0;
    virtual void on_stop() = 0;
    virtual void on_event(const market::MarketEvent& event) = 0;
    virtual void on_position_update(const oms::Position& position) = 0;
    virtual void on_timer(int64_t timestamp) = 0;

    // 策略状态和信号
    virtual StrategyState get_state() const = 0;
    virtual std::vector<exec::PlaceOrderRequest> get_signals() = 0;

    // 状态重置
    virtual void reset() = 0;
};
```

### 2.3 策略类型

框架支持多种策略类型，包括：

| 策略类型 | 描述 |
|---------|------|
| TrendFollowing | 趋势跟踪策略 |
| MeanReversion | 均值回归策略 |
| Momentum | 动量策略 |
| Arbitrage | 套利策略 |
| MarketMaking | 做市策略 |
| Grid | 网格策略 |
| Custom | 自定义策略 |

### 2.4 高级策略类型

框架还支持多种高级策略类型：

#### 技术指标策略
- **RsiStrategy**：RSI（相对强弱指数）策略
- **MacdStrategy**：MACD（移动平均收敛发散）策略
- **BollingerBandsStrategy**：布林带策略
- **StochasticOscillatorStrategy**：随机振荡器策略

#### 套利策略
- **CrossExchangeArbitrage**：跨交易所套利策略
- **CrossAssetArbitrage**：跨资产套利策略
- **TriangularArbitrage**：三角套利策略
- **StatArbitrage**：统计套利策略

#### 高频交易策略
- **MarketMakingHFT**：做市高频交易策略
- **MomentumHFT**：动量高频交易策略
- **LiquidityProvidingHFT**：流动性提供高频交易策略

#### 机器学习策略
- **ClassificationStrategy**：分类策略
- **RegressionStrategy**：回归策略
- **ReinforcementLearningStrategy**：强化学习策略

#### 投资组合管理
- **MeanVarianceOptimization**：均值方差优化
- **RiskParityPortfolio**：风险平价投资组合
- **HierarchicalRiskParity**：分层风险平价

### 2.5 策略配置

策略配置使用 `StrategyConfig` 结构，包含以下字段：

```cpp
struct StrategyConfig {
    std::string name;
    StrategyType type;
    double risk_per_trade;
    double max_position_size;
    double stop_loss;
    double take_profit;
    std::vector<std::string> symbols;
    std::map<std::string, double> parameters;
};
```

## 3. 策略开发流程

### 3.1 环境准备

1. 安装 VeloZ 框架
2. 配置开发环境（C++23 编译器、CMake、vcpkg）
3. 安装 Python 依赖（如果使用 Python SDK）

### 3.2 创建策略类

创建策略类，继承自 `BaseStrategy`（推荐）或直接实现 `IStrategy` 接口。

```cpp
class MyStrategy : public BaseStrategy {
public:
    explicit MyStrategy(const StrategyConfig& config) : BaseStrategy(config) {}

    StrategyType get_type() const override {
        return StrategyType::Custom;
    }

    void on_event(const market::MarketEvent& event) override {
        // 事件处理逻辑
    }

    void on_timer(int64_t timestamp) override {
        // 定时器逻辑
    }

    std::vector<exec::PlaceOrderRequest> get_signals() override {
        return signals_;
    }

    static std::string get_strategy_type() {
        return "MyStrategy";
    }

private:
    std::vector<exec::PlaceOrderRequest> signals_;
};
```

### 3.3 创建策略工厂

为策略创建工厂类，用于策略的实例化。

```cpp
class MyStrategyFactory : public StrategyFactory<MyStrategy> {
public:
    std::string get_strategy_type() const override {
        return MyStrategy::get_strategy_type();
    }
};
```

### 3.4 注册策略

在策略管理器中注册策略工厂。

```cpp
auto manager = std::make_shared<StrategyManager>();
auto factory = std::make_shared<MyStrategyFactory>();
manager->register_strategy_factory(factory);
```

### 3.5 配置策略

创建策略配置并创建策略实例。

```cpp
StrategyConfig config{
    .name = "My Strategy",
    .type = StrategyType::Custom,
    .risk_per_trade = 0.01,
    .max_position_size = 1.0,
    .stop_loss = 0.05,
    .take_profit = 0.1,
    .symbols = {"BTCUSDT"},
    .parameters = {{"param1", 10.0}, {"param2", 0.5}}
};

auto strategy = manager->create_strategy(config);
```

### 3.6 启动策略

启动策略并开始接收事件。

```cpp
manager->start_strategy(strategy->get_id());
```

## 4. 策略开发示例

### 4.1 RSI 策略示例

下面是一个 RSI（相对强弱指数）策略示例，使用 RSI 指标作为交易信号。

```cpp
class RsiStrategy : public TechnicalIndicatorStrategy {
public:
    explicit RsiStrategy(const StrategyConfig& config)
        : TechnicalIndicatorStrategy(config) {
        // 从配置中获取参数
        rsi_period_ = config.parameters.at("rsi_period");
        overbought_level_ = config.parameters.at("overbought_level");
        oversold_level_ = config.parameters.at("oversold_level");
    }

    StrategyType get_type() const override {
        return StrategyType::Custom;
    }

    void on_event(const market::MarketEvent& event) override {
        if (event.type == market::MarketEventType::kTicker) {
            recent_prices_.push_back(event.price);
            if (recent_prices_.size() > rsi_period_ + 1) {
                recent_prices_.erase(recent_prices_.begin());
            }

            if (recent_prices_.size() == rsi_period_ + 1) {
                double rsi = calculate_rsi(recent_prices_, rsi_period_);

                if (rsi > overbought_level_) {
                    signals_.push_back(exec::PlaceOrderRequest{
                        .symbol = event.symbol,
                        .side = exec::OrderSide::kSell,
                        .quantity = 0.1,
                        .price = event.price,
                        .type = exec::OrderType::kMarket
                    });
                } else if (rsi < oversold_level_) {
                    signals_.push_back(exec::PlaceOrderRequest{
                        .symbol = event.symbol,
                        .side = exec::OrderSide::kBuy,
                        .quantity = 0.1,
                        .price = event.price,
                        .type = exec::OrderType::kMarket
                    });
                }
            }
        }
    }

    void on_timer(int64_t timestamp) override {}

    std::vector<exec::PlaceOrderRequest> get_signals() override {
        std::vector<exec::PlaceOrderRequest> result = signals_;
        signals_.clear();
        return result;
    }

    static std::string get_strategy_type() {
        return "RsiStrategy";
    }

private:
    std::vector<double> recent_prices_;
    std::vector<exec::PlaceOrderRequest> signals_;
    int rsi_period_;
    double overbought_level_;
    double oversold_level_;
};
```

### 4.2 MACD 策略示例

下面是一个 MACD（移动平均收敛发散）策略示例：

```cpp
class MacdStrategy : public TechnicalIndicatorStrategy {
public:
    explicit MacdStrategy(const StrategyConfig& config)
        : TechnicalIndicatorStrategy(config) {
        fast_period_ = config.parameters.at("fast_period");
        slow_period_ = config.parameters.at("slow_period");
        signal_period_ = config.parameters.at("signal_period");
    }

    StrategyType get_type() const override {
        return StrategyType::Custom;
    }

    void on_event(const market::MarketEvent& event) override {
        if (event.type == market::MarketEventType::kTicker) {
            recent_prices_.push_back(event.price);
            if (recent_prices_.size() > slow_period_ + signal_period_) {
                recent_prices_.erase(recent_prices_.begin());
            }

            if (recent_prices_.size() == slow_period_ + signal_period_) {
                double signal;
                double macd = calculate_macd(recent_prices_, signal, fast_period_, slow_period_, signal_period_);

                if (macd > signal && last_macd_ <= last_signal_) {
                    signals_.push_back(exec::PlaceOrderRequest{
                        .symbol = event.symbol,
                        .side = exec::OrderSide::kBuy,
                        .quantity = 0.1,
                        .price = event.price,
                        .type = exec::OrderType::kMarket
                    });
                } else if (macd < signal && last_macd_ >= last_signal_) {
                    signals_.push_back(exec::PlaceOrderRequest{
                        .symbol = event.symbol,
                        .side = exec::OrderSide::kSell,
                        .quantity = 0.1,
                        .price = event.price,
                        .type = exec::OrderType::kMarket
                    });
                }

                last_macd_ = macd;
                last_signal_ = signal;
            }
        }
    }

    void on_timer(int64_t timestamp) override {}

    std::vector<exec::PlaceOrderRequest> get_signals() override {
        std::vector<exec::PlaceOrderRequest> result = signals_;
        signals_.clear();
        return result;
    }

    static std::string get_strategy_type() {
        return "MacdStrategy";
    }

private:
    std::vector<double> recent_prices_;
    std::vector<exec::PlaceOrderRequest> signals_;
    int fast_period_;
    int slow_period_;
    int signal_period_;
    double last_macd_ = 0.0;
    double last_signal_ = 0.0;
};
```

## 5. 策略回测

### 5.1 回测框架

VeloZ 提供了完整的回测引擎，允许用户使用历史数据测试策略。回测框架支持以下功能：

- 历史数据回放
- 策略执行模拟
- 回测结果分析
- 绩效指标计算
- 参数优化
- 报告生成

### 5.2 回测流程

1. 创建策略实例
2. 创建数据来源
3. 配置回测参数
4. 初始化回测引擎
5. 运行回测
6. 分析回测结果
7. 生成报告

### 5.3 回测示例

```cpp
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
        // 策略逻辑实现
    }

    void on_position_update(const veloz::oms::Position& position) override {
        // 持仓更新处理
    }

    void on_timer(int64_t timestamp) override {
        // 定时器处理
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
    // 创建策略
    auto strategy = std::make_shared<SimpleMovingAverageStrategy>();

    // 创建数据来源 (CSV 来源)
    auto data_source = veloz::backtest::DataSourceFactory::create_data_source("csv");

    // 创建回测引擎
    auto backtest_engine = std::make_shared<veloz::backtest::BacktestEngine>();

    // 配置回测
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

    // 初始化引擎
    if (!backtest_engine->initialize(config)) {
        std::cerr << "Failed to initialize backtest engine" << std::endl;
        return 1;
    }

    // 设置策略和数据源
    backtest_engine->set_strategy(strategy);
    backtest_engine->set_data_source(data_source);

    // 设置进度回调
    backtest_engine->on_progress([](double progress) {
        std::cout << "Progress: " << static_cast<int>(progress * 100) << "%" << std::endl;
    });

    // 运行回测
    if (!backtest_engine->run()) {
        std::cerr << "Backtest failed" << std::endl;
        return 1;
    }

    // 获取结果
    auto result = backtest_engine->get_result();

    // 分析结果
    auto analyzer = std::make_unique<veloz::backtest::BacktestAnalyzer>();
    auto analyzed_result = analyzer->analyze(result.trades);

    // 生成报告
    auto reporter = std::make_unique<veloz::backtest::BacktestReporter>();
    reporter->generate_report(result, "backtest_report.html");

    // 打印结果
    std::cout << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "       回测结果" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "策略名称: " << result.strategy_name << std::endl;
    std::cout << "交易对: " << result.symbol << std::endl;
    std::cout << "初始资金: $" << result.initial_balance << std::endl;
    std::cout << "最终资金: $" << result.final_balance << std::endl;
    std::cout << "总收益率: " << result.total_return * 100 << "%" << std::endl;
    std::cout << "最大回撤: " << result.max_drawdown * 100 << "%" << std::endl;
    std::cout << "夏普比率: " << result.sharpe_ratio << std::endl;
    std::cout << "胜率: " << result.win_rate * 100 << "%" << std::endl;
    std::cout << "盈亏比: " << result.profit_factor << std::endl;
    std::cout << "总交易次数: " << result.trade_count << std::endl;
    std::cout << "盈利交易: " << result.win_count << std::endl;
    std::cout << "亏损交易: " << result.lose_count << std::endl;
    std::cout << "平均盈利: $" << result.avg_win << std::endl;
    std::cout << "平均亏损: $" << result.avg_lose << std::endl;

    // 参数优化
    std::cout << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "       参数优化" << std::endl;
    std::cout << "==============================" << std::endl;

    auto optimizer = std::make_unique<veloz::backtest::GridSearchOptimizer>();
    optimizer->initialize(config);

    // 定义参数范围
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

        std::cout << "优化结果数量: " << optimization_results.size() << std::endl;
        std::cout << "最佳参数组合:" << std::endl;

        for (const auto& [name, value] : best_parameters) {
            std::cout << "  " << name << ": " << value << std::endl;
        }
    } else {
        std::cout << "参数优化失败" << std::endl;
    }

    return 0;
}
```

## 6. 策略调试和监控

### 6.1 日志系统

VeloZ 提供了强大的日志系统，支持不同级别的日志输出。

```cpp
// 获取日志实例
auto logger = core::Logger::get_instance();

// 输出不同级别的日志
logger->debug("Debug message");
logger->info("Info message");
logger->warn("Warning message");
logger->error("Error message");
logger->fatal("Fatal message");
```

### 6.2 策略状态监控

可以通过以下方式监控策略状态：

```cpp
// 获取策略状态
auto state = strategy->get_state();

// 输出策略状态
std::cout << "策略名称: " << state.strategy_name << std::endl;
std::cout << "运行状态: " << (state.is_running ? "运行中" : "已停止") << std::endl;
std::cout << "累计盈亏: " << state.pnl << std::endl;
std::cout << "最大回撤: " << state.max_drawdown << std::endl;
std::cout << "交易次数: " << state.trade_count << std::endl;
std::cout << "胜率: " << state.win_rate << std::endl;
std::cout << "盈亏比: " << state.profit_factor << std::endl;
```

### 6.3 事件调试

可以通过以下方式调试事件处理：

```cpp
void MyStrategy::on_event(const market::MarketEvent& event) {
    // 输出事件类型和内容
    logger_.debug("Received event type: " + market::event_type_to_string(event.type));
    logger_.debug("Symbol: " + event.symbol);
    logger_.debug("Price: " + std::to_string(event.price));
    logger_.debug("Volume: " + std::to_string(event.volume));

    // 事件处理逻辑
}
```

## 7. 策略部署

### 7.1 本地部署

1. 构建项目
2. 配置策略参数
3. 启动策略管理器
4. 加载和启动策略

### 7.2 容器部署

1. 构建 Docker 镜像
2. 配置容器参数
3. 启动容器
4. 加载和启动策略

### 7.3 云部署

1. 配置云服务器
2. 安装依赖
3. 部署应用
4. 启动策略

## 8. 最佳实践

### 8.1 策略开发原则

1. **清晰的策略逻辑**：策略应该具有明确的逻辑，便于理解和维护。
2. **适当的风险控制**：策略应该包含适当的风险控制机制，如止损和止盈。
3. **充分的回测**：策略应该经过充分的回测，以验证其有效性和稳定性。
4. **实时监控**：策略应该提供实时监控功能，以便及时发现和处理问题。
5. **版本控制**：策略应该进行版本控制，以便跟踪和管理策略的演变。

### 8.2 性能优化

1. **减少计算复杂度**：策略应该尽量减少计算复杂度，以提高执行效率。
2. **避免阻塞操作**：策略应该避免阻塞操作，以确保事件处理的及时性。
3. **内存管理**：策略应该合理管理内存，以避免内存泄漏和溢出。
4. **并发处理**：策略应该适当使用并发处理，以提高执行效率。

### 8.3 安全和合规

1. **API 密钥管理**：策略应该妥善管理 API 密钥，避免泄露。
2. **数据加密**：策略应该对敏感数据进行加密，以保护用户隐私。
3. **合规性**：策略应该符合相关法规和政策，避免违规操作。

## 9. 常见问题

### 9.1 策略未产生信号

- 检查策略逻辑是否正确
- 检查策略参数是否合理
- 检查市场数据是否正常

### 9.2 策略性能不佳

- 检查策略逻辑是否存在性能瓶颈
- 检查策略参数是否合理
- 检查硬件和网络资源是否充足

### 9.3 策略回测结果不理想

- 检查回测参数是否合理
- 检查历史数据是否完整和准确
- 检查策略逻辑是否符合市场情况

## 10. 总结

VeloZ 是一个强大的量化交易框架，提供了完整的策略开发、回测和部署功能。本指南详细介绍了 VeloZ 策略开发的各个方面，包括策略架构、开发流程、调试和部署等。

通过遵循本指南，开发者可以快速上手 VeloZ 策略开发，并创建高性能、稳定的量化交易策略。同时，VeloZ 框架的可扩展性和可维护性使得策略的迭代和升级变得更加容易。
