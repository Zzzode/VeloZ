/**
 * Main Chart Container Component
 * Integrates TradingView Lightweight Charts with real-time data
 */
import { useEffect, useRef, useCallback, useMemo } from 'react';
import {
  createChart,
  CandlestickSeries,
  HistogramSeries,
  type IChartApi,
  type ISeriesApi,
  type CandlestickData,
  type HistogramData,
  type Time,
  ColorType,
  CrosshairMode,
} from 'lightweight-charts';
import { useChartStore } from '../store/chartStore';
import type { OHLCVData } from '../types';

// =============================================================================
// Types
// =============================================================================

interface ChartContainerProps {
  className?: string;
  height?: number;
}

// =============================================================================
// Color Schemes
// =============================================================================

const COLOR_SCHEMES = {
  greenRed: {
    upColor: '#26a69a',
    downColor: '#ef5350',
    wickUpColor: '#26a69a',
    wickDownColor: '#ef5350',
    volumeUp: 'rgba(38, 166, 154, 0.5)',
    volumeDown: 'rgba(239, 83, 80, 0.5)',
  },
  blueRed: {
    upColor: '#2196F3',
    downColor: '#ef5350',
    wickUpColor: '#2196F3',
    wickDownColor: '#ef5350',
    volumeUp: 'rgba(33, 150, 243, 0.5)',
    volumeDown: 'rgba(239, 83, 80, 0.5)',
  },
  monochrome: {
    upColor: '#4a4a4a',
    downColor: '#9e9e9e',
    wickUpColor: '#4a4a4a',
    wickDownColor: '#9e9e9e',
    volumeUp: 'rgba(74, 74, 74, 0.5)',
    volumeDown: 'rgba(158, 158, 158, 0.5)',
  },
};

// =============================================================================
// Helper Functions
// =============================================================================

function convertToChartData(candles: OHLCVData[]): CandlestickData<Time>[] {
  return candles.map((c) => ({
    time: c.time as Time,
    open: c.open,
    high: c.high,
    low: c.low,
    close: c.close,
  }));
}

function convertToVolumeData(
  candles: OHLCVData[],
  colors: typeof COLOR_SCHEMES.greenRed
): HistogramData<Time>[] {
  return candles.map((c) => ({
    time: c.time as Time,
    value: c.volume,
    color: c.close >= c.open ? colors.volumeUp : colors.volumeDown,
  }));
}

// =============================================================================
// Component
// =============================================================================

