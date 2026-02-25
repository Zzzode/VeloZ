# P0-4: Real-time Charting System

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase
**Complexity**: Medium
**Estimated Time**: 3-4 weeks

---

## 1. Overview

### 1.1 Goals

- Provide professional-grade trading charts with real-time updates
- Support multiple timeframes and chart types
- Include common technical indicators
- Enable chart-based order placement
- Maintain 60fps performance with live data

### 1.2 Non-Goals

- Custom indicator scripting language
- Social/community chart sharing
- Advanced drawing tools (Fibonacci, Elliott Wave)
- Multi-chart layouts (initially)

---

## 2. Architecture

### 2.1 High-Level Architecture

```
                    Charting System Architecture
                    ============================

+------------------------------------------------------------------+
|                         React UI Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Chart          |  | Indicator      |  | Order          |       |
|  | Container      |  | Panel          |  | Overlay        |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Lightweight    |  | Indicator      |  | Order          |       |
|  | Charts API     |  | Calculator     |  | Manager        |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (WebSocket + REST)
+------------------------------------------------------------------+
|                      Python Gateway Layer                         |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Market Data    |  | OHLCV          |  | Order          |       |
|  | Aggregator     |  | Cache          |  | Router         |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Exchange       |  | Historical     |  | Execution      |       |
|  | WebSocket      |  | Data Store     |  | Engine         |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
```

### 2.2 Data Flow Diagram

```
                        Chart Data Flow
                        ===============

Exchange WebSocket
        |
        v
+------------------+
| Gateway          |
| - Aggregate      |
| - Normalize      |
| - Cache          |
+------------------+
        |
        v (WebSocket)
+------------------+
| UI WebSocket     |
| Handler          |
+------------------+
        |
        v
+------------------+
| Chart Store      |
| (Zustand)        |
+------------------+
        |
        v
+------------------+
| Lightweight      |
| Charts           |
+------------------+
        |
        v
+------------------+
| Canvas Render    |
| (60fps)          |
+------------------+
```

---

## 3. Technology Stack

### 3.1 Chart Library: TradingView Lightweight Charts

| Feature | Support |
|---------|---------|
| Candlestick charts | Yes |
| Line charts | Yes |
| Area charts | Yes |
| Histogram | Yes |
| Real-time updates | Yes |
| Custom series | Yes |
| Crosshair | Yes |
| Price scale | Yes |
| Time scale | Yes |
| Markers | Yes |
| Price lines | Yes |

**Justification**: Already integrated in VeloZ UI (package.json shows `lightweight-charts: ^5.1.0`), lightweight (~45KB), performant, MIT licensed.

### 3.2 Indicator Library

| Approach | Pros | Cons |
|----------|------|------|
| **Custom implementation** | Full control, optimized | Development time |
| technicalindicators | Comprehensive | Large bundle |
| ta-lib (WASM) | Industry standard | Complex setup |

**Recommendation**: Custom implementation for common indicators (SMA, EMA, RSI, MACD, Bollinger Bands) with option to add ta-lib later.

---

## 4. Chart Components

### 4.1 Main Chart Layout

```
+------------------------------------------------------------------+
|  BTCUSDT | Binance        [1m] [5m] [15m] [1h] [4h] [1D] [1W]    |
+------------------------------------------------------------------+
|  [Candle v] [Indicators v] [Settings v]              [Fullscreen]|
+------------------------------------------------------------------+
|                                                                   |
|  Price                                                    Volume  |
|  45,230 |                                    ___          |       |
|         |                               ___/   \___       |       |
|  45,200 |                          ___/            \      |  500  |
|         |                     ___/                  \     |       |
|  45,170 |                ___/                        \    |       |
|         |           ___/                              \   |  250  |
|  45,140 |      ___/                                    \  |       |
|         | ___/                                          \ |       |
|  45,110 |/                                               \|    0  |
|         +--------------------------------------------------+      |
|         09:00    10:00    11:00    12:00    13:00    14:00        |
|                                                                   |
+------------------------------------------------------------------+
|  RSI(14)                                                          |
|  70 |----------------------------------------------------        |
|     |            ___                                              |
|  50 |       ___/   \___                  ___                      |
|     |  ___/            \___         ___/   \___                   |
|  30 |/                     \___/                                  |
|     +----------------------------------------------------        |
+------------------------------------------------------------------+
|  Open: 45,123 | High: 45,456 | Low: 45,012 | Close: 45,234       |
|  Change: +0.24% | Volume: 1,234.56 BTC                            |
+------------------------------------------------------------------+
```

