# Real-time Charts Testing

## 1. Test Strategy

### 1.1 Objectives

- Verify data accuracy by comparing with exchange data
- Validate performance (latency, FPS) under various conditions
- Ensure UI responsiveness under load
- Test WebSocket reliability and reconnection
- Verify indicator calculations
- Confirm cross-browser compatibility

### 1.2 Scope

| In Scope | Out of Scope |
|----------|--------------|
| Candlestick charts | Advanced charting (TradingView) |
| Line charts | 3D visualizations |
| Real-time price updates | Historical data export |
| Technical indicators | Custom indicator development |
| Order book visualization | Heatmaps |
| Trade history display | Social trading features |
| WebSocket connectivity | REST API fallback |

### 1.3 Supported Chart Types

| Chart Type | Data Source | Update Frequency |
|------------|-------------|------------------|
| Candlestick (OHLCV) | WebSocket Kline | 1s - 1d |
| Line (Price) | WebSocket Trade | Real-time |
| Order Book Depth | WebSocket Depth | 100ms |
| Trade History | WebSocket Trade | Real-time |
| Volume | WebSocket Kline | 1s - 1d |

## 2. Test Environment

### 2.1 Browser Matrix

| Browser | Version | Platform | Priority |
|---------|---------|----------|----------|
| Chrome | Latest | Windows/macOS/Linux | P0 |
| Firefox | Latest | Windows/macOS/Linux | P0 |
| Safari | Latest | macOS | P0 |
| Edge | Latest | Windows | P1 |
| Chrome Mobile | Latest | Android | P1 |
| Safari Mobile | Latest | iOS | P1 |

### 2.2 Test Data Sources

| Source | Purpose | Connection |
|--------|---------|------------|
| Binance Testnet | Functional testing | WebSocket |
| Binance Mainnet | Data accuracy | WebSocket (read-only) |
| Mock Server | Performance testing | Local WebSocket |
| Recorded Data | Regression testing | Playback |

### 2.3 Network Conditions

| Condition | Latency | Packet Loss | Purpose |
|-----------|---------|-------------|---------|
| Excellent | < 20ms | 0% | Baseline |
| Good | 50-100ms | 0% | Normal usage |
| Poor | 200-500ms | 5% | Degraded network |
| Very Poor | 500ms+ | 10% | Edge cases |
| Offline | N/A | 100% | Disconnection |

## 3. Test Cases

### 3.1 Data Accuracy

#### TC-CHT-001: Candlestick Data Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-CHT-001 |
| **Priority** | P0 |
| **Preconditions** | Connected to Binance mainnet (read-only) |

**Steps:**
1. Open BTCUSDT 1-minute chart in VeloZ
2. Open same chart on Binance website
3. Wait for 5 candles to form
4. Compare OHLCV values

**Expected Results:**
- [ ] Open price matches within 0.01%
- [ ] High price matches within 0.01%
- [ ] Low price matches within 0.01%
- [ ] Close price matches within 0.01%
- [ ] Volume matches within 1%
- [ ] Timestamp matches exactly

#### TC-CHT-002: Real-time Price Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-CHT-002 |
| **Priority** | P0 |
| **Preconditions** | Connected to exchange WebSocket |

**Steps:**
1. Display real-time price ticker
2. Record 100 price updates
3. Compare with exchange API prices
4. Calculate accuracy metrics

**Expected Results:**
- [ ] Price matches exchange within 0.01%
- [ ] Update latency < 100ms from exchange
- [ ] No missed updates
- [ ] Correct decimal precision

#### TC-CHT-003: Order Book Depth Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-CHT-003 |
| **Priority** | P0 |
| **Preconditions** | Order book visualization open |

**Steps:**
1. Display order book depth chart
2. Compare with exchange order book
3. Verify bid/ask levels
4. Verify quantities at each level

**Expected Results:**
- [ ] Top 20 bid levels match
- [ ] Top 20 ask levels match
- [ ] Quantities match within 0.1%
- [ ] Spread calculated correctly
- [ ] Mid price accurate

#### TC-CHT-004: Trade History Accuracy

| Field | Value |
|-------|-------|
| **ID** | TC-CHT-004 |
| **Priority** | P0 |
| **Preconditions** | Trade history panel open |

**Steps:**
1. Display recent trades
2. Compare with exchange trade history
3. Verify trade details

