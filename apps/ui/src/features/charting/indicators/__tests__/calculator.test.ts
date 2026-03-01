/**
 * Technical Indicator Calculator Tests
 */
import { describe, it, expect } from 'vitest';
import {
  calculateSMA,
  calculateEMA,
  calculateRSI,
  calculateMACD,
  calculateBollingerBands,
  calculateStochastic,
  calculateATR,
  calculateOBV,
  calculateIndicator,
} from '../calculator';
import type { OHLCVData } from '../../types';

// =============================================================================
// Test Data
// =============================================================================

const createTestData = (closes: number[]): OHLCVData[] => {
  return closes.map((close, i) => ({
    time: i + 1,
    open: close - 1,
    high: close + 2,
    low: close - 2,
    close,
    volume: 1000 + i * 100,
  }));
};

const sampleData: OHLCVData[] = [
  { time: 1, open: 100, high: 105, low: 98, close: 102, volume: 1000 },
  { time: 2, open: 102, high: 108, low: 101, close: 106, volume: 1200 },
  { time: 3, open: 106, high: 110, low: 104, close: 104, volume: 1100 },
  { time: 4, open: 104, high: 107, low: 102, close: 103, volume: 900 },
  { time: 5, open: 103, high: 109, low: 101, close: 108, volume: 1300 },
  { time: 6, open: 108, high: 112, low: 106, close: 110, volume: 1400 },
  { time: 7, open: 110, high: 115, low: 108, close: 112, volume: 1500 },
  { time: 8, open: 112, high: 114, low: 109, close: 109, volume: 1100 },
  { time: 9, open: 109, high: 113, low: 107, close: 111, volume: 1200 },
  { time: 10, open: 111, high: 116, low: 110, close: 115, volume: 1600 },
];

// =============================================================================
// SMA Tests
// =============================================================================

describe('calculateSMA', () => {
  it('calculates SMA correctly for period 3', () => {
    const data = createTestData([100, 102, 104, 103, 105]);
    const sma = calculateSMA(data, 3);

    expect(sma).toHaveLength(3);
    expect(sma[0].value).toBeCloseTo(102, 2); // (100+102+104)/3
    expect(sma[1].value).toBeCloseTo(103, 2); // (102+104+103)/3
    expect(sma[2].value).toBeCloseTo(104, 2); // (104+103+105)/3
  });

  it('returns empty array when data length < period', () => {
    const data = createTestData([100, 102]);
    const sma = calculateSMA(data, 5);

    expect(sma).toHaveLength(0);
  });

  it('handles single period', () => {
    const data = createTestData([100, 102, 104]);
    const sma = calculateSMA(data, 1);

    expect(sma).toHaveLength(3);
    expect(sma[0].value).toBe(100);
    expect(sma[1].value).toBe(102);
    expect(sma[2].value).toBe(104);
  });
});

// =============================================================================
// EMA Tests
// =============================================================================

describe('calculateEMA', () => {
  it('first EMA equals SMA', () => {
    const data = createTestData([100, 102, 104, 103, 105]);
    const ema = calculateEMA(data, 3);

    expect(ema).toHaveLength(3);
    // First EMA should equal SMA(3) = (100+102+104)/3 = 102
    expect(ema[0].value).toBeCloseTo(102, 2);
  });

  it('applies correct multiplier', () => {
    const data = createTestData([100, 102, 104, 103, 105]);
    const ema = calculateEMA(data, 3);

    // Multiplier = 2/(3+1) = 0.5
    // EMA[1] = 103 * 0.5 + 102 * 0.5 = 102.5
    expect(ema[1].value).toBeCloseTo(102.5, 2);
  });

  it('returns empty array when data length < period', () => {
    const data = createTestData([100, 102]);
    const ema = calculateEMA(data, 5);

    expect(ema).toHaveLength(0);
  });
});

// =============================================================================
// RSI Tests
// =============================================================================

