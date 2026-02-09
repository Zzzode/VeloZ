# VeloZ Crypto Quant Trading Framework: Strategy Runtime

## 3.3 Strategy Runtime

### 3.3.1 Scope

- Strategy types:
  - Classic quant: trend/mean reversion/momentum/arbitrage
  - HFT: order-book driven, order flow, market making, microstructure
  - Grid: range grid, dynamic grid, capital management
- Multi-strategy parallelism and portfolio:
  - Shared market data with isolated risk budgets
  - Portfolio-level risk (total leverage, drawdown, correlation control)
- Lifecycle:
  - init → on_start → on_event → on_timer → on_stop → on_error
- Parameter management:
  - Hot updates (control plane)
  - Versioned parameters (reproducible experiments)

### 3.3.2 Unified Runtime Model: Backtest/Sim/Live

Introduce an Environment abstraction:

- LiveEnv: realtime market + live execution
- PaperEnv: realtime market + simulated matching (live rules)
- BacktestEnv: historical replay + simulated matching

Strategies only:

- subscribe to market data
- receive event callbacks
- call trading interfaces
- read current portfolio state (positions/funds/metrics)

### 3.3.3 Engineering Support for HFT

- Time-driven and event-driven coexistence: microsecond timers (language/runtime dependent)
- Shared memory/zero-copy: minimize copies from capture to strategy
- Fast-path risk checks in local memory with async reconciliation as needed

### 3.3.4 Implementation Details (Recommended)

- Strategy lifecycle is driven solely by event callbacks and timers
- Isolation: each strategy has its own risk budget and position view
- Deterministic replay: same event stream produces same strategy behavior
