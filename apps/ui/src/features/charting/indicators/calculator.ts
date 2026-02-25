/**
 * Technical Indicator Calculators
 * Pure functions for calculating technical indicators from OHLCV data
 */
import type { OHLCVData, IndicatorResult } from '../types';

// =============================================================================
// Helper Functions
// =============================================================================

function calculateEMAFromValues(
  values: { time: number; value: number }[],
  period: number
): IndicatorResult[] {
  if (values.length < period) return [];

  const results: IndicatorResult[] = [];
  const multiplier = 2 / (period + 1);

  // First EMA is SMA
  let sum = 0;
  for (let i = 0; i < period; i++) {
    sum += values[i].value;
  }
  let ema = sum / period;
  results.push({ time: values[period - 1].time, value: ema });

  // Calculate EMA for remaining data
  for (let i = period; i < values.length; i++) {
    ema = (values[i].value - ema) * multiplier + ema;
    results.push({ time: values[i].time, value: ema });
  }

  return results;
}

// =============================================================================
// Moving Averages
// =============================================================================

/**
 * Simple Moving Average (SMA)
 */
export function calculateSMA(data: OHLCVData[], period: number): IndicatorResult[] {
  if (data.length < period) return [];

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

/**
 * Exponential Moving Average (EMA)
 */
export function calculateEMA(data: OHLCVData[], period: number): IndicatorResult[] {
  if (data.length < period) return [];

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

/**
 * Weighted Moving Average (WMA)
 */
export function calculateWMA(data: OHLCVData[], period: number): IndicatorResult[] {
  if (data.length < period) return [];

  const results: IndicatorResult[] = [];
  const weightSum = (period * (period + 1)) / 2;

  for (let i = period - 1; i < data.length; i++) {
    let sum = 0;
    for (let j = 0; j < period; j++) {
      sum += data[i - j].close * (period - j);
    }
    results.push({
      time: data[i].time,
      value: sum / weightSum,
    });
  }

  return results;
}

// =============================================================================
// Momentum Indicators
// =============================================================================

/**
 * Relative Strength Index (RSI)
 */
export function calculateRSI(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  if (data.length < period + 1) return [];

  const results: IndicatorResult[] = [];
  const gains: number[] = [];
  const losses: number[] = [];

  // Calculate price changes
  for (let i = 1; i < data.length; i++) {
    const change = data[i].close - data[i - 1].close;
    gains.push(change > 0 ? change : 0);
    losses.push(change < 0 ? -change : 0);
  }

  // First RSI using SMA
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

/**
 * MACD (Moving Average Convergence Divergence)
 */
export function calculateMACD(
  data: OHLCVData[],
  fastPeriod: number = 12,
  slowPeriod: number = 26,
  signalPeriod: number = 9
): IndicatorResult[] {
  if (data.length < slowPeriod + signalPeriod) return [];

  const fastEMA = calculateEMA(data, fastPeriod);
  const slowEMA = calculateEMA(data, slowPeriod);

  if (fastEMA.length === 0 || slowEMA.length === 0) return [];

  // Align data - slow EMA starts later
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

/**
 * Stochastic Oscillator
 */
export function calculateStochastic(
  data: OHLCVData[],
  kPeriod: number = 14,
  dPeriod: number = 3,
  smooth: number = 3
): IndicatorResult[] {
  if (data.length < kPeriod + smooth + dPeriod) return [];

  const rawK: { time: number; value: number }[] = [];

  // Calculate raw %K
  for (let i = kPeriod - 1; i < data.length; i++) {
    let highest = data[i].high;
    let lowest = data[i].low;

    for (let j = 1; j < kPeriod; j++) {
      highest = Math.max(highest, data[i - j].high);
      lowest = Math.min(lowest, data[i - j].low);
    }

    const range = highest - lowest;
    const k = range === 0 ? 50 : ((data[i].close - lowest) / range) * 100;
    rawK.push({ time: data[i].time, value: k });
  }

  // Smooth %K
  const smoothedK = calculateEMAFromValues(rawK, smooth);

  // Calculate %D (SMA of smoothed %K)
  const results: IndicatorResult[] = [];

  for (let i = dPeriod - 1; i < smoothedK.length; i++) {
    let sum = 0;
    for (let j = 0; j < dPeriod; j++) {
      sum += smoothedK[i - j].value;
    }
    const d = sum / dPeriod;

    results.push({
      time: smoothedK[i].time,
      value: smoothedK[i].value, // %K
      d, // %D
    });
  }

  return results;
}

/**
 * Williams %R
 */
export function calculateWilliamsR(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  if (data.length < period) return [];

  const results: IndicatorResult[] = [];

  for (let i = period - 1; i < data.length; i++) {
    let highest = data[i].high;
    let lowest = data[i].low;

    for (let j = 1; j < period; j++) {
      highest = Math.max(highest, data[i - j].high);
      lowest = Math.min(lowest, data[i - j].low);
    }

    const range = highest - lowest;
    const wr = range === 0 ? -50 : ((highest - data[i].close) / range) * -100;

    results.push({ time: data[i].time, value: wr });
  }

  return results;
}

/**
 * Rate of Change (ROC)
 */
export function calculateROC(data: OHLCVData[], period: number = 12): IndicatorResult[] {
  if (data.length < period + 1) return [];

  const results: IndicatorResult[] = [];

  for (let i = period; i < data.length; i++) {
    const previousClose = data[i - period].close;
    const roc = previousClose === 0 ? 0 : ((data[i].close - previousClose) / previousClose) * 100;

    results.push({ time: data[i].time, value: roc });
  }

  return results;
}

/**
 * Commodity Channel Index (CCI)
 */
export function calculateCCI(data: OHLCVData[], period: number = 20): IndicatorResult[] {
  if (data.length < period) return [];

  const results: IndicatorResult[] = [];

  for (let i = period - 1; i < data.length; i++) {
    // Calculate typical price
    const typicalPrices: number[] = [];
    for (let j = 0; j < period; j++) {
      const tp = (data[i - j].high + data[i - j].low + data[i - j].close) / 3;
      typicalPrices.push(tp);
    }

    // Calculate SMA of typical price
    const smaTP = typicalPrices.reduce((a, b) => a + b, 0) / period;

    // Calculate mean deviation
    const meanDeviation =
      typicalPrices.reduce((sum, tp) => sum + Math.abs(tp - smaTP), 0) / period;

    // Calculate CCI
    const currentTP = typicalPrices[0];
    const cci = meanDeviation === 0 ? 0 : (currentTP - smaTP) / (0.015 * meanDeviation);

    results.push({ time: data[i].time, value: cci });
  }

  return results;
}

// =============================================================================
// Volatility Indicators
// =============================================================================

/**
 * Bollinger Bands
 */
export function calculateBollingerBands(
  data: OHLCVData[],
  period: number = 20,
  stdDev: number = 2
): IndicatorResult[] {
  if (data.length < period) return [];

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
      value: sma[i].value, // Middle band
      upper: sma[i].value + stdDev * std,
      lower: sma[i].value - stdDev * std,
    });
  }

  return results;
}

/**
 * Average True Range (ATR)
 */
export function calculateATR(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  if (data.length < period + 1) return [];

  const trueRanges: number[] = [];

  // Calculate True Range for each bar
  for (let i = 1; i < data.length; i++) {
    const high = data[i].high;
    const low = data[i].low;
    const prevClose = data[i - 1].close;

    const tr = Math.max(high - low, Math.abs(high - prevClose), Math.abs(low - prevClose));
    trueRanges.push(tr);
  }

  const results: IndicatorResult[] = [];

  // First ATR is SMA of True Range
  let atr = trueRanges.slice(0, period).reduce((a, b) => a + b, 0) / period;
  results.push({ time: data[period].time, value: atr });

  // Subsequent ATR values using smoothed average
  for (let i = period; i < trueRanges.length; i++) {
    atr = (atr * (period - 1) + trueRanges[i]) / period;
    results.push({ time: data[i + 1].time, value: atr });
  }

  return results;
}

/**
 * Keltner Channel
 */
export function calculateKeltnerChannel(
  data: OHLCVData[],
  emaPeriod: number = 20,
  atrPeriod: number = 10,
  multiplier: number = 2
): IndicatorResult[] {
  const ema = calculateEMA(data, emaPeriod);
  const atr = calculateATR(data, atrPeriod);

  if (ema.length === 0 || atr.length === 0) return [];

  // Align EMA and ATR
  const results: IndicatorResult[] = [];
  const emaOffset = emaPeriod - 1;
  const atrOffset = atrPeriod;

  const startIndex = Math.max(emaOffset, atrOffset);

  for (let i = startIndex; i < data.length; i++) {
    const emaIndex = i - emaOffset;
    const atrIndex = i - atrOffset;

    if (emaIndex >= 0 && emaIndex < ema.length && atrIndex >= 0 && atrIndex < atr.length) {
      const middle = ema[emaIndex].value;
      const band = atr[atrIndex].value * multiplier;

      results.push({
        time: data[i].time,
        value: middle,
        upper: middle + band,
        lower: middle - band,
      });
    }
  }

  return results;
}

/**
 * Donchian Channel
 */
export function calculateDonchianChannel(
  data: OHLCVData[],
  period: number = 20
): IndicatorResult[] {
  if (data.length < period) return [];

  const results: IndicatorResult[] = [];

  for (let i = period - 1; i < data.length; i++) {
    let highest = data[i].high;
    let lowest = data[i].low;

    for (let j = 1; j < period; j++) {
      highest = Math.max(highest, data[i - j].high);
      lowest = Math.min(lowest, data[i - j].low);
    }

    results.push({
      time: data[i].time,
      value: (highest + lowest) / 2, // Middle
      upper: highest,
      lower: lowest,
    });
  }

  return results;
}

// =============================================================================
// Volume Indicators
// =============================================================================

/**
 * On-Balance Volume (OBV)
 */
export function calculateOBV(data: OHLCVData[]): IndicatorResult[] {
  if (data.length < 2) return [];

  const results: IndicatorResult[] = [];
  let obv = 0;

  results.push({ time: data[0].time, value: obv });

  for (let i = 1; i < data.length; i++) {
    if (data[i].close > data[i - 1].close) {
      obv += data[i].volume;
    } else if (data[i].close < data[i - 1].close) {
      obv -= data[i].volume;
    }
    // If close equals previous close, OBV stays the same

    results.push({ time: data[i].time, value: obv });
  }

  return results;
}

/**
 * Money Flow Index (MFI)
 */
export function calculateMFI(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  if (data.length < period + 1) return [];

  const typicalPrices: number[] = [];
  const rawMoneyFlows: number[] = [];

  // Calculate typical prices and raw money flows
  for (let i = 0; i < data.length; i++) {
    const tp = (data[i].high + data[i].low + data[i].close) / 3;
    typicalPrices.push(tp);
    rawMoneyFlows.push(tp * data[i].volume);
  }

  const results: IndicatorResult[] = [];

  for (let i = period; i < data.length; i++) {
    let positiveFlow = 0;
    let negativeFlow = 0;

    for (let j = 0; j < period; j++) {
      const currentIndex = i - j;
      const previousIndex = currentIndex - 1;

      if (typicalPrices[currentIndex] > typicalPrices[previousIndex]) {
        positiveFlow += rawMoneyFlows[currentIndex];
      } else if (typicalPrices[currentIndex] < typicalPrices[previousIndex]) {
        negativeFlow += rawMoneyFlows[currentIndex];
      }
    }

    const mfRatio = negativeFlow === 0 ? 100 : positiveFlow / negativeFlow;
    const mfi = 100 - 100 / (1 + mfRatio);

    results.push({ time: data[i].time, value: mfi });
  }

  return results;
}

/**
 * Volume Weighted Average Price (VWAP)
 * Note: VWAP typically resets daily, this is a cumulative version
 */
export function calculateVWAP(data: OHLCVData[]): IndicatorResult[] {
  if (data.length === 0) return [];

  const results: IndicatorResult[] = [];
  let cumulativeTPV = 0;
  let cumulativeVolume = 0;

  for (let i = 0; i < data.length; i++) {
    const typicalPrice = (data[i].high + data[i].low + data[i].close) / 3;
    cumulativeTPV += typicalPrice * data[i].volume;
    cumulativeVolume += data[i].volume;

    const vwap = cumulativeVolume === 0 ? typicalPrice : cumulativeTPV / cumulativeVolume;

    results.push({ time: data[i].time, value: vwap });
  }

  return results;
}

// =============================================================================
// Trend Indicators
// =============================================================================

/**
 * Average Directional Index (ADX)
 */
export function calculateADX(data: OHLCVData[], period: number = 14): IndicatorResult[] {
  if (data.length < period * 2) return [];

  const trueRanges: number[] = [];
  const plusDM: number[] = [];
  const minusDM: number[] = [];

  // Calculate TR, +DM, -DM
  for (let i = 1; i < data.length; i++) {
    const high = data[i].high;
    const low = data[i].low;
    const prevHigh = data[i - 1].high;
    const prevLow = data[i - 1].low;
    const prevClose = data[i - 1].close;

    // True Range
    const tr = Math.max(high - low, Math.abs(high - prevClose), Math.abs(low - prevClose));
    trueRanges.push(tr);

    // Directional Movement
    const upMove = high - prevHigh;
    const downMove = prevLow - low;

    if (upMove > downMove && upMove > 0) {
      plusDM.push(upMove);
    } else {
      plusDM.push(0);
    }

    if (downMove > upMove && downMove > 0) {
      minusDM.push(downMove);
    } else {
      minusDM.push(0);
    }
  }

  // Smooth TR, +DM, -DM
  const smoothedTR: number[] = [];
  const smoothedPlusDM: number[] = [];
  const smoothedMinusDM: number[] = [];

  // First smoothed values are sums
  let sumTR = trueRanges.slice(0, period).reduce((a, b) => a + b, 0);
  let sumPlusDM = plusDM.slice(0, period).reduce((a, b) => a + b, 0);
  let sumMinusDM = minusDM.slice(0, period).reduce((a, b) => a + b, 0);

  smoothedTR.push(sumTR);
  smoothedPlusDM.push(sumPlusDM);
  smoothedMinusDM.push(sumMinusDM);

  // Subsequent smoothed values
  for (let i = period; i < trueRanges.length; i++) {
    sumTR = sumTR - sumTR / period + trueRanges[i];
    sumPlusDM = sumPlusDM - sumPlusDM / period + plusDM[i];
    sumMinusDM = sumMinusDM - sumMinusDM / period + minusDM[i];

    smoothedTR.push(sumTR);
    smoothedPlusDM.push(sumPlusDM);
    smoothedMinusDM.push(sumMinusDM);
  }

  // Calculate +DI, -DI, DX
  const dx: number[] = [];

  for (let i = 0; i < smoothedTR.length; i++) {
    const plusDI = smoothedTR[i] === 0 ? 0 : (smoothedPlusDM[i] / smoothedTR[i]) * 100;
    const minusDI = smoothedTR[i] === 0 ? 0 : (smoothedMinusDM[i] / smoothedTR[i]) * 100;
    const diSum = plusDI + minusDI;
    const dxValue = diSum === 0 ? 0 : (Math.abs(plusDI - minusDI) / diSum) * 100;
    dx.push(dxValue);
  }

  // Calculate ADX (smoothed DX)
  const results: IndicatorResult[] = [];

  if (dx.length < period) return results;

  let adx = dx.slice(0, period).reduce((a, b) => a + b, 0) / period;
  results.push({ time: data[period * 2 - 1].time, value: adx });

  for (let i = period; i < dx.length; i++) {
    adx = (adx * (period - 1) + dx[i]) / period;
    results.push({ time: data[i + period].time, value: adx });
  }

  return results;
}

/**
 * Parabolic SAR
 */
export function calculateParabolicSAR(
  data: OHLCVData[],
  step: number = 0.02,
  maxStep: number = 0.2
): IndicatorResult[] {
  if (data.length < 2) return [];

  const results: IndicatorResult[] = [];

  let isUpTrend = data[1].close > data[0].close;
  let sar = isUpTrend ? data[0].low : data[0].high;
  let ep = isUpTrend ? data[1].high : data[1].low;
  let af = step;

  results.push({ time: data[0].time, value: sar });

  for (let i = 1; i < data.length; i++) {
    const prevSar = sar;

    // Calculate new SAR
    sar = prevSar + af * (ep - prevSar);

    // Adjust SAR if needed
    if (isUpTrend) {
      sar = Math.min(sar, data[i - 1].low);
      if (i >= 2) {
        sar = Math.min(sar, data[i - 2].low);
      }

      // Check for trend reversal
      if (data[i].low < sar) {
        isUpTrend = false;
        sar = ep;
        ep = data[i].low;
        af = step;
      } else {
        // Update EP and AF
        if (data[i].high > ep) {
          ep = data[i].high;
          af = Math.min(af + step, maxStep);
        }
      }
    } else {
      sar = Math.max(sar, data[i - 1].high);
      if (i >= 2) {
        sar = Math.max(sar, data[i - 2].high);
      }

      // Check for trend reversal
      if (data[i].high > sar) {
        isUpTrend = true;
        sar = ep;
        ep = data[i].high;
        af = step;
      } else {
        // Update EP and AF
        if (data[i].low < ep) {
          ep = data[i].low;
          af = Math.min(af + step, maxStep);
        }
      }
    }

    results.push({ time: data[i].time, value: sar });
  }

  return results;
}

// =============================================================================
// Indicator Factory
// =============================================================================

export type IndicatorCalculator = (
  data: OHLCVData[],
  params: Record<string, number>
) => IndicatorResult[];

export const indicatorCalculators: Record<string, IndicatorCalculator> = {
  sma: (data, params) => calculateSMA(data, params.period ?? 20),
  ema: (data, params) => calculateEMA(data, params.period ?? 20),
  wma: (data, params) => calculateWMA(data, params.period ?? 20),
  rsi: (data, params) => calculateRSI(data, params.period ?? 14),
  macd: (data, params) =>
    calculateMACD(data, params.fastPeriod ?? 12, params.slowPeriod ?? 26, params.signalPeriod ?? 9),
  stochastic: (data, params) =>
    calculateStochastic(data, params.kPeriod ?? 14, params.dPeriod ?? 3, params.smooth ?? 3),
  williamsR: (data, params) => calculateWilliamsR(data, params.period ?? 14),
  roc: (data, params) => calculateROC(data, params.period ?? 12),
  cci: (data, params) => calculateCCI(data, params.period ?? 20),
  bollinger: (data, params) =>
    calculateBollingerBands(data, params.period ?? 20, params.stdDev ?? 2),
  atr: (data, params) => calculateATR(data, params.period ?? 14),
  keltner: (data, params) =>
    calculateKeltnerChannel(
      data,
      params.emaPeriod ?? 20,
      params.atrPeriod ?? 10,
      params.multiplier ?? 2
    ),
  donchian: (data, params) => calculateDonchianChannel(data, params.period ?? 20),
  obv: (data) => calculateOBV(data),
  mfi: (data, params) => calculateMFI(data, params.period ?? 14),
  vwap: (data) => calculateVWAP(data),
  adx: (data, params) => calculateADX(data, params.period ?? 14),
  parabolicSar: (data, params) =>
    calculateParabolicSAR(data, params.step ?? 0.02, params.maxStep ?? 0.2),
};

/**
 * Calculate indicator values using the factory
 */
export function calculateIndicator(
  type: string,
  data: OHLCVData[],
  params: Record<string, number>
): IndicatorResult[] {
  const calculator = indicatorCalculators[type];
  if (!calculator) {
    console.warn(`Unknown indicator type: ${type}`);
    return [];
  }
  return calculator(data, params);
}