**Expected Results:**
- [ ] Trade prices match
- [ ] Trade quantities match
- [ ] Trade timestamps match
- [ ] Buy/sell side correct
- [ ] Trade IDs match

### 3.2 Performance Testing

#### TC-PERF-001: Chart Rendering FPS

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-001 |
| **Priority** | P0 |
| **Preconditions** | Chart displayed, DevTools open |

**Steps:**
1. Open candlestick chart
2. Enable FPS meter in DevTools
3. Observe FPS during normal updates
4. Observe FPS during rapid updates (100/sec)

**Expected Results:**
- [ ] Normal updates: 60 FPS
- [ ] Rapid updates: > 30 FPS
- [ ] No frame drops > 100ms
- [ ] Smooth animations

#### TC-PERF-002: Update Latency

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-002 |
| **Priority** | P0 |
| **Preconditions** | Performance monitoring enabled |

**Steps:**
1. Inject timestamp into WebSocket message
2. Measure time from receive to render
3. Record 1000 samples
4. Calculate P50, P95, P99 latency

**Expected Results:**
- [ ] P50 latency < 16ms (60 FPS)
- [ ] P95 latency < 50ms
- [ ] P99 latency < 100ms
- [ ] No updates > 500ms

#### TC-PERF-003: Memory Usage Under Load

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-003 |
| **Priority** | P0 |
| **Preconditions** | Chart running for extended period |

**Steps:**
1. Open chart with real-time updates
2. Monitor memory usage over 1 hour
3. Check for memory leaks
4. Verify garbage collection

**Expected Results:**
- [ ] Initial memory < 100 MB
- [ ] Memory growth < 10 MB/hour
- [ ] No unbounded growth
- [ ] GC runs regularly

#### TC-PERF-004: CPU Usage

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-004 |
| **Priority** | P0 |
| **Preconditions** | Chart running |

**Steps:**
1. Open chart
2. Monitor CPU usage in Task Manager
3. Measure during idle
4. Measure during active updates

**Expected Results:**
- [ ] Idle CPU < 5%
- [ ] Active CPU < 30%
- [ ] No CPU spikes > 50%
- [ ] Efficient when tab inactive

#### TC-PERF-005: Large Dataset Rendering

| Field | Value |
|-------|-------|
| **ID** | TC-PERF-005 |
| **Priority** | P1 |
| **Preconditions** | Historical data available |

**Test Data:**
| Candles | Target Render Time |
|---------|-------------------|
| 100 | < 100ms |
| 1,000 | < 500ms |
| 10,000 | < 2s |
| 100,000 | < 10s |

**Expected Results:**
- [ ] Render times within targets
- [ ] Smooth pan/zoom
- [ ] No UI freeze
- [ ] Progressive loading for large datasets

### 3.3 UI Responsiveness

#### TC-UI-001: Chart Interaction Responsiveness

| Field | Value |
|-------|-------|
| **ID** | TC-UI-001 |
| **Priority** | P0 |
| **Preconditions** | Chart displayed |

**Steps:**
1. Pan chart left/right
2. Zoom in/out
3. Hover over candles
4. Click on candles

**Expected Results:**
- [ ] Pan response < 16ms
- [ ] Zoom response < 16ms
- [ ] Tooltip appears < 50ms
- [ ] Click handler < 100ms
- [ ] No jank during interaction

#### TC-UI-002: Timeframe Switching

| Field | Value |
|-------|-------|
| **ID** | TC-UI-002 |
| **Priority** | P0 |
| **Preconditions** | Chart displayed |

**Steps:**
1. Switch from 1m to 5m timeframe
2. Switch from 5m to 1h timeframe
3. Switch from 1h to 1d timeframe
4. Measure transition time

**Expected Results:**
- [ ] Transition < 500ms
- [ ] Loading indicator shown
- [ ] Data loads correctly
- [ ] No stale data displayed

#### TC-UI-003: Symbol Switching

| Field | Value |
|-------|-------|
| **ID** | TC-UI-003 |
| **Priority** | P0 |
| **Preconditions** | Chart displayed |

**Steps:**
1. Switch from BTCUSDT to ETHUSDT
2. Verify WebSocket reconnection
3. Verify data updates
4. Measure transition time

**Expected Results:**
- [ ] Transition < 1s
- [ ] Old WebSocket closed
- [ ] New WebSocket connected
- [ ] Correct data displayed

