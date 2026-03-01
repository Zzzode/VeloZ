# PRD: Strategy Marketplace

## Document Information

| Field | Value |
|-------|-------|
| Feature | Strategy Marketplace |
| Priority | P0 |
| Status | Draft |
| Owner | Product Manager |
| Target Release | v2.0 |
| Last Updated | 2026-02-25 |

## Executive Summary

Provide a curated library of pre-built trading strategies that personal traders can browse, configure, backtest, and deploy without writing any code. Transform VeloZ from a developer-focused framework into an accessible trading platform.

## Problem Statement

### Current Strategy Development Process

```
Current User Journey (Developers only):

1. Read strategy SDK documentation
2. Write C++ or Python strategy code
3. Implement callbacks (on_tick, on_fill, etc.)
4. Build and test locally
5. Debug compilation/runtime errors
6. Deploy and monitor
```

### User Pain Points

| Pain Point | Impact | Frequency |
|------------|--------|-----------|
| Code required for any strategy | Blocks non-developers | Always |
| No pre-built templates | High barrier to entry | Always |
| Complex backtesting setup | Prevents validation | Always |
| No visual parameter tuning | Trial-and-error coding | Every change |
| No performance comparison | Suboptimal choices | Always |

### Target User Profile

**Persona: David, Retail Trader**
- Age: 28, full-time job, trades part-time
- Technical skills: Uses trading apps, cannot code
- Goal: Run proven strategies without programming
- Frustration: "I know what grid trading is, but I can't code it"

## Goals and Success Metrics

### Goals

1. **Accessibility**: Non-technical users can deploy trading strategies
2. **Safety**: Mandatory backtesting before live deployment
3. **Education**: Users understand strategy behavior before trading
4. **Performance**: Clear metrics help users make informed decisions

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Strategy deployment rate | 0 (code required) | > 50% of users | Analytics |
| Backtest before deploy rate | N/A | > 90% | Analytics |
| Strategy-related support tickets | N/A | < 5% of tickets | Support |
| User satisfaction with strategies | N/A | > 4.0/5.0 | Survey |

## User Stories

### US-1: Browse Strategy Templates

**As a** personal trader
**I want to** browse available trading strategies
**So that** I can find one that matches my trading style

**Acceptance Criteria:**
- [ ] Strategy catalog with categories
- [ ] Search and filter functionality
- [ ] Strategy cards with key information
- [ ] Difficulty/risk indicators
- [ ] Popularity/usage statistics

### US-2: View Strategy Details

**As a** personal trader
**I want to** understand how a strategy works
**So that** I can decide if it's right for me

**Acceptance Criteria:**
- [ ] Strategy description in plain language
- [ ] How it works explanation
- [ ] When to use / when not to use
- [ ] Risk characteristics
- [ ] Historical performance (backtested)
- [ ] Parameter explanations

### US-3: Configure Strategy Parameters

**As a** personal trader
**I want to** customize strategy parameters visually
**So that** I can adapt it to my preferences

**Acceptance Criteria:**
- [ ] Visual parameter editors (sliders, inputs)
- [ ] Parameter validation with feedback
- [ ] Preset configurations (conservative/moderate/aggressive)
- [ ] Real-time preview of parameter effects
- [ ] Reset to defaults option

### US-4: Backtest Strategy

**As a** personal trader
**I want to** test a strategy on historical data
**So that** I can evaluate its performance before risking real money

**Acceptance Criteria:**
- [ ] One-click backtest initiation
- [ ] Date range selection
- [ ] Progress indicator during backtest
- [ ] Results summary (returns, drawdown, Sharpe)
- [ ] Equity curve visualization
- [ ] Trade history table
- [ ] Comparison with buy-and-hold

### US-5: Deploy Strategy

**As a** personal trader
**I want to** deploy a configured strategy
**So that** it can trade on my behalf

**Acceptance Criteria:**
- [ ] Deployment confirmation with risk warning
- [ ] Capital allocation setting
- [ ] Backtest requirement before live deployment
- [ ] Start/stop controls
- [ ] Real-time status monitoring
- [ ] Emergency stop button