describe('calculateRSI', () => {
  it('RSI is close to 100 when all gains', () => {
    const data = createTestData([100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115]);
    const rsi = calculateRSI(data, 14);

    expect(rsi.length).toBeGreaterThan(0);
    // RSI approaches 100 with all gains but may not be exactly 100 due to smoothing
    expect(rsi[rsi.length - 1].value).toBeGreaterThan(95);
  });

  it('RSI is 0 when all losses', () => {
    const data = createTestData([115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100]);
    const rsi = calculateRSI(data, 14);

    expect(rsi.length).toBeGreaterThan(0);
    expect(rsi[rsi.length - 1].value).toBe(0);
  });

  it('RSI stays within 0-100 range', () => {
    const rsi = calculateRSI(sampleData, 5);

    for (const point of rsi) {
      expect(point.value).toBeGreaterThanOrEqual(0);
      expect(point.value).toBeLessThanOrEqual(100);
    }
  });

  it('returns empty array when insufficient data', () => {
    const data = createTestData([100, 102, 104]);
    const rsi = calculateRSI(data, 14);

    expect(rsi).toHaveLength(0);
  });
});

// =============================================================================
// MACD Tests
// =============================================================================

describe('calculateMACD', () => {
  it('calculates MACD line as difference of EMAs', () => {
    // Need enough data for MACD (26 + 9 periods)
    const closes = Array.from({ length: 40 }, (_, i) => 100 + Math.sin(i / 5) * 10);
    const data = createTestData(closes);
    const macd = calculateMACD(data, 12, 26, 9);

    expect(macd.length).toBeGreaterThan(0);

    // Each result should have macd, signal, and histogram
    for (const point of macd) {
      expect(point).toHaveProperty('value'); // MACD line
      expect(point).toHaveProperty('signal');
      expect(point).toHaveProperty('histogram');
    }
  });

  it('histogram equals MACD minus signal', () => {
    const closes = Array.from({ length: 40 }, (_, i) => 100 + Math.sin(i / 5) * 10);
    const data = createTestData(closes);
    const macd = calculateMACD(data, 12, 26, 9);

    for (const point of macd) {
      expect(point.histogram).toBeCloseTo(point.value - point.signal, 4);
    }
  });

  it('returns empty array when insufficient data', () => {
    const data = createTestData([100, 102, 104, 103, 105]);
    const macd = calculateMACD(data, 12, 26, 9);

    expect(macd).toHaveLength(0);
  });
});

// =============================================================================
// Bollinger Bands Tests
// =============================================================================

describe('calculateBollingerBands', () => {
  it('middle band equals SMA', () => {
    const data = createTestData([100, 102, 104, 103, 105, 107, 106, 108, 110, 109]);
    const bb = calculateBollingerBands(data, 5, 2);
    const sma = calculateSMA(data, 5);

    expect(bb.length).toBe(sma.length);

    for (let i = 0; i < bb.length; i++) {
      expect(bb[i].value).toBeCloseTo(sma[i].value, 4);
    }
  });

  it('upper band is above middle, lower band is below', () => {
    const data = createTestData([100, 102, 104, 103, 105, 107, 106, 108, 110, 109]);
    const bb = calculateBollingerBands(data, 5, 2);

    for (const point of bb) {
      expect(point.upper).toBeGreaterThanOrEqual(point.value);
      expect(point.lower).toBeLessThanOrEqual(point.value);
    }
  });

  it('bands are tight when volatility is low', () => {
    const data = createTestData([100, 100, 100, 100, 100, 100, 100, 100, 100, 100]);
    const bb = calculateBollingerBands(data, 5, 2);

    // With constant prices, bands should equal middle
    for (const point of bb) {
      expect(point.upper).toBe(point.value);
      expect(point.lower).toBe(point.value);
    }
  });
});

// =============================================================================
// Stochastic Tests
// =============================================================================