#### TC-UI-004: Indicator Toggle

| Field | Value |
|-------|-------|
| **ID** | TC-UI-004 |
| **Priority** | P1 |
| **Preconditions** | Chart displayed |

**Steps:**
1. Add SMA indicator
2. Add RSI indicator
3. Add MACD indicator
4. Remove all indicators

**Expected Results:**
- [ ] Indicator renders < 100ms
- [ ] Indicator updates in real-time
- [ ] Removal is instant
- [ ] No memory leak on add/remove

### 3.4 WebSocket Reliability

#### TC-WS-001: Initial Connection

| Field | Value |
|-------|-------|
| **ID** | TC-WS-001 |
| **Priority** | P0 |
| **Preconditions** | VeloZ started |

**Steps:**
1. Open chart for BTCUSDT
2. Monitor WebSocket connection
3. Verify connection established
4. Verify data streaming

**Expected Results:**
- [ ] Connection established < 3s
- [ ] No connection errors
- [ ] Data streaming immediately
- [ ] Connection status indicator green

#### TC-WS-002: Reconnection After Disconnect

| Field | Value |
|-------|-------|
| **ID** | TC-WS-002 |
| **Priority** | P0 |
| **Preconditions** | WebSocket connected |

**Steps:**
1. Simulate network disconnect
2. Wait 5 seconds
3. Restore network
4. Observe reconnection

**Expected Results:**
- [ ] Disconnect detected < 5s
- [ ] Reconnection attempted automatically
- [ ] Reconnection successful < 10s
- [ ] Data resumes without gaps
- [ ] User notified of reconnection

#### TC-WS-003: Reconnection with Exponential Backoff

| Field | Value |
|-------|-------|
| **ID** | TC-WS-003 |
| **Priority** | P0 |
| **Preconditions** | Server unavailable |

**Steps:**
1. Disconnect WebSocket server
2. Monitor reconnection attempts
3. Verify backoff timing
4. Restore server
5. Verify connection

**Expected Results:**
- [ ] First retry: 1s
- [ ] Second retry: 2s
- [ ] Third retry: 4s
- [ ] Max backoff: 30s
- [ ] Successful reconnection when server available

#### TC-WS-004: Multiple Stream Management

| Field | Value |
|-------|-------|
| **ID** | TC-WS-004 |
| **Priority** | P0 |
| **Preconditions** | Multiple charts open |

**Steps:**
1. Open BTCUSDT chart
2. Open ETHUSDT chart
3. Open BNBUSDT chart
4. Verify all streams active

**Expected Results:**
- [ ] All streams connected
- [ ] No stream interference
- [ ] Efficient connection pooling
- [ ] Proper cleanup on close

#### TC-WS-005: Message Queue During Disconnect

| Field | Value |
|-------|-------|
| **ID** | TC-WS-005 |
| **Priority** | P1 |
| **Preconditions** | WebSocket connected |

**Steps:**
1. Disconnect network briefly (5s)
2. Reconnect
3. Verify data continuity
4. Check for gaps

**Expected Results:**
- [ ] Missed data fetched via REST
- [ ] No visible gaps in chart
- [ ] Sequence numbers validated
- [ ] Data integrity maintained

### 3.5 Indicator Calculations

#### TC-IND-001: Simple Moving Average (SMA)

| Field | Value |
|-------|-------|
| **ID** | TC-IND-001 |
| **Priority** | P0 |
| **Preconditions** | Chart with known data |

**Test Data:**
Close prices: [100, 102, 104, 103, 105]
SMA(3) expected: [null, null, 102, 103, 104]

**Steps:**
1. Load test data
2. Apply SMA(3) indicator
3. Verify calculated values

**Expected Results:**
- [ ] SMA values match expected
- [ ] First N-1 values are null
- [ ] Updates correctly with new data

#### TC-IND-002: Exponential Moving Average (EMA)

| Field | Value |
|-------|-------|
| **ID** | TC-IND-002 |
| **Priority** | P0 |
| **Preconditions** | Chart with known data |

**Test Data:**
Close prices: [100, 102, 104, 103, 105]
EMA(3) multiplier = 2/(3+1) = 0.5

**Steps:**
1. Load test data
2. Apply EMA(3) indicator
3. Verify calculated values

**Expected Results:**
- [ ] EMA values match formula
- [ ] Smoothing factor correct
- [ ] Updates correctly with new data

