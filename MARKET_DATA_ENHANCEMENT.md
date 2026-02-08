# VeloZ 量化交易框架 - 市场数据模块增强方案

## 1. 概述

市场数据模块是量化交易框架的核心组件之一，负责处理各种类型的市场数据。当前实现已经支持基础的交易、订单簿和K线数据，但需要进一步增强以满足更复杂的交易策略需求。

## 2. 增强目标

1. 支持更多市场数据类型（深度解析、Tick数据、Level-2数据）
2. 实现高级订单簿分析功能
3. 开发多交易所数据同步机制
4. 增加市场数据预处理和过滤模块

## 3. 详细增强方案

### 3.1 支持更多市场数据类型

#### 3.1.1 Tick 数据

```cpp
// 新增 Tick 数据类型
struct TickData {
    double bid_price{0.0};        ///< 最佳买入价格
    double bid_qty{0.0};          ///< 最佳买入数量
    double ask_price{0.0};        ///< 最佳卖出价格
    double ask_qty{0.0};          ///< 最佳卖出数量
    double last_price{0.0};       ///< 最新成交价格
    double volume{0.0};           ///< 成交量
    double open_price{0.0};       ///< 开盘价
    double high_price{0.0};       ///< 最高价
    double low_price{0.0};        ///< 最低价
    std::int64_t timestamp{0};    ///< 时间戳
};

// 更新事件类型枚举
enum class MarketEventType : std::uint8_t {
    Unknown = 0,
    Trade = 1,
    BookTop = 2,
    BookDelta = 3,
    Kline = 4,
    Ticker = 5,
    FundingRate = 6,
    MarkPrice = 7,
    Tick = 8,  ///< 新增 Tick 数据类型
};

// 更新 MarketEvent 结构
struct MarketEvent final {
    MarketEventType type{MarketEventType::Unknown};
    // ... 现有字段

    // 新增 Tick 数据类型支持
    std::variant<std::monostate, TradeData, BookData, KlineData, TickData> data;
};
```

#### 3.1.2 Level-2 深度数据

```cpp
// Level-2 深度数据类型
struct DepthData {
    std::vector<BookLevel> bids;    ///< 买入价格深度
    std::vector<BookLevel> asks;    ///< 卖出价格深度
    std::int64_t sequence{0};       ///< 序列号
    std::int64_t timestamp{0};      ///< 时间戳
};

// 更新事件类型枚举
enum class MarketEventType : std::uint8_t {
    // ... 现有类型
    Depth = 9,  ///< 新增 Level-2 深度数据类型
};
```

### 3.2 高级订单簿分析功能

#### 3.2.1 订单簿流动性分析

```cpp
class OrderBook final {
public:
    // ... 现有方法

    // 订单簿形状分析
    [[nodiscard]] double shape_score() const;
    [[nodiscard]] double imbalance_index() const;
    [[nodiscard]] double spread_to_depth_ratio() const;

    // 流动性区域分析
    [[nodiscard]] std::vector<std::pair<double, double>> liquidity_clusters(bool is_bid, double min_depth = 0.1) const;
    [[nodiscard]] double concentrated_liquidity(double price, bool is_bid, double range = 0.01) const;

    // 市场冲击成本预估
    [[nodiscard]] double market_impact_cost(double qty, bool is_bid, int order_type = 0) const;

    // 订单簿稳定性分析
    [[nodiscard]] double volatility_index() const;
    [[nodiscard]] double order_flow_imbalance() const;

private:
    // 内部计算方法
    double calculate_shape_score() const;
    std::vector<std::pair<double, double>> find_liquidity_clusters(const std::map<double, double>& levels, double min_depth) const;
};
```

#### 3.2.2 价格水平强度分析

```cpp
class OrderBook final {
public:
    // 价格水平强度分析
    [[nodiscard]] double price_level_strength(double price, bool is_bid) const;
    [[nodiscard]] std::vector<std::pair<double, double>> key_price_levels(bool is_bid, int top_n = 5) const;

    // 支撑位和阻力位计算
    [[nodiscard]] double calculate_support_level(int period = 20) const;
    [[nodiscard]] double calculate_resistance_level(int period = 20) const;

    // 价格波动范围预测
    [[nodiscard]] std::pair<double, double> price_range_prediction(double confidence = 0.95) const;

private:
    // 内部状态跟踪
    std::deque<OrderBook> historical_snapshots_;
};
```

### 3.3 多交易所数据同步机制

#### 3.3.1 数据同步管理器

