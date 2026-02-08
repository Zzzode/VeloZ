# VeloZ 量化交易框架 - 执行系统优化方案

## 1. 概述

执行系统是量化交易框架的核心组件之一，负责处理订单路由、执行和成本控制。当前实现已经支持基本的订单路由和执行功能，但需要进一步优化以满足复杂交易策略的需求。

## 2. 优化目标

1. 实现智能订单路由算法
2. 开发交易成本模型（滑点、冲击成本）
3. 优化订单执行策略（拆分、路由、执行）
4. 增强交易所适配器的稳定性

## 3. 详细优化方案

### 3.1 智能订单路由算法

#### 3.1.1 路由策略接口

```cpp
class OrderRoutingStrategy {
public:
    virtual ~OrderRoutingStrategy() = default;

    // 计算最优路由
    [[nodiscard]] virtual std::optional<veloz::common::Venue> select_venue(
        const PlaceOrderRequest& req) const = 0;

    // 获取策略名称
    [[nodiscard]] virtual std::string name() const = 0;
};

// 价格优先策略
class PricePriorityStrategy final : public OrderRoutingStrategy {
public:
    [[nodiscard]] std::optional<veloz::common::Venue> select_venue(
        const PlaceOrderRequest& req) const override;

    [[nodiscard]] std::string name() const override {
        return "PricePriority";
    }
};

// 流动性优先策略
class LiquidityPriorityStrategy final : public OrderRoutingStrategy {
public:
    [[nodiscard]] std::optional<veloz::common::Venue> select_venue(
        const PlaceOrderRequest& req) const override;

    [[nodiscard]] std::string name() const override {
        return "LiquidityPriority";
    }
};

// 成本优先策略
class CostPriorityStrategy final : public OrderRoutingStrategy {
public:
    [[nodiscard]] std::optional<veloz::common::Venue> select_venue(
        const PlaceOrderRequest& req) const override;

    [[nodiscard]] std::string name() const override {
        return "CostPriority";
    }
};

// 混合策略
class HybridStrategy final : public OrderRoutingStrategy {
public:
    HybridStrategy(double price_weight = 0.4, double liquidity_weight = 0.3,
                   double cost_weight = 0.3);

    [[nodiscard]] std::optional<veloz::common::Venue> select_venue(
        const PlaceOrderRequest& req) const override;

    [[nodiscard]] std::string name() const override {
        return "Hybrid";
    }
};
```

#### 3.1.2 增强的订单路由器

```cpp
class OrderRouter final {
public:
    OrderRouter();

    // 设置路由策略
    void set_routing_strategy(std::shared_ptr<OrderRoutingStrategy> strategy);

    // 路由订单
    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req);

    // 获取路由决策历史
    [[nodiscard]] const std::vector<std::tuple<PlaceOrderRequest, veloz::common::Venue, double>>&
    get_routing_history() const;

    // 获取路由统计信息
    [[nodiscard]] std::map<veloz::common::Venue, int> get_venue_stats() const;

private:
    std::unordered_map<veloz::common::Venue, std::shared_ptr<ExchangeAdapter>> adapters_;
    std::optional<veloz::common::Venue> default_venue_;
    std::shared_ptr<OrderRoutingStrategy> strategy_;
    std::vector<std::tuple<PlaceOrderRequest, veloz::common::Venue, double>> routing_history_;
    std::map<veloz::common::Venue, int> venue_stats_;
    std::chrono::milliseconds order_timeout_{std::chrono::seconds(30)};
    bool failover_enabled_{true};
    mutable std::mutex mu_;
};
```

### 3.2 交易成本模型

#### 3.2.1 滑点和冲击成本模型