### US-6: Monitor Running Strategy

**As a** personal trader
**I want to** monitor my running strategy
**So that** I can track its performance and intervene if needed

**Acceptance Criteria:**
- [ ] Real-time P&L display
- [ ] Open positions list
- [ ] Recent trades history
- [ ] Strategy status indicators
- [ ] Performance vs backtest comparison
- [ ] Alert notifications

## Strategy Templates

### Initial Strategy Catalog (10 Templates)

| Strategy | Category | Difficulty | Risk Level |
|----------|----------|------------|------------|
| Grid Trading | Market Making | Beginner | Medium |
| DCA (Dollar Cost Averaging) | Accumulation | Beginner | Low |
| Trend Following (MA Crossover) | Trend | Beginner | Medium |
| Mean Reversion | Mean Reversion | Intermediate | Medium |
| Momentum | Trend | Intermediate | High |
| RSI Oversold/Overbought | Mean Reversion | Beginner | Medium |
| Bollinger Band Breakout | Volatility | Intermediate | High |
| MACD Crossover | Trend | Beginner | Medium |
| Scalping | High Frequency | Advanced | High |
| Arbitrage | Market Neutral | Advanced | Low |

### Strategy Template: Grid Trading

```yaml
name: Grid Trading
category: Market Making
difficulty: Beginner
risk_level: Medium

description: |
  Grid trading places buy and sell orders at regular price intervals
  above and below a set price, creating a "grid" of orders. It profits
  from price oscillations within a range.

how_it_works: |
  1. Define a price range (upper and lower bounds)
  2. Divide the range into equal grid levels
  3. Place buy orders below current price
  4. Place sell orders above current price
  5. When a buy fills, place a sell one grid level up
  6. When a sell fills, place a buy one grid level down

when_to_use:
  - Ranging/sideways markets
  - High volatility within a range
  - When you expect price to oscillate

when_not_to_use:
  - Strong trending markets
  - During major news events
  - Low liquidity conditions

parameters:
  - name: grid_levels
    type: integer
    default: 10
    min: 5
    max: 50
    description: Number of grid levels (more = smaller profits per trade)

  - name: grid_spacing_pct
    type: percentage
    default: 1.0
    min: 0.1
    max: 5.0
    description: Price difference between grid levels

  - name: total_investment
    type: currency
    default: 1000
    min: 100
    max: 100000
    description: Total capital to allocate to this strategy

  - name: upper_price
    type: price
    default: auto
    description: Upper bound of the grid (auto = +10% from current)

  - name: lower_price
    type: price
    default: auto
    description: Lower bound of the grid (auto = -10% from current)

presets:
  conservative:
    grid_levels: 20
    grid_spacing_pct: 0.5
    description: More levels, smaller moves, lower risk per trade

  moderate:
    grid_levels: 10
    grid_spacing_pct: 1.0
    description: Balanced approach for typical market conditions

  aggressive:
    grid_levels: 5
    grid_spacing_pct: 2.0
    description: Fewer levels, larger moves, higher profit per trade

risks:
  - "Price breaking out of grid range causes unrealized losses"
  - "Trending markets can leave you holding losing positions"
  - "Requires sufficient capital to fill all grid orders"
```

## User Flows

### Flow 1: Browse and Select Strategy

```
+------------------+     +------------------+     +------------------+
|   Strategy       |     |   Filter/Search  |     |   Strategy       |
|   Marketplace    | --> |   Results        | --> |   Details        |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
                                                +------------------+
                                                |   Configure      |
                                                |   Parameters     |
                                                +------------------+
```

### Flow 2: Configure and Backtest

```
+------------------+     +------------------+     +------------------+
|   Configure      |     |   Run            |     |   View           |
|   Parameters     | --> |   Backtest       | --> |   Results        |
+------------------+     +------------------+     +--------+---------+
                                                          |
                              +---------------------------+
                              |                           |
                              v                           v
                    +------------------+        +------------------+
                    |   Adjust         |        |   Deploy         |
                    |   Parameters     |        |   Strategy       |
                    +------------------+        +------------------+
```

