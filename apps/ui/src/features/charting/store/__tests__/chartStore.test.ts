/**
 * Chart Store Tests
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { useChartStore } from '../chartStore';
import type { OHLCVData, IndicatorConfig } from '../../types';

// Reset store before each test
beforeEach(() => {
  useChartStore.setState({
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
    settings: {
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
    },
    isConnected: false,
  });
});

// =============================================================================
// Symbol/Exchange/Timeframe Tests
// =============================================================================

describe('Symbol/Exchange/Timeframe', () => {
  it('sets symbol and clears candles', () => {
    const store = useChartStore.getState();
    store.setCandles([{ time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 }]);

    store.setSymbol('ETHUSDT');

    const state = useChartStore.getState();
    expect(state.symbol).toBe('ETHUSDT');
    expect(state.candles).toHaveLength(0);
  });

  it('sets exchange and clears candles', () => {
    const store = useChartStore.getState();
    store.setCandles([{ time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 }]);

    store.setExchange('coinbase');

    const state = useChartStore.getState();
    expect(state.exchange).toBe('coinbase');
    expect(state.candles).toHaveLength(0);
  });

  it('sets timeframe and clears candles', () => {
    const store = useChartStore.getState();
    store.setCandles([{ time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 }]);

    store.setTimeframe('4h');

    const state = useChartStore.getState();
    expect(state.timeframe).toBe('4h');
    expect(state.candles).toHaveLength(0);
  });
});

// =============================================================================
// Candle Data Tests
// =============================================================================

describe('Candle Data', () => {
  it('sets candles', () => {
    const candles: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 },
      { time: 2, open: 102, high: 108, low: 100, close: 106, volume: 1200 },
    ];

    useChartStore.getState().setCandles(candles);

    const state = useChartStore.getState();
    expect(state.candles).toHaveLength(2);
    expect(state.isLoading).toBe(false);
  });

  it('updates existing candle', () => {
    const candles: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 },
    ];
    useChartStore.getState().setCandles(candles);

    const updatedCandle: OHLCVData = {
      time: 1,
      open: 100,
      high: 110,
      low: 95,
      close: 108,
      volume: 1500,
    };
    useChartStore.getState().updateCandle(updatedCandle);

    const state = useChartStore.getState();
    expect(state.candles).toHaveLength(1);
    expect(state.candles[0].high).toBe(110);
    expect(state.candles[0].close).toBe(108);
  });

  it('appends new candle when time differs', () => {
    const candles: OHLCVData[] = [
      { time: 1, open: 100, high: 105, low: 95, close: 102, volume: 1000 },
    ];
    useChartStore.getState().setCandles(candles);

    const newCandle: OHLCVData = {
      time: 2,
      open: 102,
      high: 108,
      low: 100,
      close: 106,
      volume: 1200,
    };
    useChartStore.getState().updateCandle(newCandle);

    const state = useChartStore.getState();
    expect(state.candles).toHaveLength(2);
  });

  it('appends candle', () => {
    useChartStore.getState().setCandles([]);

    const candle: OHLCVData = {
      time: 1,
      open: 100,
      high: 105,
      low: 95,
      close: 102,
      volume: 1000,
    };
    useChartStore.getState().appendCandle(candle);

    const state = useChartStore.getState();
    expect(state.candles).toHaveLength(1);
  });
});

// =============================================================================
// Indicator Tests
// =============================================================================

describe('Indicators', () => {
  const sampleIndicator: IndicatorConfig = {
    id: 'sma-1',
    type: 'sma',
    params: { period: 20 },
    color: '#2196F3',
    visible: true,
    overlay: true,
  };

  it('adds indicator', () => {
    useChartStore.getState().addIndicator(sampleIndicator);

    const state = useChartStore.getState();
    expect(state.indicators).toHaveLength(1);
    expect(state.indicators[0].id).toBe('sma-1');
  });

  it('removes indicator', () => {
    useChartStore.getState().addIndicator(sampleIndicator);
    useChartStore.getState().removeIndicator('sma-1');

    const state = useChartStore.getState();
    expect(state.indicators).toHaveLength(0);
  });

  it('updates indicator', () => {
    useChartStore.getState().addIndicator(sampleIndicator);
    useChartStore.getState().updateIndicator('sma-1', { params: { period: 50 } });

    const state = useChartStore.getState();
    expect(state.indicators[0].params.period).toBe(50);
  });

  it('toggles indicator visibility', () => {
    useChartStore.getState().addIndicator(sampleIndicator);
    useChartStore.getState().toggleIndicatorVisibility('sma-1');

    let state = useChartStore.getState();
    expect(state.indicators[0].visible).toBe(false);

    useChartStore.getState().toggleIndicatorVisibility('sma-1');

    state = useChartStore.getState();
    expect(state.indicators[0].visible).toBe(true);
  });
});

// =============================================================================
// Drawing Tests
// =============================================================================

describe('Drawings', () => {
  const sampleDrawing = {
    id: 'line-1',
    type: 'trendLine' as const,
    points: [
      { time: 1, price: 100 },
      { time: 2, price: 110 },
    ],
    style: {
      color: '#FF0000',
      lineWidth: 2,
      lineStyle: 'solid' as const,
    },
  };

  it('sets active drawing tool', () => {
    useChartStore.getState().setActiveDrawingTool('trendLine');

    const state = useChartStore.getState();
    expect(state.activeDrawingTool).toBe('trendLine');
  });

  it('adds drawing', () => {
    useChartStore.getState().addDrawing(sampleDrawing);

    const state = useChartStore.getState();
    expect(state.drawings).toHaveLength(1);
  });

  it('removes drawing', () => {
    useChartStore.getState().addDrawing(sampleDrawing);
    useChartStore.getState().removeDrawing('line-1');

    const state = useChartStore.getState();
    expect(state.drawings).toHaveLength(0);
  });

  it('clears all drawings', () => {
    useChartStore.getState().addDrawing(sampleDrawing);
    useChartStore.getState().addDrawing({ ...sampleDrawing, id: 'line-2' });
    useChartStore.getState().clearAllDrawings();

    const state = useChartStore.getState();
    expect(state.drawings).toHaveLength(0);
  });
});

// =============================================================================
// Settings Tests
// =============================================================================

describe('Settings', () => {
  it('updates settings', () => {
    useChartStore.getState().updateSettings({ chartType: 'line', showGrid: false });

    const state = useChartStore.getState();
    expect(state.settings.chartType).toBe('line');
    expect(state.settings.showGrid).toBe(false);
    // Other settings should remain unchanged
    expect(state.settings.colorScheme).toBe('greenRed');
  });
});

// =============================================================================
// Connection Tests
// =============================================================================

describe('Connection', () => {
  it('sets connected state', () => {
    useChartStore.getState().setConnected(true);

    let state = useChartStore.getState();
    expect(state.isConnected).toBe(true);

    useChartStore.getState().setConnected(false);

    state = useChartStore.getState();
    expect(state.isConnected).toBe(false);
  });
});

// =============================================================================
// Loading/Error Tests
// =============================================================================

describe('Loading/Error', () => {
  it('sets loading state', () => {
    useChartStore.getState().setLoading(true);

    let state = useChartStore.getState();
    expect(state.isLoading).toBe(true);

    useChartStore.getState().setLoading(false);

    state = useChartStore.getState();
    expect(state.isLoading).toBe(false);
  });

  it('sets error and clears loading', () => {
    useChartStore.getState().setLoading(true);
    useChartStore.getState().setError('Failed to fetch data');

    const state = useChartStore.getState();
    expect(state.error).toBe('Failed to fetch data');
    expect(state.isLoading).toBe(false);
  });
});