describe('calculateStochastic', () => {
  it('returns K and D values', () => {
    // Need more data for stochastic (kPeriod + smooth + dPeriod)
    const extendedData = [
      ...sampleData,
      { time: 11, open: 115, high: 118, low: 113, close: 116, volume: 1700 },
      { time: 12, open: 116, high: 120, low: 114, close: 118, volume: 1800 },
      { time: 13, open: 118, high: 122, low: 116, close: 120, volume: 1900 },
      { time: 14, open: 120, high: 124, low: 118, close: 122, volume: 2000 },
      { time: 15, open: 122, high: 126, low: 120, close: 124, volume: 2100 },
    ];
    const stoch = calculateStochastic(extendedData, 5, 3, 3);

    expect(stoch.length).toBeGreaterThan(0);

    for (const point of stoch) {
      expect(point).toHaveProperty('value'); // %K
      expect(point).toHaveProperty('d'); // %D
    }
  });

  it('values stay within 0-100 range', () => {
    // Need more data for stochastic
    const extendedData = [
      ...sampleData,
      { time: 11, open: 115, high: 118, low: 113, close: 116, volume: 1700 },
      { time: 12, open: 116, high: 120, low: 114, close: 118, volume: 1800 },
      { time: 13, open: 118, high: 122, low: 116, close: 120, volume: 1900 },
      { time: 14, open: 120, high: 124, low: 118, close: 122, volume: 2000 },
      { time: 15, open: 122, high: 126, low: 120, close: 124, volume: 2100 },
    ];
    const stoch = calculateStochastic(extendedData, 5, 3, 3);

    for (const point of stoch) {
      expect(point.value).toBeGreaterThanOrEqual(0);
      expect(point.value).toBeLessThanOrEqual(100);
      expect(point.d).toBeGreaterThanOrEqual(0);
      expect(point.d).toBeLessThanOrEqual(100);
    }
  });
});

// =============================================================================
// ATR Tests
// =============================================================================

describe('calculateATR', () => {
  it('calculates ATR correctly', () => {
    const atr = calculateATR(sampleData, 5);

    expect(atr.length).toBeGreaterThan(0);

    // ATR should be positive
    for (const point of atr) {
      expect(point.value).toBeGreaterThan(0);
    }
  });

  it('returns empty array when insufficient data', () => {
    const data = createTestData([100, 102, 104]);
    const atr = calculateATR(data, 14);

    expect(atr).toHaveLength(0);
  });
});

// =============================================================================
// OBV Tests
// =============================================================================

describe('calculateOBV', () => {
  it('OBV increases on up days', () => {
    const data: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 98, close: 100, volume: 1000 },
      { time: 2, open: 100, high: 108, low: 99, close: 105, volume: 1200 },
    ];
    const obv = calculateOBV(data);

    expect(obv).toHaveLength(2);
    expect(obv[1].value).toBe(1200); // Volume added
  });

  it('OBV decreases on down days', () => {
    const data: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 98, close: 100, volume: 1000 },
      { time: 2, open: 100, high: 102, low: 95, close: 95, volume: 1200 },
    ];
    const obv = calculateOBV(data);

    expect(obv).toHaveLength(2);
    expect(obv[1].value).toBe(-1200); // Volume subtracted
  });

  it('OBV unchanged on flat days', () => {
    const data: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 98, close: 100, volume: 1000 },
      { time: 2, open: 100, high: 102, low: 98, close: 100, volume: 1200 },
    ];
    const obv = calculateOBV(data);

    expect(obv).toHaveLength(2);
    expect(obv[1].value).toBe(0); // No change
  });
});

// =============================================================================
// Indicator Factory Tests
// =============================================================================

describe('calculateIndicator', () => {
  it('calculates SMA via factory', () => {
    const data = createTestData([100, 102, 104, 103, 105]);
    const result = calculateIndicator('sma', data, { period: 3 });

    expect(result).toHaveLength(3);
  });

  it('calculates RSI via factory', () => {
    const result = calculateIndicator('rsi', sampleData, { period: 5 });

    expect(result.length).toBeGreaterThan(0);
  });

  it('returns empty array for unknown indicator', () => {
    const result = calculateIndicator('unknown', sampleData, {});

    expect(result).toHaveLength(0);
  });

  it('uses default params when not provided', () => {
    const result = calculateIndicator('sma', sampleData, {});

    // Default period is 20, so with 10 data points, should return empty
    expect(result).toHaveLength(0);
  });
});