#### TC-IND-003: Relative Strength Index (RSI)

| Field | Value |
|-------|-------|
| **ID** | TC-IND-003 |
| **Priority** | P0 |
| **Preconditions** | Chart with known data |

**Steps:**
1. Load test data with gains and losses
2. Apply RSI(14) indicator
3. Verify calculated values

**Expected Results:**
- [ ] RSI range: 0-100
- [ ] RSI = 100 when all gains
- [ ] RSI = 0 when all losses
- [ ] RSI ~50 when equal gains/losses

#### TC-IND-004: MACD

| Field | Value |
|-------|-------|
| **ID** | TC-IND-004 |
| **Priority** | P0 |
| **Preconditions** | Chart with known data |

**Steps:**
1. Load test data
2. Apply MACD(12, 26, 9) indicator
3. Verify MACD line, signal line, histogram

**Expected Results:**
- [ ] MACD line = EMA(12) - EMA(26)
- [ ] Signal line = EMA(9) of MACD
- [ ] Histogram = MACD - Signal
- [ ] Crossovers detected correctly

#### TC-IND-005: Bollinger Bands

| Field | Value |
|-------|-------|
| **ID** | TC-IND-005 |
| **Priority** | P0 |
| **Preconditions** | Chart with known data |

**Steps:**
1. Load test data
2. Apply Bollinger Bands(20, 2)
3. Verify upper, middle, lower bands

**Expected Results:**
- [ ] Middle band = SMA(20)
- [ ] Upper band = SMA + 2*StdDev
- [ ] Lower band = SMA - 2*StdDev
- [ ] Bands expand with volatility

### 3.6 Cross-Browser Compatibility

#### TC-BROWSER-001: Chrome Compatibility

| Field | Value |
|-------|-------|
| **ID** | TC-BROWSER-001 |
| **Priority** | P0 |
| **Preconditions** | Chrome latest version |

**Steps:**
1. Open VeloZ charts in Chrome
2. Test all chart types
3. Test all interactions
4. Verify performance

**Expected Results:**
- [ ] All charts render correctly
- [ ] All interactions work
- [ ] Performance within targets
- [ ] No console errors

#### TC-BROWSER-002: Firefox Compatibility

| Field | Value |
|-------|-------|
| **ID** | TC-BROWSER-002 |
| **Priority** | P0 |
| **Preconditions** | Firefox latest version |

**Steps:** Same as TC-BROWSER-001

**Expected Results:** Same as TC-BROWSER-001

#### TC-BROWSER-003: Safari Compatibility

| Field | Value |
|-------|-------|
| **ID** | TC-BROWSER-003 |
| **Priority** | P0 |
| **Preconditions** | Safari latest version on macOS |

**Steps:** Same as TC-BROWSER-001

**Expected Results:** Same as TC-BROWSER-001, plus:
- [ ] WebSocket works (Safari quirks)
- [ ] Touch events work (trackpad)

#### TC-BROWSER-004: Mobile Browser Compatibility

| Field | Value |
|-------|-------|
| **ID** | TC-BROWSER-004 |
| **Priority** | P1 |
| **Preconditions** | Mobile Chrome/Safari |

**Steps:**
1. Open VeloZ charts on mobile
2. Test touch interactions
3. Test pinch zoom
4. Verify responsive layout

**Expected Results:**
- [ ] Charts render on mobile
- [ ] Touch pan works
- [ ] Pinch zoom works
- [ ] Responsive layout adapts
- [ ] Performance acceptable

## 4. Automated Test Scripts

### 4.1 Data Accuracy Tests (TypeScript)

