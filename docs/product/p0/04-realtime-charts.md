# PRD: Real-Time Charts

## Document Information

| Field | Value |
|-------|-------|
| Feature | Real-Time Charts |
| Priority | P0 |
| Status | Draft |
| Owner | Product Manager |
| Target Release | v2.0 |
| Last Updated | 2026-02-25 |

## Executive Summary

Implement professional-grade charting capabilities with candlesticks, technical indicators, drawing tools, and real-time price updates. Provide traders with the visual analysis tools they expect from modern trading platforms.

## Problem Statement

### Current State

VeloZ currently has no charting capabilities. Users must use external tools (TradingView, exchange charts) for technical analysis, then switch to VeloZ for trading. This creates:

- Fragmented workflow
- Context switching overhead
- Missed trading opportunities
- Poor user experience

### User Pain Points

| Pain Point | Impact | Frequency |
|------------|--------|-----------|
| No charts in VeloZ | Must use external tools | Every session |
| No indicator overlay | Manual analysis required | Every trade |
| No drawing tools | Cannot mark support/resistance | Every analysis |
| No multi-timeframe | Limited analysis capability | Every session |

### Target User Profile

**Persona: Sarah, Technical Trader**
- Age: 31, trades based on technical analysis
- Technical skills: Expert with TradingView, expects similar UX
- Goal: Analyze charts and execute trades in one platform
- Frustration: "I have to keep switching between TradingView and VeloZ"

## Goals and Success Metrics

### Goals

1. **Completeness**: Provide charts comparable to TradingView/exchange platforms
2. **Performance**: Real-time updates with < 100ms latency
3. **Usability**: Intuitive interface requiring no learning curve
4. **Integration**: Charts integrated with order placement

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Chart usage rate | 0% | > 80% of sessions | Analytics |
| Time in chart view | 0 | > 50% of session time | Analytics |
| External tool usage | 100% | < 30% | Survey |
| User satisfaction | N/A | > 4.2/5.0 | Survey |

## User Stories

### US-1: View Candlestick Chart

**As a** trader
**I want to** view real-time candlestick charts
**So that** I can analyze price action

**Acceptance Criteria:**
- [ ] Candlestick chart with OHLC data
- [ ] Real-time price updates
- [ ] Multiple timeframes (1m, 5m, 15m, 1h, 4h, 1d, 1w)
- [ ] Zoom and pan functionality
- [ ] Current price line indicator
- [ ] Volume bars below price chart

### US-2: Add Technical Indicators

**As a** technical trader
**I want to** add indicators to my chart
**So that** I can perform technical analysis

**Acceptance Criteria:**
- [ ] Indicator selection menu
- [ ] 30+ indicators available
- [ ] Customizable indicator parameters
- [ ] Multiple indicators simultaneously
- [ ] Indicator visibility toggle
- [ ] Save indicator presets

### US-3: Draw on Chart

**As a** trader
**I want to** draw lines and shapes on the chart
**So that** I can mark support/resistance levels

**Acceptance Criteria:**
- [ ] Trend lines
- [ ] Horizontal lines
- [ ] Fibonacci retracement
- [ ] Rectangle selection
- [ ] Text annotations
- [ ] Drawing persistence across sessions

### US-4: Multi-Timeframe Analysis

**As a** trader
**I want to** view multiple timeframes simultaneously
**So that** I can confirm signals across timeframes

**Acceptance Criteria:**
- [ ] Split view with 2-4 charts
- [ ] Independent timeframe per chart
- [ ] Synchronized crosshair
- [ ] Linked symbol across charts
- [ ] Layout presets

### US-5: Trade from Chart

**As a** trader
**I want to** place orders directly from the chart
**So that** I can execute quickly without switching views

**Acceptance Criteria:**
- [ ] Right-click to place order at price level
- [ ] Drag to set stop-loss/take-profit
- [ ] Order visualization on chart
- [ ] One-click order modification
- [ ] Position indicator on chart

### US-6: Mobile-Responsive Charts

**As a** mobile trader
**I want to** view charts on my phone/tablet
**So that** I can monitor markets on the go

**Acceptance Criteria:**
- [ ] Touch-friendly interactions
- [ ] Pinch to zoom
- [ ] Swipe to pan
- [ ] Responsive layout
- [ ] Essential indicators available

