# Custom Strategy Development

**Time:** 60 minutes | **Level:** Advanced

## What You'll Learn

- Understand the VeloZ strategy architecture and interfaces
- Implement a custom trading strategy in C++
- Generate trading signals based on market data
- Integrate with the order management and risk systems
- Write unit tests using the KJ Test framework
- Backtest your strategy with historical data
- Optimize strategy parameters using various methods
- Deploy your strategy to paper trading and live trading
- Monitor and alert on strategy performance

## Prerequisites

- Completed [Production Deployment](./production-deployment.md) tutorial
- C++ development experience (C++20 or later)
- Familiarity with KJ library types (see [KJ Library Guide](../../docs/references/kjdoc/library_usage_guide.md))
- VeloZ development environment configured

---

## Table of Contents

1. [Strategy Development Lifecycle](#strategy-development-lifecycle)
2. [Strategy Architecture](#strategy-architecture)
3. [Strategy Templates](#strategy-templates)
4. [Implementation Guide](#implementation-guide)
5. [Backtesting Best Practices](#backtesting-best-practices)
6. [Parameter Optimization](#parameter-optimization)
7. [Risk Control Integration](#risk-control-integration)
8. [Production Deployment](#production-deployment)
9. [Monitoring and Alerting](#monitoring-and-alerting)
10. [Troubleshooting](#troubleshooting)

---

## Strategy Development Lifecycle

### Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Strategy Development Lifecycle                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐  │
│  │ Planning │──>│  Design  │──>│  Develop │──>│   Test   │──>│ Backtest │  │
│  │          │   │          │   │          │   │          │   │          │  │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘  │
│                                                                     │       │
│                                                                     v       │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐  │
│  │  Monitor │<──│   Live   │<──│  Testnet │<──│  Paper   │<──│ Optimize │  │
│  │          │   │          │   │          │   │          │   │          │  │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 1: Planning and Design

Before writing code, define your strategy clearly:

| Aspect | Questions to Answer |
|--------|---------------------|
| **Market Hypothesis** | What market inefficiency are you exploiting? |
| **Entry Conditions** | What signals trigger a buy/sell? |
| **Exit Conditions** | When do you close positions? |
| **Risk Parameters** | What is your max position size, stop-loss, take-profit? |
| **Time Frame** | What data frequency does your strategy need? |
| **Target Markets** | Which symbols/exchanges will you trade? |

**Example Strategy Specification:**

```yaml
strategy_name: "SMA Crossover"
type: "Trend Following"
hypothesis: "Short-term trends persist after SMA crossover"
entry:
  buy: "Short SMA crosses above Long SMA"
  sell: "Short SMA crosses below Long SMA"
exit:
  take_profit: "10% gain"
  stop_loss: "5% loss"
  time_exit: "24 hours max hold"
parameters:
  short_period: 10
  long_period: 20
  position_size: 0.01
risk:
  max_position_size: 0.1
  max_drawdown: 10%
  risk_per_trade: 2%
target_markets:
  - BTCUSDT
  - ETHUSDT
```

### Phase 2: Implementation

1. Create strategy header file
2. Implement strategy logic
3. Register with StrategyManager
4. Write unit tests

### Phase 3: Testing and Validation

1. Unit tests for signal generation
2. Integration tests with mock market data
3. Backtesting with historical data
4. Parameter optimization
5. Walk-forward analysis

### Phase 4: Deployment

1. Paper trading (simulated execution)
2. Testnet trading (real API, test funds)
3. Live trading (minimal position size)
4. Production scaling (full position size)

---

## Strategy Architecture

### Core Components

VeloZ strategies implement the `IStrategy` interface defined in `libs/strategy/include/veloz/strategy/strategy.h`:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Strategy Architecture                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐                                                        │
│  │  IStrategy      │  Interface that all strategies must implement          │
│  │  (interface)    │                                                        │
│  └────────┬────────┘                                                        │
│           │                                                                 │
│           v                                                                 │
│  ┌─────────────────┐                                                        │
│  │  BaseStrategy   │  Common functionality (state, metrics, lifecycle)      │
│  │  (base class)   │                                                        │
│  └────────┬────────┘                                                        │
│           │                                                                 │
│     ┌─────┴─────┬─────────────┬─────────────┬─────────────┐                 │
│     v           v             v             v             v                 │
│  ┌──────┐  ┌──────────┐  ┌─────────┐  ┌──────────┐  ┌──────────┐            │
│  │Custom│  │Momentum  │  │Grid     │  │Mean      │  │Market    │            │
│  │Strat │  │Strategy  │  │Strategy │  │Reversion │  │Making    │            │
│  └──────┘  └──────────┘  └─────────┘  └──────────┘  └──────────┘            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Interfaces

| Interface | Purpose | Key Methods |
|-----------|---------|-------------|
| `IStrategy` | Base interface for all strategies | `on_event()`, `get_signals()`, `on_start()`, `on_stop()` |
| `BaseStrategy` | Provides common functionality | State management, metrics, lifecycle |
| `IStrategyFactory` | Creates strategy instances | `create_strategy()`, `get_strategy_type()` |
| `StrategyManager` | Manages strategy lifecycle | `load_strategy()`, `start_strategy()`, `on_market_event()` |

### Strategy Lifecycle States

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Strategy Lifecycle States                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────┐   initialize()   ┌─────────┐   on_start()   ┌─────────┐        │
│  │ Created │ ───────────────> │ Ready   │ ─────────────> │ Running │        │
│  └─────────┘                  └─────────┘                └────┬────┘        │
│                                                               │             │
│                                    ┌──────────────────────────┤             │
│                                    │                          │             │
│                               on_pause()                  on_stop()         │
│                                    │                          │             │
│                                    v                          v             │
│                               ┌─────────┐                ┌─────────┐        │
│                               │ Paused  │                │ Stopped │        │
│                               └────┬────┘                └─────────┘        │
│                                    │                                        │
│                               on_resume()                                   │
│                                    │                                        │
│                                    v                                        │
│                               ┌─────────┐                                   │
│                               │ Running │                                   │
│                               └─────────┘                                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### IStrategy Interface Methods

```cpp
class IStrategy : public kj::Refcounted {
public:
  // Identification
  virtual kj::StringPtr get_id() const = 0;
  virtual kj::StringPtr get_name() const = 0;
  virtual StrategyType get_type() const = 0;

  // Lifecycle
  virtual bool initialize(const StrategyConfig& config, Logger& logger) = 0;
  virtual void on_start() = 0;
  virtual void on_stop() = 0;
  virtual void on_pause() = 0;
  virtual void on_resume() = 0;

  // Event handling
  virtual void on_event(const market::MarketEvent& event) = 0;
  virtual void on_position_update(const oms::Position& position) = 0;
  virtual void on_timer(int64_t timestamp) = 0;
  virtual void on_order_rejected(const exec::PlaceOrderRequest& req,
                                 kj::StringPtr reason) = 0;

  // Signal generation
  virtual kj::Vector<exec::PlaceOrderRequest> get_signals() = 0;

  // State management
  virtual StrategyState get_state() const = 0;
  virtual void reset() = 0;

  // Hot-reload support
  virtual bool supports_hot_reload() const = 0;
  virtual bool update_parameters(const kj::TreeMap<kj::String, double>& params) = 0;
  virtual kj::Maybe<const StrategyMetrics&> get_metrics() const = 0;
};
```

---

## Strategy Templates

### Basic Strategy Template

Use this template for simple signal-based strategies:

```cpp
// libs/strategy/include/veloz/strategy/my_strategy.h
#pragma once

#include "veloz/strategy/strategy.h"
#include <kj/vector.h>

namespace veloz::strategy {

class MyStrategy : public BaseStrategy {
public:
    explicit MyStrategy(const StrategyConfig& config);
    ~MyStrategy() noexcept override = default;

    // IStrategy interface
    StrategyType get_type() const override;
    void on_event(const market::MarketEvent& event) override;
    void on_timer(int64_t timestamp) override;
    kj::Vector<exec::PlaceOrderRequest> get_signals() override;
    void reset() override;

    // Factory support
    static kj::StringPtr get_strategy_type();

    // Hot-reload support
    bool supports_hot_reload() const override;
    bool update_parameters(const kj::TreeMap<kj::String, double>& params) override;
    kj::Maybe<const StrategyMetrics&> get_metrics() const override;

private:
    // Strategy parameters
    double param1_;
    double param2_;

    // Strategy state
    bool in_position_{false};

    // Pending signals
    kj::Vector<exec::PlaceOrderRequest> signals_;

    // Metrics
    StrategyMetrics metrics_;

    // Helper methods
    void process_price(double price);
    void generate_signal(double price, exec::OrderSide side);
};

class MyStrategyFactory : public StrategyFactory<MyStrategy> {
public:
    kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
```

### Advanced Strategy Template (Multi-Symbol)

For strategies that trade multiple symbols:

```cpp
// libs/strategy/include/veloz/strategy/multi_symbol_strategy.h
#pragma once

#include "veloz/strategy/strategy.h"
#include <kj/map.h>
#include <kj/vector.h>

namespace veloz::strategy {

struct SymbolState {
    double last_price{0.0};
    double position_size{0.0};
    bool in_position{false};
    kj::Vector<double> price_history;
};

class MultiSymbolStrategy : public BaseStrategy {
public:
    explicit MultiSymbolStrategy(const StrategyConfig& config);
    ~MultiSymbolStrategy() noexcept override = default;

    StrategyType get_type() const override;
    void on_event(const market::MarketEvent& event) override;
    void on_timer(int64_t timestamp) override;
    kj::Vector<exec::PlaceOrderRequest> get_signals() override;
    void reset() override;

    static kj::StringPtr get_strategy_type();

    bool supports_hot_reload() const override;
    bool update_parameters(const kj::TreeMap<kj::String, double>& params) override;
    kj::Maybe<const StrategyMetrics&> get_metrics() const override;

private:
    // Per-symbol state
    kj::HashMap<kj::String, SymbolState> symbol_states_;

    // Strategy parameters
    double risk_per_symbol_;
    double max_total_exposure_;

    // Pending signals
    kj::Vector<exec::PlaceOrderRequest> signals_;

    // Metrics
    StrategyMetrics metrics_;

    // Helper methods
    void process_symbol_event(kj::StringPtr symbol, const market::MarketEvent& event);
    double calculate_position_size(kj::StringPtr symbol, double price);
    bool check_exposure_limits(double additional_exposure);
};

} // namespace veloz::strategy
```

### Technical Indicator Strategy Template

For strategies using technical indicators:

```cpp
// libs/strategy/include/veloz/strategy/indicator_strategy.h
#pragma once

#include "veloz/strategy/advanced_strategies.h"
#include <kj/vector.h>

namespace veloz::strategy {

class IndicatorStrategy : public TechnicalIndicatorStrategy {
public:
    explicit IndicatorStrategy(const StrategyConfig& config);
    ~IndicatorStrategy() noexcept override = default;

    StrategyType get_type() const override;
    void on_event(const market::MarketEvent& event) override;
    void on_timer(int64_t timestamp) override;
    kj::Vector<exec::PlaceOrderRequest> get_signals() override;
    void reset() override;

    static kj::StringPtr get_strategy_type();

private:
    // Price buffer for indicator calculation
    kj::Vector<double> price_buffer_;
    size_t buffer_start_{0};

    // Indicator values
    double rsi_{50.0};
    double macd_{0.0};
    double signal_{0.0};
    double upper_band_{0.0};
    double lower_band_{0.0};

    // Strategy parameters
    int rsi_period_{14};
    double rsi_overbought_{70.0};
    double rsi_oversold_{30.0};

    // Pending signals
    kj::Vector<exec::PlaceOrderRequest> signals_;

    // Metrics
    StrategyMetrics metrics_;

    // Helper methods
    void update_indicators(double price);
    void check_signals(double price);
};

} // namespace veloz::strategy
```

---

## Implementation Guide

### Step 1: Create the Strategy Header

Create a new strategy that implements a simple moving average crossover.

```cpp
// libs/strategy/include/veloz/strategy/sma_crossover_strategy.h
#pragma once

#include "veloz/strategy/strategy.h"
#include <kj/vector.h>

namespace veloz::strategy {

/**
 * @brief Simple Moving Average Crossover Strategy
 *
 * Generates buy signals when short-term SMA crosses above long-term SMA.
 * Generates sell signals when short-term SMA crosses below long-term SMA.
 *
 * Parameters:
 * - short_period: Short SMA period (default: 10)
 * - long_period: Long SMA period (default: 20)
 * - position_size: Base position size (default: 0.01)
 */
class SmaCrossoverStrategy : public BaseStrategy {
public:
    explicit SmaCrossoverStrategy(const StrategyConfig& config);
    ~SmaCrossoverStrategy() noexcept override = default;

    // IStrategy interface
    StrategyType get_type() const override;
    void on_event(const market::MarketEvent& event) override;
    void on_timer(int64_t timestamp) override;
    kj::Vector<exec::PlaceOrderRequest> get_signals() override;
    void reset() override;

    // Factory support
    static kj::StringPtr get_strategy_type();

    // Hot-reload support
    bool supports_hot_reload() const override;
    bool update_parameters(const kj::TreeMap<kj::String, double>& params) override;
    kj::Maybe<const StrategyMetrics&> get_metrics() const override;

private:
    // Price history for SMA calculation
    kj::Vector<double> price_buffer_;

    // Strategy parameters
    int short_period_;
    int long_period_;
    double position_size_;

    // SMA state
    double short_sma_{0.0};
    double long_sma_{0.0};
    double prev_short_sma_{0.0};
    double prev_long_sma_{0.0};
    bool indicators_ready_{false};

    // Position tracking
    bool in_position_{false};
    exec::OrderSide position_side_{exec::OrderSide::Buy};

    // Pending signals
    kj::Vector<exec::PlaceOrderRequest> signals_;

    // Metrics
    StrategyMetrics metrics_;

    // Helper methods
    void update_sma(double price);
    double calculate_sma(size_t period) const;
    void check_crossover_signals(double price);
};

class SmaCrossoverStrategyFactory : public StrategyFactory<SmaCrossoverStrategy> {
public:
    kj::StringPtr get_strategy_type() const override;
};

} // namespace veloz::strategy
```

### Step 2: Implement the Strategy Logic

```cpp
// libs/strategy/src/sma_crossover_strategy.cpp
#include "veloz/strategy/sma_crossover_strategy.h"

#include <algorithm>
#include <chrono>

namespace veloz::strategy {

namespace {
double get_param(const kj::TreeMap<kj::String, double>& params,
                 kj::StringPtr key, double default_val) {
    KJ_IF_SOME(val, params.find(key)) {
        return val;
    }
    return default_val;
}

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

SmaCrossoverStrategy::SmaCrossoverStrategy(const StrategyConfig& config)
    : BaseStrategy(config) {
    short_period_ = static_cast<int>(
        get_param(config_.parameters, "short_period"_kj, 10.0));
    long_period_ = static_cast<int>(
        get_param(config_.parameters, "long_period"_kj, 20.0));
    position_size_ = get_param(config_.parameters, "position_size"_kj, 0.01);

    // Reserve buffer space
    price_buffer_.reserve(static_cast<size_t>(long_period_ + 5));
}

StrategyType SmaCrossoverStrategy::get_type() const {
    return StrategyType::Custom;
}

void SmaCrossoverStrategy::on_event(const market::MarketEvent& event) {
    if (!running_) return;

    auto start = now_ns();
    double price = 0.0;

    // Extract price from event
    if (event.type == market::MarketEventType::Trade) {
        if (event.data.is<market::TradeData>()) {
            price = event.data.get<market::TradeData>().price;
        }
    } else if (event.type == market::MarketEventType::Kline) {
        if (event.data.is<market::KlineData>()) {
            price = event.data.get<market::KlineData>().close;
        }
    }

    if (price > 0.0) {
        update_sma(price);

        if (indicators_ready_) {
            check_crossover_signals(price);
        }
    }

    // Update metrics
    metrics_.events_processed.fetch_add(1);
    auto elapsed = static_cast<uint64_t>(now_ns() - start);
    metrics_.execution_time_ns.fetch_add(elapsed);
}

void SmaCrossoverStrategy::on_timer(int64_t timestamp) {
    // No timer-based logic for this strategy
    (void)timestamp;
}

kj::Vector<exec::PlaceOrderRequest> SmaCrossoverStrategy::get_signals() {
    auto result = kj::mv(signals_);
    signals_ = kj::Vector<exec::PlaceOrderRequest>();
    return result;
}

void SmaCrossoverStrategy::reset() {
    BaseStrategy::reset();
    price_buffer_.clear();
    short_sma_ = 0.0;
    long_sma_ = 0.0;
    prev_short_sma_ = 0.0;
    prev_long_sma_ = 0.0;
    indicators_ready_ = false;
    in_position_ = false;
    signals_.clear();
    metrics_.reset();
}

kj::StringPtr SmaCrossoverStrategy::get_strategy_type() {
    return "SmaCrossoverStrategy"_kj;
}

bool SmaCrossoverStrategy::supports_hot_reload() const {
    return true;
}

bool SmaCrossoverStrategy::update_parameters(
    const kj::TreeMap<kj::String, double>& params) {
    KJ_IF_SOME(val, params.find("short_period"_kj)) {
        short_period_ = static_cast<int>(val);
    }
    KJ_IF_SOME(val, params.find("long_period"_kj)) {
        long_period_ = static_cast<int>(val);
    }
    KJ_IF_SOME(val, params.find("position_size"_kj)) {
        position_size_ = val;
    }
    return true;
}

kj::Maybe<const StrategyMetrics&> SmaCrossoverStrategy::get_metrics() const {
    return metrics_;
}

void SmaCrossoverStrategy::update_sma(double price) {
    // Store previous values for crossover detection
    prev_short_sma_ = short_sma_;
    prev_long_sma_ = long_sma_;

    // Add price to buffer
    price_buffer_.add(price);

    // Maintain buffer size
    size_t max_size = static_cast<size_t>(long_period_ + 5);
    while (price_buffer_.size() > max_size) {
        // Shift buffer (simple approach)
        for (size_t i = 0; i < price_buffer_.size() - 1; ++i) {
            price_buffer_[i] = price_buffer_[i + 1];
        }
        price_buffer_.resize(price_buffer_.size() - 1);
    }

    // Calculate SMAs
    short_sma_ = calculate_sma(static_cast<size_t>(short_period_));
    long_sma_ = calculate_sma(static_cast<size_t>(long_period_));

    // Check if we have enough data
    indicators_ready_ = price_buffer_.size() >= static_cast<size_t>(long_period_);
}

double SmaCrossoverStrategy::calculate_sma(size_t period) const {
    if (price_buffer_.size() < period) {
        return 0.0;
    }

    double sum = 0.0;
    size_t start = price_buffer_.size() - period;
    for (size_t i = start; i < price_buffer_.size(); ++i) {
        sum += price_buffer_[i];
    }
    return sum / static_cast<double>(period);
}

void SmaCrossoverStrategy::check_crossover_signals(double price) {
    // Bullish crossover: short SMA crosses above long SMA
    bool bullish_cross = prev_short_sma_ <= prev_long_sma_ &&
                         short_sma_ > long_sma_;

    // Bearish crossover: short SMA crosses below long SMA
    bool bearish_cross = prev_short_sma_ >= prev_long_sma_ &&
                         short_sma_ < long_sma_;

    kj::StringPtr symbol = "BTCUSDT"_kj;
    if (config_.symbols.size() > 0) {
        symbol = config_.symbols[0];
    }

    if (bullish_cross && !in_position_) {
        // Generate buy signal
        exec::PlaceOrderRequest order;
        order.symbol = symbol;
        order.side = exec::OrderSide::Buy;
        order.qty = position_size_;
        order.price = price;
        order.type = exec::OrderType::Market;
        order.tif = exec::TimeInForce::GTC;
        order.strategy_id = kj::str(get_id());

        signals_.add(kj::mv(order));
        metrics_.signals_generated.fetch_add(1);

        in_position_ = true;
        position_side_ = exec::OrderSide::Buy;
    }
    else if (bearish_cross && in_position_) {
        // Generate sell signal to close position
        exec::PlaceOrderRequest order;
        order.symbol = symbol;
        order.side = exec::OrderSide::Sell;
        order.qty = position_size_;
        order.price = price;
        order.type = exec::OrderType::Market;
        order.tif = exec::TimeInForce::GTC;
        order.strategy_id = kj::str(get_id());

        signals_.add(kj::mv(order));
        metrics_.signals_generated.fetch_add(1);

        in_position_ = false;
    }
}

kj::StringPtr SmaCrossoverStrategyFactory::get_strategy_type() const {
    return SmaCrossoverStrategy::get_strategy_type();
}

} // namespace veloz::strategy
```

### Step 3: Integrate with Risk Management

Strategies integrate with the risk engine through the `on_order_rejected` callback:

```cpp
// Add to sma_crossover_strategy.h (in public section)
void on_order_rejected(const exec::PlaceOrderRequest& req,
                       kj::StringPtr reason) override;

// Add to sma_crossover_strategy.cpp
void SmaCrossoverStrategy::on_order_rejected(
    const exec::PlaceOrderRequest& req, kj::StringPtr reason) {
    KJ_IF_SOME(logger, logger_) {
        logger.warn(kj::str("Order rejected: ", reason).cStr());
    }

    // Reset position state if entry was rejected
    if (in_position_ && req.side == position_side_) {
        in_position_ = false;
    }

    metrics_.errors.fetch_add(1);
}
```

### Step 4: Write Unit Tests

```cpp
// libs/strategy/tests/test_sma_crossover_strategy.cpp
#include "kj/test.h"
#include "veloz/strategy/sma_crossover_strategy.h"

namespace {

using namespace veloz::strategy;
using namespace veloz::market;
using namespace veloz::exec;

StrategyConfig create_config() {
    StrategyConfig config;
    config.name = kj::heapString("TestSMA");
    config.type = StrategyType::Custom;
    config.risk_per_trade = 0.01;
    config.max_position_size = 1.0;
    config.stop_loss = 0.05;
    config.take_profit = 0.10;
    config.symbols.add(kj::heapString("BTCUSDT"));
    config.parameters.insert(kj::str("short_period"), 5.0);
    config.parameters.insert(kj::str("long_period"), 10.0);
    return config;
}

MarketEvent create_trade(double price) {
    MarketEvent event;
    event.type = MarketEventType::Trade;
    event.symbol = veloz::common::SymbolId("BTCUSDT"_kj);
    event.ts_exchange_ns = 1000000000;
    event.ts_recv_ns = 1000000001;

    TradeData trade;
    trade.price = price;
    trade.qty = 1.0;
    trade.is_buyer_maker = false;
    event.data = trade;

    return event;
}

KJ_TEST("SmaCrossoverStrategy: Construction") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);

    KJ_EXPECT(strategy.get_type() == StrategyType::Custom);
    KJ_EXPECT(strategy.get_name() == "TestSMA"_kj);
}

KJ_TEST("SmaCrossoverStrategy: No signals without enough data") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);
    strategy.on_start();

    // Add only 5 prices (need 10 for long SMA)
    for (int i = 0; i < 5; ++i) {
        strategy.on_event(create_trade(100.0 + i));
    }

    auto signals = strategy.get_signals();
    KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("SmaCrossoverStrategy: Buy signal on bullish crossover") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);
    strategy.on_start();

    // Create downtrend then uptrend for crossover
    for (int i = 0; i < 15; ++i) {
        strategy.on_event(create_trade(100.0 - i * 0.5));
    }

    // Uptrend: prices rising sharply
    bool found_buy = false;
    for (int i = 0; i < 10; ++i) {
        strategy.on_event(create_trade(92.0 + i * 2.0));
        auto signals = strategy.get_signals();
        for (const auto& sig : signals) {
            if (sig.side == OrderSide::Buy) {
                found_buy = true;
            }
        }
    }

    KJ_EXPECT(found_buy);
}

KJ_TEST("SmaCrossoverStrategy: Reset clears state") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);
    strategy.on_start();

    for (int i = 0; i < 20; ++i) {
        strategy.on_event(create_trade(100.0 + i));
    }

    strategy.reset();

    auto signals = strategy.get_signals();
    KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("SmaCrossoverStrategy: Hot reload parameters") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);

    KJ_EXPECT(strategy.supports_hot_reload());

    kj::TreeMap<kj::String, double> new_params;
    new_params.insert(kj::str("short_period"), 15.0);
    new_params.insert(kj::str("long_period"), 30.0);

    KJ_EXPECT(strategy.update_parameters(new_params));
}

KJ_TEST("SmaCrossoverStrategy: Metrics tracking") {
    auto config = create_config();
    SmaCrossoverStrategy strategy(config);
    strategy.on_start();

    for (int i = 0; i < 10; ++i) {
        strategy.on_event(create_trade(100.0));
    }

    KJ_IF_SOME(metrics, strategy.get_metrics()) {
        KJ_EXPECT(metrics.events_processed.load() == 10);
    } else {
        KJ_FAIL_EXPECT("Metrics should be available");
    }
}

} // namespace
```

### Step 5: Register the Strategy Factory

```cpp
// In your engine initialization code
#include "veloz/strategy/sma_crossover_strategy.h"

void register_strategies(StrategyManager& manager) {
    // Register built-in strategies
    manager.register_strategy_factory(
        kj::rc<MomentumStrategyFactory>());
    manager.register_strategy_factory(
        kj::rc<GridStrategyFactory>());

    // Register custom strategy
    manager.register_strategy_factory(
        kj::rc<SmaCrossoverStrategyFactory>());
}
```

---

## Backtesting Best Practices

### Historical Data Preparation

#### Data Quality Checklist

| Check | Description | Action |
|-------|-------------|--------|
| **Completeness** | No missing time periods | Fill gaps or exclude periods |
| **Accuracy** | Prices within reasonable bounds | Filter outliers |
| **Consistency** | Timestamps in order | Sort and deduplicate |
| **Survivorship** | Include delisted assets | Use point-in-time data |

#### Data Sources

```bash
# Download historical data
curl -X POST http://127.0.0.1:8080/api/data/download \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT",
    "start_date": "2024-01-01",
    "end_date": "2025-12-31",
    "data_type": "klines",
    "interval": "1h",
    "output_path": "data/btcusdt_1h.csv"
  }'
```

### Backtest Configuration

```json
{
  "strategy": {
    "name": "sma_crossover_backtest",
    "type": "SmaCrossoverStrategy",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "short_period": 10,
      "long_period": 20,
      "position_size": 0.01
    }
  },
  "backtest": {
    "start_date": "2024-01-01",
    "end_date": "2025-12-31",
    "initial_capital": 10000.0,
    "data_source": "binance_klines",
    "interval": "1h",
    "commission_rate": 0.001,
    "slippage_model": "fixed",
    "slippage_bps": 5
  }
}
```

### Running a Backtest

```bash
# Run backtest
./build/dev/apps/engine/veloz_engine --backtest config/backtest_sma.json
```

### Avoiding Overfitting

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Walk-Forward Analysis                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                          Full Data Period                           │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  Window 1:                                                                  │
│  ┌───────────────────────────┐ ┌───────────┐                                │
│  │    Training (Optimize)    │ │   Test    │                                │
│  └───────────────────────────┘ └───────────┘                                │
│                                                                             │
│  Window 2:                                                                  │
│       ┌───────────────────────────┐ ┌───────────┐                           │
│       │    Training (Optimize)    │ │   Test    │                           │
│       └───────────────────────────┘ └───────────┘                           │
│                                                                             │
│  Window 3:                                                                  │
│            ┌───────────────────────────┐ ┌───────────┐                      │
│            │    Training (Optimize)    │ │   Test    │                      │
│            └───────────────────────────┘ └───────────┘                      │
│                                                                             │
│  Final Performance = Average of all out-of-sample test results              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Best Practices:**

1. **Use out-of-sample testing**: Never optimize on the same data you test on
2. **Walk-forward analysis**: Roll the training/test windows forward
3. **Cross-validation**: Use multiple train/test splits
4. **Limit parameters**: Fewer parameters = less overfitting risk
5. **Realistic assumptions**: Include commissions, slippage, and latency

### Result Analysis

Key metrics to evaluate:

| Metric | Good Range | Description |
|--------|------------|-------------|
| **Sharpe Ratio** | > 1.0 | Risk-adjusted return |
| **Sortino Ratio** | > 1.5 | Downside risk-adjusted return |
| **Max Drawdown** | < 20% | Largest peak-to-trough decline |
| **Win Rate** | > 40% | Percentage of winning trades |
| **Profit Factor** | > 1.5 | Gross profit / gross loss |
| **Calmar Ratio** | > 1.0 | Annual return / max drawdown |

---

## Parameter Optimization

VeloZ provides four optimization algorithms in `libs/backtest/include/veloz/backtest/optimizer.h`:

### Grid Search

Exhaustively tests all parameter combinations. Best for small parameter spaces.

```cpp
#include "veloz/backtest/optimizer.h"

// Create optimizer
auto optimizer = veloz::backtest::OptimizerFactory::create(
    veloz::backtest::OptimizationAlgorithm::GridSearch);

// Set parameter ranges
kj::TreeMap<kj::String, std::pair<double, double>> ranges;
ranges.insert(kj::str("short_period"), {5.0, 20.0});
ranges.insert(kj::str("long_period"), {15.0, 50.0});
ranges.insert(kj::str("position_size"), {0.01, 0.1});

optimizer->set_parameter_ranges(ranges);
optimizer->set_optimization_target("sharpe"_kj);
optimizer->set_max_iterations(1000);
optimizer->set_data_source(data_source);

// Run optimization
optimizer->optimize(strategy);

// Get results
auto best_params = optimizer->get_best_parameters();
```

**Pros:**
- Guaranteed to find global optimum (within grid)
- Easy to understand and implement
- Parallelizable

**Cons:**
- Exponential complexity with parameters
- May miss optimal values between grid points

### Random Search

Samples random parameter combinations. Often competitive with grid search.

```cpp
auto optimizer = veloz::backtest::OptimizerFactory::create(
    veloz::backtest::OptimizationAlgorithm::RandomSearch);

optimizer->set_parameter_ranges(ranges);
optimizer->set_optimization_target("sharpe"_kj);
optimizer->set_max_iterations(500);

// Set progress callback
auto* random_opt = static_cast<veloz::backtest::RandomSearchOptimizer*>(
    optimizer.get());
random_opt->set_progress_callback([](const auto& progress) {
    std::cout << "Progress: " << progress.progress_fraction * 100 << "%"
              << " Best: " << progress.best_fitness << std::endl;
});

optimizer->optimize(strategy);
```

**Pros:**
- More efficient than grid search for high-dimensional spaces
- Can find good solutions quickly
- No grid spacing issues

**Cons:**
- No guarantee of finding global optimum
- Results vary between runs

### Bayesian Optimization

Uses a probabilistic model to guide the search. Most sample-efficient.

```cpp
auto optimizer = veloz::backtest::OptimizerFactory::create(
    veloz::backtest::OptimizationAlgorithm::BayesianOptimization);

auto* bayes_opt = static_cast<veloz::backtest::BayesianOptimizer*>(
    optimizer.get());

// Configure Bayesian optimization
bayes_opt->set_initial_samples(10);  // Random samples before using model
bayes_opt->set_acquisition_function("ei"_kj);  // Expected Improvement
bayes_opt->set_exploration_params(2.0, 0.01);  // kappa, xi

bayes_opt->set_parameter_ranges(ranges);
bayes_opt->set_optimization_target("sharpe"_kj);
bayes_opt->set_max_iterations(100);

optimizer->optimize(strategy);

// Get predictions for new parameter sets
auto [mean, std] = bayes_opt->predict(test_params);
```

**Pros:**
- Most sample-efficient
- Balances exploration and exploitation
- Works well with expensive evaluations

**Cons:**
- More complex to implement
- Computational overhead for surrogate model
- May struggle with high-dimensional spaces

### Genetic Algorithm

Evolves a population of parameter sets. Good for complex landscapes.

```cpp
auto optimizer = veloz::backtest::OptimizerFactory::create(
    veloz::backtest::OptimizationAlgorithm::GeneticAlgorithm);

auto* ga_opt = static_cast<veloz::backtest::GeneticAlgorithmOptimizer*>(
    optimizer.get());

// Configure genetic algorithm
ga_opt->set_population_size(50);
ga_opt->set_mutation_rate(0.1);
ga_opt->set_crossover_rate(0.8);
ga_opt->set_elite_count(5);
ga_opt->set_tournament_size(3);
ga_opt->set_convergence_params(0.001, 10);  // threshold, generations

ga_opt->set_parameter_ranges(ranges);
ga_opt->set_optimization_target("sharpe"_kj);
ga_opt->set_max_iterations(100);  // generations

optimizer->optimize(strategy);
```

**Pros:**
- Good for complex, non-convex landscapes
- Can escape local optima
- Parallelizable

**Cons:**
- Many hyperparameters to tune
- May converge slowly
- Results vary between runs

### Optimization Comparison

| Algorithm | Best For | Sample Efficiency | Complexity |
|-----------|----------|-------------------|------------|
| Grid Search | Small spaces (< 4 params) | Low | O(n^d) |
| Random Search | Medium spaces (4-10 params) | Medium | O(n) |
| Bayesian | Expensive evaluations | High | O(n^3) per iteration |
| Genetic | Complex landscapes | Medium | O(pop * gen) |

---

## Risk Control Integration

### Strategy-Level Risk Configuration

```json
{
  "name": "sma_crossover_btc",
  "type": "SmaCrossoverStrategy",
  "risk_per_trade": 0.02,
  "max_position_size": 0.1,
  "stop_loss": 0.05,
  "take_profit": 0.10,
  "symbols": ["BTCUSDT"],
  "parameters": {
    "short_period": 10,
    "long_period": 20,
    "position_size": 0.01
  }
}
```

### Risk Limit Types

| Limit Type | Description | Example |
|------------|-------------|---------|
| **Position Size** | Max position per symbol | 0.1 BTC |
| **Risk Per Trade** | Max loss per trade | 2% of capital |
| **Stop Loss** | Automatic exit on loss | 5% below entry |
| **Take Profit** | Automatic exit on profit | 10% above entry |
| **Max Drawdown** | Strategy shutdown trigger | 10% drawdown |
| **Daily Loss** | Daily loss limit | $1,000 |

### Implementing Stop-Loss in Strategy

```cpp
void SmaCrossoverStrategy::check_stop_loss(double current_price) {
    if (!in_position_) return;

    double pnl_pct = 0.0;
    if (position_side_ == exec::OrderSide::Buy) {
        pnl_pct = (current_price - entry_price_) / entry_price_;
    } else {
        pnl_pct = (entry_price_ - current_price) / entry_price_;
    }

    // Check stop-loss
    if (pnl_pct < -config_.stop_loss) {
        generate_exit_signal(current_price, "stop_loss");
    }

    // Check take-profit
    if (pnl_pct > config_.take_profit) {
        generate_exit_signal(current_price, "take_profit");
    }
}
```

### Circuit Breaker Integration

Strategies can respond to circuit breaker events:

```cpp
void SmaCrossoverStrategy::on_circuit_breaker(bool triggered) {
    if (triggered) {
        // Stop generating new signals
        running_ = false;

        // Optionally close existing positions
        if (in_position_) {
            generate_exit_signal(last_price_, "circuit_breaker");
        }
    }
}
```

For detailed risk management configuration, see [Risk Management Guide](../guides/user/risk-management.md).

---

## Production Deployment

### Deployment Stages

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Deployment Stages                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Stage 1: Paper Trading                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ - Real market data, simulated execution                             │    │
│  │ - Validate signal generation                                        │    │
│  │ - Test risk controls                                                │    │
│  │ - Duration: 1-2 weeks                                               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  Stage 2: Testnet Trading                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ - Real API, test funds                                              │    │
│  │ - Validate order execution                                          │    │
│  │ - Test error handling                                               │    │
│  │ - Duration: 1-2 weeks                                               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  Stage 3: Live Trading (Minimal)                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ - Real API, real funds (minimal position size)                      │    │
│  │ - Validate live performance                                         │    │
│  │ - Monitor for issues                                                │    │
│  │ - Duration: 2-4 weeks                                               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  Stage 4: Production Scaling                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ - Gradually increase position size                                  │    │
│  │ - Full monitoring and alerting                                      │    │
│  │ - Ongoing performance review                                        │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Stage 1: Paper Trading

```bash
# Set paper trading mode
export VELOZ_EXECUTION_MODE=sim_engine
export VELOZ_MARKET_SOURCE=binance_ws

# Start with your strategy
./build/release/apps/engine/veloz_engine --port 8080
```

Load your strategy via the API:

```bash
curl -X POST http://127.0.0.1:8080/api/strategy/load \
  -H "Content-Type: application/json" \
  -d '{
    "name": "sma_crossover_paper",
    "type": "SmaCrossoverStrategy",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "short_period": 10,
      "long_period": 20,
      "position_size": 0.001
    }
  }'
```

### Stage 2: Testnet Trading

```bash
# Set testnet mode
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
export VELOZ_BINANCE_BASE_URL=https://testnet.binance.vision

# Start engine
./build/release/apps/engine/veloz_engine --port 8080
```

### Stage 3: Live Trading (Minimal)

```bash
# Set live trading mode
export VELOZ_EXECUTION_MODE=binance_spot
export VELOZ_BINANCE_API_KEY=your_live_api_key
export VELOZ_BINANCE_API_SECRET=your_live_api_secret

# Start engine
./build/release/apps/engine/veloz_engine --port 8080
```

Load strategy with conservative parameters:

```bash
curl -X POST http://127.0.0.1:8080/api/strategy/load \
  -H "Content-Type: application/json" \
  -d '{
    "name": "sma_crossover_prod",
    "type": "SmaCrossoverStrategy",
    "symbols": ["BTCUSDT"],
    "risk_per_trade": 0.01,
    "max_position_size": 0.01,
    "parameters": {
      "short_period": 10,
      "long_period": 20,
      "position_size": 0.001
    }
  }'
```

### Deployment Checklist

| Stage | Checklist Item | Status |
|-------|----------------|--------|
| **Pre-Deploy** | Unit tests pass | [ ] |
| | Integration tests pass | [ ] |
| | Backtest results acceptable | [ ] |
| | Risk limits configured | [ ] |
| | Monitoring configured | [ ] |
| **Paper Trading** | Signals generated correctly | [ ] |
| | Risk controls working | [ ] |
| | No unexpected errors | [ ] |
| **Testnet** | Orders execute correctly | [ ] |
| | Fills processed correctly | [ ] |
| | Error handling works | [ ] |
| **Live (Minimal)** | Live performance matches backtest | [ ] |
| | No slippage issues | [ ] |
| | Latency acceptable | [ ] |
| **Production** | Scaling plan approved | [ ] |
| | Alerting configured | [ ] |
| | Runbook documented | [ ] |

---

## Monitoring and Alerting

### Strategy-Specific Metrics

VeloZ exposes strategy metrics via Prometheus:

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_strategy_events_processed` | Counter | Events processed by strategy |
| `veloz_strategy_signals_generated` | Counter | Signals generated |
| `veloz_strategy_execution_time_ns` | Histogram | Signal generation latency |
| `veloz_strategy_errors` | Counter | Errors encountered |
| `veloz_strategy_pnl` | Gauge | Current PnL |
| `veloz_strategy_drawdown` | Gauge | Current drawdown |
| `veloz_strategy_win_rate` | Gauge | Win rate percentage |

### Grafana Dashboard Queries

```promql
# Signals per minute
rate(veloz_strategy_signals_generated{strategy="sma_crossover"}[1m]) * 60

# Average execution time (microseconds)
rate(veloz_strategy_execution_time_ns{strategy="sma_crossover"}[5m])
  / rate(veloz_strategy_events_processed{strategy="sma_crossover"}[5m]) / 1000

# Error rate
rate(veloz_strategy_errors{strategy="sma_crossover"}[5m])

# Current drawdown
veloz_strategy_drawdown{strategy="sma_crossover"}
```

### Alert Configuration

```yaml
# prometheus/alerts/strategy_alerts.yml
groups:
  - name: strategy_alerts
    rules:
      - alert: StrategyHighDrawdown
        expr: veloz_strategy_drawdown > 0.10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Strategy {{ $labels.strategy }} drawdown > 10%"
          description: "Current drawdown: {{ $value | humanizePercentage }}"

      - alert: StrategyCriticalDrawdown
        expr: veloz_strategy_drawdown > 0.20
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Strategy {{ $labels.strategy }} drawdown > 20%"
          description: "Immediate action required. Drawdown: {{ $value | humanizePercentage }}"

      - alert: StrategyNoSignals
        expr: rate(veloz_strategy_signals_generated[1h]) == 0
        for: 2h
        labels:
          severity: warning
        annotations:
          summary: "Strategy {{ $labels.strategy }} not generating signals"
          description: "No signals in the last 2 hours"

      - alert: StrategyHighErrorRate
        expr: rate(veloz_strategy_errors[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Strategy {{ $labels.strategy }} high error rate"
          description: "Error rate: {{ $value | humanize }}/s"

      - alert: StrategyLowWinRate
        expr: veloz_strategy_win_rate < 0.30 and veloz_strategy_trade_count > 100
        for: 30m
        labels:
          severity: warning
        annotations:
          summary: "Strategy {{ $labels.strategy }} low win rate"
          description: "Win rate: {{ $value | humanizePercentage }}"
```

### Real-Time Monitoring Script

```python
#!/usr/bin/env python3
"""Monitor strategy performance in real-time."""

import json
import urllib.request
from datetime import datetime

def monitor_strategy(strategy_id, base_url="http://127.0.0.1:8080"):
    """Monitor a specific strategy."""
    url = f"{base_url}/api/strategy/{strategy_id}/state"

    print(f"Monitoring strategy: {strategy_id}")
    print("-" * 60)

    while True:
        try:
            with urllib.request.urlopen(url) as response:
                data = json.loads(response.read())

            timestamp = datetime.now().strftime("%H:%M:%S")
            status = data.get("status", "unknown")
            pnl = data.get("pnl", 0)
            drawdown = data.get("max_drawdown", 0)
            trades = data.get("trade_count", 0)
            win_rate = data.get("win_rate", 0)

            print(f"[{timestamp}] Status: {status} | "
                  f"PnL: ${pnl:.2f} | "
                  f"DD: {drawdown*100:.1f}% | "
                  f"Trades: {trades} | "
                  f"WR: {win_rate*100:.1f}%")

            import time
            time.sleep(10)

        except Exception as e:
            print(f"Error: {e}")
            import time
            time.sleep(5)

if __name__ == "__main__":
    import sys
    strategy_id = sys.argv[1] if len(sys.argv) > 1 else "sma_crossover_1"
    monitor_strategy(strategy_id)
```

For comprehensive monitoring setup, see [Monitoring Guide](../guides/user/monitoring.md).

---

## Troubleshooting

### Common Issues

#### Issue: Strategy not generating signals

**Symptom:** No signals after processing many events

**Diagnosis:**
```cpp
// Add debug logging
KJ_IF_SOME(l, logger_) {
    l.debug(kj::str("SMA short=", short_sma_, " long=", long_sma_,
                    " ready=", indicators_ready_).cStr());
}
```

**Solutions:**
1. Verify indicators are ready (enough data)
2. Check signal conditions are being met
3. Verify strategy is running (`running_ == true`)

#### Issue: Compilation errors with KJ types

**Symptom:** "no matching function" or "cannot convert"

**Solutions:**
- Use `kj::str()` for string concatenation
- Use `kj::mv()` for move semantics
- Use `KJ_IF_SOME` for Maybe types

```cpp
// Correct usage
kj::String msg = kj::str("Price: ", price, " Signal: ", signal);

// Move ownership
signals_.add(kj::mv(order));

// Handle Maybe types
KJ_IF_SOME(val, maybe_value) {
    // Safe to use val
}
```

#### Issue: Tests fail with segmentation fault

**Symptom:** Test crashes without error message

**Solutions:**
1. Check for null pointer access
2. Use ASan build for debugging

```bash
# Build with AddressSanitizer
cmake --preset asan
cmake --build --preset asan-tests -j$(nproc)

# Run tests
./build/asan/libs/strategy/veloz_strategy_tests
```

#### Issue: Strategy rejected by risk engine

**Symptom:** Orders rejected with "position_limit_exceeded"

**Solutions:**
1. Reduce position size in strategy parameters
2. Increase limits in risk configuration
3. Check current exposure levels

```bash
# Check current exposure
curl http://127.0.0.1:8080/api/risk/exposure

# Check risk limits
curl http://127.0.0.1:8080/api/risk/limits
```

#### Issue: Backtest results don't match live performance

**Symptom:** Live performance significantly worse than backtest

**Causes:**
1. **Overfitting**: Strategy optimized too closely to historical data
2. **Slippage**: Live execution has more slippage than modeled
3. **Latency**: Signal generation too slow for live markets
4. **Market impact**: Orders moving the market

**Solutions:**
1. Use walk-forward analysis
2. Increase slippage assumptions in backtest
3. Optimize signal generation latency
4. Reduce position sizes

#### Issue: Hot-reload not working

**Symptom:** Parameter updates not taking effect

**Diagnosis:**
```bash
# Check if strategy supports hot-reload
curl http://127.0.0.1:8080/api/strategy/{id}/info
```

**Solutions:**
1. Verify `supports_hot_reload()` returns true
2. Check `update_parameters()` implementation
3. Restart strategy if hot-reload not supported

### Getting Help

- Check [Operations Runbook](../guides/deployment/operations_runbook.md)
- Review [Risk Management Guide](../guides/user/risk-management.md)
- Report issues on GitHub

---

## Summary

**What you accomplished:**

1. Understood the VeloZ strategy development lifecycle
2. Learned the strategy architecture and interfaces
3. Used strategy templates for different use cases
4. Implemented a custom SMA crossover strategy
5. Wrote comprehensive unit tests
6. Learned backtesting best practices
7. Explored parameter optimization methods
8. Integrated with risk management
9. Deployed through paper, testnet, and live stages
10. Set up monitoring and alerting

## Next Steps

- [Risk Management Guide](../guides/user/risk-management.md) - Configure risk controls
- [Monitoring Guide](../guides/user/monitoring.md) - Monitor strategy performance
- [Operations Runbook](../guides/deployment/operations_runbook.md) - Manage strategies in production
- [API Reference](../api/http_api.md) - Full API documentation

## Related Documentation

- [Strategy Interface](../../libs/strategy/include/veloz/strategy/strategy.h) - Source code
- [Advanced Strategies](../../libs/strategy/include/veloz/strategy/advanced_strategies.h) - Technical indicators
- [Backtest Engine](../../libs/backtest/include/veloz/backtest/backtest_engine.h) - Backtesting
- [Optimizer](../../libs/backtest/include/veloz/backtest/optimizer.h) - Parameter optimization
- [Glossary](../guides/user/glossary.md) - Definitions of trading and technical terms