```cpp
class MarketDataSynchronizer {
public:
    // 同步源配置
    struct SyncSource {
        std::string exchange;
        std::string symbol;
        std::vector<MarketEventType> event_types;
        double priority;
    };

    // 同步策略
    enum class SyncStrategy {
        PRIMARY_ONLY,       // 仅使用主源
        ALL_SOURCES,        // 使用所有源
        MAJORITY_VOTE,      // 多数投票
        WEIGHTED_AVERAGE    // 加权平均
    };

    MarketDataSynchronizer();
    ~MarketDataSynchronizer();

    // 配置管理
    void add_sync_source(const SyncSource& source);
    void remove_sync_source(const SyncSource& source);
    void set_sync_strategy(SyncStrategy strategy);

    // 同步操作
    void start_synchronization();
    void stop_synchronization();
    [[nodiscard]] bool is_synchronizing() const;

    // 数据访问
    [[nodiscard]] std::optional<MarketEvent> get_synced_event(MarketEventType type) const;
    [[nodiscard]] std::vector<MarketEvent> get_history(MarketEventType type, int64_t start_time, int64_t end_time) const;

    // 同步状态
    [[nodiscard]] double synchronization_quality() const;
    [[nodiscard]] std::map<std::string, double> source_health() const;

private:
    // 内部同步循环
    void sync_loop();
    MarketEvent resolve_conflict(const std::vector<MarketEvent>& events) const;
    MarketEvent weighted_average(const std::vector<MarketEvent>& events) const;

    // 数据源管理
    std::vector<std::shared_ptr<MarketDataSource>> data_sources_;
    std::map<std::string, SyncSource> sync_sources_;
    SyncStrategy sync_strategy_;

    // 同步状态
    std::atomic<bool> running_;
    std::thread sync_thread_;

    // 数据缓存
    std::map<MarketEventType, MarketEvent> latest_events_;
    std::map<MarketEventType, std::deque<MarketEvent>> history_cache_;

    // 健康状态
    std::map<std::string, double> source_health_;
    std::atomic<double> sync_quality_;
};
```

#### 3.3.2 数据一致性检查

```cpp
class DataConsistencyChecker {
public:
    enum class ConsistencyLevel {
        LOW,    // 基础一致性检查
        MEDIUM, // 中等一致性检查
        HIGH,   // 高级一致性检查
    };

    DataConsistencyChecker(ConsistencyLevel level = ConsistencyLevel::MEDIUM);

    // 检查事件一致性
    [[nodiscard]] bool check_event_consistency(const MarketEvent& event1, const MarketEvent& event2) const;

    // 检查数据源一致性
    [[nodiscard]] double check_source_consistency(const std::shared_ptr<MarketDataSource>& source) const;

    // 检查历史数据一致性
    [[nodiscard]] double check_history_consistency(const std::vector<MarketEvent>& history) const;

private:
    ConsistencyLevel level_;
    double tolerance_;
};
```

### 3.4 市场数据预处理和过滤模块

#### 3.4.1 数据预处理接口

```cpp
class DataPreprocessor {
public:
    virtual ~DataPreprocessor() = default;

    [[nodiscard]] virtual MarketEvent preprocess(const MarketEvent& event) const = 0;
    [[nodiscard]] virtual std::vector<MarketEvent> preprocess(const std::vector<MarketEvent>& events) const;
};

// 价格数据平滑器
class PriceSmoother : public DataPreprocessor {
public:
    enum class SmoothingType {
        MOVING_AVERAGE,
        EXPONENTIAL_MOVING_AVERAGE,
        WELCH_LOTHAR_SMOOTHING
    };

    PriceSmoother(SmoothingType type, int window_size);
    [[nodiscard]] MarketEvent preprocess(const MarketEvent& event) const override;

private:
    SmoothingType type_;
    int window_size_;
    mutable std::deque<double> price_history_;
};

// 异常值检测器
class OutlierDetector : public DataPreprocessor {
public:
    OutlierDetector(double z_threshold = 3.0);
    [[nodiscard]] MarketEvent preprocess(const MarketEvent& event) const override;

private:
    double z_threshold_;
    mutable std::deque<double> price_history_;
};
```

#### 3.4.2 数据过滤机制

```cpp
class DataFilter {
public:
    virtual ~DataFilter() = default;

    [[nodiscard]] virtual bool should_include(const MarketEvent& event) const = 0;
    [[nodiscard]] virtual std::vector<MarketEvent> filter(const std::vector<MarketEvent>& events) const;
};

// 交易过滤
class TradeFilter : public DataFilter {
public:
    TradeFilter(double min_qty = 0.0, double max_qty = 0.0);
    [[nodiscard]] bool should_include(const MarketEvent& event) const override;

private:
    double min_qty_;
    double max_qty_;
};

// 价格波动过滤
class PriceVolatilityFilter : public DataFilter {
public:
    PriceVolatilityFilter(double max_volatility);
    [[nodiscard]] bool should_include(const MarketEvent& event) const override;

private:
    double max_volatility_;
    mutable std::deque<double> price_history_;
};

// 数据完整性过滤
class DataIntegrityFilter : public DataFilter {
public:
    [[nodiscard]] bool should_include(const MarketEvent& event) const override;
};
```

### 3.5 数据验证和质量监控

```cpp
class DataQualityMonitor {
public:
    struct QualityMetrics {
        double completeness;
        double accuracy;
        double timeliness;
        double consistency;
        int64_t latency;
    };

    [[nodiscard]] QualityMetrics check_quality(const MarketEvent& event) const;
    [[nodiscard]] QualityMetrics check_quality(const std::vector<MarketEvent>& events) const;

    void register_event_type(MarketEventType type, double required_quality);
    [[nodiscard]] bool is_quality_acceptable(const MarketEvent& event) const;

private:
    std::map<MarketEventType, double> quality_requirements_;
};
```

