# VeloZ Full-Stack Team Configuration

**Created:** 2026-02-20
**Team:** veloz-fullstack

---

## Team Structure

### Team Lead
- **Role:** Product Manager / Technical Lead
- **Responsibilities:**
  - Sprint planning and milestone tracking
  - Task assignment and dependency management
  - Technical decisions and architecture review
  - Blocker resolution and resource coordination

### Architect
- **Role:** System Architect
- **Responsibilities:**
  - Architecture design and reviews
  - Module boundary enforcement
  - Interface contract definition
  - Technology choices and patterns
  - Cross-module integration guidance

### Developers
- **Core Engine Developer** (1-2 developers)
  - P1-001: Service Mode Networking
  - P1-002: Strategy Runtime Integration
  - P1-004: Engine State Persistence
  - P1-005: WebSocket Market Data Integration

- **Market Data Developer** (1-2 developers)
  - P2-001: Binance Depth Data Processing
  - P2-002: Subscription Manager
  - P2-003: Kline Aggregator
  - P2-004: Market Quality Statistics
  - P2-005: OKX Exchange Adapter

- **Execution Developer** (2-3 developers)
  - P3-001: Binance Production Trading
  - P3-002: Bybit Exchange Adapter
  - P3-003: Coinbase Exchange Adapter
  - P3-004: Reconciliation Logic
  - P3-005: Resilient Adapter Pattern

- **Strategy Developer** (2 developers)
  - P4-001: Trend Following Strategy
  - P4-002: Mean Reversion Strategy
  - P4-003: Momentum Strategy
  - P4-004: Market Making Strategy
  - P4-005: Grid Strategy

- **Backtest Developer** (1-2 developers)
  - P5-001: Backtest Engine Event Loop
  - P5-002: CSV Data Source
  - P5-003: Binance Historical Data Download
  - P5-004: Backtest Report Visualization
  - P5-005: Parameter Optimization Algorithm

- **Risk/OMS Developer** (1 developer)
  - P6-001: Position Calculation Logic
  - P6-002: Risk Metrics
  - P6-003: Dynamic Risk Control Thresholds
  - P6-004: Risk Rule Engine

- **UI Developer** (1-2 developers)
  - P7-001: Real-Time Order Book Display
  - P7-002: Position and PnL Display
  - P7-003: Strategy Configuration UI
  - P7-004: Backtest Result Visualization

### QA Engineer
- **Role:** Quality Assurance
- **Responsibilities:**
  - Test planning and execution
  - Integration test development
  - Performance testing
  - Bug triage and verification

---

## Task Assignment Matrix

| Phase | P0 Tasks | P1 Tasks | P2 Tasks |
|--------|----------|----------|----------|
| Phase 1 | P1-001, P1-002, P1-005 | P1-003, P1-004 | - |
| Phase 2 | - | P2-001, P2-002, P2-003, P2-004 | P2-005 |
| Phase 3 | P3-001 | P3-004, P3-005 | P3-002, P3-003 |
| Phase 4 | - | - | P4-001, P4-002, P4-003, P4-004, P4-005 |
| Phase 5 | P5-001, P5-002, P5-003 | - | P5-004, P5-005 |
| Phase 6 | P6-001 | P6-002, P6-003, P6-004 | - |
| Phase 7 | P7-001, P7-002 | - | P7-003, P7-004 |

---

## Development Workflow

### Daily Standup (15 minutes)
- **Time:** 10:00 AM daily
- **Participants:** All team members
- **Agenda:**
  1. Yesterday's accomplishments
  2. Today's planned work
  3. Blockers and dependencies
  4. Any risks or concerns

### Weekly Sprint Planning (1 hour)
- **Time:** Monday 10:00 AM
- **Participants:** All team members
- **Agenda:**
  1. Review completed sprint
  2. Plan upcoming sprint tasks
  3. Task assignment and priorities
  4. Dependency planning

### Code Review Process
- **Pull Request Required:** For all non-trivial changes
- **Minimum Reviewers:** 1 from different module, 1 team lead
- **Review Criteria:**
  - Code quality and style adherence
  - KJ library usage correctness
  - Test coverage (target: >80% for new code)
  - Documentation completeness

### Testing Requirements
- **Unit Tests:** Required for all new functionality
- **Integration Tests:** Required for module boundaries
- **Performance Tests:** Required for critical paths
- **CI Pipeline:** All tests must pass before merge

---

## Communication Channels

### Channels
- **#general:** Project announcements, general discussion
- **#engine:** Core engine development
- **#market:** Market data module
- **#exec:** Execution adapters
- **#strategy:** Strategy development
- **#backtest:** Backtesting system
- **#risk:** Risk management
- **#ui:** UI development
- **#qa:** Testing and quality
- **#releases:** Release coordination

