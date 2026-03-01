/**
 * Chart Header Component
 * Symbol selector, timeframe buttons, and toolbar
 */
import { useState } from 'react';
import { Button, Select } from '@/shared/components';
import { useChartStore } from '../store/chartStore';
import { IndicatorMenu } from './IndicatorMenu';
import { DrawingToolbar } from './DrawingToolbar';
import { ChartSettingsModal } from './ChartSettingsModal';
import type { Timeframe } from '../types';
import {
  Settings,
  Maximize2,
  TrendingUp,
  PenTool,
} from 'lucide-react';

// =============================================================================
// Types
// =============================================================================

interface ChartHeaderProps {
  onFullscreen?: () => void;
}

// =============================================================================
// Constants
// =============================================================================

const TIMEFRAMES: { value: Timeframe; label: string }[] = [
  { value: '1m', label: '1m' },
  { value: '5m', label: '5m' },
  { value: '15m', label: '15m' },
  { value: '30m', label: '30m' },
  { value: '1h', label: '1H' },
  { value: '4h', label: '4H' },
  { value: '1d', label: '1D' },
  { value: '1w', label: '1W' },
];

const SYMBOLS = [
  { value: 'BTCUSDT', label: 'BTC/USDT' },
  { value: 'ETHUSDT', label: 'ETH/USDT' },
  { value: 'BNBUSDT', label: 'BNB/USDT' },
  { value: 'SOLUSDT', label: 'SOL/USDT' },
  { value: 'XRPUSDT', label: 'XRP/USDT' },
  { value: 'ADAUSDT', label: 'ADA/USDT' },
  { value: 'DOGEUSDT', label: 'DOGE/USDT' },
  { value: 'DOTUSDT', label: 'DOT/USDT' },
];

// =============================================================================
// Component
// =============================================================================

export function ChartHeader({ onFullscreen }: ChartHeaderProps) {
  const [showIndicatorMenu, setShowIndicatorMenu] = useState(false);
  const [showDrawingTools, setShowDrawingTools] = useState(false);
  const [showSettings, setShowSettings] = useState(false);

  const { symbol, timeframe, setSymbol, setTimeframe, isConnected } = useChartStore();

  return (
    <div className="flex flex-wrap items-center justify-between gap-4 p-3 border-b border-border bg-bg">
      {/* Left Section: Symbol & Exchange */}
      <div className="flex items-center gap-4">
        {/* Symbol Selector */}
        <Select
          options={SYMBOLS}
          value={symbol}
          onChange={(e) => setSymbol(e.target.value)}
          className="w-36"
        />

        {/* Connection Status */}
        <div className="flex items-center gap-2">
          <div
            className={`w-2 h-2 rounded-full ${
              isConnected ? 'bg-success' : 'bg-danger'
            }`}
          />
          <span className="text-xs text-text-muted">
            {isConnected ? 'Live' : 'Disconnected'}
          </span>
        </div>
      </div>

      {/* Center Section: Timeframe Buttons */}
      <div className="flex items-center gap-1">
        {TIMEFRAMES.map(({ value, label }) => (
          <Button
            key={value}
            variant={timeframe === value ? 'primary' : 'secondary'}
            size="sm"
            onClick={() => setTimeframe(value)}
            className="min-w-[40px]"
          >
            {label}
          </Button>
        ))}
      </div>

      {/* Right Section: Tools */}
      <div className="flex items-center gap-2">
        {/* Indicators Button */}
        <Button
          variant="secondary"
          size="sm"
          onClick={() => setShowIndicatorMenu(!showIndicatorMenu)}
          className="flex items-center gap-1"
        >
          <TrendingUp className="w-4 h-4" />
          <span className="hidden sm:inline">Indicators</span>
        </Button>

        {/* Drawing Tools Button */}
        <Button
          variant="secondary"
          size="sm"
          onClick={() => setShowDrawingTools(!showDrawingTools)}
          className="flex items-center gap-1"
        >
          <PenTool className="w-4 h-4" />
          <span className="hidden sm:inline">Draw</span>
        </Button>

        {/* Settings Button */}
        <Button
          variant="secondary"
          size="sm"
          onClick={() => setShowSettings(true)}
        >
          <Settings className="w-4 h-4" />
        </Button>

        {/* Fullscreen Button */}
        {onFullscreen && (
          <Button variant="secondary" size="sm" onClick={onFullscreen}>
            <Maximize2 className="w-4 h-4" />
          </Button>
        )}
      </div>

      {/* Indicator Menu Dropdown */}
      {showIndicatorMenu && (
        <div className="absolute top-full left-0 right-0 z-50 mt-1">
          <IndicatorMenu onClose={() => setShowIndicatorMenu(false)} />
        </div>
      )}

      {/* Drawing Toolbar */}
      {showDrawingTools && (
        <div className="absolute top-full left-0 z-50 mt-1">
          <DrawingToolbar onClose={() => setShowDrawingTools(false)} />
        </div>
      )}

      {/* Settings Modal */}
      {showSettings && (
        <ChartSettingsModal onClose={() => setShowSettings(false)} />
      )}
    </div>
  );
}
