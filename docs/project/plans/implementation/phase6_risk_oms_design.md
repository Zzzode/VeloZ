# Phase 6: Risk/OMS Design Document

## Overview

This document provides detailed design specifications for Phase 6 tasks:
- Task #26: P6-001 - Complete Position Calculation Logic
- Task #27: P6-002 - Enhance Risk Metrics
- Task #28: P6-003 - Implement Dynamic Risk Control Thresholds
- Task #29: P6-004 - Implement Risk Rule Engine

## 1. Position Calculation Logic (Task #26)

### 1.1 Current State Analysis

The existing `Position` class in `libs/oms/position.h` provides:
- Basic position tracking (size, avg_price, side)
- Weighted average cost calculation
- Realized/unrealized PnL calculation
- `apply_fill()` for processing order fills

**Gaps identified:**
- No FIFO cost basis option
- No multi-symbol position manager
- No position snapshot/history
- No direct integration with `ExecutionReport`

### 1.2 Proposed Enhancements

#### 1.2.1 Cost Basis Methods

```cpp
enum class CostBasisMethod : uint8_t {
  WeightedAverage = 0,  // Current implementation
  FIFO = 1,             // First-In-First-Out
  LIFO = 2,             // Last-In-First-Out (future)
};
```

#### 1.2.2 Position Lot Tracking (for FIFO/LIFO)

```cpp
struct PositionLot {
  double quantity;
  double price;
  int64_t timestamp_ns;
  kj::String order_id;  // Originating order
};
```

#### 1.2.3 Enhanced Position Class

```cpp
class Position final {
public:
  // Existing interface preserved

  // New: Cost basis configuration
  void set_cost_basis_method(CostBasisMethod method);
  [[nodiscard]] CostBasisMethod cost_basis_method() const;

  // New: Apply fill from ExecutionReport directly
  void apply_execution_report(const veloz::exec::ExecutionReport& report,
                              veloz::exec::OrderSide side);

  // New: Position lot access (for FIFO/LIFO)
  [[nodiscard]] const kj::Vector<PositionLot>& lots() const;

  // New: Position snapshot
  struct Snapshot {
    double size;
    double avg_price;
    double realized_pnl;
    double unrealized_pnl;
    int64_t timestamp_ns;
  };
  [[nodiscard]] Snapshot snapshot(double current_price) const;

private:
  CostBasisMethod cost_basis_method_{CostBasisMethod::WeightedAverage};
  kj::Vector<PositionLot> lots_;  // For FIFO/LIFO tracking
};
```

#### 1.2.4 Position Manager

**Thread Safety Note (per architect review):** PositionManager is accessed from multiple
contexts (execution callbacks, HTTP queries). Uses `kj::MutexGuarded` for thread safety.

```cpp
class PositionManager final {
public:
  // Position access
  [[nodiscard]] kj::Maybe<Position&> get_position(kj::StringPtr symbol);
  [[nodiscard]] Position& get_or_create_position(const veloz::common::SymbolId& symbol);

  // Bulk operations
  void apply_execution_report(const veloz::exec::ExecutionReport& report,
                              veloz::exec::OrderSide side);

  // Aggregation
  [[nodiscard]] double total_unrealized_pnl(
      const kj::HashMap<kj::String, double>& prices) const;
  [[nodiscard]] double total_realized_pnl() const;
  [[nodiscard]] double total_notional(
      const kj::HashMap<kj::String, double>& prices) const;

  // Iteration
  void for_each_position(kj::Function<void(const Position&)> callback) const;

  // Reconciliation
  void reconcile_with_exchange(const kj::Vector<Position>& exchange_positions);

  // Callbacks for position updates
  using PositionUpdateCallback = kj::Function<void(const Position&)>;
  void set_position_update_callback(PositionUpdateCallback callback);

private:
  // Thread-safe state (architect recommendation)
  struct State {
    kj::HashMap<kj::String, Position> positions;
    kj::Maybe<PositionUpdateCallback> on_position_update;
  };
  kj::MutexGuarded<State> state_;

  // Callbacks for position updates
  using PositionUpdateCallback = kj::Function<void(const Position&)>;
  void set_position_update_callback(PositionUpdateCallback callback);

private:
  kj::HashMap<kj::String, Position> positions_;
  kj::Maybe<PositionUpdateCallback> on_position_update_;
};
```

### 1.3 Integration with Strategy Runtime

The `PositionManager` integrates with `StrategyManager` via callbacks:

