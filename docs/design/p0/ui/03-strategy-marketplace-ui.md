# P0 Feature 3: Strategy Marketplace UI

## Overview

The Strategy Marketplace provides a curated collection of trading strategies that users can browse, preview, configure, and deploy. It should feel like an app store for trading strategies, with clear information about performance, risk, and requirements.

---

## User Flow

```
[Browse/Search] -> [Strategy Detail] -> [Configure Parameters] -> [Backtest Preview] -> [Deploy]
```

---

## Main Marketplace View

### Layout Structure

```
+------------------------------------------------------------------+
|  Strategy Marketplace                                            |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  | [Search Icon] Search strategies...              [Filters v]  ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  Categories:                                                     |
|  [All] [Momentum] [Mean Reversion] [Grid] [Market Making] [DCA]  |
|                                                                  |
|  Sort by: [Performance v]    Risk Level: [All v]    [Grid][List] |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------+  +--------------------+  +---------------+|
|  |                    |  |                    |  |               ||
|  | [Strategy Card 1]  |  | [Strategy Card 2]  |  | [Card 3]      ||
|  |                    |  |                    |  |               ||
|  +--------------------+  +--------------------+  +---------------+|
|                                                                  |
|  +--------------------+  +--------------------+  +---------------+|
|  |                    |  |                    |  |               ||
|  | [Strategy Card 4]  |  | [Strategy Card 5]  |  | [Card 6]      ||
|  |                    |  |                    |  |               ||
|  +--------------------+  +--------------------+  +---------------+|
|                                                                  |
|  [Load More]                                                     |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Strategy Card Component

### Standard Card View

```
+------------------------------------------+
|  [Momentum Badge]           [Star] 4.8   |
|                                          |
|  Trend Following Pro                     |
|  by VeloZ Team                           |
|                                          |
|  Captures momentum breakouts in          |
|  trending markets with adaptive          |
|  position sizing.                        |
|                                          |
|  +--------------------------------------+|
|  |  +127.4%    |  32.1%    |  1.8      ||
|  |  12M Return |  Max DD   |  Sharpe   ||
|  +--------------------------------------+|
|                                          |
|  Risk: [====------] Medium               |
|                                          |
|  [Preview]              [Use Strategy]   |
+------------------------------------------+
```

### Specifications
- **Card Size**: 320px width, auto height
- **Badge Colors** (by strategy type):
  - Momentum: `#7c3aed` (purple)
  - Mean Reversion: `#2563eb` (blue)
  - Grid: `#059669` (green)
  - Market Making: `#d97706` (amber)
  - DCA: `#db2777` (pink)
- **Rating**: Star icon + number, text-warning color
- **Metrics Row**: 3-column grid, monospace font for values
- **Risk Indicator**: 10-segment bar, filled segments indicate risk level

### Card Hover State
```
+------------------------------------------+
|  [Momentum Badge]           [Star] 4.8   |
|                                          |
|  Trend Following Pro                     |
|  by VeloZ Team                           |
|                                          |
|  +--------------------------------------+|
|  |  Quick Stats                         ||
|  |                                      ||
|  |  Win Rate:        68.5%              ||
|  |  Avg Trade:       +2.3%              ||
|  |  Trades/Month:    45                 ||
|  |  Min Capital:     $1,000             ||
|  +--------------------------------------+|
|                                          |
|  [Preview]              [Use Strategy]   |
+------------------------------------------+
```

---

## Filter Panel

### Expanded Filters