### 4.2 Chart Container Component

```typescript
// src/features/charting/ChartContainer.tsx
import { createChart, IChartApi, ISeriesApi } from 'lightweight-charts';

interface ChartContainerProps {
  symbol: string;
  exchange: string;
  initialTimeframe: Timeframe;
}

export function ChartContainer({
  symbol,
  exchange,
  initialTimeframe,
}: ChartContainerProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candleSeriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const volumeSeriesRef = useRef<ISeriesApi<'Histogram'> | null>(null);

  const [timeframe, setTimeframe] = useState(initialTimeframe);
  const [indicators, setIndicators] = useState<IndicatorConfig[]>([]);

  const { theme } = useTheme();

  // Initialize chart
  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height: 500,
      layout: {
        background: { type: 'solid', color: theme === 'dark' ? '#1a1a1a' : '#ffffff' },
        textColor: theme === 'dark' ? '#d1d4dc' : '#191919',
      },
      grid: {
        vertLines: { color: theme === 'dark' ? '#2B2B43' : '#e1e1e1' },
        horzLines: { color: theme === 'dark' ? '#2B2B43' : '#e1e1e1' },
      },
      crosshair: {
        mode: 1, // Normal
      },
      rightPriceScale: {
        borderColor: theme === 'dark' ? '#2B2B43' : '#e1e1e1',
      },
      timeScale: {
        borderColor: theme === 'dark' ? '#2B2B43' : '#e1e1e1',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    // Add candlestick series
    const candleSeries = chart.addCandlestickSeries({
      upColor: '#26a69a',
      downColor: '#ef5350',
      borderVisible: false,
      wickUpColor: '#26a69a',
      wickDownColor: '#ef5350',
    });

    // Add volume series
    const volumeSeries = chart.addHistogramSeries({
      color: '#26a69a',
      priceFormat: { type: 'volume' },
      priceScaleId: 'volume',
    });

    chart.priceScale('volume').applyOptions({
      scaleMargins: { top: 0.8, bottom: 0 },
    });

    chartRef.current = chart;
    candleSeriesRef.current = candleSeries;
    volumeSeriesRef.current = volumeSeries;

    // Handle resize
    const handleResize = () => {
      if (containerRef.current) {
        chart.applyOptions({ width: containerRef.current.clientWidth });
      }
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, [theme]);

  // Load historical data
  useEffect(() => {
    loadHistoricalData(symbol, exchange, timeframe).then((data) => {
      candleSeriesRef.current?.setData(data.candles);
      volumeSeriesRef.current?.setData(data.volume);
    });
  }, [symbol, exchange, timeframe]);

  // Subscribe to real-time updates
  useEffect(() => {
    const ws = subscribeToKlines(symbol, exchange, timeframe, (kline) => {
      candleSeriesRef.current?.update(kline.candle);
      volumeSeriesRef.current?.update(kline.volume);
    });

    return () => ws.close();
  }, [symbol, exchange, timeframe]);

  return (
    <div className="chart-container">
      <ChartHeader
        symbol={symbol}
        exchange={exchange}
        timeframe={timeframe}
        onTimeframeChange={setTimeframe}
        onIndicatorAdd={(indicator) => setIndicators([...indicators, indicator])}
      />
      <div ref={containerRef} className="chart-canvas" />
      <IndicatorPanels
        chart={chartRef.current}
        indicators={indicators}
        symbol={symbol}
        exchange={exchange}
        timeframe={timeframe}
      />
      <ChartFooter symbol={symbol} exchange={exchange} />
    </div>
  );
}
```

---

