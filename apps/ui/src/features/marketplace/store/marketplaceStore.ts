/**
 * Strategy Marketplace Store
 * Zustand store for marketplace state management
 */

import { create } from 'zustand';
import { devtools, persist } from 'zustand/middleware';
import type {
  StrategyFilters,
  StrategyTemplateSummary,
  FavoriteStrategy,
  ComparisonItem,
} from '../types';

// =============================================================================
// Store State Interface
// =============================================================================

interface MarketplaceState {
  // Filter state
  filters: StrategyFilters;

  // Selected strategy for detail view
  selectedStrategyId: string | null;

  // Comparison state
  comparisonItems: ComparisonItem[];
  isComparisonMode: boolean;

  // Favorites (persisted)
  favorites: FavoriteStrategy[];

  // View mode
  viewMode: 'grid' | 'list';

  // UI state
  isFilterPanelOpen: boolean;
  isConfigModalOpen: boolean;
  configuringStrategyId: string | null;
}

interface MarketplaceActions {
  // Filter actions
  setFilters: (filters: Partial<StrategyFilters>) => void;
  resetFilters: () => void;
  setSearch: (search: string) => void;
  toggleCategory: (category: StrategyFilters['categories'][number]) => void;
  toggleRiskLevel: (level: StrategyFilters['risk_levels'][number]) => void;
  setSortBy: (sortBy: StrategyFilters['sort_by']) => void;

  // Selection actions
  selectStrategy: (id: string | null) => void;

  // Comparison actions
  addToComparison: (strategy: StrategyTemplateSummary) => void;
  removeFromComparison: (strategyId: string) => void;
  clearComparison: () => void;
  toggleComparisonMode: () => void;

  // Favorites actions
  addFavorite: (strategyId: string) => void;
  removeFavorite: (strategyId: string) => void;
  isFavorite: (strategyId: string) => boolean;

  // View actions
  setViewMode: (mode: 'grid' | 'list') => void;
  toggleFilterPanel: () => void;

  // Config modal actions
  openConfigModal: (strategyId: string) => void;
  closeConfigModal: () => void;
}

type MarketplaceStore = MarketplaceState & MarketplaceActions;

// =============================================================================
// Default State
// =============================================================================

const DEFAULT_STATE: MarketplaceState = {
  filters: {
    search: '',
    categories: [],
    risk_levels: [],
    difficulties: [],
    time_frames: [],
    market_types: [],
    exchanges: [],
    min_performance: 0,
    max_drawdown: 100,
    min_rating: 0,
    sort_by: 'performance',
    sort_order: 'desc',
  },
  selectedStrategyId: null,
  comparisonItems: [],
  isComparisonMode: false,
  favorites: [],
  viewMode: 'grid',
  isFilterPanelOpen: false,
  isConfigModalOpen: false,
  configuringStrategyId: null,
};

// =============================================================================
// Store Implementation
// =============================================================================

export const useMarketplaceStore = create<MarketplaceStore>()(
  devtools(
    persist(
      (set, get) => ({
        ...DEFAULT_STATE,

        // Filter actions
        setFilters: (filters) =>
          set(
            (state) => ({
              filters: { ...state.filters, ...filters },
            }),
            false,
            'setFilters'
          ),

        resetFilters: () =>
          set(
            { filters: DEFAULT_STATE.filters },
            false,
            'resetFilters'
          ),

        setSearch: (search) =>
          set(
            (state) => ({
              filters: { ...state.filters, search },
            }),
            false,
            'setSearch'
          ),

        toggleCategory: (category) =>
          set(
            (state) => {
              const categories = state.filters.categories.includes(category)
                ? state.filters.categories.filter((c) => c !== category)
                : [...state.filters.categories, category];
              return { filters: { ...state.filters, categories } };
            },
            false,
            'toggleCategory'
          ),

        toggleRiskLevel: (level) =>
          set(
            (state) => {
              const risk_levels = state.filters.risk_levels.includes(level)
                ? state.filters.risk_levels.filter((l) => l !== level)
                : [...state.filters.risk_levels, level];
              return { filters: { ...state.filters, risk_levels } };
            },
            false,
            'toggleRiskLevel'
          ),

        setSortBy: (sortBy) =>
          set(
            (state) => ({
              filters: { ...state.filters, sort_by: sortBy },
            }),
            false,
            'setSortBy'
          ),

        // Selection actions
        selectStrategy: (id) =>
          set({ selectedStrategyId: id }, false, 'selectStrategy'),

        // Comparison actions
        addToComparison: (strategy) =>
          set(
            (state) => {
              if (state.comparisonItems.length >= 4) {
                return state;
              }
              if (state.comparisonItems.some((item) => item.strategy.id === strategy.id)) {
                return state;
              }
              return {
                comparisonItems: [
                  ...state.comparisonItems,
                  { strategy, selected: true },
                ],
              };
            },
            false,
            'addToComparison'
          ),

        removeFromComparison: (strategyId) =>
          set(
            (state) => ({
              comparisonItems: state.comparisonItems.filter(
                (item) => item.strategy.id !== strategyId
              ),
            }),
            false,
            'removeFromComparison'
          ),

        clearComparison: () =>
          set(
            { comparisonItems: [], isComparisonMode: false },
            false,
            'clearComparison'
          ),

        toggleComparisonMode: () =>
          set(
            (state) => ({ isComparisonMode: !state.isComparisonMode }),
            false,
            'toggleComparisonMode'
          ),

        // Favorites actions
        addFavorite: (strategyId) =>
          set(
            (state) => {
              if (state.favorites.some((f) => f.strategy_id === strategyId)) {
                return state;
              }
              return {
                favorites: [
                  ...state.favorites,
                  { strategy_id: strategyId, added_at: new Date().toISOString() },
                ],
              };
            },
            false,
            'addFavorite'
          ),

        removeFavorite: (strategyId) =>
          set(
            (state) => ({
              favorites: state.favorites.filter((f) => f.strategy_id !== strategyId),
            }),
            false,
            'removeFavorite'
          ),

        isFavorite: (strategyId) =>
          get().favorites.some((f) => f.strategy_id === strategyId),

        // View actions
        setViewMode: (mode) =>
          set({ viewMode: mode }, false, 'setViewMode'),

        toggleFilterPanel: () =>
          set(
            (state) => ({ isFilterPanelOpen: !state.isFilterPanelOpen }),
            false,
            'toggleFilterPanel'
          ),

        // Config modal actions
        openConfigModal: (strategyId) =>
          set(
            { isConfigModalOpen: true, configuringStrategyId: strategyId },
            false,
            'openConfigModal'
          ),

        closeConfigModal: () =>
          set(
            { isConfigModalOpen: false, configuringStrategyId: null },
            false,
            'closeConfigModal'
          ),
      }),
      {
        name: 'veloz-marketplace',
        partialize: (state) => ({
          favorites: state.favorites,
          viewMode: state.viewMode,
        }),
      }
    ),
    { name: 'MarketplaceStore' }
  )
);

// =============================================================================
// Selector Hooks
// =============================================================================

export const useMarketplaceFilters = () =>
  useMarketplaceStore((state) => state.filters);

export const useMarketplaceComparison = () =>
  useMarketplaceStore((state) => ({
    items: state.comparisonItems,
    isComparisonMode: state.isComparisonMode,
  }));

export const useMarketplaceFavorites = () =>
  useMarketplaceStore((state) => state.favorites);

export const useMarketplaceViewMode = () =>
  useMarketplaceStore((state) => state.viewMode);
