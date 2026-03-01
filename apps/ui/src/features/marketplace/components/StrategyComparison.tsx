/**
 * Strategy Comparison Component
 * Side-by-side comparison of up to 4 strategies
 */

import { X, Plus, Play } from 'lucide-react';
import { Button, Card } from '@/shared/components';
import type { StrategyTemplateSummary } from '../types';
import { CategoryBadge } from './CategoryBadge';
import { RiskIndicator } from './RiskIndicator';
import { RatingStars } from './RatingStars';
import { useMarketplaceStore } from '../store';

interface StrategyComparisonProps {
  onAddStrategy?: () => void;
  onUseStrategy?: (strategy: StrategyTemplateSummary) => void;
  className?: string;
}

interface ComparisonMetric {
  label: string;
  key: keyof StrategyTemplateSummary | 'risk_level';
  format: (value: unknown) => string;
  highlight?: 'higher' | 'lower';
}

const COMPARISON_METRICS: ComparisonMetric[] = [
  {
    label: '12M Return',
    key: 'return_12m',
    format: (v) => `${(v as number) >= 0 ? '+' : ''}${(v as number).toFixed(1)}%`,
    highlight: 'higher',
  },
  {
    label: 'Max Drawdown',
    key: 'max_drawdown',
    format: (v) => `${(v as number).toFixed(1)}%`,
    highlight: 'lower',
  },
  {
    label: 'Sharpe Ratio',
    key: 'sharpe_ratio',
    format: (v) => (v as number).toFixed(2),
    highlight: 'higher',
  },
  {
    label: 'Win Rate',
    key: 'win_rate',
    format: (v) => `${(v as number).toFixed(1)}%`,
    highlight: 'higher',
  },
  {
    label: 'Min Capital',
    key: 'min_capital',
    format: (v) => `$${(v as number).toLocaleString()}`,
    highlight: 'lower',
  },
  {
    label: 'Trades/Month',
    key: 'trades_per_month',
    format: (v) => (v as number).toString(),
  },
  {
    label: 'Users',
    key: 'user_count',
    format: (v) => (v as number).toLocaleString(),
    highlight: 'higher',
  },
];

export function StrategyComparison({
  onAddStrategy,
  onUseStrategy,
  className = '',
}: StrategyComparisonProps) {
  const { comparisonItems, removeFromComparison, clearComparison } =
    useMarketplaceStore();

  const strategies = comparisonItems.map((item) => item.strategy);
  const maxStrategies = 4;
  const canAddMore = strategies.length < maxStrategies;

  // Find best values for highlighting
  const getBestValue = (key: keyof StrategyTemplateSummary, highlight?: 'higher' | 'lower') => {
    if (!highlight || strategies.length === 0) return null;

    const values = strategies.map((s) => s[key] as number);
    return highlight === 'higher' ? Math.max(...values) : Math.min(...values);
  };

  if (strategies.length === 0) {
    return (
      <Card className={className}>
        <div className="text-center py-12">
          <p className="text-text-muted mb-4">
            Select strategies to compare (up to {maxStrategies})
          </p>
          <Button variant="primary" onClick={onAddStrategy}>
            <Plus className="w-4 h-4 mr-2" />
            Add Strategy
          </Button>
        </div>
      </Card>
    );
  }

  return (
    <div className={`space-y-4 ${className}`}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <h3 className="text-lg font-semibold text-text">
          Compare Strategies ({strategies.length}/{maxStrategies})
        </h3>
        <div className="flex gap-2">
          {canAddMore && (
            <Button variant="secondary" size="sm" onClick={onAddStrategy}>
              <Plus className="w-4 h-4 mr-1" />
              Add
            </Button>
          )}
          <Button variant="secondary" size="sm" onClick={clearComparison}>
            Clear All
          </Button>
        </div>
      </div>

      {/* Comparison Grid */}
      <div className="overflow-x-auto">
        <div
          className="grid gap-4"
          style={{
            gridTemplateColumns: `repeat(${strategies.length}, minmax(250px, 1fr))`,
          }}
        >
          {/* Strategy Cards */}
          {strategies.map((strategy) => (
            <Card key={strategy.id} className="relative">
              {/* Remove Button */}
              <button
                onClick={() => removeFromComparison(strategy.id)}
                className="absolute top-2 right-2 p-1 rounded-full hover:bg-surface-hover text-text-muted"
                aria-label={`Remove ${strategy.name} from comparison`}
              >
                <X className="w-4 h-4" />
              </button>

              {/* Strategy Header */}
              <div className="space-y-3 pb-4 border-b border-border">
                <CategoryBadge category={strategy.category} size="sm" />
                <h4 className="font-semibold text-text pr-6">{strategy.name}</h4>
                <p className="text-xs text-text-muted">by {strategy.author}</p>
                <RatingStars
                  rating={strategy.rating}
                  reviewCount={strategy.review_count}
                  size="sm"
                />
                <RiskIndicator level={strategy.risk_level} size="sm" />
              </div>

              {/* Metrics */}
              <div className="space-y-3 pt-4">
                {COMPARISON_METRICS.map((metric) => {
                  const value = strategy[metric.key as keyof StrategyTemplateSummary];
                  const bestValue = getBestValue(
                    metric.key as keyof StrategyTemplateSummary,
                    metric.highlight
                  );
                  const isBest = bestValue !== null && value === bestValue;

                  return (
                    <div key={metric.key} className="flex justify-between items-center">
                      <span className="text-sm text-text-muted">{metric.label}</span>
                      <span
                        className={`text-sm font-medium ${
                          isBest ? 'text-success' : 'text-text'
                        }`}
                      >
                        {metric.format(value)}
                        {isBest && strategies.length > 1 && (
                          <span className="ml-1 text-xs">Best</span>
                        )}
                      </span>
                    </div>
                  );
                })}
              </div>

              {/* Action */}
              <div className="pt-4 mt-4 border-t border-border">
                <Button
                  variant="primary"
                  size="sm"
                  className="w-full"
                  onClick={() => onUseStrategy?.(strategy)}
                  leftIcon={<Play className="w-3 h-3" />}
                >
                  Use This
                </Button>
              </div>
            </Card>
          ))}
        </div>
      </div>

      {/* Summary */}
      {strategies.length > 1 && (
        <Card className="bg-surface-hover">
          <h4 className="font-medium text-text mb-3">Quick Summary</h4>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4 text-sm">
            <div>
              <span className="text-text-muted">Best Return: </span>
              <span className="font-medium text-success">
                {strategies.reduce((best, s) =>
                  s.return_12m > best.return_12m ? s : best
                ).name}
              </span>
            </div>
            <div>
              <span className="text-text-muted">Lowest Risk: </span>
              <span className="font-medium text-success">
                {strategies.reduce((best, s) =>
                  s.max_drawdown < best.max_drawdown ? s : best
                ).name}
              </span>
            </div>
            <div>
              <span className="text-text-muted">Highest Rating: </span>
              <span className="font-medium text-success">
                {strategies.reduce((best, s) =>
                  s.rating > best.rating ? s : best
                ).name}
              </span>
            </div>
          </div>
        </Card>
      )}
    </div>
  );
}