### Flow 3: Deploy and Monitor

```
+------------------+     +------------------+     +------------------+
|   Review         |     |   Confirm        |     |   Strategy       |
|   Configuration  | --> |   Deployment     | --> |   Running        |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
                                                +------------------+
                                                |   Monitor        |
                                                |   Dashboard      |
                                                +------------------+
```

## Wireframes

### Strategy Marketplace - Browse

```
+------------------------------------------------------------------+
|  Strategy Marketplace                                             |
+------------------------------------------------------------------+
|                                                                   |
|  +------------------+  +--------------------------------------+   |
|  | Categories       |  | Search strategies...            [Q]  |   |
|  +------------------+  +--------------------------------------+   |
|  | All Strategies   |                                             |
|  | Trend Following  |  Sort by: [Popular v]  Risk: [All v]        |
|  | Mean Reversion   |                                             |
|  | Market Making    |  +------------------+  +------------------+ |
|  | Accumulation     |  | Grid Trading     |  | DCA Strategy     | |
|  | Volatility       |  | [Grid Icon]      |  | [DCA Icon]       | |
|  +------------------+  |                  |  |                  | |
|                        | Profit from price|  | Accumulate asset | |
|                        | oscillations     |  | over time        | |
|                        |                  |  |                  | |
|                        | Risk: Medium     |  | Risk: Low        | |
|                        | Users: 1,234     |  | Users: 2,567     | |
|                        |                  |  |                  | |
|                        | [View Details]   |  | [View Details]   | |
|                        +------------------+  +------------------+ |
|                                                                   |
|                        +------------------+  +------------------+ |
|                        | Trend Following  |  | Mean Reversion   | |
|                        | [Trend Icon]     |  | [Revert Icon]    | |
|                        |                  |  |                  | |
|                        | Follow market    |  | Buy low, sell    | |
|                        | momentum         |  | high             | |
|                        |                  |  |                  | |
|                        | Risk: Medium     |  | Risk: Medium     | |
|                        | Users: 890       |  | Users: 654       | |
|                        |                  |  |                  | |
|                        | [View Details]   |  | [View Details]   | |
|                        +------------------+  +------------------+ |
+------------------------------------------------------------------+
```

### Strategy Details

```
+------------------------------------------------------------------+
|  Grid Trading Strategy                              [< Back]      |
+------------------------------------------------------------------+
|                                                                   |
|  +-------------------------------+  +---------------------------+ |
|  |                               |  | Quick Stats               | |
|  |       [Strategy Diagram]      |  +---------------------------+ |
|  |                               |  | Category: Market Making   | |
|  |    Upper Grid ----+----      |  | Difficulty: Beginner      | |
|  |                   |           |  | Risk Level: Medium        | |
|  |    Current  ------+------    |  | Users: 1,234              | |
|  |                   |           |  | Avg Return: +12.5%/month  | |
|  |    Lower Grid ----+----      |  +---------------------------+ |
|  |                               |  |                           | |
|  +-------------------------------+  | [Configure Strategy]      | |
|                                     +---------------------------+ |
|  Description                                                      |
|  --------------------------------------------------------         |
|  Grid trading places buy and sell orders at regular price         |
|  intervals above and below a set price, creating a "grid"         |
|  of orders. It profits from price oscillations within a range.    |
|                                                                   |
|  How It Works                                                     |
|  --------------------------------------------------------         |
|  1. Define a price range (upper and lower bounds)                 |
|  2. Divide the range into equal grid levels                       |
|  3. Place buy orders below current price                          |
|  4. Place sell orders above current price                         |
|  5. When orders fill, new orders are placed to maintain grid      |
|                                                                   |
|  +---------------------------+  +---------------------------+     |
|  | When to Use              |  | When NOT to Use           |     |
|  +---------------------------+  +---------------------------+     |
|  | - Ranging markets        |  | - Strong trends           |     |
|  | - High volatility        |  | - Major news events       |     |
|  | - Sideways price action  |  | - Low liquidity           |     |
|  +---------------------------+  +---------------------------+     |
|                                                                   |
|  Risks                                                            |
|  --------------------------------------------------------         |
|  [!] Price breaking out of range causes unrealized losses         |
|  [!] Trending markets can leave you holding losing positions      |
|  [!] Requires sufficient capital to fill all grid orders          |
|                                                                   |
+------------------------------------------------------------------+
```