## Technical Indicators

### Indicator Categories

| Category | Indicators |
|----------|------------|
| Trend | SMA, EMA, WMA, DEMA, TEMA, Ichimoku Cloud |
| Momentum | RSI, MACD, Stochastic, CCI, Williams %R, ROC |
| Volatility | Bollinger Bands, ATR, Keltner Channel, Donchian |
| Volume | Volume, OBV, VWAP, Volume Profile, MFI |
| Support/Resistance | Pivot Points, Fibonacci, Support/Resistance |

### Initial Indicator Set (30 Indicators)

| # | Indicator | Category | Overlay |
|---|-----------|----------|---------|
| 1 | Simple Moving Average (SMA) | Trend | Yes |
| 2 | Exponential Moving Average (EMA) | Trend | Yes |
| 3 | Weighted Moving Average (WMA) | Trend | Yes |
| 4 | Bollinger Bands | Volatility | Yes |
| 5 | Ichimoku Cloud | Trend | Yes |
| 6 | Parabolic SAR | Trend | Yes |
| 7 | VWAP | Volume | Yes |
| 8 | Pivot Points | Support/Resistance | Yes |
| 9 | Fibonacci Retracement | Support/Resistance | Yes |
| 10 | Keltner Channel | Volatility | Yes |
| 11 | RSI | Momentum | No |
| 12 | MACD | Momentum | No |
| 13 | Stochastic | Momentum | No |
| 14 | CCI | Momentum | No |
| 15 | Williams %R | Momentum | No |
| 16 | ROC | Momentum | No |
| 17 | ADX | Trend | No |
| 18 | ATR | Volatility | No |
| 19 | Volume | Volume | No |
| 20 | OBV | Volume | No |
| 21 | MFI | Volume | No |
| 22 | Chaikin Money Flow | Volume | No |
| 23 | Accumulation/Distribution | Volume | No |
| 24 | TRIX | Momentum | No |
| 25 | Ultimate Oscillator | Momentum | No |
| 26 | Awesome Oscillator | Momentum | No |
| 27 | Momentum | Momentum | No |
| 28 | Donchian Channel | Volatility | Yes |
| 29 | Standard Deviation | Volatility | No |
| 30 | Historical Volatility | Volatility | No |

## User Flows

### Flow 1: Basic Chart Viewing

```
+------------------+     +------------------+     +------------------+
|   Select         |     |   Chart          |     |   Interact       |
|   Symbol         | --> |   Loads          | --> |   (Zoom/Pan)     |
+------------------+     +------------------+     +------------------+
```

### Flow 2: Add Indicator

```
+------------------+     +------------------+     +------------------+
|   Click          |     |   Select         |     |   Configure      |
|   Indicators     | --> |   Indicator      | --> |   Parameters     |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
                                                +------------------+
                                                |   Indicator      |
                                                |   Displayed      |
                                                +------------------+
```

### Flow 3: Trade from Chart

```
+------------------+     +------------------+     +------------------+
|   Right-Click    |     |   Select Order   |     |   Confirm        |
|   at Price       | --> |   Type           | --> |   Order          |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
                                                +------------------+
                                                |   Order on       |
                                                |   Chart          |
                                                +------------------+
```

## Wireframes

### Main Chart View

```
+------------------------------------------------------------------+
|  Charts                                                           |
+------------------------------------------------------------------+
|  BTCUSDT [v]  |  1H [v]  |  [Indicators]  [Draw]  [Settings]      |
+------------------------------------------------------------------+
|                                                                   |
|  $52,000 |----------------------------------------------------+   |
|          |                                         ___/        |   |
|  $51,500 |                                    ___/             |   |
|          |                               ___/                  |   |
|  $51,000 |                          ___/                       |   |
|          |                     ___/                            |   |
|  $50,500 |                ___/                                 |   |
|          |           ___/                                      |   |
|  $50,000 |      ___/                                           |   |
|          | ___/                                                |   |
|  $49,500 |----------------------------------------------------+   |
|          | Jan 15    Jan 16    Jan 17    Jan 18    Jan 19      |   |
|          +----------------------------------------------------+   |
|                                                                   |
|  Volume  |----------------------------------------------------+   |
|          |  |  ||  |   ||  | |  ||  |  | ||  |  |  ||  |  |   |   |
|          +----------------------------------------------------+   |
|                                                                   |
|  Current: $51,234.56  High: $51,890  Low: $49,123  Vol: 12.5K     |
+------------------------------------------------------------------+
```

