/**
 * Main Chart Component
 * Combines chart container, header, footer, and order panel
 */
import { useState, useCallback, useMemo } from 'react';
import { Card } from '@/shared/components';
import { ChartContainer } from './ChartContainer';
import { ChartHeader } from './ChartHeader';
import { ChartFooter } from './ChartFooter';
import { QuickOrderPanel } from './QuickOrderPanel';
import { useChartStore } from '../store/chartStore';
import { useKlineData } from '../hooks/useKlineData';

// =============================================================================
// Types
// =============================================================================

interface ChartProps {
  showOrderPanel?: boolean;
  height?: number;
  className?: string;
}

// =============================================================================
// Component
// =============================================================================

export function Chart({ showOrderPanel = true, height = 500, className = '' }: ChartProps) {
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [hoveredCandle, _setHoveredCandle] = useState<{
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  } | null>(null);

  const { candles, settings } = useChartStore();

  // Load kline data
  useKlineData();

  // Get current price from last candle
  const currentPrice = useMemo(() => {
    if (candles.length === 0) return 0;
    return candles[candles.length - 1].close;
  }, [candles]);

  // Handle fullscreen toggle
  const handleFullscreen = useCallback(() => {
    setIsFullscreen(!isFullscreen);
  }, [isFullscreen]);

  // Fullscreen container styles
  const fullscreenStyles = isFullscreen
    ? 'fixed inset-0 z-50 bg-bg'
    : '';

  return (
    <div className={`${fullscreenStyles} ${className}`}>
      <div className={`flex ${showOrderPanel ? 'gap-4' : ''}`}>
        {/* Main Chart */}
        <Card className="flex-1 overflow-hidden">
          <div className="relative">
            <ChartHeader onFullscreen={handleFullscreen} />
            <ChartContainer height={isFullscreen ? window.innerHeight - 150 : height} />
            <ChartFooter hoveredCandle={hoveredCandle} />
          </div>
        </Card>

        {/* Quick Order Panel */}
        {showOrderPanel && settings.enableChartTrading && (
          <div className="w-72 flex-shrink-0">
            <QuickOrderPanel currentPrice={currentPrice} />
          </div>
        )}
      </div>
    </div>
  );
}