export function ChartContainer({ className = '', height = 500 }: ChartContainerProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candleSeriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const volumeSeriesRef = useRef<ISeriesApi<'Histogram'> | null>(null);

  const { candles, settings, isLoading } = useChartStore();

  const colors = useMemo(
    () => COLOR_SCHEMES[settings.colorScheme] || COLOR_SCHEMES.greenRed,
    [settings.colorScheme]
  );

  // Initialize chart
  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height,
      layout: {
        background: { type: ColorType.Solid, color: 'transparent' },
        textColor: '#6b7280',
      },
      grid: {
        vertLines: {
          color: settings.showGrid ? '#f1f5f9' : 'transparent',
        },
        horzLines: {
          color: settings.showGrid ? '#f1f5f9' : 'transparent',
        },
      },
      crosshair: {
        mode: CrosshairMode.Normal,
        vertLine: {
          color: '#e5e7eb',
          width: 1,
          style: 2,
          labelBackgroundColor: '#f9fafb',
        },
        horzLine: {
          color: '#e5e7eb',
          width: 1,
          style: 2,
          labelBackgroundColor: '#f9fafb',
        },
      },
      rightPriceScale: {
        visible: settings.showPriceScale,
        borderColor: '#e5e7eb',
        scaleMargins: {
          top: 0.1,
          bottom: 0.2,
        },
      },
      timeScale: {
        visible: settings.showTimeScale,
        borderColor: '#e5e7eb',
        timeVisible: true,
        secondsVisible: false,
      },
      handleScroll: {
        mouseWheel: true,
        pressedMouseMove: true,
        horzTouchDrag: true,
        vertTouchDrag: true,
      },
      handleScale: {
        axisPressedMouseMove: true,
        mouseWheel: true,
        pinch: true,
      },
    });

    // Add candlestick series using new API
    const candleSeries = chart.addSeries(CandlestickSeries, {
      upColor: colors.upColor,
      downColor: colors.downColor,
      borderVisible: false,
      wickUpColor: colors.wickUpColor,
      wickDownColor: colors.wickDownColor,
    });

    // Add volume series using new API
    const volumeSeries = chart.addSeries(HistogramSeries, {
      color: colors.volumeUp,
      priceFormat: { type: 'volume' },
      priceScaleId: 'volume',
    });

    chart.priceScale('volume').applyOptions({
      scaleMargins: { top: 0.85, bottom: 0 },
    });

    chartRef.current = chart;
    candleSeriesRef.current = candleSeries;
    volumeSeriesRef.current = volumeSeries;

    // Handle resize
    const handleResize = () => {
      if (containerRef.current && chartRef.current) {
        chartRef.current.applyOptions({
          width: containerRef.current.clientWidth,
        });
      }
    };

    const resizeObserver = new ResizeObserver(handleResize);
    resizeObserver.observe(containerRef.current);

    return () => {
      resizeObserver.disconnect();
      chart.remove();
      chartRef.current = null;
      candleSeriesRef.current = null;
      volumeSeriesRef.current = null;
    };
  }, [height]);

  // Update chart settings
  useEffect(() => {
    if (!chartRef.current) return;

    chartRef.current.applyOptions({
      grid: {
        vertLines: {
          color: settings.showGrid ? '#f1f5f9' : 'transparent',
        },
        horzLines: {
          color: settings.showGrid ? '#f1f5f9' : 'transparent',
        },
      },
      rightPriceScale: {
        visible: settings.showPriceScale,
      },
      timeScale: {
        visible: settings.showTimeScale,
      },
    });
  }, [settings.showGrid, settings.showPriceScale, settings.showTimeScale]);

  // Update colors
  useEffect(() => {
    if (!candleSeriesRef.current) return;

    candleSeriesRef.current.applyOptions({
      upColor: colors.upColor,
      downColor: colors.downColor,
      wickUpColor: colors.wickUpColor,
      wickDownColor: colors.wickDownColor,
    });
  }, [colors]);

  // Update data
  useEffect(() => {
    if (!candleSeriesRef.current || !volumeSeriesRef.current || candles.length === 0) return;

    const chartData = convertToChartData(candles);
    const volumeData = convertToVolumeData(candles, colors);

    candleSeriesRef.current.setData(chartData);
    volumeSeriesRef.current.setData(volumeData);

    // Fit content
    if (chartRef.current) {
      chartRef.current.timeScale().fitContent();
    }
  }, [candles, colors]);

  // Update single candle (for real-time updates)
  const updateCandle = useCallback(
    (candle: OHLCVData) => {
      if (!candleSeriesRef.current || !volumeSeriesRef.current) return;

      candleSeriesRef.current.update({
        time: candle.time as Time,
        open: candle.open,
        high: candle.high,
        low: candle.low,
        close: candle.close,
      });

      volumeSeriesRef.current.update({
        time: candle.time as Time,
        value: candle.volume,
        color: candle.close >= candle.open ? colors.volumeUp : colors.volumeDown,
      });
    },
    [colors]
  );

  // Expose update method via ref (for parent components)
  useEffect(() => {
    (window as unknown as { updateChartCandle?: typeof updateCandle }).updateChartCandle =
      updateCandle;
    return () => {
      delete (window as unknown as { updateChartCandle?: typeof updateCandle }).updateChartCandle;
    };
  }, [updateCandle]);

  return (
    <div className={`relative ${className}`}>
      {isLoading && (
        <div className="absolute inset-0 flex items-center justify-center bg-white/50 z-10">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-gray-900" />
        </div>
      )}
      <div ref={containerRef} className="w-full" style={{ height }} />
    </div>
  );
}
