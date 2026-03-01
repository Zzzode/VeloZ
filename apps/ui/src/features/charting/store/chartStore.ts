/**
 * Chart Zustand store for charting state management
 */
import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type {
  Timeframe,
  OHLCVData,
  IndicatorConfig,
  Drawing,
  DrawingToolType,
  ChartSettings,
  ChartLayout,
  ChartOrder,
} from '../types';

// =============================================================================
// Store Types
// =============================================================================

interface ChartState {
  // Selected symbol and timeframe
  symbol: string;
  exchange: string;
  timeframe: Timeframe;

  // OHLCV data
  candles: OHLCVData[];
  isLoading: boolean;
  error: string | null;

  // Indicators
  indicators: IndicatorConfig[];

  // Drawings
  drawings: Drawing[];
  activeDrawingTool: DrawingToolType;
  isDrawing: boolean;

  // Orders overlay
  chartOrders: ChartOrder[];
  showOrderOverlay: boolean;

  // Layout
  layout: ChartLayout;

  // Settings
  settings: ChartSettings;

  // Connection state
  isConnected: boolean;

  // Actions
  setSymbol: (symbol: string) => void;
  setExchange: (exchange: string) => void;
  setTimeframe: (timeframe: Timeframe) => void;
  setCandles: (candles: OHLCVData[]) => void;
  updateCandle: (candle: OHLCVData) => void;
  appendCandle: (candle: OHLCVData) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string | null) => void;

  // Indicator actions
  addIndicator: (indicator: IndicatorConfig) => void;
  removeIndicator: (id: string) => void;
  updateIndicator: (id: string, updates: Partial<IndicatorConfig>) => void;
  toggleIndicatorVisibility: (id: string) => void;

  // Drawing actions
  setActiveDrawingTool: (tool: DrawingToolType) => void;
  addDrawing: (drawing: Drawing) => void;
  removeDrawing: (id: string) => void;
  updateDrawing: (id: string, updates: Partial<Drawing>) => void;
  clearAllDrawings: () => void;
  setIsDrawing: (isDrawing: boolean) => void;

  // Order overlay actions
  setChartOrders: (orders: ChartOrder[]) => void;
  toggleOrderOverlay: () => void;

  // Layout actions
  setLayout: (layout: ChartLayout) => void;

  // Settings actions
  updateSettings: (settings: Partial<ChartSettings>) => void;

  // Connection actions
  setConnected: (connected: boolean) => void;
}

// =============================================================================
// Default Values
// =============================================================================

const DEFAULT_SETTINGS: ChartSettings = {
  chartType: 'candlestick',
  colorScheme: 'greenRed',
  showGrid: true,
  showPriceScale: true,
  showTimeScale: true,
  logarithmic: false,
  showOpenOrders: true,
  showPositions: true,
  showTradeHistory: false,
  enableChartTrading: true,
};

// =============================================================================
// Store Implementation
// =============================================================================

export const useChartStore = create<ChartState>()(
  subscribeWithSelector((set) => ({
    // Initial state
    symbol: 'BTCUSDT',
    exchange: 'binance',
    timeframe: '1h',
    candles: [],
    isLoading: false,
    error: null,
    indicators: [],
    drawings: [],
    activeDrawingTool: 'cursor',
    isDrawing: false,
    chartOrders: [],
    showOrderOverlay: true,
    layout: 'single',
    settings: DEFAULT_SETTINGS,
    isConnected: false,

    // Symbol/Exchange/Timeframe actions
    setSymbol: (symbol) => set({ symbol, candles: [], error: null }),
    setExchange: (exchange) => set({ exchange, candles: [], error: null }),
    setTimeframe: (timeframe) => set({ timeframe, candles: [], error: null }),

    // Candle data actions
    setCandles: (candles) => set({ candles, isLoading: false }),

    updateCandle: (candle) => set((state) => {
      const candles = [...state.candles];
      const lastIndex = candles.length - 1;

      if (lastIndex >= 0 && candles[lastIndex].time === candle.time) {
        // Update existing candle
        candles[lastIndex] = candle;
      } else {
        // Append new candle
        candles.push(candle);
      }

      return { candles };
    }),

    appendCandle: (candle) => set((state) => ({
      candles: [...state.candles, candle],
    })),

    setLoading: (isLoading) => set({ isLoading }),
    setError: (error) => set({ error, isLoading: false }),

    // Indicator actions
    addIndicator: (indicator) => set((state) => ({
      indicators: [...state.indicators, indicator],
    })),

    removeIndicator: (id) => set((state) => ({
      indicators: state.indicators.filter((i) => i.id !== id),
    })),

    updateIndicator: (id, updates) => set((state) => ({
      indicators: state.indicators.map((i) =>
        i.id === id ? { ...i, ...updates } : i
      ),
    })),

    toggleIndicatorVisibility: (id) => set((state) => ({
      indicators: state.indicators.map((i) =>
        i.id === id ? { ...i, visible: !i.visible } : i
      ),
    })),

    // Drawing actions
    setActiveDrawingTool: (tool) => set({ activeDrawingTool: tool }),

    addDrawing: (drawing) => set((state) => ({
      drawings: [...state.drawings, drawing],
    })),

    removeDrawing: (id) => set((state) => ({
      drawings: state.drawings.filter((d) => d.id !== id),
    })),

    updateDrawing: (id, updates) => set((state) => ({
      drawings: state.drawings.map((d) =>
        d.id === id ? { ...d, ...updates } : d
      ),
    })),

    clearAllDrawings: () => set({ drawings: [] }),

    setIsDrawing: (isDrawing) => set({ isDrawing }),

    // Order overlay actions
    setChartOrders: (chartOrders) => set({ chartOrders }),
    toggleOrderOverlay: () => set((state) => ({
      showOrderOverlay: !state.showOrderOverlay,
    })),

    // Layout actions
    setLayout: (layout) => set({ layout }),

    // Settings actions
    updateSettings: (updates) => set((state) => ({
      settings: { ...state.settings, ...updates },
    })),

    // Connection actions
    setConnected: (isConnected) => set({ isConnected }),
  }))
);

// =============================================================================
// Selectors
// =============================================================================

export const selectCurrentCandles = (state: ChartState) => state.candles;
export const selectIndicators = (state: ChartState) => state.indicators;
export const selectVisibleIndicators = (state: ChartState) =>
  state.indicators.filter((i) => i.visible);
export const selectOverlayIndicators = (state: ChartState) =>
  state.indicators.filter((i) => i.visible && i.overlay);
export const selectPaneIndicators = (state: ChartState) =>
  state.indicators.filter((i) => i.visible && !i.overlay);
export const selectDrawings = (state: ChartState) => state.drawings;
export const selectActiveDrawingTool = (state: ChartState) => state.activeDrawingTool;
export const selectChartSettings = (state: ChartState) => state.settings;
export const selectChartOrders = (state: ChartState) => state.chartOrders;