### Strategy Configuration

```
+------------------------------------------------------------------+
|  Configure Grid Trading                             [< Back]      |
+------------------------------------------------------------------+
|                                                                   |
|  Quick Setup                                                      |
|  +----------------+  +----------------+  +----------------+       |
|  | Conservative   |  | Moderate       |  | Aggressive     |       |
|  | Lower risk     |  | Balanced       |  | Higher risk    |       |
|  | 20 levels      |  | 10 levels      |  | 5 levels       |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  Custom Parameters                                                |
|  --------------------------------------------------------         |
|                                                                   |
|  Trading Pair:                                                    |
|  +--------------------------------------------------+             |
|  | BTCUSDT                                      [v] |             |
|  +--------------------------------------------------+             |
|                                                                   |
|  Grid Levels:                                        [?]          |
|  [==========|----------] 10 levels                                |
|  More levels = smaller profits per trade, lower risk              |
|                                                                   |
|  Grid Spacing:                                       [?]          |
|  [==========|----------] 1.0%                                     |
|  Price difference between each grid level                         |
|                                                                   |
|  Investment Amount:                                  [?]          |
|  +------------------+                                             |
|  | $1,000           |  Available: $10,000                         |
|  +------------------+                                             |
|                                                                   |
|  Price Range:                                        [?]          |
|  Lower: +------------------+  Upper: +------------------+         |
|         | $45,000 (Auto)   |         | $55,000 (Auto)   |         |
|         +------------------+         +------------------+         |
|  [ ] Auto-adjust to current price +/- 10%                         |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | Preview                                                   |     |
|  +----------------------------------------------------------+     |
|  | Grid range: $45,000 - $55,000                             |     |
|  | Grid spacing: $1,000 per level                            |     |
|  | Order size: $100 per grid level                           |     |
|  | Potential profit per round-trip: ~$10 (1%)                |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|                              [Run Backtest]  [Deploy Strategy]    |
+------------------------------------------------------------------+
```

### Backtest Results

```
+------------------------------------------------------------------+
|  Backtest Results: Grid Trading                     [< Back]      |
+------------------------------------------------------------------+
|                                                                   |
|  +---------------------------+  +---------------------------+     |
|  | Total Return              |  | Max Drawdown              |     |
|  | +24.5%                    |  | -8.2%                     |     |
|  | vs Buy & Hold: +18.2%     |  |                           |     |
|  +---------------------------+  +---------------------------+     |
|                                                                   |
|  +---------------------------+  +---------------------------+     |
|  | Sharpe Ratio              |  | Win Rate                  |     |
|  | 1.85                      |  | 68%                       |     |
|  +---------------------------+  +---------------------------+     |
|                                                                   |
|  Equity Curve                                                     |
|  +----------------------------------------------------------+     |
|  |                                              ___/         |     |
|  |                                         ___/              |     |
|  |                                    ___/                   |     |
|  |                               ___/                        |     |
|  |                          ___/                             |     |
|  |                     ___/                                  |     |
|  |                ___/                                       |     |
|  |           ___/                                            |     |
|  |      ___/                                                 |     |
|  | ___/                                                      |     |
|  +----------------------------------------------------------+     |
|    Jan       Feb       Mar       Apr       May       Jun          |
|                                                                   |
|  --- Strategy    --- Buy & Hold                                   |
|                                                                   |
|  Trade Summary                                                    |
|  --------------------------------------------------------         |
|  | Total Trades | Winning | Losing | Avg Win | Avg Loss |         |
|  |     156      |   106   |   50   | +$15.20 | -$12.40  |         |
|  --------------------------------------------------------         |
|                                                                   |
|  [View Trade History]  [Adjust Parameters]  [Deploy Strategy]     |
+------------------------------------------------------------------+
```

