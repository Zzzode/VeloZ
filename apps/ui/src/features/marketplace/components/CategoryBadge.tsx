/**
 * Category Badge Component
 * Displays strategy category with color-coded badge
 */

import {
  TrendingUp,
  RefreshCw,
  Grid3X3,
  Layers,
  Calendar,
  Shuffle,
} from 'lucide-react';
import type { StrategyCategory } from '../types';
import { CATEGORY_INFO } from '../types';

interface CategoryBadgeProps {
  category: StrategyCategory;
  size?: 'sm' | 'md' | 'lg';
  showIcon?: boolean;
  className?: string;
}

const CATEGORY_ICONS: Record<StrategyCategory, React.ComponentType<{ className?: string }>> = {
  momentum: TrendingUp,
  mean_reversion: RefreshCw,
  grid: Grid3X3,
  market_making: Layers,
  dca: Calendar,
  arbitrage: Shuffle,
};

export function CategoryBadge({
  category,
  size = 'md',
  showIcon = true,
  className = '',
}: CategoryBadgeProps) {
  const info = CATEGORY_INFO[category];
  const Icon = CATEGORY_ICONS[category];

  const sizeClasses = {
    sm: { badge: 'px-1.5 py-0.5 text-xs', icon: 'w-3 h-3' },
    md: { badge: 'px-2 py-1 text-sm', icon: 'w-4 h-4' },
    lg: { badge: 'px-3 py-1.5 text-base', icon: 'w-5 h-5' },
  };

  const { badge, icon } = sizeClasses[size];

  return (
    <span
      className={`inline-flex items-center gap-1 rounded-full font-medium ${badge} ${className}`}
      style={{
        backgroundColor: `${info.color}20`,
        color: info.color,
      }}
    >
      {showIcon && <Icon className={icon} aria-hidden="true" />}
      {info.label}
    </span>
  );
}
