/**
 * Indicator Menu Component
 * Panel for adding and managing technical indicators
 */
import { useState, useMemo } from 'react';
import { Button, Input } from '@/shared/components';
import { useChartStore } from '../store/chartStore';
import {
  INDICATOR_DEFINITIONS,
  INDICATOR_COLORS,
  POPULAR_INDICATORS,
} from '../indicators';
import type { IndicatorType, IndicatorConfig, IndicatorCategory } from '../types';
import { X, Eye, EyeOff, Settings, Search, Plus, Trash2 } from 'lucide-react';

// =============================================================================
// Types
// =============================================================================

interface IndicatorMenuProps {
  onClose: () => void;
}

// =============================================================================
// Constants
// =============================================================================

const CATEGORIES: { value: IndicatorCategory | 'all'; label: string }[] = [
  { value: 'all', label: 'All' },
  { value: 'trend', label: 'Trend' },
  { value: 'momentum', label: 'Momentum' },
  { value: 'volatility', label: 'Volatility' },
  { value: 'volume', label: 'Volume' },
];

// =============================================================================
// Component
// =============================================================================

export function IndicatorMenu({ onClose }: IndicatorMenuProps) {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<IndicatorCategory | 'all'>('all');
  const [configuring, setConfiguring] = useState<string | null>(null);

  const { indicators, addIndicator, removeIndicator, toggleIndicatorVisibility, updateIndicator } =
    useChartStore();

  // Filter indicators based on search and category
  const filteredIndicators = useMemo(() => {
    let result = Object.values(INDICATOR_DEFINITIONS);

    if (selectedCategory !== 'all') {
      result = result.filter((def) => def.category === selectedCategory);
    }

    if (searchQuery) {
      const query = searchQuery.toLowerCase();
      result = result.filter(
        (def) =>
          def.name.toLowerCase().includes(query) || def.type.toLowerCase().includes(query)
      );
    }

    return result;
  }, [selectedCategory, searchQuery]);

  // Add new indicator
  const handleAddIndicator = (type: IndicatorType) => {
    const definition = INDICATOR_DEFINITIONS[type];
    if (!definition) return;

    const newIndicator: IndicatorConfig = {
      id: `${type}-${Date.now()}`,
      type,
      params: { ...definition.defaultParams },
      color: INDICATOR_COLORS[type] || '#2196F3',
      visible: true,
      overlay: definition.overlay,
    };

    addIndicator(newIndicator);
  };

  // Check if indicator type is already added
  const isIndicatorAdded = (type: IndicatorType) => {
    return indicators.some((i) => i.type === type);
  };

  return (
    <div className="bg-bg border border-border rounded-lg shadow-modal w-[400px] max-h-[500px] overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between p-3 border-b border-border">
        <h3 className="font-semibold text-text">Indicators</h3>
        <Button variant="secondary" size="sm" onClick={onClose}>
          <X className="w-4 h-4" />
        </Button>
      </div>

      {/* Search */}
      <div className="p-3 border-b border-border">
        <div className="relative">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-text-muted" />
          <Input
            type="text"
            placeholder="Search indicators..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="pl-10"
          />
        </div>
      </div>

      {/* Active Indicators */}
      {indicators.length > 0 && (
        <div className="p-3 border-b border-border">
          <h4 className="text-sm font-medium text-text-muted mb-2">Active Indicators</h4>
          <div className="space-y-2">
            {indicators.map((indicator) => {
              const definition = INDICATOR_DEFINITIONS[indicator.type];
              return (
                <div
                  key={indicator.id}
                  className="flex items-center justify-between p-2 bg-bg-secondary rounded"
                >
                  <div className="flex items-center gap-2">
                    <div
                      className="w-3 h-3 rounded-full"
                      style={{ backgroundColor: indicator.color }}
                    />
                    <span className="text-sm text-text">{definition?.name || indicator.type}</span>
                    <span className="text-xs text-text-muted">
                      ({Object.values(indicator.params).join(', ')})
                    </span>
                  </div>
                  <div className="flex items-center gap-1">
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={() => toggleIndicatorVisibility(indicator.id)}
                      className="p-1"
                    >
                      {indicator.visible ? (
                        <Eye className="w-4 h-4" />
                      ) : (
                        <EyeOff className="w-4 h-4" />
                      )}
                    </Button>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={() =>
                        setConfiguring(configuring === indicator.id ? null : indicator.id)
                      }
                      className="p-1"
                    >
                      <Settings className="w-4 h-4" />
                    </Button>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={() => removeIndicator(indicator.id)}
                      className="p-1 text-danger"
                    >
                      <Trash2 className="w-4 h-4" />
                    </Button>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}

      {/* Category Tabs */}
      <div className="flex gap-1 p-2 border-b border-border overflow-x-auto">
        {CATEGORIES.map(({ value, label }) => (
          <Button
            key={value}
            variant={selectedCategory === value ? 'primary' : 'secondary'}
            size="sm"
            onClick={() => setSelectedCategory(value)}
            className="whitespace-nowrap"
          >
            {label}
          </Button>
        ))}
      </div>

      {/* Popular Indicators */}
      {selectedCategory === 'all' && !searchQuery && (
        <div className="p-3 border-b border-border">
          <h4 className="text-sm font-medium text-text-muted mb-2">Popular</h4>
          <div className="flex flex-wrap gap-2">
            {POPULAR_INDICATORS.map((type) => {
              const definition = INDICATOR_DEFINITIONS[type];
              const isAdded = isIndicatorAdded(type);
              return (
                <Button
                  key={type}
                  variant="secondary"
                  size="sm"
                  onClick={() => !isAdded && handleAddIndicator(type)}
                  disabled={isAdded}
                  className={isAdded ? 'opacity-50' : ''}
                >
                  {definition?.name || type}
                  {!isAdded && <Plus className="w-3 h-3 ml-1" />}
                </Button>
              );
            })}
          </div>
        </div>
      )}

      {/* Indicator List */}
      <div className="overflow-y-auto max-h-[200px]">
        {filteredIndicators.map((definition) => {
          const isAdded = isIndicatorAdded(definition.type);
          return (
            <div
              key={definition.type}
              className="flex items-center justify-between p-3 hover:bg-bg-secondary border-b border-border-light"
            >
              <div>
                <div className="text-sm font-medium text-text">{definition.name}</div>
                <div className="text-xs text-text-muted capitalize">
                  {definition.category} {definition.overlay ? '(Overlay)' : '(Pane)'}
                </div>
              </div>
              <Button
                variant="secondary"
                size="sm"
                onClick={() => handleAddIndicator(definition.type)}
                disabled={isAdded}
              >
                {isAdded ? 'Added' : 'Add'}
              </Button>
            </div>
          );
        })}
      </div>

      {/* Indicator Configuration Panel */}
      {configuring && (
        <IndicatorConfigPanel
          indicator={indicators.find((i) => i.id === configuring)!}
          onUpdate={(updates) => updateIndicator(configuring, updates)}
          onClose={() => setConfiguring(null)}
        />
      )}
    </div>
  );
}

// =============================================================================
// Indicator Config Panel
// =============================================================================

interface IndicatorConfigPanelProps {
  indicator: IndicatorConfig;
  onUpdate: (updates: Partial<IndicatorConfig>) => void;
  onClose: () => void;
}

function IndicatorConfigPanel({ indicator, onUpdate, onClose }: IndicatorConfigPanelProps) {
  const definition = INDICATOR_DEFINITIONS[indicator.type];
  const [params, setParams] = useState(indicator.params);
  const [color, setColor] = useState(indicator.color || '#2196F3');

  const handleApply = () => {
    onUpdate({ params, color });
    onClose();
  };

  const handleReset = () => {
    setParams({ ...definition.defaultParams });
    setColor(INDICATOR_COLORS[indicator.type] || '#2196F3');
  };

  return (
    <div className="absolute inset-0 bg-bg p-4 z-10">
      <div className="flex items-center justify-between mb-4">
        <h4 className="font-semibold text-text">{definition?.name} Settings</h4>
        <Button variant="secondary" size="sm" onClick={onClose}>
          <X className="w-4 h-4" />
        </Button>
      </div>

      {/* Parameters */}
      <div className="space-y-3 mb-4">
        {Object.entries(definition?.paramLabels || {}).map(([key, label]) => (
          <div key={key}>
            <label className="block text-sm text-text-muted mb-1">{label}</label>
            <Input
              type="number"
              value={params[key] ?? definition?.defaultParams[key] ?? 0}
              onChange={(e) =>
                setParams({ ...params, [key]: parseFloat(e.target.value) || 0 })
              }
            />
          </div>
        ))}
      </div>

      {/* Color */}
      <div className="mb-4">
        <label className="block text-sm text-text-muted mb-1">Color</label>
        <div className="flex items-center gap-2">
          <input
            type="color"
            value={color}
            onChange={(e) => setColor(e.target.value)}
            className="w-10 h-10 rounded border border-border cursor-pointer"
          />
          <Input
            type="text"
            value={color}
            onChange={(e) => setColor(e.target.value)}
            className="flex-1"
          />
        </div>
      </div>

      {/* Actions */}
      <div className="flex gap-2">
        <Button variant="secondary" onClick={handleReset} className="flex-1">
          Reset
        </Button>
        <Button variant="primary" onClick={handleApply} className="flex-1">
          Apply
        </Button>
      </div>
    </div>
  );
}
