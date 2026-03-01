/**
 * Marketplace Card Component
 * Strategy card for marketplace browsing view
 */

import { Heart, Eye, Play } from 'lucide-react';
import { Card, Button } from '@/shared/components';
import type { StrategyTemplateSummary } from '../types';
import { CategoryBadge } from './CategoryBadge';
import { RatingStars } from './RatingStars';
import { RiskIndicator } from './RiskIndicator';
import { useMarketplaceStore } from '../store';

interface MarketplaceCardProps {
  strategy: StrategyTemplateSummary;
  onPreview?: (strategy: StrategyTemplateSummary) => void;
  onUse?: (strategy: StrategyTemplateSummary) => void;
  className?: string;
}

export function MarketplaceCard({
  strategy,
  onPreview,
  onUse,
  className = '',
}: MarketplaceCardProps) {
  const { addFavorite, removeFavorite, isFavorite, addToComparison, isComparisonMode } =
    useMarketplaceStore();

  const favorite = isFavorite(strategy.id);

  const handleFavoriteClick = (e: React.MouseEvent) => {
    e.stopPropagation();
    if (favorite) {
      removeFavorite(strategy.id);
    } else {
      addFavorite(strategy.id);
    }
  };

  const handlePreview = (e: React.MouseEvent) => {
    e.stopPropagation();
    onPreview?.(strategy);
  };

  const handleUse = (e: React.MouseEvent) => {
    e.stopPropagation();
    onUse?.(strategy);
  };

  const handleCompare = (e: React.MouseEvent) => {
    e.stopPropagation();
    addToComparison(strategy);
  };

  const formatReturn = (value: number) => {
    const sign = value >= 0 ? '+' : '';
    return `${sign}${value.toFixed(1)}%`;
  };

  const formatCurrency = (value: number) => {
    return `$${value.toLocaleString()}`;
  };

  return (
    <Card
      className={`w-full max-w-[320px] cursor-pointer hover:shadow-lg transition-all duration-200 ${className}`}
      onClick={() => onPreview?.(strategy)}
    >
      <div className="space-y-4">
        {/* Header */}
        <div className="flex items-start justify-between">
          <CategoryBadge category={strategy.category} size="sm" />
          <div className="flex items-center gap-2">
            <button
              onClick={handleFavoriteClick}
              className={`p-1 rounded-full transition-colors ${
                favorite
                  ? 'text-danger hover:bg-danger/10'
                  : 'text-text-muted hover:bg-surface-hover'
              }`}
              aria-label={favorite ? 'Remove from favorites' : 'Add to favorites'}
            >
              <Heart
                className={`w-4 h-4 ${favorite ? 'fill-current' : ''}`}
              />
            </button>
            <RatingStars rating={strategy.rating} showCount={false} size="sm" />
          </div>
        </div>

        {/* Title and Author */}
        <div>
          <h3 className="font-semibold text-text text-lg leading-tight">
            {strategy.name}
          </h3>
          <p className="text-xs text-text-muted mt-0.5">by {strategy.author}</p>
        </div>

        {/* Description */}
        <p className="text-sm text-text-muted line-clamp-2">
          {strategy.description}
        </p>

        {/* Metrics Grid */}
        <div className="grid grid-cols-3 gap-2 py-3 border-y border-border">
          <div className="text-center">
            <p
              className={`text-lg font-bold font-mono ${
                strategy.return_12m >= 0 ? 'text-success' : 'text-danger'
              }`}
            >
              {formatReturn(strategy.return_12m)}
            </p>
            <p className="text-xs text-text-muted">12M Return</p>
          </div>
          <div className="text-center">
            <p className="text-lg font-bold font-mono text-danger">
              {strategy.max_drawdown.toFixed(1)}%
            </p>
            <p className="text-xs text-text-muted">Max DD</p>
          </div>
          <div className="text-center">
            <p className="text-lg font-bold font-mono text-text">
              {strategy.sharpe_ratio.toFixed(1)}
            </p>
            <p className="text-xs text-text-muted">Sharpe</p>
          </div>
        </div>

        {/* Risk Level */}
        <div className="flex items-center justify-between">
          <span className="text-sm text-text-muted">Risk:</span>
          <RiskIndicator level={strategy.risk_level} size="sm" />
        </div>

        {/* Quick Stats on Hover */}
        <div className="grid grid-cols-2 gap-2 text-sm">
          <div className="flex justify-between">
            <span className="text-text-muted">Win Rate:</span>
            <span className="font-medium text-text">{strategy.win_rate.toFixed(1)}%</span>
          </div>
          <div className="flex justify-between">
            <span className="text-text-muted">Trades/Mo:</span>
            <span className="font-medium text-text">{strategy.trades_per_month}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-text-muted">Min Capital:</span>
            <span className="font-medium text-text">{formatCurrency(strategy.min_capital)}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-text-muted">Users:</span>
            <span className="font-medium text-text">{strategy.user_count.toLocaleString()}</span>
          </div>
        </div>

        {/* Actions */}
        <div className="flex gap-2 pt-2">
          {isComparisonMode ? (
            <Button
              variant="secondary"
              size="sm"
              onClick={handleCompare}
              className="flex-1"
            >
              Add to Compare
            </Button>
          ) : (
            <>
              <Button
                variant="secondary"
                size="sm"
                onClick={handlePreview}
                leftIcon={<Eye className="w-3 h-3" />}
                className="flex-1"
              >
                Preview
              </Button>
              <Button
                variant="primary"
                size="sm"
                onClick={handleUse}
                leftIcon={<Play className="w-3 h-3" />}
                className="flex-1"
              >
                Use Strategy
              </Button>
            </>
          )}
        </div>
      </div>
    </Card>
  );
}