```cpp
class TransactionCostModel {
public:
    virtual ~TransactionCostModel() = default;

    // 计算预期成本
    [[nodiscard]] virtual double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const = 0;

    // 获取模型名称
    [[nodiscard]] virtual std::string name() const = 0;
};

// 固定成本模型
class FixedCostModel final : public TransactionCostModel {
public:
    FixedCostModel(double fixed_cost);

    [[nodiscard]] double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const override;

    [[nodiscard]] std::string name() const override {
        return "FixedCost";
    }
};

// 百分比成本模型
class PercentageCostModel final : public TransactionCostModel {
public:
    PercentageCostModel(double percentage);

    [[nodiscard]] double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const override;

    [[nodiscard]] std::string name() const override {
        return "PercentageCost";
    }
};

// 滑点模型
class SlippageModel final : public TransactionCostModel {
public:
    SlippageModel(double base_slippage = 0.001, double volatility_factor = 0.5);

    [[nodiscard]] double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const override;

    [[nodiscard]] std::string name() const override {
        return "Slippage";
    }
};

// 冲击成本模型
class ImpactCostModel final : public TransactionCostModel {
public:
    ImpactCostModel(double liquidity_factor = 0.0001, double volatility_factor = 0.1);

    [[nodiscard]] double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const override;

    [[nodiscard]] std::string name() const override {
        return "ImpactCost";
    }
};

// 综合成本模型
class ComprehensiveCostModel final : public TransactionCostModel {
public:
    ComprehensiveCostModel(std::vector<std::shared_ptr<TransactionCostModel>> models);

    [[nodiscard]] double calculate_expected_cost(
        const PlaceOrderRequest& req, const veloz::common::Venue& venue) const override;

    [[nodiscard]] std::string name() const override {
        return "ComprehensiveCost";
    }
};
```

#### 3.2.2 成本监控和分析

```cpp
class CostMonitor {
public:
    struct CostRecord {
        PlaceOrderRequest request;
        veloz::common::Venue venue;
        double expected_cost;
        double actual_cost;
        std::int64_t timestamp;
    };

    void record_cost(const CostRecord& record);
    [[nodiscard]] std::vector<CostRecord> get_cost_history() const;
    [[nodiscard]] double get_average_cost() const;
    [[nodiscard]] double get_max_cost() const;
    [[nodiscard]] double get_min_cost() const;
    [[nodiscard]] std::map<veloz::common::Venue, double> get_venue_avg_cost() const;

private:
    std::vector<CostRecord> cost_history_;
    mutable std::mutex mu_;
};
```

### 3.3 订单执行策略

#### 3.3.1 订单拆分策略

```cpp
class OrderSplitter {
public:
    virtual ~OrderSplitter() = default;

    [[nodiscard]] virtual std::vector<PlaceOrderRequest> split_order(
        const PlaceOrderRequest& original) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;
};

// 等额拆分策略
class EqualSizeSplitter final : public OrderSplitter {
public:
    EqualSizeSplitter(int parts = 10);

    [[nodiscard]] std::vector<PlaceOrderRequest> split_order(
        const PlaceOrderRequest& original) const override;

    [[nodiscard]] std::string name() const override {
        return "EqualSize";
    }
};

// 等量拆分策略
class EqualValueSplitter final : public OrderSplitter {
public:
    EqualValueSplitter(double part_value = 1000.0);

    [[nodiscard]] std::vector<PlaceOrderRequest> split_order(
        const PlaceOrderRequest& original) const override;

    [[nodiscard]] std::string name() const override {
        return "EqualValue";
    }
};

// 基于流动性的拆分策略
class LiquidityBasedSplitter final : public OrderSplitter {
public:
    [[nodiscard]] std::vector<PlaceOrderRequest> split_order(
        const PlaceOrderRequest& original) const override;

    [[nodiscard]] std::string name() const override {
        return "LiquidityBased";
    }
};
```

#### 3.3.2 执行调度策略