### Async Meetings
- **Architecture Review:** Ad-hoc, requested by Architect
- **Bug Triage:** Weekly, Wednesday 2:00 PM
- **Release Planning:** As needed, 2 weeks before release

---

## Work Prioritization Rules

### Priority Levels
- **P0 (Critical):** Blocks other phases, production bugs, security issues
- **P1 (High):** Core functionality, important features
- **P2 (Medium):** Enhancements, nice-to-have features

### Sprint Goal Setting
- Each sprint focuses on 1-2 phases
- Ensure P0 tasks are completed first
- Maintain capacity buffer for unexpected issues

---

## Risk Management

### Common Risks
1. **Technical Debt:** Manage through code reviews and refactoring sprints
2. **Dependencies:** Track and escalate blockers early
3. **Scope Creep:** Architect maintains scope boundaries
4. **Resource Constraints:** PM tracks capacity and adjusts timeline

### Escalation Path
- Technical blockers → Architect → Team Lead
- Timeline blockers → PM → Team Lead
- Resource constraints → PM → Architect

---

## Quality Standards

### Code Standards
- Follow KJ library patterns (see `docs/kj/library_usage_guide.md`)
- Use VeloZ encoding style (see `.claude/rules/encoding-style.md`)
- Write tests alongside implementation
- Add comments for complex logic

### Documentation Standards
- Update public API documentation
- Document architectural decisions in `docs/design/`
- Keep CLAUDE.md current
- Update README for user-facing changes

### Performance Standards
- Order processing latency < 50ms (p99)
- Market data processing latency < 10ms (p99)
- UI response time < 200ms (p95)
- Backtest: 1M+ events in < 1 minute

---

## Milestones and Releases

### Milestone M1: Engine Foundation (Sprint 1-2)
**Target:** End of Week 2
**Tasks:** P1-001 through P1-005
**Deliverables:**
- Service mode networking stack
- Strategy runtime integration
- Engine persistence and recovery
- WebSocket market data integration

### Milestone M2: Market Data Ready (Sprint 3)
**Target:** End of Week 4
**Tasks:** P2-001 through P2-005
**Deliverables:**
- Binance depth data processing
- Subscription manager
- Kline aggregator
- Market quality monitoring
- OKX exchange adapter

### Milestone M3: Multi-Exchange Trading (Sprint 4-5)
**Target:** End of Week 6
**Tasks:** P3-001 through P3-005
**Deliverables:**
- All 4 exchange adapters
- Reconciliation system
- Resilient adapter pattern

### Milestone M4: Strategy Suite (Sprint 7-8)
**Target:** End of Week 8
**Tasks:** P4-001 through P4-005
**Deliverables:**
- 5 production strategies
- Strategy hot-reload
- Strategy backtesting integration

### Milestone M5: Backtest Complete (Sprint 9-10)
**Target:** End of Week 10
**Tasks:** P5-001 through P5-005
**Deliverables:**
- Production backtest engine
- Multiple data sources
- Parameter optimization
- Rich reporting and visualization

### Milestone M6: Production Risk (Sprint 11-12)
**Target:** End of Week 12
**Tasks:** P6-001 through P6-004
**Deliverables:**
- Complete position tracking
- Advanced risk metrics
- Dynamic risk controls
- Rule-based risk engine

### Milestone M7: Production Ready (Sprint 13-14)
**Target:** End of Week 14
**Tasks:** P7-001 through P7-004
**Deliverables:**
- Production-ready UI
- Real-time order book
- Position and PnL tracking
- Strategy configuration
- Backtest visualization

---

## Onboarding Checklist

For new team members:
- [ ] Read project overview (`docs/crypto_quant_framework_design.md`)
- [ ] Understand KJ library patterns (`docs/kj/library_usage_guide.md`)
- [ ] Review code style (`.claude/rules/encoding-style.md`)
- [ ] Set up development environment (CMake, KJ library)
- [ ] Run existing tests to verify setup
- [ ] Review relevant module architecture (`docs/design/`)
- [ ] Get assigned first task from team lead

---

## Glossary

- **P0/P1/P2:** Priority levels (Critical/High/Medium)
- **KJ:** Cap'n Proto KJ library (async I/O framework)
- **WAL:** Write-Ahead Log (durable storage)
- **PnL:** Profit and Loss
- **OHLCV:** Open, High, Low, Close, Volume
- **SSE:** Server-Sent Events (real-time web protocol)

---

**Last Updated:** 2026-02-20
**Next Review:** After Sprint 1 completion