```typescript
// File: tests/charts/test_data_accuracy.spec.ts

import { test, expect } from '@playwright/test';
import WebSocket from 'ws';

test.describe('Chart Data Accuracy', () => {
  test('candlestick data matches exchange', async ({ page }) => {
    // Connect to exchange WebSocket directly
    const exchangeData: any[] = [];
    const ws = new WebSocket('wss://stream.binance.com:9443/ws/btcusdt@kline_1m');

    ws.on('message', (data) => {
      exchangeData.push(JSON.parse(data.toString()));
    });

    // Open VeloZ chart
    await page.goto('/charts/BTCUSDT');
    await page.waitForSelector('.candlestick-chart');

    // Wait for data
    await page.waitForTimeout(5000);

    // Get VeloZ candle data
    const velozData = await page.evaluate(() => {
      return (window as any).chartData.candles.slice(-5);
    });

    // Compare
    for (let i = 0; i < velozData.length; i++) {
      const veloz = velozData[i];
      const exchange = exchangeData.find(e => e.k.t === veloz.timestamp);

      if (exchange) {
        expect(Math.abs(veloz.open - parseFloat(exchange.k.o))).toBeLessThan(0.01);
        expect(Math.abs(veloz.high - parseFloat(exchange.k.h))).toBeLessThan(0.01);
        expect(Math.abs(veloz.low - parseFloat(exchange.k.l))).toBeLessThan(0.01);
        expect(Math.abs(veloz.close - parseFloat(exchange.k.c))).toBeLessThan(0.01);
      }
    }

    ws.close();
  });

  test('order book depth matches exchange', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');
    await page.waitForSelector('.order-book');

    // Get VeloZ order book
    const velozBook = await page.evaluate(() => {
      return (window as any).orderBook;
    });

    // Fetch exchange order book via REST
    const response = await fetch('https://api.binance.com/api/v3/depth?symbol=BTCUSDT&limit=20');
    const exchangeBook = await response.json();

    // Compare top bid
    expect(Math.abs(velozBook.bids[0].price - parseFloat(exchangeBook.bids[0][0]))).toBeLessThan(1);

    // Compare top ask
    expect(Math.abs(velozBook.asks[0].price - parseFloat(exchangeBook.asks[0][0]))).toBeLessThan(1);
  });
});
```

### 4.2 Performance Tests (TypeScript)

```typescript
// File: tests/charts/test_performance.spec.ts

import { test, expect } from '@playwright/test';

test.describe('Chart Performance', () => {
  test('maintains 60 FPS during updates', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');
    await page.waitForSelector('.candlestick-chart');

    // Start performance measurement
    const metrics = await page.evaluate(async () => {
      const frames: number[] = [];
      let lastTime = performance.now();

      return new Promise<{ avgFps: number; minFps: number }>((resolve) => {
        const measure = () => {
          const now = performance.now();
          const fps = 1000 / (now - lastTime);
          frames.push(fps);
          lastTime = now;

          if (frames.length < 300) {
            requestAnimationFrame(measure);
          } else {
            const avgFps = frames.reduce((a, b) => a + b) / frames.length;
            const minFps = Math.min(...frames);
            resolve({ avgFps, minFps });
          }
        };
        requestAnimationFrame(measure);
      });
    });

    expect(metrics.avgFps).toBeGreaterThan(55);
    expect(metrics.minFps).toBeGreaterThan(30);
  });

  test('update latency under 50ms P95', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');

    const latencies = await page.evaluate(async () => {
      const results: number[] = [];

      return new Promise<number[]>((resolve) => {
        const ws = (window as any).chartWebSocket;
        const originalOnMessage = ws.onmessage;

        ws.onmessage = (event: MessageEvent) => {
          const start = performance.now();
          originalOnMessage.call(ws, event);
          const end = performance.now();
          results.push(end - start);

          if (results.length >= 100) {
            ws.onmessage = originalOnMessage;
            resolve(results);
          }
        };
      });
    });

    latencies.sort((a, b) => a - b);
    const p95 = latencies[Math.floor(latencies.length * 0.95)];

    expect(p95).toBeLessThan(50);
  });

  test('memory usage stays stable', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');

    // Initial memory
    const initialMemory = await page.evaluate(() => {
      return (performance as any).memory?.usedJSHeapSize || 0;
    });

    // Wait 5 minutes with updates
    await page.waitForTimeout(300000);

    // Final memory
    const finalMemory = await page.evaluate(() => {
      return (performance as any).memory?.usedJSHeapSize || 0;
    });

    // Memory growth should be < 50MB
    const growth = (finalMemory - initialMemory) / 1024 / 1024;
    expect(growth).toBeLessThan(50);
  });

  test('large dataset renders within time limit', async ({ page }) => {
    await page.goto('/charts/BTCUSDT?candles=10000');

    const renderTime = await page.evaluate(async () => {
      const start = performance.now();
      await (window as any).chart.render();
      return performance.now() - start;
    });

    expect(renderTime).toBeLessThan(2000);
  });
});
```

### 4.3 WebSocket Tests (TypeScript)

