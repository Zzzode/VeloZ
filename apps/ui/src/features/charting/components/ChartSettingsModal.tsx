/**
 * Chart Settings Modal Component
 * Configuration panel for chart appearance and behavior
 */
import { useState } from 'react';
import { Button, Modal, Select } from '@/shared/components';
import { useChartStore } from '../store/chartStore';
import type { ChartSettings, ChartType, ColorScheme } from '../types';

// =============================================================================
// Types
// =============================================================================

interface ChartSettingsModalProps {
  onClose: () => void;
}

// =============================================================================
// Constants
// =============================================================================

const CHART_TYPES: { value: ChartType; label: string }[] = [
  { value: 'candlestick', label: 'Candlestick' },
  { value: 'line', label: 'Line' },
  { value: 'bar', label: 'Bar (OHLC)' },
  { value: 'area', label: 'Area' },
  { value: 'heikinAshi', label: 'Heikin Ashi' },
];

const COLOR_SCHEMES: { value: ColorScheme; label: string }[] = [
  { value: 'greenRed', label: 'Green / Red' },
  { value: 'blueRed', label: 'Blue / Red' },
  { value: 'monochrome', label: 'Monochrome' },
];

// =============================================================================
// Component
// =============================================================================

export function ChartSettingsModal({ onClose }: ChartSettingsModalProps) {
  const { settings, updateSettings } = useChartStore();
  const [localSettings, setLocalSettings] = useState<ChartSettings>(settings);
  const [isOpen] = useState(true);

  const handleChange = <K extends keyof ChartSettings>(key: K, value: ChartSettings[K]) => {
    setLocalSettings((prev) => ({ ...prev, [key]: value }));
  };

  const handleApply = () => {
    updateSettings(localSettings);
    onClose();
  };

  const handleReset = () => {
    const defaultSettings: ChartSettings = {
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
    setLocalSettings(defaultSettings);
  };

  return (
    <Modal isOpen={isOpen} title="Chart Settings" onClose={onClose} size="md">
      <div className="space-y-6">
        {/* Appearance Section */}
        <div>
          <h4 className="text-sm font-semibold text-text mb-3">Appearance</h4>
          <div className="space-y-4">
            {/* Chart Type */}
            <div>
              <label className="block text-sm text-text-muted mb-1">Chart Type</label>
              <Select
                options={CHART_TYPES}
                value={localSettings.chartType}
                onChange={(e) => handleChange('chartType', e.target.value as ChartType)}
              />
            </div>

            {/* Color Scheme */}
            <div>
              <label className="block text-sm text-text-muted mb-1">Color Scheme</label>
              <Select
                options={COLOR_SCHEMES}
                value={localSettings.colorScheme}
                onChange={(e) => handleChange('colorScheme', e.target.value as ColorScheme)}
              />
            </div>
          </div>
        </div>

        {/* Grid & Axes Section */}
        <div>
          <h4 className="text-sm font-semibold text-text mb-3">Grid & Axes</h4>
          <div className="space-y-3">
            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showGrid}
                onChange={(e) => handleChange('showGrid', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show grid lines</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showPriceScale}
                onChange={(e) => handleChange('showPriceScale', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show price scale</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showTimeScale}
                onChange={(e) => handleChange('showTimeScale', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show time scale</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.logarithmic}
                onChange={(e) => handleChange('logarithmic', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Logarithmic scale</span>
            </label>
          </div>
        </div>

        {/* Trading Section */}
        <div>
          <h4 className="text-sm font-semibold text-text mb-3">Trading</h4>
          <div className="space-y-3">
            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showOpenOrders}
                onChange={(e) => handleChange('showOpenOrders', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show open orders</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showPositions}
                onChange={(e) => handleChange('showPositions', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show positions</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.showTradeHistory}
                onChange={(e) => handleChange('showTradeHistory', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Show trade history</span>
            </label>

            <label className="flex items-center gap-3 cursor-pointer">
              <input
                type="checkbox"
                checked={localSettings.enableChartTrading}
                onChange={(e) => handleChange('enableChartTrading', e.target.checked)}
                className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
              />
              <span className="text-sm text-text">Enable chart trading</span>
            </label>
          </div>
        </div>

        {/* Actions */}
        <div className="flex gap-3 pt-4 border-t border-border">
          <Button variant="secondary" onClick={handleReset} className="flex-1">
            Reset to Defaults
          </Button>
          <Button variant="secondary" onClick={onClose} className="flex-1">
            Cancel
          </Button>
          <Button variant="primary" onClick={handleApply} className="flex-1">
            Apply
          </Button>
        </div>
      </div>
    </Modal>
  );
}