```
ExecutionReport -> PositionManager::apply_execution_report()
                        |
                        v
              Position::apply_fill()
                        |
                        v
              on_position_update_ callback
                        |
                        v
              StrategyManager::on_position_update()
                        |
                        v
              IStrategy::on_position_update()
```

---

## 2. Enhanced Risk Metrics (Task #27)

### 2.1 Current State Analysis

The existing `RiskMetrics` struct provides:
- VaR (95%, 99%) using historical simulation
- Max drawdown
- Sharpe ratio
- Trade statistics (win rate, profit factor)

**Gaps identified:**
- No exposure metrics (gross/net)
- No concentration analysis
- No correlation analysis
- VaR uses per-trade returns, not daily aggregation
- No real-time streaming metrics

### 2.2 Proposed Enhancements

#### 2.2.1 Exposure Metrics

```cpp
struct ExposureMetrics {
  double gross_exposure;      // Sum of absolute position values
  double net_exposure;        // Sum of signed position values
  double long_exposure;       // Sum of long position values
  double short_exposure;      // Sum of short position values
  double leverage_ratio;      // gross_exposure / account_equity
  double net_leverage_ratio;  // net_exposure / account_equity
};
```

#### 2.2.2 Concentration Metrics

```cpp
struct ConcentrationMetrics {
  kj::String largest_position_symbol;
  double largest_position_pct;     // As % of total exposure
  double top3_concentration_pct;   // Top 3 positions as % of total
  double herfindahl_index;         // HHI for concentration
  int position_count;
};
```

#### 2.2.3 Enhanced RiskMetrics

```cpp
struct RiskMetrics {
  // Existing fields preserved
  double var_95{0.0};
  double var_99{0.0};
  double max_drawdown{0.0};
  double sharpe_ratio{0.0};
  // ... other existing fields

  // New: Exposure metrics
  ExposureMetrics exposure;

  // New: Concentration metrics
  ConcentrationMetrics concentration;

  // New: Correlation metrics
  double avg_correlation{0.0};      // Average pairwise correlation
  double max_correlation{0.0};      // Maximum pairwise correlation

  // New: Drawdown metrics
  double current_drawdown{0.0};     // Current drawdown from peak
  int64_t drawdown_start_ns{0};     // When current drawdown started
  int64_t max_drawdown_duration_ns{0}; // Longest drawdown duration

  // New: Risk-adjusted returns
  double sortino_ratio{0.0};        // Downside risk-adjusted return
  double calmar_ratio{0.0};         // Return / max drawdown
};
```

#### 2.2.4 Real-time Metrics Calculator

```cpp
class RealTimeRiskMetrics final {
public:
  // Update with new data
  void on_position_update(const veloz::oms::Position& position, double price);
  void on_price_update(kj::StringPtr symbol, double price);
  void on_trade_complete(const TradeHistory& trade);

  // Get current metrics
  [[nodiscard]] ExposureMetrics get_exposure_metrics() const;
  [[nodiscard]] ConcentrationMetrics get_concentration_metrics() const;
  [[nodiscard]] double get_current_drawdown() const;

  // Configuration
  void set_account_equity(double equity);
  void set_correlation_window(int days);

private:
  kj::HashMap<kj::String, double> position_values_;
  double account_equity_{0.0};
  double peak_equity_{0.0};
  // Rolling windows for correlation calculation
};
```

---

## 3. Dynamic Risk Control Thresholds (Task #28)

### 3.1 Current State Analysis

The existing `RiskEngine` uses static thresholds:
- `max_position_size_`
- `max_leverage_`
- `max_price_deviation_`
- `max_order_rate_`
- `stop_loss_percentage_`

**Gaps identified:**
- No volatility-based adjustment
- No market condition detection
- No drawdown-based reduction
- No time-of-day adjustments

### 3.2 Proposed Design

#### 3.2.1 Market Condition Detection

**Data Source (per architect review):** MarketConditionState is provided by MarketDataManager,
which calculates volatility/liquidity metrics from market data. Wired via callback in EngineApp:
```cpp
// In EngineApp setup
marketData_->set_condition_callback([this](const MarketConditionState& state) {
    riskEngine_.update_market_condition(state);
});
```

```cpp
enum class MarketCondition : uint8_t {
  Normal = 0,
  HighVolatility = 1,
  LowLiquidity = 2,
  Trending = 3,
  MeanReverting = 4,
  Crisis = 5,
};

struct MarketConditionState {
  MarketCondition condition;
  double volatility_percentile;  // Current vol vs historical (0-100)
  double liquidity_score;        // 0-1, based on spread and depth
  double trend_strength;         // ADX or similar
  int64_t last_update_ns;
};
```

