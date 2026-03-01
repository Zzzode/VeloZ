# VeloZ Development Roadmap - Team Plan

**Last Updated:** 2026-02-21
**Status:** **Phase 1-7 Complete (33/33) | Phase 8 Planning Complete (7 new tasks)**

---

## Overview

This document outlines the complete development roadmap for VeloZ quantitative trading framework, organized into 7 phases with 33 specific tasks. **All phases completed successfully.**

---

## Task Summary by Phase

### Phase 1: Core Engine (5 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P1-001 | Complete Service Mode Networking Stack | Engine | P0 | 5 | **Completed** |
| P1-002 | Integrate Strategy Runtime into Engine | Engine | P0 | 4 | **Completed** |
| P1-003 | Implement Order WAL Recovery Logic | OMS | P1 | 3 | **Completed** |
| P1-004 | Implement Engine State Persistence | Engine | P1 | 3 | **Completed** |
| P1-005 | WebSocket Market Data Integration | Market | P0 | 4 | **Completed** |

### Phase 2: Market Data (5 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P2-001 | Complete Binance Depth Data Processing | Market | P1 | 3 | **Completed** |
| P2-002 | Implement Subscription Manager | Market | P1 | 4 | **Completed** |
| P2-003 | Implement Kline Aggregator | Market | P2 | 3 | **Completed** |
| P2-004 | Enhance Market Quality Statistics | Market | P2 | 3 | **Completed** |
| P2-005 | Implement OKX Exchange Adapter | Exec | P2 | 5 | **Completed** |

### Phase 3: Execution Module (5 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P3-001 | Implement Binance Production Trading | Exec | P0 | 5 | **Completed** |
| P3-002 | Implement Bybit Exchange Adapter | Exec | P2 | 4 | **Completed** |
| P3-003 | Implement Coinbase Exchange Adapter | Exec | P2 | 4 | **Completed** |
| P3-004 | Complete Reconciliation Logic | Exec | P1 | 3 | **Completed** |
| P3-005 | Implement Resilient Adapter Pattern | Exec | P1 | 4 | **Completed** |

### Phase 4: Strategy System (5 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P4-001 | Implement Trend Following Strategy | Strategy | P2 | 4 | **Completed** |
| P4-002 | Implement Mean Reversion Strategy | Strategy | P2 | 4 | **Completed** |
| P4-003 | Implement Momentum Strategy | Strategy | P2 | 3 | **Completed** |
| P4-004 | Implement Market Making Strategy | Strategy | P2 | 5 | **Completed** |
| P4-005 | Implement Grid Strategy | Strategy | P2 | 4 | **Completed** |

### Phase 5: Backtest System (5 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P5-001 | Complete Backtest Engine Event Loop | Backtest | P1 | 4 | **Completed** |
| P5-002 | Implement CSV Data Source | Backtest | P1 | 3 | **Completed** |
| P5-003 | Implement Binance Historical Data Download | Backtest | P1 | 3 | **Completed** |
| P5-004 | Implement Backtest Report Visualization | Backtest | P2 | 4 | **Completed** |
| P5-005 | Implement Parameter Optimization Algorithm | Backtest | P2 | 5 | **Completed** |

### Phase 6: Risk & Position (4 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P6-001 | Complete Position Calculation Logic | OMS | P1 | 3 | **Completed** |
| P6-002 | Enhance Risk Metrics | Risk | P2 | 3 | **Completed** |
| P6-003 | Implement Dynamic Risk Control Thresholds | Risk | P2 | 4 | **Completed** |
| P6-004 | Implement Risk Rule Engine | Risk | P2 | 4 | **Completed** |

### Phase 7: UI & User Experience (4 tasks)