```typescript
// File: tests/charts/test_websocket.spec.ts

import { test, expect } from '@playwright/test';

test.describe('WebSocket Reliability', () => {
  test('reconnects after disconnect', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');
    await page.waitForSelector('.connection-status.connected');

    // Simulate disconnect
    await page.evaluate(() => {
      (window as any).chartWebSocket.close();
    });

    // Wait for reconnection
    await page.waitForSelector('.connection-status.reconnecting', { timeout: 5000 });
    await page.waitForSelector('.connection-status.connected', { timeout: 15000 });

    // Verify data resumes
    const dataUpdating = await page.evaluate(async () => {
      const initialPrice = (window as any).chartData.lastPrice;
      await new Promise(r => setTimeout(r, 2000));
      return (window as any).chartData.lastPrice !== initialPrice;
    });

    expect(dataUpdating).toBe(true);
  });

  test('handles rapid reconnections', async ({ page }) => {
    await page.goto('/charts/BTCUSDT');

    // Rapidly disconnect/reconnect 5 times
    for (let i = 0; i < 5; i++) {
      await page.evaluate(() => {
        (window as any).chartWebSocket.close();
      });
      await page.waitForTimeout(500);
    }

    // Should eventually stabilize
    await page.waitForSelector('.connection-status.connected', { timeout: 30000 });

    // Verify no duplicate connections
    const connectionCount = await page.evaluate(() => {
      return (window as any).activeWebSockets?.length || 1;
    });

    expect(connectionCount).toBe(1);
  });

  test('manages multiple streams', async ({ page }) => {
    // Open multiple charts
    await page.goto('/charts/multi?symbols=BTCUSDT,ETHUSDT,BNBUSDT');

    await page.waitForSelector('[data-symbol="BTCUSDT"] .connection-status.connected');
    await page.waitForSelector('[data-symbol="ETHUSDT"] .connection-status.connected');
    await page.waitForSelector('[data-symbol="BNBUSDT"] .connection-status.connected');

    // All should be updating
    const allUpdating = await page.evaluate(async () => {
      const symbols = ['BTCUSDT', 'ETHUSDT', 'BNBUSDT'];
      const initial = symbols.map(s => (window as any).charts[s].lastPrice);

      await new Promise(r => setTimeout(r, 5000));

      return symbols.every((s, i) =>
        (window as any).charts[s].lastPrice !== initial[i]
      );
    });

    expect(allUpdating).toBe(true);
  });
});
```

### 4.4 Indicator Tests (TypeScript)