```
+------------------------------------------------------------------+
|  Filters                                              [Clear All] |
+------------------------------------------------------------------+
|                                                                  |
|  Strategy Type                    Risk Level                     |
|  [ ] Momentum                     [ ] Low                        |
|  [ ] Mean Reversion               [x] Medium                     |
|  [ ] Grid Trading                 [ ] High                       |
|  [ ] Market Making                                               |
|  [ ] DCA                          Time Frame                     |
|  [ ] Arbitrage                    [ ] Scalping (< 1 hour)        |
|                                   [x] Intraday (1-24 hours)      |
|  Market Type                      [ ] Swing (1-7 days)           |
|  [x] Spot                         [ ] Position (> 7 days)        |
|  [x] Futures                                                     |
|                                   Minimum Performance            |
|  Exchange                         [Slider: 0% ----[20%]--- 100%] |
|  [x] Binance                                                     |
|  [ ] OKX                          Maximum Drawdown               |
|  [ ] Bybit                        [Slider: 0% ----[30%]--- 100%] |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Strategy Detail Page

### Full Detail View

```
+------------------------------------------------------------------+
|  [<- Back to Marketplace]                                        |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  [Momentum Badge]                                            ||
|  |                                                              ||
|  |  Trend Following Pro                          [Star] 4.8     ||
|  |  by VeloZ Team                                (127 reviews)  ||
|  |                                                              ||
|  |  +----------------------------------------------------------+||
|  |  |  Captures momentum breakouts in trending markets with    |||
|  |  |  adaptive position sizing and dynamic stop-losses.       |||
|  |  |  Optimized for BTC and ETH perpetual futures.           |||
|  |  +----------------------------------------------------------+||
|  |                                                              ||
|  |  [Use This Strategy]    [Run Backtest]    [Add to Favorites] ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +---------------------------+  +-------------------------------+|
|  | Performance Metrics       |  | Risk Metrics                  ||
|  +---------------------------+  +-------------------------------+|
|  |                           |  |                               ||
|  |  12M Return    +127.4%    |  |  Max Drawdown      32.1%     ||
|  |  6M Return     +58.2%     |  |  Avg Drawdown      12.4%     ||
|  |  3M Return     +24.1%     |  |  Recovery Time     8 days    ||
|  |  1M Return     +8.7%      |  |  Volatility        24.5%     ||
|  |                           |  |                               ||
|  |  Win Rate      68.5%      |  |  Risk Level        Medium    ||
|  |  Profit Factor 2.4        |  |  [====------]                ||
|  |  Sharpe Ratio  1.8        |  |                               ||
|  |                           |  |                               ||
|  +---------------------------+  +-------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

### Performance Chart Section

```
+------------------------------------------------------------------+
|  Historical Performance                                          |
+------------------------------------------------------------------+
|                                                                  |
|  [1M] [3M] [6M] [1Y] [All]                    vs [BTC] [ETH]     |
|                                                                  |
|  +--------------------------------------------------------------+|
|  |                                                    ^         ||
|  |                                               /\  /          ||
|  |                                    /\        /  \/           ||
|  |                          /\      /  \      /                 ||
|  |                    /\   /  \    /    \    /                  ||
|  |              /\   /  \ /    \  /      \  /                   ||
|  |         /\ /  \ /     v      \/        \/                    ||
|  |    /\  /  v                                                  ||
|  |   /  \/                                                      ||
|  |  /                                                           ||
|  | /                                                            ||
|  |/____________________________________________________________ ||
|  | Jan   Feb   Mar   Apr   May   Jun   Jul   Aug   Sep   Oct   ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  [---] Strategy    [---] Benchmark (BTC)                        |
|                                                                  |
+------------------------------------------------------------------+
```

### Trade Statistics Section

