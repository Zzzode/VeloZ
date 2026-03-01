# Phase 6 Risk/OMS Test Scenarios

This document defines comprehensive test scenarios for Phase 6 tasks to accelerate QA work when implementation begins.

## 1. Position Calculation Logic (Task #26)

### 1.1 Cost Basis Method Tests

#### WeightedAverage (Default)

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| POS-WA-001 | Single buy | Buy 1.0 @ 50000 | size=1.0, avg_price=50000 |
| POS-WA-002 | Multiple buys | Buy 1.0 @ 50000, Buy 0.5 @ 51000 | size=1.5, avg_price=50333.33 |
| POS-WA-003 | Partial close | Buy 1.0 @ 50000, Sell 0.3 @ 51000 | size=0.7, avg_price=50000, realized_pnl=300 |
| POS-WA-004 | Full close | Buy 1.0 @ 50000, Sell 1.0 @ 51000 | size=0.0, realized_pnl=1000 |
| POS-WA-005 | Flip long to short | Buy 1.0 @ 50000, Sell 1.5 @ 51000 | size=-0.5, realized_pnl=1000 |

#### FIFO Cost Basis

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| POS-FIFO-001 | Two lots, partial close | Buy 1.0 @ 50000, Buy 1.0 @ 51000, Sell 0.5 @ 52000 | lots=[{0.5, 50000}, {1.0, 51000}], realized_pnl=1000 |
| POS-FIFO-002 | Two lots, close first lot | Buy 1.0 @ 50000, Buy 1.0 @ 51000, Sell 1.0 @ 52000 | lots=[{1.0, 51000}], realized_pnl=2000 |
| POS-FIFO-003 | Three lots, close 1.5 | Buy 1.0 @ 50000, Buy 1.0 @ 51000, Buy 1.0 @ 52000, Sell 1.5 @ 53000 | lots=[{0.5, 51000}, {1.0, 52000}], realized_pnl=4000 |
| POS-FIFO-004 | Lot tracking accuracy | Multiple buys/sells | Verify lot_count matches expected |

### 1.2 PositionManager Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| PM-001 | Get non-existent position | Returns kj::none |
| PM-002 | Get or create position | Creates new position if not exists |
| PM-003 | Multi-symbol tracking | Track BTC, ETH, SOL independently |
| PM-004 | Total unrealized PnL | Sum of all positions' unrealized PnL |
| PM-005 | Total realized PnL | Sum of all positions' realized PnL |
| PM-006 | Position update callback | Callback fired on each fill |
| PM-007 | Reconciliation | Positions match exchange state after reconcile |

### 1.3 ExecutionReport Integration

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| ER-001 | Apply partial fill | Position updated with fill qty/price |
| ER-002 | Apply full fill | Position updated, order marked filled |
| ER-003 | Multiple fills same order | Cumulative position update |
| ER-004 | Fill timestamp tracking | Position tracks latest fill time |

### 1.4 Edge Cases

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| EDGE-001 | Zero quantity fill | No position change |
| EDGE-002 | Negative price | Reject or handle gracefully |
| EDGE-003 | Very small quantity (1e-9) | Handle floating point precision |
| EDGE-004 | Very large quantity | No overflow |
| EDGE-005 | Position reset | All values reset to zero |

---

## 2. Enhanced Risk Metrics (Task #27)

### 2.1 Exposure Metrics Tests

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| EXP-001 | Long only exposure | BTC +1.0 @ 50000 | gross=50000, net=50000, long=50000, short=0 |
| EXP-002 | Short only exposure | BTC -1.0 @ 50000 | gross=50000, net=-50000, long=0, short=50000 |
| EXP-003 | Mixed exposure | BTC +1.0 @ 50000, ETH -0.5 @ 2000 | gross=51000, net=49000 |
| EXP-004 | Leverage ratio | equity=10000, gross=50000 | leverage=5.0 |
| EXP-005 | Net leverage | equity=10000, net=49000 | net_leverage=4.9 |

### 2.2 Concentration Metrics Tests

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| CONC-001 | Single position | BTC 100% | largest_pct=100, hhi=10000 |
| CONC-002 | Two equal positions | BTC 50%, ETH 50% | largest_pct=50, hhi=5000 |
| CONC-003 | Three positions | BTC 60%, ETH 30%, SOL 10% | top3_pct=100, hhi=4600 |
| CONC-004 | Diversified (10 positions) | 10 x 10% each | hhi=1000 |
| CONC-005 | Position count | 5 positions | position_count=5 |

### 2.3 Correlation Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| CORR-001 | Perfect correlation | Two assets move identically | corr=1.0 |
| CORR-002 | Perfect negative correlation | Assets move opposite | corr=-1.0 |
| CORR-003 | No correlation | Random movements | corr≈0.0 |
| CORR-004 | Rolling window | 30-day window updates correctly |
| CORR-005 | Exponential weighting | Recent data weighted higher |

### 2.4 Real-time Metrics Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| RT-001 | Position update triggers recalc | Exposure metrics update |
| RT-002 | Price update triggers recalc | Unrealized PnL updates |
| RT-003 | Drawdown tracking | Current drawdown from peak |
| RT-004 | Peak equity tracking | Peak updates on new high |

---

## 3. Dynamic Risk Control Thresholds (Task #28)

### 3.1 Volatility Adjustment Tests

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| VOL-001 | Normal volatility | vol_percentile=50 | multiplier=1.0 |
| VOL-002 | High volatility | vol_percentile=90 | multiplier<1.0 (reduced) |
| VOL-003 | Low volatility | vol_percentile=10 | multiplier=1.0 (no increase) |
| VOL-004 | Extreme volatility | vol_percentile=99 | multiplier significantly reduced |

