# Custom Strategy Development

**Time:** 60 minutes | **Level:** Advanced

## What You'll Learn

- Understand the VeloZ strategy architecture and interfaces
- Implement a custom trading strategy in C++
- Generate trading signals based on market data
- Integrate with the order management and risk systems
- Write unit tests using the KJ Test framework
- Backtest your strategy with historical data
- Deploy your strategy to paper trading and live trading

## Prerequisites

- Completed [Production Deployment](./production-deployment.md) tutorial
- C++ development experience (C++20 or later)
- Familiarity with KJ library types (see [KJ Library Guide](../../.claude/skills/kj-library/library_usage_guide.md))
- VeloZ development environment configured

---

## Step 1: Understand the Strategy Architecture

VeloZ strategies implement the `IStrategy` interface defined in `libs/strategy/include/veloz/strategy/strategy.h`. The key components are:

- **IStrategy**: Base interface for all strategies
- **BaseStrategy**: Provides common functionality (state management, metrics)
- **StrategyManager**: Manages strategy lifecycle and event routing
- **StrategyFactory**: Creates strategy instances from configuration

Review the interface:

```bash
# View the strategy interface
cat libs/strategy/include/veloz/strategy/strategy.h | head -100
```

Key methods to implement:

| Method | Purpose |
|--------|---------|
| `on_event()` | Process market data events |
| `get_signals()` | Return pending order requests |
| `on_position_update()` | Handle position changes |
| `on_start()` / `on_stop()` | Lifecycle management |

---

## Step 2: Create the Strategy Header

Create a new strategy that implements a simple moving average crossover.

```bash
# Create header file
nano libs/strategy/include/veloz/strategy/sma_crossover_strategy.h
```

Add the following code:

```cpp
// Strategy header: libs/strategy/include/veloz/strategy/sma_crossover_strategy.h
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

---

## Step 3: Implement the Strategy Logic

Create the implementation file.

```bash
# Create implementation file
nano libs/strategy/src/sma_crossover_strategy.cpp
```

Add the following code:

```cpp
// Strategy implementation: libs/strategy/src/sma_crossover_strategy.cpp
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

---

## Step 4: Integrate with Risk Management

Strategies integrate with the risk engine through the `on_order_rejected` callback. Add risk-aware behavior:

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

Configure risk limits in your strategy configuration:

```json
{
  "name": "sma_crossover_btc",
  "type": "Custom",
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

---

## Step 5: Write Unit Tests

Create tests using the KJ Test framework.

```bash
# Create test file
nano libs/strategy/tests/test_sma_crossover_strategy.cpp
```

Add the following tests:

```cpp
// Test file: libs/strategy/tests/test_sma_crossover_strategy.cpp
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
    // Downtrend: prices falling
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

Run the tests:

```bash
# Build tests
cmake --preset dev
cmake --build --preset dev-tests -j$(nproc)

# Run strategy tests
./build/dev/libs/strategy/veloz_strategy_tests --gtest_filter="*SmaCrossover*"
```

**Expected Output:**
```
[==========] 6 tests from 1 test suite ran.
[  PASSED  ] 6 tests.
```

---

## Step 6: Register the Strategy Factory

Register your strategy with the StrategyManager.

```cpp
// In your engine initialization code
#include "veloz/strategy/sma_crossover_strategy.h"

void register_strategies(StrategyManager& manager) {
    // Register built-in strategies
    manager.register_strategy_factory(
        kj::rc<MomentumStrategyFactory>());

    // Register custom strategy
    manager.register_strategy_factory(
        kj::rc<SmaCrossoverStrategyFactory>());
}
```

---

## Step 7: Backtest Your Strategy

Run backtests using historical data.

```bash
# Create backtest configuration
nano config/backtest_sma.json
```

Add:

```json
{
  "strategy": {
    "name": "sma_crossover_backtest",
    "type": "Custom",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "short_period": 10,
      "long_period": 20,
      "position_size": 0.01
    }
  },
  "backtest": {
    "start_date": "2025-01-01",
    "end_date": "2025-12-31",
    "initial_capital": 10000.0,
    "data_source": "binance_klines",
    "interval": "1h"
  }
}
```

Run the backtest:

```bash
# Run backtest
./build/dev/apps/engine/veloz_engine --backtest config/backtest_sma.json
```

**Expected Output:**
```json
{
  "total_trades": 156,
  "win_rate": 0.52,
  "profit_factor": 1.35,
  "max_drawdown": 0.08,
  "sharpe_ratio": 1.2,
  "total_return": 0.24
}
```

---

## Step 8: Deploy to Paper Trading

Test your strategy with live market data but simulated execution.

```bash
# Set paper trading mode
export VELOZ_EXECUTION_MODE=sim_engine
export VELOZ_MARKET_SOURCE=binance_ws

# Start with your strategy
./build/release/apps/engine/veloz_engine --port 8080
```

Load your strategy via the API:

```bash
# Load strategy
curl -X POST http://127.0.0.1:8080/api/strategy/load \
  -H "Content-Type: application/json" \
  -d '{
    "name": "sma_crossover_live",
    "type": "SmaCrossoverStrategy",
    "symbols": ["BTCUSDT"],
    "parameters": {
      "short_period": 10,
      "long_period": 20,
      "position_size": 0.001
    }
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "strategy_id": "sma_crossover_live_1"
}
```

Monitor strategy performance:

```bash
# Get strategy state
curl http://127.0.0.1:8080/api/strategy/sma_crossover_live_1/state
```

---

## Step 9: Deploy to Live Trading

After successful paper trading validation, deploy to live trading.

> **Warning:** Live trading involves real financial risk. Start with minimal position sizes.

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
# Load strategy with reduced position size
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

---

## Summary

**What you accomplished:**
- Understood the VeloZ strategy architecture
- Implemented a custom SMA crossover strategy in C++
- Integrated with order management and risk systems
- Wrote comprehensive unit tests with KJ Test
- Backtested the strategy with historical data
- Deployed to paper trading for validation
- Prepared for live trading deployment

## Troubleshooting

### Issue: Strategy not generating signals
**Symptom:** No signals after processing many events
**Solution:** Verify indicators are ready:
```cpp
// Add debug logging
KJ_IF_SOME(l, logger_) {
    l.debug(kj::str("SMA short=", short_sma_, " long=", long_sma_).cStr());
}
```

### Issue: Compilation errors with KJ types
**Symptom:** "no matching function" or "cannot convert"
**Solution:** Ensure you use KJ types correctly:
- Use `kj::str()` for string concatenation
- Use `kj::mv()` for move semantics
- Use `KJ_IF_SOME` for Maybe types

### Issue: Tests fail with segmentation fault
**Symptom:** Test crashes without error message
**Solution:** Check for null pointer access:
```cpp
// Always check Maybe types
KJ_IF_SOME(val, maybe_value) {
    // Safe to use val
}
```

### Issue: Strategy rejected by risk engine
**Symptom:** Orders rejected with "position_limit_exceeded"
**Solution:** Reduce position size or increase limits in configuration.

## Next Steps

- [Monitoring Guide](../guides/deployment/monitoring.md) - Monitor strategy performance
- [Operations Runbook](../guides/deployment/operations_runbook.md) - Manage strategies in production
- [API Reference](../api/http_api.md) - Full API documentation for strategy management