| Task ID | Task Name | Module | Priority | Est. Days | Status |
|---------|------------|---------|-----------|-------------|--------|
| P7-001 | Implement Real-Time Order Book Display in UI | UI | P1 | 4 | **Completed** |
| P7-002 | Implement Position and PnL Display in UI | UI | P1 | 3 | **Completed** |
| P7-003 | Implement Strategy Configuration UI | UI | P2 | 4 | **Completed** |
| P7-004 | Implement Backtest Result Visualization in UI | UI | P2 | 4 | **Completed** |

---

## Dependency Graph

```
Phase 1: Core Engine
├── P1-001 (Service Mode) ────────┐
├── P1-002 (Strategy Runtime)          │
├── P1-003 (WAL Recovery) ──────────┤
├── P1-004 (State Persistence)        ├────→ Phase 4: Strategy
└── P1-005 (WebSocket Market) ──────┘
            │
            └──────→ Phase 2: Market Data
                     │
                     └──────→ Phase 7: UI (Order Book)

Phase 2: Market Data
├── P2-001 (Binance Depth) ──────────┐
├── P2-002 (Subscription Manager)        │
├── P2-003 (Kline Aggregator)        ├────→ Phase 7: UI (Order Book)
├── P2-004 (Market Quality)          │
└── P2-005 (OKX Adapter)            └────→ Phase 3: Exec

Phase 3: Execution
├── P3-001 (Binance Production) ──────┐
├── P3-002 (Bybit Adapter)          │
├── P3-003 (Coinbase Adapter)        ├────→ Phase 6: Risk (P6-001)
├── P3-004 (Reconciliation)          │
└── P3-005 (Resilient Adapter)       └────→ Phase 7: UI (Position)

Phase 4: Strategy
├── P4-001 (Trend Following)          │
├── P4-002 (Mean Reversion)           ├────→ Phase 7: UI (Strategy Config)
├── P4-003 (Momentum)               │
├── P4-004 (Market Making)            │
└── P4-005 (Grid Strategy)            └────→ Phase 5: Backtest

Phase 5: Backtest
├── P5-001 (Engine Event Loop) ─────────┐
├── P5-002 (CSV Data Source)           │
├── P5-003 (Binance Historical)       ├────→ Phase 7: UI (Backtest Results)
├── P5-004 (Report Visualization)       │
└── P5-005 (Parameter Optimization)    │
                                          └────→ Phase 5 (Self-optimization)

Phase 6: Risk & Position
├── P6-001 (Position Logic) ─────────────┘
├── P6-002 (Risk Metrics)
├── P6-003 (Dynamic Risk Control)
└── P6-004 (Risk Rule Engine)
```

---

## Recommended Development Timeline

### Sprint 1 (Weeks 1-2): Core Engine Foundation
**Goal:** Establish production-ready engine runtime

**Sprint Focus:** Phase 1 Tasks
- Week 1: P1-001 (Service Mode) + P1-002 (Strategy Runtime)
- Week 2: P1-003 (WAL) + P1-004 (Persistence) + P1-005 (WebSocket)

**Sprint Outcomes:**
- Engine runs in service mode
- Strategies integrate with engine
- Engine state persists across restarts
- Real-time market data flows into engine

### Sprint 2 (Weeks 3-4): Market Data & Basic Trading
**Goal:** Multi-exchange market data and production trading

**Sprint Focus:** Phase 2 + Phase 3 (P3-001 only)
- Week 3: P2-001 (Depth) + P2-002 (Subscription) + P3-001 (Binance Trading)
- Week 4: P2-003 (Kline) + P2-004 (Quality) + P2-005 (OKX)

**Sprint Outcomes:**
- Binance depth streams working
- OKX adapter functional
- Production trading on Binance
- Kline aggregation operational

### Sprint 3 (Weeks 5-6): Full Exchange Support
**Goal:** Complete exchange adapter layer

**Sprint Focus:** Phase 3 remaining
- Week 5: P3-002 (Bybit) + P3-004 (Reconciliation)
- Week 6: P3-003 (Coinbase) + P3-005 (Resilient Pattern)

