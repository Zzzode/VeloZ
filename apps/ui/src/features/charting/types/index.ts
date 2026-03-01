/**
 * Charting System Type Definitions
 */

// =============================================================================
// Timeframe Types
// =============================================================================

export type Timeframe = '1m' | '5m' | '15m' | '30m' | '1h' | '4h' | '1d' | '1w';

export const TIMEFRAME_MS: Record<Timeframe, number> = {
  '1m': 60 * 1000,
  '5m': 5 * 60 * 1000,
  '15m': 15 * 60 * 1000,
  '30m': 30 * 60 * 1000,
  '1h': 60 * 60 * 1000,
  '4h': 4 * 60 * 60 * 1000,
  '1d': 24 * 60 * 60 * 1000,
  '1w': 7 * 24 * 60 * 60 * 1000,
};

// =============================================================================
// OHLCV Data Types
// =============================================================================

export interface OHLCVData {
  time: number; // Unix timestamp in seconds
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface CandleData {
  time: number;
  open: number;
  high: number;
  low: number;
  close: number;
}

export interface VolumeData {
  time: number;
  value: number;
  color: string;
}

// =============================================================================
// Indicator Types
// =============================================================================

export type IndicatorType =
  | 'sma'
  | 'ema'
  | 'wma'
  | 'bollinger'
  | 'rsi'
  | 'macd'
  | 'stochastic'
  | 'atr'
  | 'vwap'
  | 'obv'
  | 'adx'
  | 'cci'
  | 'williamsR'
  | 'roc'
  | 'mfi'
  | 'ichimoku'
  | 'parabolicSar'
  | 'keltner'
  | 'donchian'
  | 'pivotPoints';

export type IndicatorCategory = 'trend' | 'momentum' | 'volatility' | 'volume';

export interface IndicatorConfig {
  id: string;
  type: IndicatorType;
  params: Record<string, number>;
  color?: string;
  visible: boolean;
  overlay: boolean; // true = overlay on price chart, false = separate pane
}

export interface IndicatorResult {
  time: number;
  value: number;
  [key: string]: number; // For multi-line indicators
}

export interface IndicatorDefinition {
  type: IndicatorType;
  name: string;
  category: IndicatorCategory;
  overlay: boolean;
  defaultParams: Record<string, number>;
  paramLabels: Record<string, string>;
}

// =============================================================================
// Drawing Types
// =============================================================================

export type DrawingToolType =
  | 'cursor'
  | 'trendLine'
  | 'horizontalLine'
  | 'verticalLine'
  | 'ray'
  | 'channel'
  | 'fibonacci'
  | 'rectangle'
  | 'text'
  | 'measure';

export interface DrawingPoint {
  time: number;
  price: number;
}

export interface Drawing {
  id: string;
  type: DrawingToolType;
  points: DrawingPoint[];
  style: DrawingStyle;
  text?: string;
}

export interface DrawingStyle {
  color: string;
  lineWidth: number;
  lineStyle: 'solid' | 'dashed' | 'dotted';
  extendLeft?: boolean;
  extendRight?: boolean;
  showLabels?: boolean;
}

// =============================================================================
// Chart Layout Types
// =============================================================================

export type ChartLayout = 'single' | 'split' | 'grid';

export interface ChartPaneConfig {
  id: string;
  symbol: string;
  exchange: string;
  timeframe: Timeframe;
  indicators: IndicatorConfig[];
  drawings: Drawing[];
}

// =============================================================================
// Order Overlay Types
// =============================================================================

export interface ChartOrder {
  id: string;
  side: 'buy' | 'sell';
  type: 'limit' | 'market' | 'stop' | 'takeProfit' | 'stopLoss';
  price: number;
  quantity: number;
  status: 'pending' | 'open' | 'filled' | 'cancelled';
}

// =============================================================================
// Chart Settings Types
// =============================================================================

export type ChartType = 'candlestick' | 'line' | 'bar' | 'area' | 'heikinAshi';

export type ColorScheme = 'greenRed' | 'blueRed' | 'monochrome';

export interface ChartSettings {
  chartType: ChartType;
  colorScheme: ColorScheme;
  showGrid: boolean;
  showPriceScale: boolean;
  showTimeScale: boolean;
  logarithmic: boolean;
  showOpenOrders: boolean;
  showPositions: boolean;
  showTradeHistory: boolean;
  enableChartTrading: boolean;
}

// =============================================================================
// WebSocket Message Types
// =============================================================================

export interface KlineMessage {
  type: 'kline';
  symbol: string;
  exchange: string;
  timeframe: Timeframe;
  data: OHLCVData;
  isClosed: boolean;
}

export interface KlineSubscribeRequest {
  action: 'subscribe';
  symbol: string;
  exchange: string;
  timeframe: Timeframe;
}

export interface KlineUnsubscribeRequest {
  action: 'unsubscribe';
  symbol: string;
  exchange: string;
  timeframe: Timeframe;
}