```typescript
// File: tests/charts/test_indicators.spec.ts

import { test, expect } from '@playwright/test';

test.describe('Technical Indicators', () => {
  const testData = [100, 102, 104, 103, 105, 107, 106, 108, 110, 109];

  test('SMA calculation is correct', async ({ page }) => {
    await page.goto('/charts/test');

    const smaValues = await page.evaluate((data) => {
      const indicator = new (window as any).SMA(3);
      return data.map(price => indicator.update(price));
    }, testData);

    // SMA(3) for [100, 102, 104] = (100+102+104)/3 = 102
    expect(smaValues[2]).toBeCloseTo(102, 2);

    // SMA(3) for [102, 104, 103] = (102+104+103)/3 = 103
    expect(smaValues[3]).toBeCloseTo(103, 2);
  });

  test('EMA calculation is correct', async ({ page }) => {
    await page.goto('/charts/test');

    const emaValues = await page.evaluate((data) => {
      const indicator = new (window as any).EMA(3);
      return data.map(price => indicator.update(price));
    }, testData);

    // EMA multiplier = 2/(3+1) = 0.5
    // First EMA = SMA = 102
    // Second EMA = 103 * 0.5 + 102 * 0.5 = 102.5
    expect(emaValues[3]).toBeCloseTo(102.5, 1);
  });

  test('RSI calculation is correct', async ({ page }) => {
    await page.goto('/charts/test');

    // All gains scenario
    const allGains = [100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114];

    const rsiAllGains = await page.evaluate((data) => {
      const indicator = new (window as any).RSI(14);
      let result;
      data.forEach(price => result = indicator.update(price));
      return result;
    }, allGains);

    expect(rsiAllGains).toBe(100);

    // All losses scenario
    const allLosses = [114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100];

    const rsiAllLosses = await page.evaluate((data) => {
      const indicator = new (window as any).RSI(14);
      let result;
      data.forEach(price => result = indicator.update(price));
      return result;
    }, allLosses);

    expect(rsiAllLosses).toBe(0);
  });

  test('MACD calculation is correct', async ({ page }) => {
    await page.goto('/charts/test');

    // Generate enough data for MACD
    const data = Array.from({ length: 50 }, (_, i) => 100 + Math.sin(i / 5) * 10);

    const macdValues = await page.evaluate((data) => {
      const indicator = new (window as any).MACD(12, 26, 9);
      return data.map(price => indicator.update(price));
    }, data);

    // MACD line should exist after 26 periods
    expect(macdValues[25]).not.toBeNull();

    // Signal line should exist after 26 + 9 periods
    expect(macdValues[34].signal).not.toBeNull();

    // Histogram = MACD - Signal
    const last = macdValues[macdValues.length - 1];
    expect(last.histogram).toBeCloseTo(last.macd - last.signal, 4);
  });

  test('Bollinger Bands calculation is correct', async ({ page }) => {
    await page.goto('/charts/test');

    // Constant price = bands should be tight
    const constantData = Array(20).fill(100);

    const bbConstant = await page.evaluate((data) => {
      const indicator = new (window as any).BollingerBands(20, 2);
      let result;
      data.forEach(price => result = indicator.update(price));
      return result;
    }, constantData);

    expect(bbConstant.middle).toBe(100);
    expect(bbConstant.upper).toBe(100); // No volatility
    expect(bbConstant.lower).toBe(100);

    // Volatile price = bands should be wide
    const volatileData = Array.from({ length: 20 }, (_, i) => 100 + (i % 2 === 0 ? 10 : -10));

    const bbVolatile = await page.evaluate((data) => {
      const indicator = new (window as any).BollingerBands(20, 2);
      let result;
      data.forEach(price => result = indicator.update(price));
      return result;
    }, volatileData);

    expect(bbVolatile.upper).toBeGreaterThan(bbVolatile.middle);
    expect(bbVolatile.lower).toBeLessThan(bbVolatile.middle);
  });
});
```

## 5. Performance Benchmarks

### 5.1 Rendering Performance

| Metric | Target | Minimum | Maximum |
|--------|--------|---------|---------|
| FPS (normal) | 60 | 55 | - |
| FPS (heavy load) | 30 | 24 | - |
| Frame time | 16ms | - | 33ms |
| First paint | 500ms | - | 1000ms |

### 5.2 Update Latency

| Percentile | Target | Maximum |
|------------|--------|---------|
| P50 | 10ms | 20ms |
| P95 | 30ms | 50ms |
| P99 | 50ms | 100ms |
| Max | 100ms | 500ms |

### 5.3 Resource Usage

| Resource | Idle | Active | Maximum |
|----------|------|--------|---------|
| CPU | < 2% | < 20% | 50% |
| Memory | < 50MB | < 150MB | 500MB |
| Network | 0 | < 100KB/s | 1MB/s |

## 6. Security Testing Checklist

- [ ] WebSocket uses WSS (encrypted)
- [ ] No sensitive data in WebSocket messages
- [ ] XSS prevention in chart labels
- [ ] CORS properly configured
- [ ] Rate limiting on WebSocket connections
- [ ] Input validation for chart parameters
- [ ] No code injection via indicator formulas

## 7. UAT Plan

### 7.1 UAT Scenarios

1. **Basic Chart Usage**
   - View candlestick chart
   - Change timeframes
   - Pan and zoom
   - Add indicators

2. **Multi-Chart Layout**
   - Open multiple charts
   - Arrange in grid
   - Sync timeframes
   - Compare symbols

3. **Real-time Trading View**
   - Monitor price updates
   - Watch order book
   - Track trades
   - Set alerts

### 7.2 UAT Success Criteria

- Chart accuracy confirmed by traders
- Performance acceptable on target hardware
- All browsers supported
- No data discrepancies reported
- Indicators match TradingView calculations

## 8. Bug Tracking

### 8.1 Common Issues

| ID | Description | Severity | Status |
|----|-------------|----------|--------|
| CHT-001 | Candle gaps on reconnect | S1 | Fixed |
| CHT-002 | Memory leak in indicator | S1 | In Progress |
| CHT-003 | Safari WebSocket issues | S2 | Fixed |