**Sprint Outcomes:**
- All 4 exchanges (Binance, OKX, Bybit, Coinbase) functional
- Reconciliation working
- Resilient patterns in place

### Sprint 4 (Weeks 7-8): Strategy System
**Goal:** Implement production trading strategies

**Sprint Focus:** Phase 4
- Week 7: P4-001 (Trend) + P4-002 (Mean Reversion) + P4-003 (Momentum)
- Week 8: P4-004 (Market Making) + P4-005 (Grid)

**Sprint Outcomes:**
- 5 strategy types implemented
- All strategies testable via backtest
- Strategy runtime complete

### Sprint 5 (Weeks 9-10): Backtest & Optimization
**Goal:** Complete backtesting capabilities

**Sprint Focus:** Phase 5
- Week 9: P5-001 (Engine) + P5-002 (CSV) + P5-003 (Binance Historical)
- Week 10: P5-004 (Visualization) + P5-005 (Optimization)

**Sprint Outcomes:**
- Backtest engine production-ready
- Multiple data sources supported
- Parameter optimization functional
- Rich reporting and visualization

### Sprint 6 (Weeks 11-12): Risk & Production Readiness
**Goal:** Production-grade risk controls

**Sprint Focus:** Phase 6
- Week 11: P6-001 (Position) + P6-002 (Risk Metrics)
- Week 12: P6-003 (Dynamic Controls) + P6-004 (Rule Engine)

**Sprint Outcomes:**
- Position tracking accurate and complete
- Advanced risk metrics available
- Dynamic risk controls operational
- Rule-based risk management enabled

### Sprint 7 (Weeks 13-14): UI & User Experience
**Goal:** Production-ready trading UI

**Sprint Focus:** Phase 7
- Week 13: P7-001 (Order Book) + P7-002 (Position/PnL)
- Week 14: P7-003 (Strategy Config) + P7-004 (Backtest Results)

**Sprint Outcomes:**
- Real-time order book display
- Complete position and PnL tracking
- Strategy configuration interface
- Backtest result visualization
- Production-ready UI

---

## Task Assignment Guidelines

### Developer Specializations

For efficient parallel development, recommend the following specializations:

**Core Engine Team:**
- C++ developers with KJ library experience
- Focus on async I/O, persistence, networking

**Market Data Team:**
- WebSocket specialists
- Real-time data processing experience
- Financial data structures

**Execution Team:**
- Exchange API integration experience
- Error handling and retry patterns
- Authentication and security

**Strategy Team:**
- Quantitative finance background
- Trading strategy algorithms
- Backtesting experience

**Backtest Team:**
- Data processing specialists
- Algorithm optimization
- Visualization libraries

**Risk Team:**
- Risk management expertise
- Statistical analysis
- Real-time monitoring

**UI Team:**
- Frontend developers (JavaScript/TypeScript)
- Real-time data visualization
- SSE/WebSocket integration

### Task Assignment Process

1. **Review Task Dependencies** - Ensure dependent tasks are assigned first
2. **Match to Specialization** - Assign tasks to developers with relevant experience
3. **Set Priority Levels:**
   - P0: Critical path, blocks other phases
   - P1: High priority, important but not blocking
   - P2: Medium priority, nice to have
4. **Track Progress** - Use TaskUpdate to set in_progress/completed
5. **Handle Blocking** - If a task is blocked, add to blockedBy list

---

## Acceptance Criteria Summary

### Phase 1 Acceptance
- Engine service mode responds to HTTP requests
- Strategies can be loaded/unloaded at runtime
- Engine survives restart without data loss
- Real-time market data flows through engine

### Phase 2 Acceptance
- Order book depth < 10ms latency
- Klines generated within 1ms of candle close
- Market quality alerts generated within 100ms
- OKX adapter passes all CRUD tests

### Phase 3 Acceptance
- All 4 exchanges place/cancel orders successfully
- Reconciliation catches > 95% of discrepancies
- Circuit breaker activates within 3 failures
- User streams receive all fills/updates