## 5. Technical Indicators

### 5.1 Supported Indicators

| Indicator | Category | Parameters |
|-----------|----------|------------|
| SMA | Trend | Period |
| EMA | Trend | Period |
| Bollinger Bands | Volatility | Period, StdDev |
| RSI | Momentum | Period |
| MACD | Momentum | Fast, Slow, Signal |
| Stochastic | Momentum | K, D, Smooth |
| ATR | Volatility | Period |
| Volume | Volume | - |
| VWAP | Volume | - |

### 5.2 Indicator Calculator

```typescript
// src/features/charting/indicators/calculator.ts

export interface OHLCVData {
  time: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface IndicatorResult {
  time: number;
  value: number;
  [key: string]: number;  // For multi-line indicators
}

// Simple Moving Average
export function calculateSMA(data: OHLCVData[], period: number): IndicatorResult[] {
  const results: IndicatorResult[] = [];

  for (let i = period - 1; i < data.length; i++) {
    let sum = 0;
    for (let j = 0; j < period; j++) {
      sum += data[i - j].close;
    }
    results.push({
      time: data[i].time,
      value: sum / period,
    });
  }

  return results;
}

// Exponential Moving Average
export function calculateEMA(data: OHLCVData[], period: number): IndicatorResult[] {
  const results: IndicatorResult[] = [];
  const multiplier = 2 / (period + 1);

  // First EMA is SMA
  let sum = 0;
  for (let i = 0; i < period; i++) {
    sum += data[i].close;
  }
  let ema = sum / period;
  results.push({ time: data[period - 1].time, value: ema });

  // Calculate EMA for remaining data
  for (let i = period; i < data.length; i++) {
    ema = (data[i].close - ema) * multiplier + ema;
    results.push({ time: data[i].time, value: ema });
  }

  return results;
}

// Relative Strength Index
export function calculateRSI(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  const results: IndicatorResult[] = [];
  const gains: number[] = [];
  const losses: number[] = [];

  // Calculate price changes
  for (let i = 1; i < data.length; i++) {
    const change = data[i].close - data[i - 1].close;
    gains.push(change > 0 ? change : 0);
    losses.push(change < 0 ? -change : 0);
  }

  // First RSI
  let avgGain = gains.slice(0, period).reduce((a, b) => a + b, 0) / period;
  let avgLoss = losses.slice(0, period).reduce((a, b) => a + b, 0) / period;

  for (let i = period; i < data.length; i++) {
    if (i > period) {
      avgGain = (avgGain * (period - 1) + gains[i - 1]) / period;
      avgLoss = (avgLoss * (period - 1) + losses[i - 1]) / period;
    }

    const rs = avgLoss === 0 ? 100 : avgGain / avgLoss;
    const rsi = 100 - 100 / (1 + rs);

    results.push({ time: data[i].time, value: rsi });
  }

  return results;
}

// MACD
export function calculateMACD(
  data: OHLCVData[],
  fastPeriod: number = 12,
  slowPeriod: number = 26,
  signalPeriod: number = 9
): IndicatorResult[] {
  const fastEMA = calculateEMA(data, fastPeriod);
  const slowEMA = calculateEMA(data, slowPeriod);

  // Align data
  const startIndex = slowPeriod - fastPeriod;
  const macdLine: { time: number; value: number }[] = [];

  for (let i = 0; i < slowEMA.length; i++) {
    macdLine.push({
      time: slowEMA[i].time,
      value: fastEMA[i + startIndex].value - slowEMA[i].value,
    });
  }

  // Calculate signal line (EMA of MACD)
  const signalLine = calculateEMAFromValues(macdLine, signalPeriod);

  // Combine results
  const results: IndicatorResult[] = [];
  const signalStartIndex = signalPeriod - 1;

  for (let i = signalStartIndex; i < macdLine.length; i++) {
    const macd = macdLine[i].value;
    const signal = signalLine[i - signalStartIndex].value;
    results.push({
      time: macdLine[i].time,
      value: macd,
      signal,
      histogram: macd - signal,
    });
  }

  return results;
}

// Bollinger Bands
export function calculateBollingerBands(
  data: OHLCVData[],
  period: number = 20,
  stdDev: number = 2
): IndicatorResult[] {
  const sma = calculateSMA(data, period);
  const results: IndicatorResult[] = [];

  for (let i = 0; i < sma.length; i++) {
    const dataIndex = i + period - 1;
    let sumSquares = 0;

    for (let j = 0; j < period; j++) {
      const diff = data[dataIndex - j].close - sma[i].value;
      sumSquares += diff * diff;
    }

    const std = Math.sqrt(sumSquares / period);

    results.push({
      time: sma[i].time,
      value: sma[i].value,  // Middle band
      upper: sma[i].value + stdDev * std,
      lower: sma[i].value - stdDev * std,
    });
  }

  return results;
}
```