```
+------------------------------------------------------------------+
|  Trade Statistics                                                |
+------------------------------------------------------------------+
|                                                                  |
|  +---------------------------+  +-------------------------------+|
|  | Trade Distribution        |  | Monthly Returns               ||
|  +---------------------------+  +-------------------------------+|
|  |                           |  |                               ||
|  |  Total Trades:    1,247   |  |  Jan 2026:  +12.4%           ||
|  |  Winning:         854     |  |  Dec 2025:  +8.7%            ||
|  |  Losing:          393     |  |  Nov 2025:  -3.2%            ||
|  |                           |  |  Oct 2025:  +15.1%           ||
|  |  Avg Win:         +3.8%   |  |  Sep 2025:  +6.8%            ||
|  |  Avg Loss:        -1.6%   |  |  Aug 2025:  +22.3%           ||
|  |                           |  |                               ||
|  |  Best Trade:      +24.5%  |  |  [Show All Months]           ||
|  |  Worst Trade:     -8.2%   |  |                               ||
|  |                           |  |                               ||
|  +---------------------------+  +-------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

### Requirements Section

```
+------------------------------------------------------------------+
|  Requirements                                                    |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Minimum Capital         $1,000 USDT                         ||
|  |  Recommended Capital     $5,000 USDT                         ||
|  |                                                              ||
|  |  Supported Exchanges     Binance, OKX, Bybit                 ||
|  |  Supported Markets       BTC/USDT, ETH/USDT Perpetual        ||
|  |                                                              ||
|  |  API Permissions         Read, Spot Trading, Futures Trading ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

---

## Parameter Configuration Screen

### Configuration Modal

```
+------------------------------------------------------------------+
|  Configure Strategy Parameters                           [X]     |
+------------------------------------------------------------------+
|                                                                  |
|  Trend Following Pro                                             |
|                                                                  |
|  +--------------------------------------------------------------+|
|  | [i] Parameters affect strategy behavior. Start with defaults ||
|  |     and adjust based on backtest results.                    ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Entry Parameters                                             ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Momentum Period                                             ||
|  |  +--------------------------------------------------+        ||
|  |  | 14                                           | bars|       ||
|  |  +--------------------------------------------------+        ||
|  |  Number of bars to calculate momentum (10-50)                ||
|  |                                                              ||
|  |  Entry Threshold                                             ||
|  |  +--------------------------------------------------+        ||
|  |  | 2.0                                          | %  |       ||
|  |  +--------------------------------------------------+        ||
|  |  Minimum momentum strength to trigger entry (0.5-5.0)        ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Exit Parameters                                              ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Take Profit                                                 ||
|  |  +--------------------------------------------------+        ||
|  |  | 5.0                                          | %  |       ||
|  |  +--------------------------------------------------+        ||
|  |                                                              ||
|  |  Stop Loss                                                   ||
|  |  +--------------------------------------------------+        ||
|  |  | 2.0                                          | %  |       ||
|  |  +--------------------------------------------------+        ||
|  |                                                              ||
|  |  Trailing Stop                                               ||
|  |  [x] Enable    Distance: [1.5] %                             ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Position Sizing                                              ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Position Size Mode                                          ||
|  |  ( ) Fixed Amount    (x) Percentage    ( ) Kelly Criterion   ||
|  |                                                              ||
|  |  Position Size                                               ||
|  |  +--------------------------------------------------+        ||
|  |  | 10                                           | %  |       ||
|  |  +--------------------------------------------------+        ||
|  |  Percentage of account balance per trade                     ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  [Reset to Defaults]        [Run Backtest]  [Deploy Strategy]    |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Backtest Results Visualization

### Backtest Results Panel

```
+------------------------------------------------------------------+
|  Backtest Results                                                |
+------------------------------------------------------------------+
|                                                                  |
|  Configuration: Momentum=14, Entry=2.0%, TP=5.0%, SL=2.0%        |
|  Period: Jan 2025 - Jan 2026 (12 months)                         |
|                                                                  |
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  +------------------+  +------------------+  +--------------+ ||
|  |  |    +87.4%        |  |    28.5%         |  |    1.65     | ||
|  |  |    Total Return  |  |    Max Drawdown  |  |    Sharpe   | ||
|  |  +------------------+  +------------------+  +--------------+ ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Equity Curve                                                 ||
|  +--------------------------------------------------------------+|
|  |                                                    ^         ||
|  |                                               /\  /          ||
|  |                                    /\        /  \/           ||
|  |                          /\      /  \      /                 ||
|  |                    /\   /  \    /    \    /                  ||
|  |              /\   /  \ /    \  /      \  /                   ||
|  |         /\ /  \ /     v      \/        \/                    ||
|  |    /\  /  v                                                  ||
|  |   /  \/                                                      ||
|  |  /                                                           ||
|  | /                                                            ||
|  |/____________________________________________________________ ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +---------------------------+  +-------------------------------+|
|  | Trade Summary             |  | Drawdown Analysis             ||
|  +---------------------------+  +-------------------------------+|
|  |                           |  |                               ||
|  |  Total Trades:    156     |  |  Max Drawdown:    28.5%      ||
|  |  Win Rate:        64.1%   |  |  Avg Drawdown:    8.2%       ||
|  |  Profit Factor:   2.1     |  |  Max DD Duration: 23 days    ||
|  |  Avg Trade:       +0.56%  |  |  Recovery Factor: 3.1        ||
|  |                           |  |                               ||
|  +---------------------------+  +-------------------------------+|
|                                                                  |
|  [Export Results]    [Compare Parameters]    [Deploy Strategy]   |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Strategy Comparison View