#### 3.2.2 Dynamic Threshold Controller

```cpp
class DynamicThresholdController final {
public:
  // Configuration
  struct Config {
    // Base thresholds (used in Normal conditions)
    double base_max_position_size;
    double base_max_leverage;
    double base_stop_loss_pct;

    // Volatility adjustment
    double vol_scale_factor;      // How much to scale with volatility
    double vol_lookback_days;     // Lookback for volatility calculation

    // Drawdown adjustment
    double drawdown_reduction_start;  // Start reducing at this drawdown %
    double drawdown_reduction_rate;   // Reduction per % of drawdown

    // Time-based adjustment
    bool reduce_before_close;     // Reduce exposure before market close
    int minutes_before_close;
  };

  void set_config(Config config);

  // Update market state
  void update_market_condition(const MarketConditionState& state);
  void update_current_drawdown(double drawdown_pct);
  void update_time_to_close(int minutes);

  // Get adjusted thresholds
  [[nodiscard]] double get_max_position_size() const;
  [[nodiscard]] double get_max_leverage() const;
  [[nodiscard]] double get_stop_loss_pct() const;
  [[nodiscard]] double get_position_size_multiplier() const;

  // Explain current adjustments
  [[nodiscard]] kj::String explain_adjustments() const;

private:
  Config config_;
  MarketConditionState market_state_;
  double current_drawdown_{0.0};
  int minutes_to_close_{-1};

  double calculate_volatility_adjustment() const;
  double calculate_drawdown_adjustment() const;
  double calculate_time_adjustment() const;
};
```

#### 3.2.3 Integration with RiskEngine

```cpp
class RiskEngine final {
public:
  // New: Set dynamic threshold controller
  void set_dynamic_threshold_controller(
      kj::Own<DynamicThresholdController> controller);

  // Modified: check_pre_trade uses dynamic thresholds
  [[nodiscard]] RiskCheckResult check_pre_trade(
      const veloz::exec::PlaceOrderRequest& req);

private:
  kj::Maybe<kj::Own<DynamicThresholdController>> dynamic_controller_;

  // Helper to get effective threshold
  double get_effective_max_position_size() const;
  double get_effective_max_leverage() const;
};
```

---

## 4. Risk Rule Engine (Task #29)

### 4.1 Design Goals

- Declarative rule definition
- Composable conditions
- Hot-reload capability
- Audit trail for rule evaluations

### 4.2 Rule Definition Language

#### 4.2.1 Rule Structure

```cpp
enum class RuleAction : uint8_t {
  Allow = 0,
  Reject = 1,
  Warn = 2,
  RequireApproval = 3,
};

enum class RuleConditionType : uint8_t {
  // Order conditions
  OrderSize = 0,
  OrderValue = 1,
  OrderPrice = 2,
  OrderSide = 3,

  // Position conditions
  PositionSize = 4,
  PositionValue = 5,
  PositionPnL = 6,

  // Account conditions
  AccountExposure = 7,
  AccountDrawdown = 8,
  AccountLeverage = 9,

  // Market conditions
  MarketVolatility = 10,
  MarketSpread = 11,

  // Time conditions
  TimeOfDay = 12,
  DayOfWeek = 13,

  // Composite
  And = 100,
  Or = 101,
  Not = 102,
};

enum class ComparisonOp : uint8_t {
  Equal = 0,
  NotEqual = 1,
  GreaterThan = 2,
  GreaterOrEqual = 3,
  LessThan = 4,
  LessOrEqual = 5,
  Between = 6,
};
```

#### 4.2.2 Rule Condition

```cpp
struct RuleCondition {
  RuleConditionType type;
  ComparisonOp op;
  double value;
  double value2;  // For Between operator
  kj::String symbol;  // Optional: symbol-specific condition

  // For composite conditions (And, Or, Not)
  kj::Vector<RuleCondition> children;
};
```

#### 4.2.3 Rule Definition

```cpp
struct RiskRule {
  kj::String id;
  kj::String name;
  kj::String description;
  int priority;  // Lower = higher priority, evaluated first
  bool enabled;

  RuleCondition condition;
  RuleAction action;
  kj::String rejection_reason;

  // Metadata
  int64_t created_at_ns;
  int64_t updated_at_ns;
  kj::String created_by;
};
```

### 4.3 Rule Engine Implementation