### 5.3 Indicator Panel Component

```typescript
// src/features/charting/IndicatorPanel.tsx
interface IndicatorPanelProps {
  chart: IChartApi | null;
  indicator: IndicatorConfig;
  data: OHLCVData[];
  onRemove: () => void;
}

export function IndicatorPanel({
  chart,
  indicator,
  data,
  onRemove,
}: IndicatorPanelProps) {
  const seriesRef = useRef<ISeriesApi<any> | null>(null);

  useEffect(() => {
    if (!chart) return;

    // Calculate indicator values
    const values = calculateIndicator(indicator.type, data, indicator.params);

    // Create series based on indicator type
    switch (indicator.type) {
      case 'sma':
      case 'ema':
        seriesRef.current = chart.addLineSeries({
          color: indicator.color || '#2196F3',
          lineWidth: 2,
          priceScaleId: 'right',
        });
        seriesRef.current.setData(values);
        break;

      case 'bollinger':
        // Add three lines for Bollinger Bands
        const upperSeries = chart.addLineSeries({
          color: '#9C27B0',
          lineWidth: 1,
        });
        const middleSeries = chart.addLineSeries({
          color: '#9C27B0',
          lineWidth: 2,
        });
        const lowerSeries = chart.addLineSeries({
          color: '#9C27B0',
          lineWidth: 1,
        });

        upperSeries.setData(values.map((v) => ({ time: v.time, value: v.upper })));
        middleSeries.setData(values.map((v) => ({ time: v.time, value: v.value })));
        lowerSeries.setData(values.map((v) => ({ time: v.time, value: v.lower })));
        break;

      case 'rsi':
        // Create separate pane for RSI
        seriesRef.current = chart.addLineSeries({
          color: '#FF9800',
          lineWidth: 2,
          priceScaleId: 'rsi',
        });
        chart.priceScale('rsi').applyOptions({
          scaleMargins: { top: 0.8, bottom: 0 },
        });
        seriesRef.current.setData(values);

        // Add overbought/oversold lines
        // ...
        break;

      case 'macd':
        // Create MACD pane with histogram
        // ...
        break;
    }

    return () => {
      if (seriesRef.current) {
        chart.removeSeries(seriesRef.current);
      }
    };
  }, [chart, indicator, data]);

  return null;  // Indicator renders directly on chart
}
```

---

## 6. Real-time Data Pipeline

### 6.1 WebSocket Data Flow

```
                    WebSocket Data Pipeline
                    =======================

Exchange WS (Binance)
        |
        v
+------------------+
| Gateway WS       |
| Handler          |
| - Parse message  |
| - Normalize      |
+------------------+
        |
        v
+------------------+
| OHLCV Aggregator |
| - Build candles  |
| - Update cache   |
+------------------+
        |
        v
+------------------+
| Broadcast to     |
| UI clients       |
+------------------+
        |
        v (WebSocket)
+------------------+
| UI WS Handler    |
| - Parse message  |
| - Update store   |
+------------------+
        |
        v
+------------------+
| Chart Update     |
| - series.update()|
+------------------+
```

### 6.2 Gateway WebSocket Handler