### 3.2 Drawdown Adjustment Tests

| Test ID | Scenario | Input | Expected Output |
|---------|----------|-------|-----------------|
| DD-001 | No drawdown | drawdown=0% | multiplier=1.0 |
| DD-002 | Moderate drawdown | drawdown=5% | multiplier slightly reduced |
| DD-003 | Severe drawdown | drawdown=15% | multiplier significantly reduced |
| DD-004 | Below threshold | drawdown=2% (threshold=5%) | multiplier=1.0 |

### 3.3 Market Condition Detection Tests

| Test ID | Scenario | Expected Condition |
|---------|----------|-------------------|
| MC-001 | Normal market | vol<50%, liquidity>0.7 | MarketCondition::Normal |
| MC-002 | High volatility | vol>80% | MarketCondition::HighVolatility |
| MC-003 | Low liquidity | spread>1%, depth low | MarketCondition::LowLiquidity |
| MC-004 | Crisis | vol>95%, liquidity<0.3 | MarketCondition::Crisis |

### 3.4 Effective Threshold Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| EFF-001 | Normal conditions | Effective = Base threshold |
| EFF-002 | High vol + drawdown | Effective = Base * vol_adj * dd_adj |
| EFF-003 | Time-based reduction | Near close: reduced thresholds |
| EFF-004 | Explain adjustments | Human-readable explanation |

---

## 4. Risk Rule Engine (Task #29)

### 4.1 Rule Evaluation Tests

| Test ID | Scenario | Rule | Input | Expected |
|---------|----------|------|-------|----------|
| RULE-001 | Simple condition | OrderSize > 10 | qty=15 | Reject |
| RULE-002 | Simple condition pass | OrderSize > 10 | qty=5 | Allow |
| RULE-003 | Price condition | OrderPrice > 60000 | price=55000 | Allow |
| RULE-004 | Position condition | PositionSize > 5 | pos=3 | Allow |

### 4.2 Composite Condition Tests

| Test ID | Scenario | Rule | Input | Expected |
|---------|----------|------|-------|----------|
| COMP-001 | AND both true | A AND B | A=true, B=true | true |
| COMP-002 | AND one false | A AND B | A=true, B=false | false |
| COMP-003 | OR both false | A OR B | A=false, B=false | false |
| COMP-004 | OR one true | A OR B | A=true, B=false | true |
| COMP-005 | NOT | NOT A | A=true | false |
| COMP-006 | Nested | (A AND B) OR C | A=false, B=true, C=true | true |

### 4.3 Priority Tests

| Test ID | Scenario | Rules | Expected |
|---------|----------|-------|----------|
| PRI-001 | Higher priority wins | Rule1(p=1, Reject), Rule2(p=2, Allow) | Reject |
| PRI-002 | Lower number = higher priority | Rule1(p=10), Rule2(p=5) | Rule2 evaluated first |
| PRI-003 | Same priority | Rule1(p=1), Rule2(p=1) | First match wins |

### 4.4 Hot-Reload Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| HR-001 | Load valid config | Rules loaded, active |
| HR-002 | Load invalid config | Fallback to previous rules |
| HR-003 | Atomic swap | No gap in rule enforcement |
| HR-004 | Reload during evaluation | Current evaluation completes with old rules |

### 4.5 Audit Trail Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| AUDIT-001 | Log evaluation | Rule ID, result, timestamp logged |
| AUDIT-002 | Callback fired | Audit callback receives evaluation result |
| AUDIT-003 | Recent evaluations | get_recent_evaluations returns last N |

---

## 5. Integration Tests

### 5.1 Strategy -> Risk -> OMS Flow

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| INT-001 | Order allowed | Strategy signal -> Risk allows -> OMS receives |
| INT-002 | Order rejected | Strategy signal -> Risk rejects -> EventEmitter notified |
| INT-003 | Circuit breaker trip | Risk trips -> EventLoop posts -> Strategies paused |
| INT-004 | Circuit breaker reset | Manual reset -> Trading resumes |

### 5.2 Position Update Flow

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| INT-005 | Fill -> Position -> Strategy | ExecutionReport -> PositionManager -> Strategy callback |
| INT-006 | Fill -> Position -> Risk | Position update triggers risk metrics recalc |

### 5.3 End-to-End Scenarios

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| E2E-001 | Full trade lifecycle | Order -> Fill -> Position -> PnL |
| E2E-002 | Risk limit hit | Order rejected, alert generated |
| E2E-003 | Drawdown triggers circuit breaker | Positions closed, strategies paused |

---

## 6. Performance Tests

### 6.1 Latency Requirements

| Test ID | Operation | Target Latency |
|---------|-----------|----------------|
| PERF-001 | Pre-trade risk check | < 100 us |
| PERF-002 | Position update | < 50 us |
| PERF-003 | Rule evaluation (100 rules) | < 500 us |
| PERF-004 | Metrics calculation | < 1 ms |

### 6.2 Throughput Requirements

| Test ID | Operation | Target Throughput |
|---------|-----------|-------------------|
| PERF-005 | Position updates | > 10,000/sec |
| PERF-006 | Risk checks | > 5,000/sec |
| PERF-007 | Rule evaluations | > 10,000/sec |

---

## 7. Error Handling Tests

| Test ID | Scenario | Expected Behavior |
|---------|----------|-------------------|
| ERR-001 | Invalid symbol | Graceful rejection |
| ERR-002 | Null position | Return kj::none, no crash |
| ERR-003 | Division by zero (leverage) | Handle gracefully |
| ERR-004 | Corrupted rule config | Fallback to previous |
| ERR-005 | Callback throws | Exception caught, logged |

---

*Document Version: 1.0*
*Author: Risk/OMS Developer*
*Date: 2026-02-20*
