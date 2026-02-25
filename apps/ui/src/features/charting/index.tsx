/**
 * Charting Feature Module
 * Real-time charting with TradingView Lightweight Charts
 */
import { Chart } from './components';

export default function Charting() {
  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold text-text">Charts</h1>
        <p className="text-text-muted mt-1">Real-time market charts with technical analysis</p>
      </div>

      {/* Main Chart */}
      <Chart showOrderPanel={true} height={600} />
    </div>
  );
}

// Re-export components and hooks for external use
export { Chart, ChartContainer, ChartHeader, ChartFooter } from './components';
export { useChartStore } from './store';
export { useKlineData, useChartWebSocket } from './hooks';
export {
  calculateSMA,
  calculateEMA,
  calculateRSI,
  calculateMACD,
  calculateBollingerBands,
  calculateIndicator,
  INDICATOR_DEFINITIONS,
  INDICATOR_COLORS,
} from './indicators';
export type {
  Timeframe,
  OHLCVData,
  IndicatorType,
  IndicatorConfig,
  DrawingToolType,
  Drawing,
  ChartSettings,
} from './types';