```python
# apps/gateway/ws_market_data.py
import asyncio
import json
from typing import Dict, Set
from dataclasses import dataclass
from collections import defaultdict

@dataclass
class KlineData:
    time: int
    open: float
    high: float
    low: float
    close: float
    volume: float
    is_closed: bool

class MarketDataAggregator:
    def __init__(self):
        self.klines: Dict[str, Dict[str, KlineData]] = defaultdict(dict)
        self.subscribers: Dict[str, Set[asyncio.Queue]] = defaultdict(set)

    def get_subscription_key(self, symbol: str, exchange: str, timeframe: str) -> str:
        return f"{exchange}:{symbol}:{timeframe}"

    async def subscribe(
        self,
        symbol: str,
        exchange: str,
        timeframe: str,
        queue: asyncio.Queue
    ):
        key = self.get_subscription_key(symbol, exchange, timeframe)
        self.subscribers[key].add(queue)

        # Send current candle immediately
        if key in self.klines:
            await queue.put({
                'type': 'kline',
                'data': self.klines[key],
            })

    async def unsubscribe(
        self,
        symbol: str,
        exchange: str,
        timeframe: str,
        queue: asyncio.Queue
    ):
        key = self.get_subscription_key(symbol, exchange, timeframe)
        self.subscribers[key].discard(queue)

    async def on_kline(
        self,
        symbol: str,
        exchange: str,
        timeframe: str,
        kline: KlineData
    ):
        key = self.get_subscription_key(symbol, exchange, timeframe)

        # Update cache
        self.klines[key] = kline

        # Broadcast to subscribers
        message = {
            'type': 'kline',
            'symbol': symbol,
            'exchange': exchange,
            'timeframe': timeframe,
            'data': {
                'time': kline.time,
                'open': kline.open,
                'high': kline.high,
                'low': kline.low,
                'close': kline.close,
                'volume': kline.volume,
            },
        }

        for queue in self.subscribers[key]:
            try:
                await queue.put(message)
            except asyncio.QueueFull:
                pass  # Drop if client is slow


class MarketDataWebSocket:
    def __init__(self, aggregator: MarketDataAggregator):
        self.aggregator = aggregator

    async def handle_client(self, websocket):
        queue = asyncio.Queue(maxsize=100)
        subscriptions = set()

        try:
            # Handle incoming messages and outgoing data concurrently
            await asyncio.gather(
                self._handle_incoming(websocket, queue, subscriptions),
                self._handle_outgoing(websocket, queue),
            )
        finally:
            # Cleanup subscriptions
            for sub in subscriptions:
                symbol, exchange, timeframe = sub.split(':')
                await self.aggregator.unsubscribe(symbol, exchange, timeframe, queue)

    async def _handle_incoming(self, websocket, queue, subscriptions):
        async for message in websocket:
            data = json.loads(message)

            if data['action'] == 'subscribe':
                symbol = data['symbol']
                exchange = data['exchange']
                timeframe = data['timeframe']
                key = f"{symbol}:{exchange}:{timeframe}"

                if key not in subscriptions:
                    subscriptions.add(key)
                    await self.aggregator.subscribe(symbol, exchange, timeframe, queue)

            elif data['action'] == 'unsubscribe':
                symbol = data['symbol']
                exchange = data['exchange']
                timeframe = data['timeframe']
                key = f"{symbol}:{exchange}:{timeframe}"

                if key in subscriptions:
                    subscriptions.remove(key)
                    await self.aggregator.unsubscribe(symbol, exchange, timeframe, queue)

    async def _handle_outgoing(self, websocket, queue):
        while True:
            message = await queue.get()
            await websocket.send(json.dumps(message))
```

### 6.3 UI WebSocket Hook

