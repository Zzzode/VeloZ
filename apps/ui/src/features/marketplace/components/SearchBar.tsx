/**
 * Search Bar Component
 * Search input with filter toggle and sort options
 */

import { Search, Filter, Grid, List, ArrowUpDown } from 'lucide-react';
import { Button, Select } from '@/shared/components';
import { useDebounce } from '@/shared/hooks';
import type { StrategySortOption } from '../types';
import { useMarketplaceStore } from '../store';
import { useState, useEffect } from 'react';

interface SearchBarProps {
  className?: string;
}

const SORT_OPTIONS: Array<{ value: StrategySortOption; label: string }> = [
  { value: 'performance', label: 'Performance' },
  { value: 'rating', label: 'Rating' },
  { value: 'popularity', label: 'Popularity' },
  { value: 'newest', label: 'Newest' },
  { value: 'name', label: 'Name' },
];

export function SearchBar({ className = '' }: SearchBarProps) {
  const {
    filters,
    setSearch,
    setSortBy,
    viewMode,
    setViewMode,
    isFilterPanelOpen,
    toggleFilterPanel,
  } = useMarketplaceStore();

  const [localSearch, setLocalSearch] = useState(filters.search);
  const debouncedSearch = useDebounce(localSearch, 300);

  useEffect(() => {
    setSearch(debouncedSearch);
  }, [debouncedSearch, setSearch]);

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

  return (
    <div className={`space-y-4 ${className}`}>
      {/* Search and Controls Row */}
      <div className="flex flex-col sm:flex-row gap-4">
        {/* Search Input */}
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-text-muted" />
          <input
            type="text"
            value={localSearch}
            onChange={(e) => setLocalSearch(e.target.value)}
            placeholder="Search strategies..."
            className="w-full pl-10 pr-4 py-2.5 bg-surface border border-border rounded-lg text-text placeholder:text-text-muted focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
          />
        </div>

        {/* Filter Button */}
        <Button
          variant={isFilterPanelOpen ? 'primary' : 'secondary'}
          onClick={toggleFilterPanel}
          leftIcon={<Filter className="w-4 h-4" />}
          className="relative"
        >
          Filters
          {activeFilterCount > 0 && (
            <span className="absolute -top-1 -right-1 w-5 h-5 flex items-center justify-center text-xs font-bold bg-danger text-white rounded-full">
              {activeFilterCount}
            </span>
          )}
        </Button>
      </div>

      {/* Sort and View Controls */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
        {/* Sort */}
        <div className="flex items-center gap-2">
          <ArrowUpDown className="w-4 h-4 text-text-muted" />
          <span className="text-sm text-text-muted">Sort by:</span>
          <Select
            value={filters.sort_by}
            onChange={(e) => setSortBy(e.target.value as StrategySortOption)}
            options={SORT_OPTIONS}
            className="w-40"
          />
        </div>

        {/* View Mode Toggle */}
        <div className="flex items-center gap-1 p-1 bg-surface-hover rounded-lg">
          <button
            onClick={() => setViewMode('grid')}
            className={`p-2 rounded ${
              viewMode === 'grid'
                ? 'bg-primary text-white'
                : 'text-text-muted hover:text-text'
            }`}
            aria-label="Grid view"
            aria-pressed={viewMode === 'grid'}
          >
            <Grid className="w-4 h-4" />
          </button>
          <button
            onClick={() => setViewMode('list')}
            className={`p-2 rounded ${
              viewMode === 'list'
                ? 'bg-primary text-white'
                : 'text-text-muted hover:text-text'
            }`}
            aria-label="List view"
            aria-pressed={viewMode === 'list'}
          >
            <List className="w-4 h-4" />
          </button>
        </div>
      </div>
    </div>
  );
}
