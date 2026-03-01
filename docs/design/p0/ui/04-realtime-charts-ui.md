# P0 Feature 4: Real-Time Charts UI

## Overview

The charting system provides professional-grade, real-time price charts with technical indicators, drawing tools, and chart-based order entry. It should feel familiar to traders coming from platforms like TradingView while maintaining VeloZ's clean aesthetic.

---

## Chart Layouts

### Single Chart View

```
+------------------------------------------------------------------+
|  [Symbol Selector]  BTC/USDT Perp  |  [Timeframe] 1H  |  [Layout]|
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  | [Indicators v] [Drawing Tools] [Templates v]      [Settings] ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  68,450.25                                        +2.34%     ||
|  |  H: 68,892.00  L: 66,234.50  O: 67,125.00  C: 68,450.25     ||
|  |                                                              ||
|  |  +----------------------------------------------------------+||
|  |  |                                                          |||
|  |  |                                              ^           |||
|  |  |                                             /|           |||
|  |  |                                    |       / |           |||
|  |  |                              |     |      /  |           |||
|  |  |                        |     |     |     /   |           |||
|  |  |                  |     |     |     |    /    |           |||
|  |  |            |     |     |     |     |   /     |           |||
|  |  |      |     |     |     |     |     |  /      |           |||
|  |  | |    |     |     |     |     |     | /       |           |||
|  |  | |    |     |     |     |     |     |/        |           |||
|  |  |_|____|_____|_____|_____|_____|_____|_________|___________|||
|  |  |                                                          |||
|  |  | [Volume bars]                                            |||
|  |  |_|____|_____|_____|_____|_____|_____|_________|___________|||
|  |  |                                                          |||
|  |  | [RSI indicator]                              70 -------- |||
|  |  |                                              30 -------- |||
|  |  |__________________________________________________________|||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

### Split Chart View (2 Charts)

```
+------------------------------------------------------------------+
|  [Layout: Split Horizontal]                                      |
+------------------------------------------------------------------+
|                                                                  |
|  +-------------------------------+  +---------------------------+|
|  | BTC/USDT Perp  1H             |  | ETH/USDT Perp  1H         ||
|  |                               |  |                           ||
|  |  [Candlestick Chart]          |  |  [Candlestick Chart]      ||
|  |                               |  |                           ||
|  |                               |  |                           ||
|  |                               |  |                           ||
|  |                               |  |                           ||
|  |                               |  |                           ||
|  +-------------------------------+  +---------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

### Grid Layout (4 Charts)