```typescript
// src/hooks/useChartWebSocket.ts
interface UseChartWebSocketOptions {
  symbol: string;
  exchange: string;
  timeframe: Timeframe;
  onKline: (kline: KlineData) => void;
}

export function useChartWebSocket({
  symbol,
  exchange,
  timeframe,
  onKline,
}: UseChartWebSocketOptions) {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout>();

  useEffect(() => {
    const connect = () => {
      const ws = new WebSocket(`${WS_BASE}/ws/market`);

      ws.onopen = () => {
        ws.send(JSON.stringify({
          action: 'subscribe',
          symbol,
          exchange,
          timeframe,
        }));
      };

      ws.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.type === 'kline') {
          onKline(message.data);
        }
      };

      ws.onclose = () => {
        // Reconnect after delay
        reconnectTimeoutRef.current = setTimeout(connect, 3000);
      };

      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        ws.close();
      };

      wsRef.current = ws;
    };

    connect();

    return () => {
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [symbol, exchange, timeframe, onKline]);

  return wsRef;
}
```

---

## 7. Chart-Based Order Placement

### 7.1 Order Overlay UI

```
+------------------------------------------------------------------+
|                          Chart with Orders                        |
+------------------------------------------------------------------+
|                                                                   |
|  45,500 |----------------------------------------[Sell Limit]----| <- Drag to adjust
|         |                                                         |
|  45,400 |                                    ___                  |
|         |                               ___/   \___               |
|  45,300 |                          ___/            \              |
|         |                     ___/                  \             |
|  45,200 |                ___/                        \            |
|         |           ___/                              \           |
|  45,100 |      ___/                                    \          |
|         | ___/                                          \         |
|  45,000 |/                                               \        |
|         |                                                         |
|  44,900 |----------------------------------------[Buy Limit]-----| <- Drag to adjust
|         |                                                         |
|         +----------------------------------------------------------
|                                                                   |
|  [Open Orders]                                                    |
|  +--------------------------------------------------------------+ |
|  | Buy Limit  | 44,900 | 0.1 BTC | $4,490 |        [Cancel]    | |
|  | Sell Limit | 45,500 | 0.1 BTC | $4,550 |        [Cancel]    | |
|  +--------------------------------------------------------------+ |
|                                                                   |
+------------------------------------------------------------------+
```

### 7.2 Order Line Component

```typescript
// src/features/charting/OrderLine.tsx
interface OrderLineProps {
  chart: IChartApi;
  order: Order;
  onDrag: (newPrice: number) => void;
  onCancel: () => void;
}

export function OrderLine({ chart, order, onDrag, onCancel }: OrderLineProps) {
  const lineRef = useRef<IPriceLine | null>(null);

  useEffect(() => {
    const series = chart.addLineSeries({
      color: 'transparent',
      priceScaleId: 'right',
    });

    const line = series.createPriceLine({
      price: order.price,
      color: order.side === 'buy' ? '#26a69a' : '#ef5350',
      lineWidth: 2,
      lineStyle: 2,  // Dashed
      axisLabelVisible: true,
      title: `${order.side.toUpperCase()} ${order.quantity}`,
    });

    lineRef.current = line;

    return () => {
      chart.removeSeries(series);
    };
  }, [chart, order]);

  // Handle drag (simplified - actual implementation needs mouse events)
  const handleDrag = useCallback((event: MouseEvent) => {
    const price = chart.priceScale('right').coordinateToPrice(event.clientY);
    if (price) {
      onDrag(price);
    }
  }, [chart, onDrag]);

  return null;
}
```

### 7.3 Quick Order Panel

