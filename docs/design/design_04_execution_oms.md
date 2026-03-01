# VeloZ Crypto Quant Trading Framework: Execution and OMS

## 3.2 High-Performance Execution (Execution / OMS)

### 3.2.1 Scope

- Place/cancel/modify orders (mapped by exchange capability)
- Account, position, balance sync (periodic REST + WS incremental)
- Order state machine:
  - New → Accepted → PartiallyFilled → Filled / Canceled / Rejected / Expired
  - Handle async receipts and out-of-order updates
- Routing:
  - Separate spot vs derivatives routes
  - Multi-exchange best execution/slicing (later iteration)
- Risk pre-check:
  - Available funds, max leverage, max position, max order rate, max order size
  - Price protection (deviation/slippage caps/market protection)
  - Kill Switch: one-click stop and forced protection mode

### 3.2.2 Low-Latency Critical Path

Optimize the “market → strategy → order → receipt” path:

- Single-thread event loop + lock-free queues (reduce contention)
- Pre-allocated object pools (reduce GC/alloc)
- Batch sending and cancel merging when supported by exchanges
- Network layer:
  - Long-lived connections
  - TLS session reuse
  - Close-to-exchange deployment

### 3.2.3 Idempotency and Consistency

- clientOrderId: generated internally and unique for idempotency and traceability
- Order Journal: write-ahead log (local WAL or DB) for order state recovery
- Account reconciliation:
  - periodic full pull (REST) + realtime incremental (WS)
  - inconsistencies trigger “freeze strategy + auto repair/manual intervention”

### 3.2.4 Trading Interface (Recommended)

Expose a unified interface to strategies:

- place_order(symbol, side, type, qty, price?, tif?, reduce_only?, post_only?, client_order_id?)
- cancel_order(order_id or client_order_id)
- cancel_all(symbol?)
- amend_order(order_id, qty?, price?)
- get_position(symbol)
- get_account()

Strategies only depend on these interfaces, not exchange SDKs.

### 3.2.5 Implementation Details (Recommended)

- Order state machine: explicit transitions, reject invalid state changes, track last update timestamp
- Idempotency: clientOrderId must be unique and stored with every exchange receipt
- Routing: route by venue/market type, enforce capability constraints (IOC/FOK/GTX)
- Receipt ordering: accept out-of-order updates and reconcile via sequence/time
- WAL: persist every order and receipt before applying to in-memory state

### 3.2.6 Advanced Risk Components (Implemented)

#### Portfolio Risk Management (`libs/risk/portfolio_risk.h`)
Portfolio-level risk monitoring and management:
- Correlation tracking between positions across multiple symbols
- Portfolio-level leverage and margin usage calculation
- Multi-asset position netting and hedging recognition
- Portfolio VaR (Value at Risk) aggregation
- Cross-exchange position tracking

#### VaR Models (`libs/risk/var_models.h`)
Value at Risk calculation engines:
- Historical simulation VaR
- Parametric VaR (variance-covariance method)
- Monte Carlo simulation VaR
- Conditional VaR (Expected Shortfall)
- Confidence intervals: 95%, 99%, 99.9%
- Time horizons: 1-day, 10-day scaling

#### Stress Testing Framework (`libs/risk/stress_testing.h`)
Scenario-based stress testing for risk assessment:
- Historical scenario replay (e.g., 2020 COVID crash, 2022 crypto winter)
- Hypothetical stress scenarios (extreme volatility, liquidity crisis)
- Sensitivity analysis (Greeks for derivatives)
- Reverse stress testing (find breaking point scenarios)
- Automated stress test scheduling and reporting

#### Scenario Analysis (`libs/risk/scenario_analysis.h`)
What-if analysis for trading decisions:
- Position impact analysis under user-defined scenarios
- P&L distribution under various market conditions
- Margin requirement projections
- Liquidation risk assessment

#### Dynamic Thresholds (`libs/risk/dynamic_threshold.h`)
Adaptive risk thresholds based on market conditions:
- Volatility-adjusted position limits
- Market regime detection (trending, ranging, volatile)
- Dynamic leverage adjustment
- Real-time threshold calibration

#### Circuit Breaker Enhancements (`libs/risk/circuit_breaker.h`)
Multi-level circuit breaker with granular control:
- Per-strategy circuit breakers
- Per-symbol circuit breakers
- Portfolio-level circuit breakers
- Graduated response (warn, throttle, halt)
- Automatic recovery with cooldown periods
