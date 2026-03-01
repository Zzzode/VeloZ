/**
 * Indicator Definitions
 * Metadata for all supported technical indicators
 */
import type { IndicatorDefinition, IndicatorType } from '../types';

export const INDICATOR_DEFINITIONS: Record<IndicatorType, IndicatorDefinition> = {
  // Trend Indicators
  sma: {
    type: 'sma',
    name: 'Simple Moving Average',
    category: 'trend',
    overlay: true,
    defaultParams: { period: 20 },
    paramLabels: { period: 'Period' },
  },
  ema: {
    type: 'ema',
    name: 'Exponential Moving Average',
    category: 'trend',
    overlay: true,
    defaultParams: { period: 20 },
    paramLabels: { period: 'Period' },
  },
  wma: {
    type: 'wma',
    name: 'Weighted Moving Average',
    category: 'trend',
    overlay: true,
    defaultParams: { period: 20 },
    paramLabels: { period: 'Period' },
  },
  ichimoku: {
    type: 'ichimoku',
    name: 'Ichimoku Cloud',
    category: 'trend',
    overlay: true,
    defaultParams: {
      conversionPeriod: 9,
      basePeriod: 26,
      spanPeriod: 52,
      displacement: 26,
    },
    paramLabels: {
      conversionPeriod: 'Conversion Period',
      basePeriod: 'Base Period',
      spanPeriod: 'Span Period',
      displacement: 'Displacement',
    },
  },
  parabolicSar: {
    type: 'parabolicSar',
    name: 'Parabolic SAR',
    category: 'trend',
    overlay: true,
    defaultParams: { step: 0.02, maxStep: 0.2 },
    paramLabels: { step: 'Step', maxStep: 'Max Step' },
  },
  adx: {
    type: 'adx',
    name: 'Average Directional Index',
    category: 'trend',
    overlay: false,
    defaultParams: { period: 14 },
    paramLabels: { period: 'Period' },
  },

  // Momentum Indicators
  rsi: {
    type: 'rsi',
    name: 'Relative Strength Index',
    category: 'momentum',
    overlay: false,
    defaultParams: { period: 14 },
    paramLabels: { period: 'Period' },
  },
  macd: {
    type: 'macd',
    name: 'MACD',
    category: 'momentum',
    overlay: false,
    defaultParams: { fastPeriod: 12, slowPeriod: 26, signalPeriod: 9 },
    paramLabels: {
      fastPeriod: 'Fast Period',
      slowPeriod: 'Slow Period',
      signalPeriod: 'Signal Period',
    },
  },
  stochastic: {
    type: 'stochastic',
    name: 'Stochastic Oscillator',
    category: 'momentum',
    overlay: false,
    defaultParams: { kPeriod: 14, dPeriod: 3, smooth: 3 },
    paramLabels: { kPeriod: '%K Period', dPeriod: '%D Period', smooth: 'Smoothing' },
  },
  cci: {
    type: 'cci',
    name: 'Commodity Channel Index',
    category: 'momentum',
    overlay: false,
    defaultParams: { period: 20 },
    paramLabels: { period: 'Period' },
  },
  williamsR: {
    type: 'williamsR',
    name: "Williams %R",
    category: 'momentum',
    overlay: false,
    defaultParams: { period: 14 },
    paramLabels: { period: 'Period' },
  },
  roc: {
    type: 'roc',
    name: 'Rate of Change',
    category: 'momentum',
    overlay: false,
    defaultParams: { period: 12 },
    paramLabels: { period: 'Period' },
  },

  // Volatility Indicators
  bollinger: {
    type: 'bollinger',
    name: 'Bollinger Bands',
    category: 'volatility',
    overlay: true,
    defaultParams: { period: 20, stdDev: 2 },
    paramLabels: { period: 'Period', stdDev: 'Standard Deviation' },
  },
  atr: {
    type: 'atr',
    name: 'Average True Range',
    category: 'volatility',
    overlay: false,
    defaultParams: { period: 14 },
    paramLabels: { period: 'Period' },
  },
  keltner: {
    type: 'keltner',
    name: 'Keltner Channel',
    category: 'volatility',
    overlay: true,
    defaultParams: { emaPeriod: 20, atrPeriod: 10, multiplier: 2 },
    paramLabels: {
      emaPeriod: 'EMA Period',
      atrPeriod: 'ATR Period',
      multiplier: 'Multiplier',
    },
  },
  donchian: {
    type: 'donchian',
    name: 'Donchian Channel',
    category: 'volatility',
    overlay: true,
    defaultParams: { period: 20 },
    paramLabels: { period: 'Period' },
  },

  // Volume Indicators
  vwap: {
    type: 'vwap',
    name: 'VWAP',
    category: 'volume',
    overlay: true,
    defaultParams: {},
    paramLabels: {},
  },
  obv: {
    type: 'obv',
    name: 'On-Balance Volume',
    category: 'volume',
    overlay: false,
    defaultParams: {},
    paramLabels: {},
  },
  mfi: {
    type: 'mfi',
    name: 'Money Flow Index',
    category: 'volume',
    overlay: false,
    defaultParams: { period: 14 },
    paramLabels: { period: 'Period' },
  },

  // Support/Resistance (placeholders)
  pivotPoints: {
    type: 'pivotPoints',
    name: 'Pivot Points',
    category: 'trend',
    overlay: true,
    defaultParams: {},
    paramLabels: {},
  },
};

export const INDICATOR_COLORS: Record<string, string> = {
  sma: '#2196F3',
  ema: '#FF9800',
  wma: '#9C27B0',
  rsi: '#FF5722',
  macd: '#4CAF50',
  stochastic: '#00BCD4',
  bollinger: '#E91E63',
  atr: '#795548',
  vwap: '#3F51B5',
  obv: '#607D8B',
  adx: '#FFC107',
  cci: '#009688',
  williamsR: '#673AB7',
  roc: '#8BC34A',
  mfi: '#CDDC39',
  keltner: '#FF4081',
  donchian: '#00E676',
  parabolicSar: '#FF6D00',
  ichimoku: '#7C4DFF',
  pivotPoints: '#448AFF',
};

export const POPULAR_INDICATORS: IndicatorType[] = [
  'rsi',
  'macd',
  'bollinger',
  'ema',
  'sma',
  'stochastic',
];

export function getIndicatorsByCategory(category: string): IndicatorDefinition[] {
  return Object.values(INDICATOR_DEFINITIONS).filter((def) => def.category === category);
}

export function getOverlayIndicators(): IndicatorDefinition[] {
  return Object.values(INDICATOR_DEFINITIONS).filter((def) => def.overlay);
}

export function getPaneIndicators(): IndicatorDefinition[] {
  return Object.values(INDICATOR_DEFINITIONS).filter((def) => !def.overlay);
}