```typescript
// src/features/charting/QuickOrderPanel.tsx
interface QuickOrderPanelProps {
  symbol: string;
  exchange: string;
  currentPrice: number;
}

export function QuickOrderPanel({
  symbol,
  exchange,
  currentPrice,
}: QuickOrderPanelProps) {
  const [orderType, setOrderType] = useState<'limit' | 'market'>('limit');
  const [side, setSide] = useState<'buy' | 'sell'>('buy');
  const [price, setPrice] = useState(currentPrice);
  const [quantity, setQuantity] = useState(0);

  const { mutate: placeOrder, isLoading } = usePlaceOrder();

  const handleSubmit = () => {
    placeOrder({
      symbol,
      exchange,
      side,
      type: orderType,
      price: orderType === 'limit' ? price : undefined,
      quantity,
    });
  };

  return (
    <div className="quick-order-panel">
      <div className="order-type-toggle">
        <Button
          type={orderType === 'limit' ? 'primary' : 'default'}
          onClick={() => setOrderType('limit')}
        >
          Limit
        </Button>
        <Button
          type={orderType === 'market' ? 'primary' : 'default'}
          onClick={() => setOrderType('market')}
        >
          Market
        </Button>
      </div>

      <div className="side-toggle">
        <Button
          className={side === 'buy' ? 'buy-active' : ''}
          onClick={() => setSide('buy')}
        >
          Buy
        </Button>
        <Button
          className={side === 'sell' ? 'sell-active' : ''}
          onClick={() => setSide('sell')}
        >
          Sell
        </Button>
      </div>

      {orderType === 'limit' && (
        <Form.Item label="Price">
          <InputNumber
            value={price}
            onChange={(v) => setPrice(v || 0)}
            precision={2}
          />
        </Form.Item>
      )}

      <Form.Item label="Quantity">
        <InputNumber
          value={quantity}
          onChange={(v) => setQuantity(v || 0)}
          precision={8}
        />
      </Form.Item>

      <div className="quick-amounts">
        {[25, 50, 75, 100].map((pct) => (
          <Button
            key={pct}
            size="small"
            onClick={() => setQuantity(calculateQuantity(pct))}
          >
            {pct}%
          </Button>
        ))}
      </div>

      <Button
        type="primary"
        block
        onClick={handleSubmit}
        loading={isLoading}
        className={side === 'buy' ? 'buy-button' : 'sell-button'}
      >
        {side === 'buy' ? 'Buy' : 'Sell'} {symbol}
      </Button>
    </div>
  );
}
```

---

## 8. Performance Optimization

### 8.1 Data Windowing

```typescript
// Only keep visible data in memory
const MAX_CANDLES = 1000;

function windowData(data: CandleData[], visibleRange: TimeRange): CandleData[] {
  const startIndex = data.findIndex((d) => d.time >= visibleRange.from);
  const endIndex = data.findIndex((d) => d.time > visibleRange.to);

  // Add buffer for smooth scrolling
  const bufferSize = 50;
  const start = Math.max(0, startIndex - bufferSize);
  const end = Math.min(data.length, endIndex + bufferSize);

  return data.slice(start, end);
}
```

### 8.2 Update Batching

```typescript
// Batch updates to reduce render frequency
const updateQueue = useRef<KlineData[]>([]);
const updateTimer = useRef<NodeJS.Timeout>();

const batchUpdate = useCallback((kline: KlineData) => {
  updateQueue.current.push(kline);

  if (!updateTimer.current) {
    updateTimer.current = setTimeout(() => {
      // Apply all queued updates
      const updates = updateQueue.current;
      updateQueue.current = [];
      updateTimer.current = undefined;

      // Apply last update for each time
      const latestByTime = new Map<number, KlineData>();
      for (const update of updates) {
        latestByTime.set(update.time, update);
      }

      for (const kline of latestByTime.values()) {
        candleSeriesRef.current?.update(kline);
      }
    }, 16);  // ~60fps
  }
}, []);
```

### 8.3 Indicator Caching

```typescript
// Cache indicator calculations
const indicatorCache = useRef<Map<string, IndicatorResult[]>>(new Map());

function getCachedIndicator(
  type: string,
  params: Record<string, number>,
  data: OHLCVData[]
): IndicatorResult[] {
  const cacheKey = `${type}-${JSON.stringify(params)}-${data.length}`;

  if (indicatorCache.current.has(cacheKey)) {
    return indicatorCache.current.get(cacheKey)!;
  }

  const result = calculateIndicator(type, data, params);
  indicatorCache.current.set(cacheKey, result);

  // Limit cache size
  if (indicatorCache.current.size > 50) {
    const firstKey = indicatorCache.current.keys().next().value;
    indicatorCache.current.delete(firstKey);
  }

  return result;
}
```

---

## 9. API Contracts

### 9.1 Historical Data API