### Chart with Indicators

```
+------------------------------------------------------------------+
|  Charts                                                           |
+------------------------------------------------------------------+
|  BTCUSDT [v]  |  1H [v]  |  [Indicators]  [Draw]  [Settings]      |
+------------------------------------------------------------------+
|  Active Indicators: [EMA 20 x] [EMA 50 x] [BB x]                  |
+------------------------------------------------------------------+
|                                                                   |
|  $52,000 |----------------------------------------------------+   |
|          |                              ___/ ~~~~ (EMA 20)     |   |
|  $51,500 |                         ___/  ---- (EMA 50)         |   |
|          |                    ___/   .... (BB Upper)           |   |
|  $51,000 |               ___/                                  |   |
|          |          ___/                                       |   |
|  $50,500 |     ___/                                            |   |
|          |___/           .... (BB Lower)                       |   |
|  $50,000 |----------------------------------------------------+   |
|          | Jan 15    Jan 16    Jan 17    Jan 18    Jan 19      |   |
|          +----------------------------------------------------+   |
|                                                                   |
|  RSI(14) |----------------------------------------------------+   |
|      70  |                    ___                              |   |
|      50  |               ___/   \___                           |   |
|      30  |          ___/            \___                       |   |
|          +----------------------------------------------------+   |
|                                                                   |
+------------------------------------------------------------------+
```

### Indicator Selection Panel

```
+------------------------------------------------------------------+
|  Add Indicator                                          [X]       |
+------------------------------------------------------------------+
|                                                                   |
|  Search: [                                              ]         |
|                                                                   |
|  +------------------+  +----------------------------------+       |
|  | Categories       |  | Indicators                       |       |
|  +------------------+  +----------------------------------+       |
|  | All              |  | [Star] RSI                       |       |
|  | Trend            |  |   Relative Strength Index        |       |
|  | Momentum    [*]  |  |                          [Add]   |       |
|  | Volatility       |  +----------------------------------+       |
|  | Volume           |  | [Star] MACD                      |       |
|  | Support/Resist   |  |   Moving Average Convergence     |       |
|  +------------------+  |                          [Add]   |       |
|                        +----------------------------------+       |
|  Popular               | [Star] Stochastic                |       |
|  +------------------+  |   Stochastic Oscillator          |       |
|  | RSI              |  |                          [Add]   |       |
|  | MACD             |  +----------------------------------+       |
|  | Bollinger Bands  |  | CCI                              |       |
|  | EMA              |  |   Commodity Channel Index        |       |
|  +------------------+  |                          [Add]   |       |
|                        +----------------------------------+       |
|                                                                   |
+------------------------------------------------------------------+
```

### Indicator Configuration

```
+------------------------------------------------------------------+
|  Configure RSI                                          [X]       |
+------------------------------------------------------------------+
|                                                                   |
|  Parameters                                                       |
|  --------------------------------------------------------         |
|                                                                   |
|  Period:                                                          |
|  +------------------+                                             |
|  | 14               |  (Default: 14)                              |
|  +------------------+                                             |
|                                                                   |
|  Overbought Level:                                                |
|  +------------------+                                             |
|  | 70               |  (Default: 70)                              |
|  +------------------+                                             |
|                                                                   |
|  Oversold Level:                                                  |
|  +------------------+                                             |
|  | 30               |  (Default: 30)                              |
|  +------------------+                                             |
|                                                                   |
|  Style                                                            |
|  --------------------------------------------------------         |
|                                                                   |
|  Line Color: [Blue v]                                             |
|  Line Width: [1 v]                                                |
|  Show Overbought/Oversold Zones: [x]                              |
|                                                                   |
|                              [Reset to Default]  [Apply]          |
+------------------------------------------------------------------+
```

### Drawing Tools