```cpp
class RiskRuleEngine final {
public:
  // Rule management
  void add_rule(RiskRule rule);
  void update_rule(kj::StringPtr rule_id, RiskRule rule);
  void remove_rule(kj::StringPtr rule_id);
  void enable_rule(kj::StringPtr rule_id);
  void disable_rule(kj::StringPtr rule_id);

  // Hot-reload with validation (per architect review)
  // Validates all rules before atomic swap; keeps old rules on validation failure
  void load_rules_from_config(kj::StringPtr config_path);
  void reload_rules();  // Atomic swap pattern with fallback

  // Evaluation context
  struct EvaluationContext {
    const veloz::exec::PlaceOrderRequest* order;
    const veloz::oms::Position* position;
    double account_equity;
    double account_drawdown;
    double market_volatility;
    double market_spread;
    int64_t current_time_ns;
  };

  // Evaluate rules
  struct EvaluationResult {
    RuleAction action;
    kj::String rule_id;
    kj::String rule_name;
    kj::String reason;
    int64_t evaluation_time_ns;
  };

  // Short-circuit evaluation: first matching rule wins (per architect review)
  // Rules are pre-sorted by priority (lower number = higher priority)
  // Returns first Reject/Allow match, or default Allow if no rules match
  [[nodiscard]] EvaluationResult evaluate(const EvaluationContext& ctx) const;

  // Evaluate all rules (for debugging/audit purposes)
  [[nodiscard]] kj::Vector<EvaluationResult> evaluate_all(
      const EvaluationContext& ctx) const;

  // Audit
  [[nodiscard]] kj::Vector<EvaluationResult> get_recent_evaluations(
      int count) const;
  void set_audit_callback(
      kj::Function<void(const EvaluationResult&)> callback);

private:
  kj::Vector<RiskRule> rules_;  // Sorted by priority
  kj::Vector<EvaluationResult> audit_log_;
  kj::Maybe<kj::Function<void(const EvaluationResult&)>> audit_callback_;

  bool evaluate_condition(const RuleCondition& cond,
                          const EvaluationContext& ctx) const;
};
```

### 4.4 JSON Configuration Format

```json
{
  "rules": [
    {
      "id": "max-order-size",
      "name": "Maximum Order Size",
      "description": "Reject orders larger than 10 BTC",
      "priority": 1,
      "enabled": true,
      "condition": {
        "type": "OrderSize",
        "op": "GreaterThan",
        "value": 10.0
      },
      "action": "Reject",
      "rejection_reason": "Order size exceeds maximum of 10 BTC"
    },
    {
      "id": "high-vol-position-limit",
      "name": "High Volatility Position Limit",
      "description": "Reduce position limit during high volatility",
      "priority": 2,
      "enabled": true,
      "condition": {
        "type": "And",
        "children": [
          {
            "type": "MarketVolatility",
            "op": "GreaterThan",
            "value": 0.8
          },
          {
            "type": "PositionSize",
            "op": "GreaterThan",
            "value": 5.0
          }
        ]
      },
      "action": "Reject",
      "rejection_reason": "Position limit reduced during high volatility"
    }
  ]
}
```

---

## 5. Interface Contracts

### 5.1 Risk <-> Strategy Interface

```cpp
// Strategy submits order request
// Risk engine validates before execution
class IRiskGate {
public:
  virtual ~IRiskGate() = default;

  // Pre-trade validation (single order)
  [[nodiscard]] virtual RiskCheckResult validate_order(
      const veloz::exec::PlaceOrderRequest& req) = 0;

  // Batch validation for strategy signal bursts (per architect suggestion)
  [[nodiscard]] virtual kj::Vector<RiskCheckResult> validate_orders(
      kj::ArrayPtr<const veloz::exec::PlaceOrderRequest> requests) = 0;

  // Post-trade notification
  virtual void on_fill(const veloz::exec::ExecutionReport& report) = 0;

  // Query risk state
  [[nodiscard]] virtual bool is_trading_allowed() const = 0;
  [[nodiscard]] virtual double get_available_buying_power() const = 0;
};
```

### 5.2 OMS <-> Risk Interface

```cpp
// Position manager notifies risk engine of position changes
class IPositionObserver {
public:
  virtual ~IPositionObserver() = default;

  virtual void on_position_opened(const veloz::oms::Position& position) = 0;
  virtual void on_position_updated(const veloz::oms::Position& position) = 0;
  virtual void on_position_closed(const veloz::oms::Position& position,
                                  double realized_pnl) = 0;
};
```

### 5.3 Risk <-> Engine Interface