### Running Strategy Monitor

```
+------------------------------------------------------------------+
|  My Strategies                                                    |
+------------------------------------------------------------------+
|                                                                   |
|  +----------------------------------------------------------+     |
|  | Grid Trading - BTCUSDT                    [Running]       |     |
|  +----------------------------------------------------------+     |
|  |                                                           |     |
|  | +-------------------------+  +-------------------------+  |     |
|  | | Today's P&L             |  | Total P&L               |  |     |
|  | | +$45.20 (+0.45%)        |  | +$234.50 (+2.35%)       |  |     |
|  | +-------------------------+  +-------------------------+  |     |
|  |                                                           |     |
|  | Open Positions                                            |     |
|  | --------------------------------------------------------  |     |
|  | | Side | Price    | Qty    | P&L      |                   |     |
|  | | BUY  | $49,500  | 0.002  | +$10.00  |                   |     |
|  | | BUY  | $49,000  | 0.002  | +$20.00  |                   |     |
|  | | BUY  | $48,500  | 0.002  | +$30.00  |                   |     |
|  | --------------------------------------------------------  |     |
|  |                                                           |     |
|  | Recent Trades                                             |     |
|  | --------------------------------------------------------  |     |
|  | | Time     | Side | Price    | Qty    | P&L     |         |     |
|  | | 10:45:23 | SELL | $50,000  | 0.002  | +$10.00 |         |     |
|  | | 10:32:15 | BUY  | $49,500  | 0.002  | -       |         |     |
|  | | 10:15:42 | SELL | $50,500  | 0.002  | +$15.00 |         |     |
|  | --------------------------------------------------------  |     |
|  |                                                           |     |
|  | [Pause Strategy]  [Edit Parameters]  [Stop Strategy]      |     |
|  +----------------------------------------------------------+     |
|                                                                   |
+------------------------------------------------------------------+
```

## Performance Metrics

### Backtest Metrics

| Metric | Description | Display Format |
|--------|-------------|----------------|
| Total Return | Percentage gain/loss | +24.5% |
| Max Drawdown | Largest peak-to-trough decline | -8.2% |
| Sharpe Ratio | Risk-adjusted return | 1.85 |
| Win Rate | Percentage of winning trades | 68% |
| Profit Factor | Gross profit / gross loss | 2.1 |
| Total Trades | Number of completed trades | 156 |
| Avg Win | Average winning trade | +$15.20 |
| Avg Loss | Average losing trade | -$12.40 |

### Live Performance Metrics

| Metric | Description | Update Frequency |
|--------|-------------|------------------|
| Today's P&L | Profit/loss since midnight | Real-time |
| Total P&L | Cumulative profit/loss | Real-time |
| Open Positions | Current holdings | Real-time |
| Unrealized P&L | P&L on open positions | Real-time |
| Realized P&L | P&L on closed trades | Per trade |

## Technical Requirements

### Strategy Engine Integration

| Component | Technology | Purpose |
|-----------|------------|---------|
| Strategy Templates | YAML definitions | Configuration |
| Parameter Validation | JSON Schema | Input validation |
| Backtest Engine | C++ (existing) | Historical simulation |
| Live Execution | C++ (existing) | Order management |

### API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/strategies` | GET | List available strategies |
| `/api/strategies/{id}` | GET | Get strategy details |
| `/api/strategies/{id}/configure` | POST | Save configuration |
| `/api/strategies/{id}/backtest` | POST | Run backtest |
| `/api/strategies/{id}/deploy` | POST | Deploy strategy |
| `/api/strategies/{id}/stop` | POST | Stop strategy |
| `/api/deployments` | GET | List running strategies |
| `/api/deployments/{id}` | GET | Get deployment status |