## 4. 实现计划

### 4.1 阶段划分

#### 阶段 1：基础数据类型增强（2周）
1. 实现 Tick 数据类型和事件解析
2. 实现 Level-2 深度数据类型
3. 更新市场事件处理系统

#### 阶段 2：高级订单簿分析（3周）
1. 实现流动性分析方法
2. 实现价格水平强度分析
3. 实现订单簿稳定性和波动性分析

#### 阶段 3：多交易所数据同步（3周）
1. 设计数据同步架构
2. 实现同步管理器和策略
3. 实现数据一致性检查和健康监控

#### 阶段 4：数据预处理和过滤（2周）
1. 实现数据预处理接口和平滑算法
2. 实现数据过滤机制
3. 实现数据质量监控

#### 阶段 5：测试与优化（2周）
1. 编写单元测试
2. 进行性能测试和优化
3. 集成和验收测试

### 4.2 依赖和风险

#### 技术依赖
- 需要与所有支持的交易所API进行集成
- 需要高性能计算能力处理大量数据
- 需要可靠的网络通信机制

#### 风险评估
- 数据源可靠性风险
- 网络延迟和连接稳定性风险
- 数据格式一致性风险
- 性能瓶颈风险

#### 缓解措施
- 实现多源数据验证和容错机制
- 添加网络重试和失败恢复逻辑
- 设计数据格式转换和标准化层
- 进行性能分析和优化

## 5. 架构优化

### 5.1 模块接口改进

```cpp
// 统一的数据访问接口
class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    virtual bool subscribe(const SymbolId& symbol, MarketEventType event_type) = 0;
    virtual bool unsubscribe(const SymbolId& symbol, MarketEventType event_type) = 0;

    virtual std::optional<MarketEvent> get_latest_event(MarketEventType type) const = 0;
    virtual std::vector<MarketEvent> get_history(MarketEventType type, int64_t start, int64_t end) const = 0;

    virtual void set_data_quality_callback(std::function<void(const QualityMetrics&)> callback) = 0;
};
```

### 5.2 性能优化

1. **内存优化**：使用对象池和预分配内存
2. **计算优化**：使用多线程和并发数据处理
3. **存储优化**：使用压缩和索引技术
4. **网络优化**：使用高效的网络通信和数据压缩

## 6. 测试策略

### 6.1 单元测试

```cpp
// 订单簿分析测试
TEST(OrderBookAnalysisTest, ImbalanceIndexTest) {
    OrderBook book;

    // 添加测试数据
    std::vector<BookLevel> bids = {{100.0, 10}, {99.5, 20}, {99.0, 30}};
    std::vector<BookLevel> asks = {{101.0, 5}, {101.5, 10}, {102.0, 15}};
    book.apply_snapshot(bids, asks, 1);

    // 测试计算
    EXPECT_GT(book.imbalance_index(), 0.0);
    EXPECT_LT(book.imbalance_index(), 1.0);
}

// 数据同步测试
TEST(DataSynchronizerTest, SyncQualityTest) {
    MarketDataSynchronizer sync;

    // 模拟同步过程
    sync.start_synchronization();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 检查同步质量
    EXPECT_GT(sync.synchronization_quality(), 0.5);
}
```

### 6.2 集成测试

```cpp
// 完整市场数据流程测试
TEST(MarketDataFlowTest, CompleteProcessTest) {
    // 1. 创建数据源
    auto binance_ws = std::make_unique<BinanceWebSocket>();

    // 2. 配置同步器
    MarketDataSynchronizer sync;
    sync.add_sync_source({
        "binance", "BTCUSDT",
        {MarketEventType::Trade, MarketEventType::Depth, MarketEventType::Tick},
        1.0
    });

    // 3. 启动同步
    sync.start_synchronization();

    // 4. 验证数据完整性
    auto latest_trade = sync.get_synced_event(MarketEventType::Trade);
    EXPECT_TRUE(latest_trade.has_value());

    auto history = sync.get_history(MarketEventType::Depth,
                                   get_unix_timestamp() - 3600,
                                   get_unix_timestamp());
    EXPECT_GT(history.size(), 0);

    // 5. 停止同步
    sync.stop_synchronization();
}
```

## 7. 总结

市场数据模块的增强将显著提升 VeloZ 框架在处理复杂市场数据方面的能力，为开发更高级的交易策略提供基础。通过引入新的数据类型、高级分析功能、多源同步机制和数据质量控制，框架将具备更好的市场响应能力和策略执行精度。

这些增强将使量化交易系统能够：
1. 更准确地评估市场流动性和价格行为
2. 更有效地处理多个数据源的信息
3. 提供更高质量和更可靠的市场数据
4. 支持更复杂的交易策略开发

通过分阶段实施和严格的测试，我们可以确保这些增强功能的正确性和性能，同时保持系统的可维护性和可扩展性。