```cpp
class ExecutionScheduler {
public:
    virtual ~ExecutionScheduler() = default;

    virtual void schedule_order(const PlaceOrderRequest& req,
                                std::function<void(const ExecutionReport&)> callback) = 0;

    [[nodiscard]] virtual std::string name() const = 0;
};

// 立即执行策略
class ImmediateExecutionScheduler final : public ExecutionScheduler {
public:
    void schedule_order(const PlaceOrderRequest& req,
                       std::function<void(const ExecutionReport&)> callback) override;

    [[nodiscard]] std::string name() const override {
        return "Immediate";
    }
};

// 时间加权执行策略
class TimeWeightedExecutionScheduler final : public ExecutionScheduler {
public:
    TimeWeightedExecutionScheduler(std::chrono::milliseconds total_time = std::chrono::seconds(60));

    void schedule_order(const PlaceOrderRequest& req,
                       std::function<void(const ExecutionReport&)> callback) override;

    [[nodiscard]] std::string name() const override {
        return "TimeWeighted";
    }
};

// 成交量加权执行策略
class VolumeWeightedExecutionScheduler final : public ExecutionScheduler {
public:
    void schedule_order(const PlaceOrderRequest& req,
                       std::function<void(const ExecutionReport&)> callback) override;

    [[nodiscard]] std::string name() const override {
        return "VolumeWeighted";
    }
};
```

### 3.4 增强交易所适配器的稳定性

#### 3.4.1 连接管理优化

```cpp
class ExchangeAdapter {
public:
    virtual ~ExchangeAdapter() = default;

    // 连接管理
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
    [[nodiscard]] virtual bool is_healthy() const = 0;

    // 健康检查
    virtual void perform_health_check() = 0;
    [[nodiscard]] virtual double get_health_score() const = 0;

    // 连接统计
    [[nodiscard]] virtual int get_connection_attempts() const = 0;
    [[nodiscard]] virtual int get_successful_connections() const = 0;
    [[nodiscard]] virtual std::int64_t get_last_connect_time() const = 0;
    [[nodiscard]] virtual std::int64_t get_last_disconnect_time() const = 0;

    // 订单管理
    virtual std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) = 0;
    virtual std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) = 0;

    // 市场数据获取
    [[nodiscard]] virtual std::optional<double> get_best_bid() const = 0;
    [[nodiscard]] virtual std::optional<double> get_best_ask() const = 0;

    // 适配器信息
    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual const char* version() const = 0;
};

class BinanceExchangeAdapter final : public ExchangeAdapter {
public:
    BinanceExchangeAdapter(bool testnet = false);

    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool is_connected() const override;
    [[nodiscard]] bool is_healthy() const override;
    void perform_health_check() override;
    [[nodiscard]] double get_health_score() const override;

    [[nodiscard]] int get_connection_attempts() const override;
    [[nodiscard]] int get_successful_connections() const override;
    [[nodiscard]] std::int64_t get_last_connect_time() const override;
    [[nodiscard]] std::int64_t get_last_disconnect_time() const override;

    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override;
    std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override;

    [[nodiscard]] std::optional<double> get_best_bid() const override;
    [[nodiscard]] std::optional<double> get_best_ask() const override;

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] const char* version() const override;

private:
    bool connected_;
    bool healthy_;
    int connection_attempts_;
    int successful_connections_;
    std::int64_t last_connect_time_;
    std::int64_t last_disconnect_time_;
    std::int64_t last_health_check_time_;
    double health_score_;
    mutable std::mutex mu_;
};
```

#### 3.4.2 故障恢复和重试机制

```cpp
class OrderRetryManager {
public:
    struct RetryConfig {
        int max_retries;
        std::chrono::milliseconds retry_delay;
        std::vector<OrderStatus> retryable_statuses;
    };

    OrderRetryManager(RetryConfig config = {3, std::chrono::milliseconds(1000),
                                           {OrderStatus::Rejected}});

    bool should_retry(const ExecutionReport& report) const;
    void record_attempt(const PlaceOrderRequest& req, const ExecutionReport& report);
    [[nodiscard]] int get_retry_count(const std::string& client_order_id) const;
    void reset_retry_count(const std::string& client_order_id);

private:
    RetryConfig config_;
    std::unordered_map<std::string, int> retry_counts_;
    mutable std::mutex mu_;
};
```