```
+------------------------------------------------------------------+
|  Drawing Tools                                                    |
+------------------------------------------------------------------+
|                                                                   |
|  Lines                                                            |
|  +--------+  +--------+  +--------+  +--------+                   |
|  | Trend  |  | Horiz  |  | Vert   |  | Ray    |                   |
|  | Line   |  | Line   |  | Line   |  |        |                   |
|  +--------+  +--------+  +--------+  +--------+                   |
|                                                                   |
|  Fibonacci                                                        |
|  +--------+  +--------+  +--------+                               |
|  | Retrace|  | Extend |  | Fan    |                               |
|  | ment   |  | sion   |  |        |                               |
|  +--------+  +--------+  +--------+                               |
|                                                                   |
|  Shapes                                                           |
|  +--------+  +--------+  +--------+  +--------+                   |
|  | Rect   |  | Circle |  | Triangle | Text   |                   |
|  | angle  |  |        |  |        |  |        |                   |
|  +--------+  +--------+  +--------+  +--------+                   |
|                                                                   |
|  [Clear All Drawings]                                             |
|                                                                   |
+------------------------------------------------------------------+
```

### Multi-Chart Layout

```
+------------------------------------------------------------------+
|  Charts  |  Layout: [2x2 v]                                       |
+------------------------------------------------------------------+
|                              |                                    |
|  BTCUSDT - 1H                |  BTCUSDT - 4H                      |
|  +------------------------+  |  +------------------------+        |
|  |                        |  |  |                        |        |
|  |    [Chart Area]        |  |  |    [Chart Area]        |        |
|  |                        |  |  |                        |        |
|  +------------------------+  |  +------------------------+        |
|                              |                                    |
|------------------------------|------------------------------------+
|                              |                                    |
|  BTCUSDT - 1D                |  BTCUSDT - 1W                      |
|  +------------------------+  |  +------------------------+        |
|  |                        |  |  |                        |        |
|  |    [Chart Area]        |  |  |    [Chart Area]        |        |
|  |                        |  |  |                        |        |
|  +------------------------+  |  +------------------------+        |
|                              |                                    |
+------------------------------------------------------------------+
```

### Trade from Chart

```
+------------------------------------------------------------------+
|  Charts                                                           |
+------------------------------------------------------------------+
|                                                                   |
|  $52,000 |----------------------------------------------------+   |
|          |                                         ___/        |   |
|  $51,500 |  [Right-click menu]                ___/             |   |
|          |  +------------------+         ___/                  |   |
|  $51,000 |  | Buy Limit $51,000|    ___/                       |   |
|          |  | Sell Limit $51,000                               |   |
|  $50,500 |  | Set Alert        |                               |   |
|          |  | Draw Line Here   |                               |   |
|  $50,000 |  +------------------+                               |   |
|          |                                                     |   |
|  $49,500 |----------------------------------------------------+   |
|                                                                   |
|  [Order Entry Panel]                                              |
|  +----------------------------------------------------------+     |
|  | Buy Limit @ $51,000                                       |     |
|  | Qty: [0.01    ]  Total: $510.00                           |     |
|  | Stop Loss: [________]  Take Profit: [________]            |     |
|  |                                    [Cancel]  [Place Order]|     |
|  +----------------------------------------------------------+     |
|                                                                   |
+------------------------------------------------------------------+
```

## Performance Requirements

### Rendering Performance

| Metric | Target | Measurement |
|--------|--------|-------------|
| Initial chart load | < 1 second | Time to first render |
| Candle update latency | < 100ms | WebSocket to render |
| Zoom/pan responsiveness | 60 FPS | Frame rate |
| Indicator calculation | < 500ms | For 1000 candles |

### Data Requirements

| Metric | Target |
|--------|--------|
| Candles per request | 1000 max |
| Historical data depth | 2 years |
| Real-time update rate | 100ms minimum |
| Concurrent symbols | 10 per session |

### Memory Requirements

| Metric | Target |
|--------|--------|
| Chart memory usage | < 100 MB per chart |
| Indicator memory | < 10 MB per indicator |
| Drawing storage | < 1 MB per chart |

## Technical Requirements

### Chart Library Options