### Backtest Requirements

| Requirement | Value |
|-------------|-------|
| Minimum backtest period | 30 days |
| Maximum backtest period | 2 years |
| Data granularity | 1-minute candles |
| Slippage simulation | 0.1% default |
| Fee simulation | Exchange-specific |

## Non-Functional Requirements

### Performance

| Requirement | Target |
|-------------|--------|
| Strategy list load | < 1 second |
| Backtest (1 month) | < 30 seconds |
| Backtest (1 year) | < 5 minutes |
| Live update latency | < 500ms |

### Scalability

| Requirement | Target |
|-------------|--------|
| Concurrent backtests | 10 per user |
| Running strategies | 5 per user |
| Historical data storage | 2 years per symbol |

### Security

| Requirement | Implementation |
|-------------|----------------|
| Strategy isolation | Sandboxed execution |
| Capital limits | Enforced by risk module |
| Audit logging | All deployments logged |

## Sprint Planning

### Sprint 1-2: Strategy Catalog (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Strategy template schema | 2 days | Backend |
| Create 5 strategy templates | 3 days | Backend |
| Strategy list API | 2 days | Backend |
| Strategy browse UI | 3 days | Frontend |

**Deliverable:** Browsable strategy catalog

### Sprint 3-4: Strategy Configuration (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Parameter validation | 2 days | Backend |
| Configuration API | 2 days | Backend |
| Configuration UI | 4 days | Frontend |
| Preset system | 2 days | Backend |

**Deliverable:** Visual strategy configuration

### Sprint 5-6: Backtesting (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Backtest API integration | 3 days | Backend |
| Results calculation | 2 days | Backend |
| Results UI | 3 days | Frontend |
| Equity curve chart | 2 days | Frontend |

**Deliverable:** One-click backtesting

### Sprint 7-8: Deployment (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Deployment API | 3 days | Backend |
| Strategy lifecycle management | 3 days | Backend |
| Deployment UI | 2 days | Frontend |
| Monitoring dashboard | 2 days | Frontend |

**Deliverable:** Strategy deployment and monitoring

### Sprint 9-10: Additional Strategies (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Create 5 more strategy templates | 5 days | Backend |
| Strategy documentation | 3 days | Content |
| User testing | 2 days | QA |

**Deliverable:** Complete strategy catalog (10 templates)

## Dependencies

### External Dependencies

| Dependency | Type | Risk |
|------------|------|------|
| Historical market data | Data | Medium |
| Backtest engine | Existing code | Low |

### Internal Dependencies

| Dependency | Feature | Impact |
|------------|---------|--------|
| GUI Configuration | Exchange connection | Blocking |
| Security Education | Live deployment | Blocking |
| Real-Time Charts | Performance visualization | Enhancing |

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Strategy performance liability | High | High | Clear disclaimers, mandatory backtesting |
| Backtest overfitting | Medium | Medium | Out-of-sample testing, warnings |
| User losses | High | High | Paper trading requirement, risk limits |
| Strategy bugs | Medium | High | Extensive testing, kill switches |

## Legal Considerations

### Required Disclaimers

1. "Past performance does not guarantee future results"
2. "Backtest results may not reflect actual trading conditions"
3. "Trading involves risk of loss"
4. "VeloZ does not provide financial advice"

### User Acknowledgments

Before deploying any strategy, users must acknowledge:
- [ ] Understanding of strategy risks
- [ ] Acceptance of potential losses
- [ ] Completion of security education
- [ ] Successful backtest review

## Success Criteria

### MVP (Month 3)

- [ ] 5 strategy templates available
- [ ] Visual configuration working
- [ ] Backtesting functional
- [ ] Basic deployment working

### Full Release (Month 5)

- [ ] 10 strategy templates available
- [ ] Full backtest analytics
- [ ] Live monitoring dashboard
- [ ] Strategy deployment rate > 50%
- [ ] Backtest before deploy > 90%

---

*Document Version: 1.0*
*Last Updated: 2026-02-25*