### Phase 4 Acceptance
- Each strategy generates valid signals
- All strategies backtestable
- Hot-reload works within 1 second
- Strategy state queries return accurate data

### Phase 5 Acceptance
- Backtest processes 1M+ events in < 1 minute
- Optimization explores 1000+ parameter combos in < 1 hour
- Reports generate within 5 seconds
- CSV parsing handles 10M+ rows

### Phase 6 Acceptance
- PnL calculations accurate to 4 decimal places
- Risk metrics calculate within 100ms
- Dynamic thresholds adjust within 1 market cycle
- Rules evaluate within 10ms per decision

### Phase 7 Acceptance
- Order book updates within 100ms of change
- PnL displays within 500ms of fill
- Strategy config saves/loads < 2 seconds
- Backtest charts render within 3 seconds

---

## Milestones

| Milestone | Date (Target) | Tasks Required | Status |
|------------|-----------------|-----------------|--------|
| M1: Engine Foundation | 2026-02-20 | P1-001 to P1-005 | **Achieved** |
| M2: Market Data Ready | 2026-02-20 | P2-001 to P2-005 | **Achieved** |
| M3: Multi-Exchange Trading | 2026-02-20 | P3-001 to P3-005 | **Achieved** |
| M4: Strategy Suite | 2026-02-20 | P4-001 to P4-005 | **Achieved** |
| M5: Backtest Complete | 2026-02-20 | P5-001 to P5-005 | **Achieved** |
| M6: Production Risk | 2026-02-20 | P6-001 to P6-004 | **Achieved** |
| M7: Production Ready | 2026-02-20 | All Phases (33/33) | **Achieved** |

---

## Tracking and Reporting

### Daily Standup Format
Each developer should report:
1. What I did yesterday
2. What I'm doing today
3. Blockers/Dependencies
4. ETA for current task

### Weekly Review
At end of each sprint:
1. Completed tasks review
2. Remaining tasks reassessment
3. Timeline adjustments if needed
4. Risk and issue mitigation

---

## Completion Summary

**Final Status:** All 7 phases completed successfully

| Phase | Tasks | Status |
|--------|---------|--------|
| Phase 1: Core Engine | 5/5 | ✅ Complete |
| Phase 2: Market Data | 5/5 | ✅ Complete |
| Phase 3: Execution Module | 5/5 | ✅ Complete |
| Phase 4: Strategy System | 5/5 | ✅ Complete |
| Phase 5: Backtest System | 5/5 | ✅ Complete |
| Phase 6: Risk & Position | 4/4 | ✅ Complete |
| Phase 7: UI & User Experience | 4/4 | ✅ Complete |
| **Total** | **33/33** | **✅ 100%** |

**Deliverables:**
- C++23 quantitative trading engine with service mode
- 4 exchange adapters (Binance, OKX, Bybit, Coinbase)
- 5 production trading strategies (Trend, Mean Reversion, Momentum, Market Making, Grid)
- Complete backtest engine with parameter optimization
- Risk management system with rule engine
- UI with 4 major views (Order Book, Positions, Strategies, Backtest)
- 16/16 C++ test suites passing
- 200+ UI tests passing

**Build Status:** Clean (79/79 targets)

---

## Files and References

### Design Documents
- `docs/design/` - Module design specifications
- `docs/requirements/` - Feature requirements
- `docs/kj/` - KJ library usage guide

### Implementation Directories
- `apps/engine/` - Core engine code
- `libs/market/` - Market data module
- `libs/exec/` - Execution module
- `libs/oms/` - Order management
- `libs/risk/` - Risk management
- `libs/strategy/` - Strategy system
- `libs/backtest/` - Backtesting
- `apps/gateway/` - Python gateway
- `apps/ui/` - User interface

---

**Document Owner:** Development Team
**Review Cycle:** Weekly
**Last Review:** 2026-02-20
**Completion Date:** 2026-02-20