```
GET /api/market/klines
Query: {
  symbol: string,
  exchange: string,
  timeframe: string,
  from?: number,  // Unix timestamp
  to?: number,    // Unix timestamp
  limit?: number  // Default 500, max 1000
}
Response: {
  klines: Array<{
    time: number,
    open: number,
    high: number,
    low: number,
    close: number,
    volume: number
  }>
}
```

### 9.2 WebSocket Messages

```typescript
// Subscribe to klines
{
  action: 'subscribe',
  symbol: 'BTCUSDT',
  exchange: 'binance',
  timeframe: '1h'
}

// Kline update
{
  type: 'kline',
  symbol: 'BTCUSDT',
  exchange: 'binance',
  timeframe: '1h',
  data: {
    time: 1709251200,
    open: 45123.45,
    high: 45456.78,
    low: 45012.34,
    close: 45234.56,
    volume: 1234.567
  }
}

// Unsubscribe
{
  action: 'unsubscribe',
  symbol: 'BTCUSDT',
  exchange: 'binance',
  timeframe: '1h'
}
```

---

## 10. Implementation Plan

### 10.1 Phase Breakdown

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Core Chart** | 1 week | Chart container, candlesticks, volume |
| **Phase 2: Real-time** | 0.5 week | WebSocket integration, live updates |
| **Phase 3: Indicators** | 1 week | SMA, EMA, RSI, MACD, Bollinger |
| **Phase 4: Orders** | 0.5 week | Order overlay, quick order panel |
| **Phase 5: Polish** | 0.5 week | Performance optimization, testing |

### 10.2 Dependencies

```
                    Implementation Dependencies
                    ===========================

+------------------+
| Chart Container  |
+------------------+
        |
        +------------------+
        |                  |
        v                  v
+------------------+ +------------------+
| Historical Data  | | WebSocket        |
| API              | | Integration      |
+------------------+ +------------------+
        |                  |
        +------------------+
                |
                v
        +------------------+
        | Indicators       |
        +------------------+
                |
                v
        +------------------+
        | Order Overlay    |
        +------------------+
```

---

## 11. Testing Strategy

### 11.1 Unit Tests

```typescript
// tests/indicators.test.ts
describe('Indicator Calculations', () => {
  const testData: OHLCVData[] = [
    { time: 1, open: 100, high: 105, low: 98, close: 102, volume: 1000 },
    { time: 2, open: 102, high: 108, low: 101, close: 106, volume: 1200 },
    // ... more test data
  ];

  test('SMA calculates correctly', () => {
    const sma = calculateSMA(testData, 3);
    expect(sma[0].value).toBeCloseTo(103.33, 2);
  });

  test('RSI stays within 0-100', () => {
    const rsi = calculateRSI(testData, 14);
    for (const point of rsi) {
      expect(point.value).toBeGreaterThanOrEqual(0);
      expect(point.value).toBeLessThanOrEqual(100);
    }
  });

  test('MACD signal line lags MACD line', () => {
    const macd = calculateMACD(testData, 12, 26, 9);
    // Verify signal line calculation
  });
});
```

### 11.2 Performance Tests

```typescript
// tests/chart-performance.test.ts
describe('Chart Performance', () => {
  test('renders 1000 candles under 100ms', async () => {
    const start = performance.now();
    render(<ChartContainer symbol="BTCUSDT" exchange="binance" />);
    await waitFor(() => expect(screen.getByTestId('chart-canvas')).toBeVisible());
    const duration = performance.now() - start;
    expect(duration).toBeLessThan(100);
  });

  test('updates at 60fps with live data', async () => {
    // Measure frame rate during updates
  });
});
```

---

## 12. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Initial render time | < 100ms | Performance monitoring |
| Update latency | < 16ms (60fps) | Performance monitoring |
| Memory usage | < 100MB | Browser profiling |
| Chart usage rate | > 80% of users | Analytics |
| Order placement from chart | > 30% of orders | Analytics |

---

## 13. Related Documentation

- [Strategy Marketplace](03-strategy-marketplace.md)
- [UI Architecture](../ui-react-architecture.md)
- [Market Data Design](../design_03_market_data.md)
- [SSE API Reference](../../api/sse_api.md)