```cpp
// Engine queries risk state for system health
class IRiskStateProvider {
public:
  virtual ~IRiskStateProvider() = default;

  [[nodiscard]] virtual RiskMetrics get_current_metrics() const = 0;
  [[nodiscard]] virtual kj::Vector<RiskAlert> get_active_alerts() const = 0;
  [[nodiscard]] virtual bool is_circuit_breaker_active() const = 0;
};
```

### 5.4 Centralized Risk Events (EventEmitter Integration)

```cpp
// Risk event types for centralized event streaming
enum class RiskEventType {
  OrderRejected,
  CircuitBreakerTripped,
  CircuitBreakerReset,
  ExposureLimitWarning,
  ExposureLimitBreached,
  PositionLimitWarning,
  PositionLimitBreached
};

struct RiskEvent {
  RiskEventType type;
  kj::String reason;
  kj::Maybe<kj::String> strategy_id;
  kj::Maybe<kj::String> symbol;
  int64_t timestamp_ns;
};

// Circuit breaker event for async notification
struct CircuitBreakerEvent {
  bool triggered;
  kj::String reason;
  int64_t timestamp_ns;
};

// RiskEngine callback for circuit breaker
using CircuitBreakerCallback = kj::Function<void(CircuitBreakerEvent)>;
```

---

## 6. Data Flow Diagram (Updated with Architect Guidance)

**Architectural Decisions (from architect review):**
- Pre-trade checks: **Synchronous** in order callback
- Circuit breaker: **Async via EventLoop** (Critical priority)
- Rejection events: **Centralized via EventEmitter**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              ENGINE                                      │
│                                                                         │
│  MarketData ──► EventLoop ──► StrategyRuntime                           │
│                                      │                                  │
│                                      ▼                                  │
│                              PlaceOrderRequest                          │
│                                      │                                  │
│                                      ▼                                  │
│                    ┌─────────────────────────────────┐                  │
│                    │      RiskEngine (sync)          │                  │
│                    │  - Position limits              │                  │
│                    │  - Exposure checks              │                  │
│                    │  - Rate limits                  │                  │
│                    └──────────────┬──────────────────┘                  │
│                                   │                                     │
│                         ┌─────────┴─────────┐                           │
│                         │                   │                           │
│                    [Allowed]           [Rejected]                       │
│                         │                   │                           │
│                         ▼                   ▼                           │
│                       OMS           EventEmitter                        │
│                         │           (rejection event)                   │
│                         ▼                                               │
│                    Execution                                            │
│                         │                                               │
│                         ▼                                               │
│                  ExecutionReport                                        │
│                         │                                               │
│                         ▼                                               │
│            ┌────────────────────────┐                                   │
│            │   RiskEngine (async)   │◄── Circuit breaker monitoring     │
│            │   - Post-trade checks  │                                   │
│            │   - P&L monitoring     │                                   │
│            └───────────┬────────────┘                                   │
│                        │                                                │
│              [Circuit breaker trip]                                     │
│                        │                                                │
│                        ▼                                                │
│              EventLoop.post(Critical)                                   │
│                        │                                                │
│                        ▼                                                │
│              StrategyRuntime.pause_all()                                │
│              EventEmitter.emit_circuit_breaker()                        │
└─────────────────────────────────────────────────────────────────────────┘
```

**Integration Code Pattern:**
```cpp
// In EngineApp - wire strategy signals through risk (synchronous)
strategyRuntime_->set_order_callback([this](const PlaceOrderRequest& req) {
    auto result = riskEngine_.check_pre_trade(req);
    if (result.allowed) {
        oms_.submit_order(req);
    } else {
        // Track rejection metrics for ALL origins (manual, external, algorithmic)
        riskEngine_.track_rejection(req.strategy_id);

        // Emit to EventEmitter (UI/logging)
        eventEmitter_.emit_order_rejected(req, result.reason);

        // Route to StrategyManager unconditionally
        // StrategyManager handles unknown IDs gracefully (find() returns none -> no-op)
        // This keeps RiskEngine simple - StrategyManager owns routing logic
        strategyRuntime_->on_order_rejected(req.strategy_id, req, result.reason);
    }
});

// Wire circuit breaker through EventLoop (async, critical priority)
riskEngine_.set_circuit_breaker_callback([this](CircuitBreakerEvent event) {
    eventLoop_.post([this, event]() {
        if (event.triggered) {
            strategyRuntime_->pause_all_strategies();
            eventEmitter_.emit_circuit_breaker(event);
        }
    }, EventPriority::Critical);
});
```

---

## 7. Dependencies

### 7.1 Task Dependencies

```
Task #11 (Binance Production) ─┐
                               ├──▶ Task #26 (Position Calc)