| Library | Pros | Cons | Recommendation |
|---------|------|------|----------------|
| TradingView Widget | Full-featured, familiar | Cost, limited customization | Consider for MVP |
| Lightweight Charts | Free, performant | Fewer features | Good alternative |
| D3.js + Custom | Full control | Development time | Long-term option |
| Apache ECharts | Free, feature-rich | Learning curve | Consider |

**Recommendation:** Start with TradingView Lightweight Charts for MVP, evaluate TradingView Widget for premium features.

### Data Architecture

```
+------------------+     +------------------+     +------------------+
|   Exchange       |     |   VeloZ          |     |   Chart          |
|   WebSocket      | --> |   Gateway        | --> |   Component      |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +------------------+     +------------------+
|   Real-time      |     |   Data           |     |   Indicator      |
|   Candles        |     |   Aggregation    |     |   Calculation    |
+------------------+     +------------------+     +------------------+
```

### API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/candles` | GET | Historical candle data |
| `/api/stream/candles` | SSE | Real-time candle updates |
| `/api/indicators/calculate` | POST | Server-side indicator calculation |
| `/api/drawings` | GET/POST | User drawing persistence |

## Non-Functional Requirements

### Browser Support

| Browser | Minimum Version |
|---------|-----------------|
| Chrome | 90+ |
| Firefox | 88+ |
| Safari | 14+ |
| Edge | 90+ |

### Accessibility

| Requirement | Implementation |
|-------------|----------------|
| Keyboard navigation | Arrow keys for pan, +/- for zoom |
| Screen reader | Price announcements |
| Color blindness | Configurable colors |
| High contrast | Dark/light themes |

### Mobile Support

| Feature | Mobile Implementation |
|---------|----------------------|
| Touch zoom | Pinch gesture |
| Touch pan | Single finger drag |
| Indicator menu | Bottom sheet |
| Drawing tools | Simplified toolbar |

## Sprint Planning

### Sprint 1-2: Basic Charting (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Chart library integration | 3 days | Frontend |
| Candlestick rendering | 2 days | Frontend |
| Historical data API | 2 days | Backend |
| Real-time updates | 2 days | Backend |
| Zoom/pan controls | 1 day | Frontend |

**Deliverable:** Basic candlestick chart with real-time updates

### Sprint 3-4: Technical Indicators (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Indicator calculation engine | 3 days | Backend |
| 15 core indicators | 4 days | Backend |
| Indicator UI | 2 days | Frontend |
| Indicator configuration | 1 day | Frontend |

**Deliverable:** 15 indicators with configuration

### Sprint 5-6: Additional Indicators + Drawing (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| 15 more indicators | 3 days | Backend |
| Drawing tools | 4 days | Frontend |
| Drawing persistence | 2 days | Backend |
| Drawing UI | 1 day | Frontend |

**Deliverable:** 30 indicators + drawing tools

### Sprint 7-8: Advanced Features (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Multi-chart layout | 3 days | Frontend |
| Trade from chart | 3 days | Frontend |
| Mobile optimization | 2 days | Frontend |
| Performance optimization | 2 days | Full stack |

**Deliverable:** Multi-chart, trade integration, mobile support

## Dependencies

### External Dependencies

| Dependency | Type | Risk |
|------------|------|------|
| Chart library | Third-party | Low |
| Historical data | Exchange API | Low |
| Real-time data | WebSocket | Low |

### Internal Dependencies

| Dependency | Feature | Impact |
|------------|---------|--------|
| GUI Configuration | Symbol selection | Blocking |
| Strategy Marketplace | Performance overlay | Enhancing |

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Chart library limitations | Medium | Medium | Evaluate multiple options |
| Performance issues | Medium | High | Virtualization, lazy loading |
| Mobile UX challenges | Medium | Medium | Progressive enhancement |
| Data latency | Low | High | Local caching, optimistic updates |

## Success Criteria

### MVP (Month 3)

- [ ] Candlestick chart with real-time updates
- [ ] 15 technical indicators
- [ ] Basic zoom/pan
- [ ] Single timeframe

### Full Release (Month 5)

- [ ] 30+ technical indicators
- [ ] Drawing tools
- [ ] Multi-chart layout
- [ ] Trade from chart
- [ ] Mobile responsive
- [ ] Chart usage > 80% of sessions

---

*Document Version: 1.0*
*Last Updated: 2026-02-25*