```
+------------------------------------------------------------------+
|  [Layout: 2x2 Grid]                                              |
+------------------------------------------------------------------+
|                                                                  |
|  +-----------------------------+  +-----------------------------+|
|  | BTC/USDT  1H                |  | ETH/USDT  1H                ||
|  |                             |  |                             ||
|  |  [Chart]                    |  |  [Chart]                    ||
|  |                             |  |                             ||
|  +-----------------------------+  +-----------------------------+|
|                                                                  |
|  +-----------------------------+  +-----------------------------+|
|  | SOL/USDT  1H                |  | BNB/USDT  1H                ||
|  |                             |  |                             ||
|  |  [Chart]                    |  |  [Chart]                    ||
|  |                             |  |                             ||
|  +-----------------------------+  +-----------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

---

## Symbol Selector

### Dropdown with Search

```
+------------------------------------------+
|  [Search] BTC                            |
+------------------------------------------+
|                                          |
|  Recent                                  |
|  +--------------------------------------+|
|  | [Star] BTC/USDT Perp     68,450.25  ||
|  | [Star] ETH/USDT Perp      3,245.80  ||
|  | [Star] SOL/USDT Perp        142.35  ||
|  +--------------------------------------+|
|                                          |
|  All Markets                             |
|  +--------------------------------------+|
|  | [Spot] [Perp] [Futures]              ||
|  +--------------------------------------+|
|  | BTC/USDT Spot            68,448.00  ||
|  | BTC/USDT Perp            68,450.25  ||
|  | BTC/USDT 0328            68,520.00  ||
|  | ETH/USDT Spot             3,244.50  ||
|  | ETH/USDT Perp             3,245.80  ||
|  +--------------------------------------+|
|                                          |
+------------------------------------------+
```

### Specifications
- **Search**: Instant filter as user types
- **Recent**: Last 5 viewed symbols, starred items pinned
- **Star Toggle**: Click to add/remove from favorites
- **Price Display**: Real-time updates, green/red flash on change
- **Market Tabs**: Filter by market type

---

## Timeframe Selector

### Timeframe Bar

```
+------------------------------------------------------------------+
|  [1m] [5m] [15m] [30m] [1H] [4H] [1D] [1W] [1M]  |  [Custom...]  |
+------------------------------------------------------------------+
```

### Custom Timeframe Modal

```
+------------------------------------------+
|  Custom Timeframe                [X]     |
+------------------------------------------+
|                                          |
|  Value:  [5]                             |
|                                          |
|  Unit:   ( ) Minutes                     |
|          (x) Hours                       |
|          ( ) Days                        |
|                                          |
|  Preview: 5 Hour candles                 |
|                                          |
|              [Cancel]  [Apply]           |
|                                          |
+------------------------------------------+
```

### Specifications
- **Active State**: Primary background, white text
- **Hover State**: Background-secondary
- **Custom Button**: Opens modal for non-standard timeframes

---

## Indicator Menu

### Indicator Panel

```
+------------------------------------------+
|  Indicators                      [X]     |
+------------------------------------------+
|                                          |
|  [Search indicators...]                  |
|                                          |
|  Active Indicators                       |
|  +--------------------------------------+|
|  | [Eye] RSI (14)              [Gear] [X]|
|  | [Eye] EMA (20)              [Gear] [X]|
|  | [Eye] Volume                [Gear] [X]|
|  +--------------------------------------+|
|                                          |
|  Popular                                 |
|  +--------------------------------------+|
|  | Moving Average (MA)           [Add]  ||
|  | Exponential MA (EMA)          [Add]  ||
|  | Bollinger Bands               [Add]  ||
|  | MACD                          [Add]  ||
|  | RSI                           [Add]  ||
|  | Stochastic                    [Add]  ||
|  +--------------------------------------+|
|                                          |
|  Categories                              |
|  [Trend] [Momentum] [Volatility] [Volume]|
|                                          |
+------------------------------------------+
```

### Indicator Settings Modal

```
+------------------------------------------+
|  RSI Settings                    [X]     |
+------------------------------------------+
|                                          |
|  Period                                  |
|  +--------------------------------------+|
|  | 14                                   ||
|  +--------------------------------------+|
|                                          |
|  Source                                  |
|  [Close v]                               |
|                                          |
|  Overbought Level                        |
|  +--------------------------------------+|
|  | 70                                   ||
|  +--------------------------------------+|
|                                          |
|  Oversold Level                          |
|  +--------------------------------------+|
|  | 30                                   ||
|  +--------------------------------------+|
|                                          |
|  Style                                   |
|  Line Color: [#2563eb]  Width: [2]       |
|                                          |
|  [Reset Defaults]      [Cancel] [Apply]  |
|                                          |
+------------------------------------------+
```

---

## Drawing Tools Toolbar

### Vertical Toolbar

```
+----+
| [Cursor] |  <- Selection tool
+----+
| [Line]   |  <- Trend line
+----+
| [HLine]  |  <- Horizontal line
+----+
| [VLine]  |  <- Vertical line
+----+
| [Ray]    |  <- Ray
+----+
| [Channel]|  <- Parallel channel
+----+
| [Fib]    |  <- Fibonacci retracement
+----+
| [Rect]   |  <- Rectangle
+----+
| [Text]   |  <- Text annotation
+----+
| [Measure]|  <- Price/time measure
+----+
| [Trash]  |  <- Delete all drawings
+----+
```

### Drawing Tool Properties

```
+------------------------------------------+
|  Trend Line Properties                   |
+------------------------------------------+
|                                          |
|  Color:     [#dc2626]  (color picker)    |
|  Width:     [2] px                       |
|  Style:     [Solid v]                    |
|                                          |
|  [x] Extend left                         |
|  [x] Extend right                        |
|  [ ] Show price labels                   |
|                                          |
|  [Delete]              [Apply]           |
|                                          |
+------------------------------------------+
```

### Specifications
- **Toolbar Position**: Left side, vertical, 48px width
- **Tool Icons**: 24px, monochrome
- **Active Tool**: Primary background highlight
- **Tooltip**: Tool name on hover

---

## Chart-Based Order Entry

### Quick Order Panel (Right Side)

```
+---------------------------+
|  Quick Order              |
+---------------------------+
|                           |
|  BTC/USDT Perp            |
|  68,450.25                |
|                           |
|  [BUY]      [SELL]        |
|                           |
|  Order Type               |
|  [Limit v]                |
|                           |
|  Price                    |
|  +---------------------+  |
|  | 68,400.00           |  |
|  +---------------------+  |
|                           |
|  Amount                   |
|  +---------------------+  |
|  | 0.1                 |  |
|  +---------------------+  |
|  [25%] [50%] [75%] [100%] |
|                           |
|  Total: $6,840.00         |
|                           |
|  +---------------------+  |
|  |   Place Buy Order   |  |
|  +---------------------+  |
|                           |
+---------------------------+
```

### Chart Click Order Entry

When user clicks on chart with order mode active:

```
+------------------------------------------+
|  Place Order at 68,250.00?               |
+------------------------------------------+
|                                          |
|  [BUY Limit]    [SELL Limit]             |
|                                          |
|  Amount: [0.1] BTC                       |
|                                          |
|  [Cancel]              [Confirm]         |
|                                          |
+------------------------------------------+
```

### Order Lines on Chart

```
+------------------------------------------------------------------+
|                                                                  |
|  -------- 68,500.00 [TP] Take Profit (0.1 BTC) -------- [X]     |
|                                                                  |
|  ======== 68,450.25 [Current Price] ========                     |
|                                                                  |
|  -------- 68,200.00 [Limit Buy] (0.1 BTC) -------- [X] [Edit]   |
|                                                                  |
|  -------- 67,800.00 [SL] Stop Loss (0.1 BTC) -------- [X]       |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Order Lines**:
  - Buy orders: success color (#059669)
  - Sell orders: danger color (#dc2626)
  - Take profit: info color (#2563eb)
  - Stop loss: danger color (#dc2626)
- **Draggable**: Order lines can be dragged to modify price
- **Quick Actions**: X to cancel, Edit to modify

---

## Chart Settings

### Settings Panel

```
+------------------------------------------+
|  Chart Settings                  [X]     |
+------------------------------------------+
|                                          |
|  Appearance                              |
|  +--------------------------------------+|
|  |                                      ||
|  |  Chart Type                          ||
|  |  [Candles v]                         ||
|  |                                      ||
|  |  Color Scheme                        ||
|  |  (x) Green/Red                       ||
|  |  ( ) Blue/Red                        ||
|  |  ( ) Monochrome                      ||
|  |                                      ||
|  |  Background                          ||
|  |  (x) Light    ( ) Dark               ||
|  |                                      ||
|  +--------------------------------------+|
|                                          |
|  Grid & Axes                             |
|  +--------------------------------------+|
|  |                                      ||
|  |  [x] Show grid lines                 ||
|  |  [x] Show price scale                ||
|  |  [x] Show time scale                 ||
|  |  [ ] Logarithmic scale               ||
|  |                                      ||
|  +--------------------------------------+|
|                                          |
|  Trading                                 |
|  +--------------------------------------+|
|  |                                      ||
|  |  [x] Show open orders                ||
|  |  [x] Show positions                  ||
|  |  [x] Show trade history              ||
|  |  [x] Enable chart trading            ||
|  |                                      ||
|  +--------------------------------------+|
|                                          |
|  [Reset to Defaults]           [Save]    |
|                                          |
+------------------------------------------+
```

---

## Chart Types

### Candlestick (Default)

```
     |
     |  <- High (wick)
   +---+
   |   |  <- Body (open to close)
   |   |
   +---+
     |
     |  <- Low (wick)
```

### Line Chart

```
         /\
        /  \
       /    \    /\
      /      \  /  \
     /        \/    \
    /                \
```

### Bar Chart (OHLC)

```
     |
     |--- <- Close
     |
     |
  ---|  <- Open
     |
```

### Heikin Ashi

```
   +---+
   |   |  <- Smoothed candles
   +---+
     |
```

### Area Chart

```
         /\
        /  \
       /    \    /\
      /      \  /  \
     /        \/    \
    /////////////////  <- Filled area
```

---

## Real-Time Updates

### Price Ticker Animation

```
Before:  68,450.25
         [Flash green background]
After:   68,452.50
```

### Candle Formation

```
Live candle updates in real-time:
- Body extends as price moves
- Wicks update on new highs/lows
- Color changes based on open vs current
```

### Specifications
- **Update Frequency**: 100ms for price, 1s for candle
- **Flash Duration**: 200ms
- **Smooth Transitions**: CSS transitions for non-critical updates

---

## Responsive Layouts

### Desktop (>= 1024px)
- Full chart with all toolbars
- Side panel for quick orders
- Multiple chart layouts
- Floating indicator panel

### Tablet (768px - 1023px)
- Single chart view default
- Collapsible toolbars
- Bottom sheet for order entry
- Simplified indicator menu

### Mobile (< 768px)
- Full-screen chart
- Swipe for timeframes
- Bottom toolbar for essential tools
- Modal for order entry
- Pinch to zoom

### Mobile Chart Layout

```
+----------------------------------+
|  BTC/USDT  68,450.25  +2.34%    |
+----------------------------------+
|                                  |
|                                  |
|                                  |
|      [Full Screen Chart]         |
|                                  |
|                                  |
|                                  |
+----------------------------------+
|  [1m][5m][15m][1H][4H][1D][More] |
+----------------------------------+
|  [Draw] [Indicators] [Order]     |
+----------------------------------+
```

---

## Accessibility

### Keyboard Navigation
- Arrow keys to pan chart
- +/- to zoom
- Tab through toolbar items
- Enter to select tool
- Escape to cancel drawing

### Screen Reader Support
- Price changes announced
- Chart summary available ("BTC at 68,450, up 2.34% in last hour")
- Indicator values announced on focus
- Order placement confirmation

### Visual Accessibility
- High contrast mode for chart colors
- Configurable candle colors
- Grid line visibility toggle
- Minimum text size 12px

---

## Performance Considerations

### Data Loading
- Initial load: Last 500 candles
- Lazy load: Historical data on scroll
- WebSocket: Real-time updates
- Caching: Recent data in memory

### Rendering
- Canvas-based chart rendering
- RequestAnimationFrame for updates
- Debounced resize handling
- Virtual scrolling for large datasets

---

## User Testing Recommendations

### Test Scenarios
1. **Basic Navigation**: Change symbol, timeframe, zoom/pan
2. **Indicator Setup**: Add RSI, configure settings, remove
3. **Drawing Tools**: Draw trend line, modify, delete
4. **Chart Trading**: Place limit order from chart
5. **Multi-Chart**: Set up 4-chart grid layout
6. **Mobile Usage**: Complete trading flow on mobile

### Metrics to Track
- Chart load time
- Indicator usage frequency
- Drawing tool usage
- Chart-based order placement rate
- Layout preference distribution

### A/B Testing Ideas
- Default chart type (candles vs. line)
- Order panel position (right vs. bottom)
- Toolbar orientation (vertical vs. horizontal)
- Default timeframe for new users