Task #14 (Reconciliation) ─────┘           │
                                           ▼
                                   Task #27 (Risk Metrics)
                                           │
                               ┌───────────┴───────────┐
                               ▼                       ▼
                       Task #28 (Dynamic)      Task #29 (Rules)
                               │                       │
                               └───────────┬───────────┘
                                           ▼
                                   Task #31 (UI Display)
```

### 7.2 Module Dependencies

- `libs/oms` depends on `libs/exec` (for OrderSide, ExecutionReport)
- `libs/risk` depends on `libs/oms` (for Position)
- `libs/risk` depends on `libs/exec` (for PlaceOrderRequest)
- `libs/strategy` depends on `libs/risk` (for IRiskGate)

---

## 8. Testing Strategy

### 8.1 Unit Tests

- Position calculation with FIFO/LIFO cost basis
- Risk metrics calculations (VaR, Sharpe, exposure)
- Dynamic threshold adjustments
- Rule condition evaluation

### 8.2 Integration Tests

- Strategy -> Risk -> Execution flow
- Position updates propagating to strategies
- Circuit breaker triggering and recovery
- Rule hot-reload

### 8.3 Performance Tests

- Risk check latency (target: < 100us)
- Position update throughput (target: > 10k/sec)
- Rule evaluation with 100+ rules

---

## 9. Implementation Order

1. **Task #26**: Position calculation enhancements
   - Add CostBasisMethod enum
   - Implement FIFO lot tracking
   - Create PositionManager class
   - Add ExecutionReport integration

2. **Task #27**: Risk metrics enhancements
   - Add ExposureMetrics struct
   - Add ConcentrationMetrics struct
   - Implement RealTimeRiskMetrics class
   - Add correlation calculation

3. **Task #28**: Dynamic thresholds
   - Implement MarketCondition detection
   - Create DynamicThresholdController
   - Integrate with RiskEngine

4. **Task #29**: Rule engine
   - Define rule data structures
   - Implement RiskRuleEngine
   - Add JSON configuration support
   - Implement hot-reload

---

## 10. Design Decisions (Approved 2026-02-20)

The following decisions were approved by team-lead-2:

1. **FIFO vs LIFO**:
   - **Decision**: Implement WeightedAverage as default, FIFO as secondary
   - LIFO deferred to future sprint if not immediately needed

2. **Correlation calculation**:
   - **Decision**: Use rolling window correlation (30-day) with exponential weighting for recent data
   - Keep implementation simple for Phase 6

3. **Rule conflict resolution**:
   - **Decision**: Priority-based (higher priority rules win)
   - Add explicit priority field to rule definition
   - Log conflicts for audit trail

4. **Hot-reload mechanism**:
   - **Decision**: Live reload preferred (no restart required)
   - Use atomic swap pattern: load new rules → validate → swap
   - Fallback to previous rules on validation failure

5. **strategy_id in PlaceOrderRequest** (Architect decision 2026-02-20):
   - **Decision**: Required `kj::String` field (not optional)
   - Reserved pseudo-strategy IDs for non-algorithmic orders:
     - `STRATEGY_ID_MANUAL` ("manual") - UI/CLI orders
     - `STRATEGY_ID_EXTERNAL` ("external") - API/third-party orders
     - `STRATEGY_ID_SYSTEM` ("system") - Internal hedging/liquidation
   - Benefits: Uniform tracking, no null checks, all orders controllable

6. **ExecutionReport strategy_id enrichment** (Architect decision 2026-02-20):
   - **Decision**: OMS enriches ExecutionReport at order submission time
   - Exchange adapters do NOT need strategy awareness
   - OMS stores strategy_id with order state, copies to ExecutionReport
   - Benefits: Exchange-agnostic, WAL-recoverable, single source of truth

---

*Document Version: 1.4*
*Author: Risk/OMS Developer*
*Created: 2026-02-20*
*Updated: 2026-02-20 - Design decisions approved by team-lead-2*
*Updated: 2026-02-20 - Architecture integration patterns added per architect review*
*Updated: 2026-02-20 - Architect review incorporated: MutexGuarded on PositionManager, rule short-circuit behavior, market condition source, batch validation*
*Updated: 2026-02-20 - Added strategy_id decisions: required field with reserved IDs, OMS enrichment pattern*
