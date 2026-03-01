/**
 * Chart Footer Component
 * Displays OHLCV data for the current/hovered candle
 */
import { useMemo } from 'react';
import { useChartStore } from '../store/chartStore';

// =============================================================================
// Types
// =============================================================================

interface ChartFooterProps {
  hoveredCandle?: {
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  } | null;
}

// =============================================================================
// Helper Functions
// =============================================================================

function formatPrice(value: number): string {
  return value.toLocaleString(undefined, {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  });
}

function formatVolume(value: number): string {
  if (value >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(2)}M`;
  }
  if (value >= 1_000) {
    return `${(value / 1_000).toFixed(2)}K`;
  }
  return value.toFixed(2);
}

function formatChange(open: number, close: number): { value: string; isPositive: boolean } {
  const change = ((close - open) / open) * 100;
  return {
    value: `${change >= 0 ? '+' : ''}${change.toFixed(2)}%`,
    isPositive: change >= 0,
  };
}

// =============================================================================
// Component
// =============================================================================

export function ChartFooter({ hoveredCandle }: ChartFooterProps) {
  const { candles, symbol } = useChartStore();

  // Use hovered candle or last candle
  const displayCandle = useMemo(() => {
    if (hoveredCandle) return hoveredCandle;
    if (candles.length === 0) return null;
    return candles[candles.length - 1];
  }, [hoveredCandle, candles]);

  if (!displayCandle) {
    return (
      <div className="flex items-center gap-4 px-4 py-2 border-t border-border bg-bg text-sm text-text-muted">
        No data available
      </div>
    );
  }

  const change = formatChange(displayCandle.open, displayCandle.close);

  return (
    <div className="flex flex-wrap items-center gap-x-6 gap-y-1 px-4 py-2 border-t border-border bg-bg text-sm">
      {/* Symbol */}
      <span className="font-semibold text-text">{symbol}</span>

      {/* OHLC Values */}
      <div className="flex items-center gap-1">
        <span className="text-text-muted">O:</span>
        <span className="text-text">{formatPrice(displayCandle.open)}</span>
      </div>

      <div className="flex items-center gap-1">
        <span className="text-text-muted">H:</span>
        <span className="text-success">{formatPrice(displayCandle.high)}</span>
      </div>

      <div className="flex items-center gap-1">
        <span className="text-text-muted">L:</span>
        <span className="text-danger">{formatPrice(displayCandle.low)}</span>
      </div>

      <div className="flex items-center gap-1">
        <span className="text-text-muted">C:</span>
        <span className="text-text">{formatPrice(displayCandle.close)}</span>
      </div>

      {/* Change */}
      <div className="flex items-center gap-1">
        <span className="text-text-muted">Change:</span>
        <span className={change.isPositive ? 'text-success' : 'text-danger'}>{change.value}</span>
      </div>

      {/* Volume */}
      <div className="flex items-center gap-1">
        <span className="text-text-muted">Vol:</span>
        <span className="text-text">{formatVolume(displayCandle.volume)}</span>
      </div>
    </div>
  );
}