### 3.5 执行质量评估

#### 3.5.1 执行质量指标

```cpp
class ExecutionQualityMetrics {
public:
    struct Metrics {
        double price_improvement;
        double execution_shortfall;
        double implementation_shortfall;
        double realized_slippage;
        double order_fill_rate;
        double average_fill_time;
    };

    [[nodiscard]] Metrics calculate(const PlaceOrderRequest& req,
                                   const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_price_improvement(const PlaceOrderRequest& req,
                                                    const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_execution_shortfall(const PlaceOrderRequest& req,
                                                    const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_implementation_shortfall(const PlaceOrderRequest& req,
                                                          const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_realized_slippage(const PlaceOrderRequest& req,
                                                   const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_order_fill_rate(const PlaceOrderRequest& req,
                                                 const std::vector<ExecutionReport>& reports) const;

    [[nodiscard]] double calculate_average_fill_time(const std::vector<ExecutionReport>& reports) const;
};
```

#### 3.5.2 执行质量监控

```cpp
class ExecutionQualityMonitor {
public:
    void record_execution(const PlaceOrderRequest& req,
                         const std::vector<ExecutionReport>& reports);

    [[nodiscard]] std::vector<ExecutionQualityMetrics::Metrics> get_all_metrics() const;
    [[nodiscard]] ExecutionQualityMetrics::Metrics get_average_metrics() const;
    [[nodiscard]] std::map<veloz::common::Venue, ExecutionQualityMetrics::Metrics>
    get_venue_metrics() const;

    [[nodiscard]] std::map<veloz::common::Venue, double> get_venue_fill_rate() const;
    [[nodiscard]] std::map<veloz::common::Venue, double> get_venue_slippage() const;

private:
    std::vector<std::tuple<PlaceOrderRequest, std::vector<ExecutionReport>,
                           ExecutionQualityMetrics::Metrics>> executions_;
    mutable std::mutex mu_;
};
```

## 4. 架构优化

### 4.1 执行系统重构

```cpp
class ExecutionManager {
public:
    ExecutionManager();

    // 初始化执行管理器
    void initialize();

    // 设置路由策略
    void set_routing_strategy(std::shared_ptr<OrderRoutingStrategy> strategy);

    // 设置成本模型
    void set_cost_model(std::shared_ptr<TransactionCostModel> model);

    // 设置订单拆分策略
    void set_order_splitter(std::shared_ptr<OrderSplitter> splitter);

    // 设置执行调度策略
    void set_execution_scheduler(std::shared_ptr<ExecutionScheduler> scheduler);

    // 提交订单
    void submit_order(const PlaceOrderRequest& req,
                     std::function<void(const ExecutionReport&)> callback);

    // 取消订单
    void cancel_order(const CancelOrderRequest& req);

    // 获取执行状态
    [[nodiscard]] std::vector<ExecutionReport> get_order_status(
        const std::string& client_order_id) const;

    // 获取执行质量指标
    [[nodiscard]] ExecutionQualityMetrics::Metrics get_execution_metrics(
        const std::string& client_order_id) const;

private:
    std::shared_ptr<OrderRouter> order_router_;
    std::shared_ptr<TransactionCostModel> cost_model_;
    std::shared_ptr<OrderSplitter> order_splitter_;
    std::shared_ptr<ExecutionScheduler> execution_scheduler_;
    std::shared_ptr<OrderRetryManager> retry_manager_;
    std::shared_ptr<ExecutionQualityMonitor> quality_monitor_;
    std::unordered_map<std::string, std::vector<ExecutionReport>> order_history_;
    std::unordered_map<std::string, std::function<void(const ExecutionReport&)>> order_callbacks_;
    mutable std::mutex mu_;
};
```

