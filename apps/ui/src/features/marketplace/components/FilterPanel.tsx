/**
 * Filter Panel Component
 * Advanced filtering options for strategy marketplace
 */

import { X, Filter, RotateCcw } from 'lucide-react';
import { Button } from '@/shared/components';
import type { StrategyCategory, RiskLevel, StrategyDifficulty, TimeFrame, MarketType, Exchange } from '../types';
import { CATEGORY_INFO, RISK_LEVEL_INFO, DIFFICULTY_INFO } from '../types';
import { useMarketplaceStore } from '../store';

interface FilterPanelProps {
  isOpen: boolean;
  onClose: () => void;
  className?: string;
}

export function FilterPanel({ isOpen, onClose, className = '' }: FilterPanelProps) {
  const { filters, setFilters, resetFilters } = useMarketplaceStore();

  const categories = Object.keys(CATEGORY_INFO) as StrategyCategory[];
  const riskLevels = Object.keys(RISK_LEVEL_INFO) as RiskLevel[];
  const difficulties: StrategyDifficulty[] = ['beginner', 'intermediate', 'advanced'];
  const timeFrames: TimeFrame[] = ['scalping', 'intraday', 'swing', 'position'];
  const marketTypes: MarketType[] = ['spot', 'futures'];
  const exchanges: Exchange[] = ['binance', 'okx', 'bybit'];

  const toggleArrayFilter = <T extends string>(
    key: keyof typeof filters,
    value: T
  ) => {
    const current = filters[key] as T[];
    const updated = current.includes(value)
      ? current.filter((v) => v !== value)
      : [...current, value];
    setFilters({ [key]: updated });
  };

  const activeFilterCount =
    filters.categories.length +
    filters.risk_levels.length +
    filters.difficulties.length +
    filters.time_frames.length +
    filters.market_types.length +
    filters.exchanges.length +
    (filters.min_performance > 0 ? 1 : 0) +
    (filters.max_drawdown < 100 ? 1 : 0) +
    (filters.min_rating > 0 ? 1 : 0);

  if (!isOpen) return null;

  return (
    <div
      className={`bg-surface border border-border rounded-lg shadow-lg p-4 space-y-6 ${className}`}
    >
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Filter className="w-5 h-5 text-text-muted" />
          <h3 className="font-semibold text-text">Filters</h3>
          {activeFilterCount > 0 && (
            <span className="px-2 py-0.5 text-xs font-medium bg-primary text-white rounded-full">
              {activeFilterCount}
            </span>
          )}
        </div>
        <div className="flex items-center gap-2">
          {activeFilterCount > 0 && (
            <Button
              variant="secondary"
              size="sm"
              onClick={resetFilters}
              leftIcon={<RotateCcw className="w-3 h-3" />}
            >
              Clear All
            </Button>
          )}
          <button
            onClick={onClose}
            className="p-1 rounded hover:bg-surface-hover text-text-muted"
            aria-label="Close filters"
          >
            <X className="w-5 h-5" />
          </button>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {/* Strategy Type */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Strategy Type</h4>
          <div className="space-y-1">
            {categories.map((category) => (
              <label
                key={category}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.categories.includes(category)}
                  onChange={() => toggleArrayFilter('categories', category)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span
                  className="text-sm"
                  style={{ color: CATEGORY_INFO[category].color }}
                >
                  {CATEGORY_INFO[category].label}
                </span>
              </label>
            ))}
          </div>
        </div>

        {/* Risk Level */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Risk Level</h4>
          <div className="space-y-1">
            {riskLevels.map((level) => (
              <label
                key={level}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.risk_levels.includes(level)}
                  onChange={() => toggleArrayFilter('risk_levels', level)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span
                  className="text-sm"
                  style={{ color: RISK_LEVEL_INFO[level].color }}
                >
                  {RISK_LEVEL_INFO[level].label}
                </span>
              </label>
            ))}
          </div>
        </div>

        {/* Difficulty */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Difficulty</h4>
          <div className="space-y-1">
            {difficulties.map((difficulty) => (
              <label
                key={difficulty}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.difficulties.includes(difficulty)}
                  onChange={() => toggleArrayFilter('difficulties', difficulty)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span
                  className="text-sm"
                  style={{ color: DIFFICULTY_INFO[difficulty].color }}
                >
                  {DIFFICULTY_INFO[difficulty].label}
                </span>
              </label>
            ))}
          </div>
        </div>

        {/* Time Frame */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Time Frame</h4>
          <div className="space-y-1">
            {timeFrames.map((tf) => (
              <label
                key={tf}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.time_frames.includes(tf)}
                  onChange={() => toggleArrayFilter('time_frames', tf)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span className="text-sm text-text capitalize">
                  {tf === 'scalping' && 'Scalping (< 1 hour)'}
                  {tf === 'intraday' && 'Intraday (1-24 hours)'}
                  {tf === 'swing' && 'Swing (1-7 days)'}
                  {tf === 'position' && 'Position (> 7 days)'}
                </span>
              </label>
            ))}
          </div>
        </div>

        {/* Market Type */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Market Type</h4>
          <div className="space-y-1">
            {marketTypes.map((mt) => (
              <label
                key={mt}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.market_types.includes(mt)}
                  onChange={() => toggleArrayFilter('market_types', mt)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span className="text-sm text-text capitalize">{mt}</span>
              </label>
            ))}
          </div>
        </div>

        {/* Exchange */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">Exchange</h4>
          <div className="space-y-1">
            {exchanges.map((exchange) => (
              <label
                key={exchange}
                className="flex items-center gap-2 cursor-pointer"
              >
                <input
                  type="checkbox"
                  checked={filters.exchanges.includes(exchange)}
                  onChange={() => toggleArrayFilter('exchanges', exchange)}
                  className="w-4 h-4 rounded border-border text-primary focus:ring-primary"
                />
                <span className="text-sm text-text capitalize">{exchange}</span>
              </label>
            ))}
          </div>
        </div>

        {/* Performance Slider */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">
            Minimum Performance: {filters.min_performance}%
          </h4>
          <input
            type="range"
            min={0}
            max={100}
            step={5}
            value={filters.min_performance}
            onChange={(e) =>
              setFilters({ min_performance: Number(e.target.value) })
            }
            className="w-full h-2 bg-border rounded-lg appearance-none cursor-pointer accent-primary"
          />
          <div className="flex justify-between text-xs text-text-muted">
            <span>0%</span>
            <span>100%</span>
          </div>
        </div>

        {/* Max Drawdown Slider */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">
            Maximum Drawdown: {filters.max_drawdown}%
          </h4>
          <input
            type="range"
            min={0}
            max={100}
            step={5}
            value={filters.max_drawdown}
            onChange={(e) =>
              setFilters({ max_drawdown: Number(e.target.value) })
            }
            className="w-full h-2 bg-border rounded-lg appearance-none cursor-pointer accent-primary"
          />
          <div className="flex justify-between text-xs text-text-muted">
            <span>0%</span>
            <span>100%</span>
          </div>
        </div>

        {/* Min Rating Slider */}
        <div className="space-y-2">
          <h4 className="text-sm font-medium text-text">
            Minimum Rating: {filters.min_rating.toFixed(1)}
          </h4>
          <input
            type="range"
            min={0}
            max={5}
            step={0.5}
            value={filters.min_rating}
            onChange={(e) =>
              setFilters({ min_rating: Number(e.target.value) })
            }
            className="w-full h-2 bg-border rounded-lg appearance-none cursor-pointer accent-primary"
          />
          <div className="flex justify-between text-xs text-text-muted">
            <span>0</span>
            <span>5</span>
          </div>
        </div>
      </div>
    </div>
  );
}
