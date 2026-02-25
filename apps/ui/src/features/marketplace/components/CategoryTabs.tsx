/**
 * Category Tabs Component
 * Horizontal scrollable category filter tabs
 */

import type { StrategyCategory } from '../types';
import { CATEGORY_INFO } from '../types';
import { useMarketplaceStore } from '../store';

interface CategoryTabsProps {
  className?: string;
}

export function CategoryTabs({ className = '' }: CategoryTabsProps) {
  const { filters, toggleCategory, setFilters } = useMarketplaceStore();

  const categories = Object.keys(CATEGORY_INFO) as StrategyCategory[];
  const hasActiveCategory = filters.categories.length > 0;

  const handleAllClick = () => {
    setFilters({ categories: [] });
  };

  return (
    <div className={`flex items-center gap-2 overflow-x-auto pb-2 ${className}`}>
      {/* All Tab */}
      <button
        onClick={handleAllClick}
        className={`px-4 py-2 rounded-full text-sm font-medium whitespace-nowrap transition-colors ${
          !hasActiveCategory
            ? 'bg-primary text-white'
            : 'bg-surface-hover text-text-muted hover:text-text'
        }`}
      >
        All
      </button>

      {/* Category Tabs */}
      {categories.map((category) => {
        const info = CATEGORY_INFO[category];
        const isActive = filters.categories.includes(category);

        return (
          <button
            key={category}
            onClick={() => toggleCategory(category)}
            className={`px-4 py-2 rounded-full text-sm font-medium whitespace-nowrap transition-colors ${
              isActive
                ? 'text-white'
                : 'bg-surface-hover text-text-muted hover:text-text'
            }`}
            style={{
              backgroundColor: isActive ? info.color : undefined,
            }}
          >
            {info.label}
          </button>
        );
      })}
    </div>
  );
}