### Side-by-Side Comparison

```
+------------------------------------------------------------------+
|  Compare Strategies                                              |
+------------------------------------------------------------------+
|                                                                  |
|  Select strategies to compare (max 4):                           |
|  [Trend Following Pro] [x]  [Grid Bot Classic] [x]  [+ Add]      |
|                                                                  |
|  +-------------------------------+  +---------------------------+|
|  | Trend Following Pro          |  | Grid Bot Classic          ||
|  +-------------------------------+  +---------------------------+|
|  |                               |  |                           ||
|  | Type:     Momentum            |  | Type:     Grid            ||
|  | Risk:     Medium              |  | Risk:     Low             ||
|  |                               |  |                           ||
|  | 12M Return:   +127.4%         |  | 12M Return:   +45.2%      ||
|  | Max DD:       32.1%           |  | Max DD:       12.4%       ||
|  | Sharpe:       1.8             |  | Sharpe:       1.2         ||
|  | Win Rate:     68.5%           |  | Win Rate:     82.1%       ||
|  |                               |  |                           ||
|  | Min Capital:  $1,000          |  | Min Capital:  $500        ||
|  | Trades/Mo:    45              |  | Trades/Mo:    120         ||
|  |                               |  |                           ||
|  | [Use This]                    |  | [Use This]                ||
|  |                               |  |                           ||
|  +-------------------------------+  +---------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Performance Comparison Chart                                 ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  [Chart showing both equity curves overlaid]                 ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

---

## Responsive Layouts

### Desktop (>= 1024px)
- 3-column grid for strategy cards
- Side-by-side comparison (up to 4)
- Full filter panel visible
- Charts with detailed tooltips

### Tablet (768px - 1023px)
- 2-column grid for strategy cards
- Collapsible filter panel
- Stacked comparison view
- Simplified charts

### Mobile (< 768px)
- Single-column card list
- Bottom sheet for filters
- Full-screen strategy detail
- Swipeable comparison cards

---

## Accessibility

### Strategy Cards
- Cards are focusable and keyboard navigable
- Rating announced as "4.8 out of 5 stars"
- Risk level announced as "Medium risk, 4 out of 10"

### Charts
- Alternative text descriptions for charts
- Data table available as fallback
- High contrast mode for chart colors

### Filters
- Filter changes announced to screen readers
- Clear all button announces number of filters cleared
- Checkbox states properly announced

---

## User Testing Recommendations

### Test Scenarios
1. **Browse and Filter**: Find a low-risk momentum strategy
2. **Compare Strategies**: Compare 3 strategies and choose one
3. **Configure and Backtest**: Adjust parameters and run backtest
4. **Deploy Strategy**: Complete deployment flow
5. **Mobile Browse**: Complete flow on mobile device

### Metrics to Track
- Strategy view-to-deploy conversion rate
- Average time on strategy detail page
- Filter usage patterns
- Backtest run frequency
- Comparison feature usage

### A/B Testing Ideas
- Card layout (grid vs. list default)
- Performance metric prominence
- Backtest CTA placement
- Filter panel position (sidebar vs. top)