### 4.2 异步执行架构

```cpp
class AsyncExecutionEngine {
public:
    AsyncExecutionEngine();
    ~AsyncExecutionEngine();

    // 启动引擎
    void start();

    // 停止引擎
    void stop();

    // 提交订单（异步）
    std::future<ExecutionReport> submit_order(const PlaceOrderRequest& req);

    // 取消订单（异步）
    std::future<ExecutionReport> cancel_order(const CancelOrderRequest& req);

    // 获取订单状态（异步）
    std::future<std::vector<ExecutionReport>> get_order_status(
        const std::string& client_order_id);

private:
    void worker_loop();
    void handle_order(const PlaceOrderRequest& req,
                     std::promise<ExecutionReport>& promise);
    void handle_cancel(const CancelOrderRequest& req,
                     std::promise<ExecutionReport>& promise);
    void handle_status(const std::string& client_order_id,
                     std::promise<std::vector<ExecutionReport>>& promise);

    std::shared_ptr<ExecutionManager> exec_manager_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::queue<std::function<void()>> task_queue_;
    std::condition_variable task_cv_;
    std::mutex task_mutex_;
};
```

## 5. 实施计划

### 5.1 阶段划分

#### 阶段 1：智能路由和成本模型（2周）
1. 实现智能路由策略接口和基础策略
2. 开发交易成本模型
3. 增强订单路由器功能

#### 阶段 2：订单执行策略（2周）
1. 实现订单拆分策略
2. 开发执行调度策略
3. 实现异步执行架构

#### 阶段 3：适配器稳定性增强（2周）
1. 优化交易所适配器连接管理
2. 实现故障恢复和重试机制
3. 增强健康检查和统计功能

#### 阶段 4：执行质量评估（1周）
1. 实现执行质量指标计算
2. 开发执行质量监控系统
3. 实现执行报告和分析功能

#### 阶段 5：系统集成和测试（2周）
1. 集成所有优化组件
2. 编写单元测试和集成测试
3. 进行性能测试和优化

### 5.2 依赖和风险

#### 技术依赖
- 需要与所有支持的交易所API进行集成
- 需要高性能异步编程框架
- 需要复杂的数学计算库

#### 风险评估
- 策略实现风险：智能路由和成本模型的准确性
- 执行风险：订单拆分和调度的执行效率
- 稳定性风险：异步执行的可靠性和错误处理

#### 缓解措施
- 实施分阶段开发和测试
- 进行充分的性能和压力测试
- 实现全面的错误处理和恢复机制

## 6. 预期成果

### 功能增强
- 智能订单路由算法
- 全面的交易成本模型
- 优化的订单执行策略
- 稳定的交易所适配器

### 性能改进
- 减少交易成本和滑点
- 提高订单执行效率
- 增强系统稳定性和可靠性
- 提供详细的执行质量报告

## 7. 后续计划

### 短期优化
- 实施价格和流动性优先级策略
- 开发基本的滑点和冲击成本模型
- 优化订单路由和执行

### 中期优化
- 实现高级的执行策略（TWAP、VWAP）
- 开发更复杂的成本模型
- 增强执行质量监控和分析

### 长期优化
- 实现机器学习驱动的执行策略
- 开发自动化的执行质量优化
- 提供更高级的执行报告和可视化功能

## 8. 总结

执行系统优化是 VeloZ 量化交易框架的关键改进，旨在提高交易执行效率、降低成本并增强系统稳定性。通过实现智能路由算法、高级成本模型和优化的执行策略，框架将能够更好地处理复杂的交易场景。

这些优化将使策略开发者能够：
1. 获得更优的订单执行价格
2. 减少交易成本和滑点
3. 提高执行质量和稳定性
4. 获得详细的执行报告和分析

通过分阶段实施和严格的测试，我们可以确保这些优化功能的正确性和性能，同时保持系统的可维护性和可扩展性。
