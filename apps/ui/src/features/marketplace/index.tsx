/**
 * Strategy Marketplace Feature Module
 * Main page for browsing and selecting strategy templates
 */

import { useState, useCallback } from 'react';
import { useNavigate, useParams } from 'react-router-dom';
import { GitCompare, Store } from 'lucide-react';
import { Card, Button, Spinner } from '@/shared/components';
import type { StrategyTemplateSummary, BacktestProgress as BacktestProgressType } from './types';
import {
  SearchBar,
  CategoryTabs,
  FilterPanel,
  MarketplaceCard,
  StrategyComparison,
  StrategyDetailPage,
} from './components';
import { useMarketplaceStore } from './store';
import {
  useMarketplaceTemplates,
  useMarketplaceTemplate,
  useRunBacktest,
  useDeployStrategy,
} from './hooks';
// Keep mock data as fallback during development
import { MOCK_STRATEGY_SUMMARIES, MOCK_STRATEGY_DETAIL } from './mocks';

export default function Marketplace() {
  const navigate = useNavigate();
  const { id } = useParams<{ id?: string }>();
  const {
    filters,
    viewMode,
    isFilterPanelOpen,
    toggleFilterPanel,
    isComparisonMode,
    toggleComparisonMode,
    comparisonItems,
  } = useMarketplaceStore();

  // Backtest progress state
  const [backtestProgress, setBacktestProgress] = useState<BacktestProgressType | null>(null);

  // API hooks
  const {
    data: templatesData,
    isLoading: isLoadingTemplates,
    error: templatesError,
  } = useMarketplaceTemplates(filters);

  const {
    data: templateDetail,
    isLoading: isLoadingDetail,
  } = useMarketplaceTemplate(id);

  const runBacktestMutation = useRunBacktest();
  const deployMutation = useDeployStrategy();

  // Use API data or fallback to mock data
  const strategies = templatesData?.templates ?? MOCK_STRATEGY_SUMMARIES;
  const strategyDetail = templateDetail ?? (id ? MOCK_STRATEGY_DETAIL : null);

  // If we have an ID, show the detail page
  if (id) {
    if (isLoadingDetail) {
      return (
        <div className="flex items-center justify-center h-64">
          <Spinner size="lg" />
        </div>
      );
    }

    if (!strategyDetail) {
      return (
        <Card className="text-center py-12">
          <p className="text-text-muted">Strategy not found.</p>
          <Button variant="secondary" onClick={() => navigate('/marketplace')} className="mt-4">
            Back to Marketplace
          </Button>
        </Card>
      );
    }

    const handleBack = () => {
      setBacktestProgress(null);
      navigate('/marketplace');
    };

    const handleRunBacktest = async (params: Record<string, number | string | boolean>) => {
      try {
        setBacktestProgress({
          backtest_id: '',
          status: 'queued',
          progress_pct: 0,
          message: 'Starting backtest...',
        });

        const result = await runBacktestMutation.mutateAsync({
          strategy_id: id,
          parameters: params,
          symbol: 'BTCUSDT',
          start_date: '2025-01-01',
          end_date: '2026-01-01',
          initial_capital: 10000,
        });

        // Start polling for progress (or connect to SSE)
        setBacktestProgress({
          backtest_id: result.backtest_id,
          status: 'running',
          progress_pct: 0,
          message: 'Backtest started...',
        });

        // Simulate progress polling (replace with real SSE/polling when available)
        let progress = 0;
        const interval = setInterval(() => {
          progress += 10;
          if (progress >= 100) {
            setBacktestProgress({
              backtest_id: result.backtest_id,
              status: 'completed',
              progress_pct: 100,
              message: 'Backtest completed!',
            });
            clearInterval(interval);
          } else {
            setBacktestProgress({
              backtest_id: result.backtest_id,
              status: 'running',
              progress_pct: progress,
              current_date: `2025-${String(Math.floor(progress / 10) + 1).padStart(2, '0')}-15`,
              message: 'Processing historical data...',
            });
          }
        }, 500);
      } catch (error) {
        setBacktestProgress({
          backtest_id: '',
          status: 'failed',
          progress_pct: 0,
          message: error instanceof Error ? error.message : 'Backtest failed',
        });
      }
    };

    const handleDeploy = async (params: Record<string, number | string | boolean>) => {
      try {
        await deployMutation.mutateAsync({
          strategy_id: id,
          parameters: params,
          symbol: 'BTCUSDT',
          capital: 1000,
        });
        navigate('/strategies');
      } catch (error) {
        console.error('Deploy failed:', error);
      }
    };

    return (
      <StrategyDetailPage
        strategy={strategyDetail}
        onBack={handleBack}
        onRunBacktest={handleRunBacktest}
        onDeploy={handleDeploy}
        backtestProgress={backtestProgress}
      />
    );
  }

  const handlePreview = useCallback(
    (strategy: StrategyTemplateSummary) => {
      navigate(`/marketplace/${strategy.id}`);
    },
    [navigate]
  );

  const handleUse = useCallback(
    (strategy: StrategyTemplateSummary) => {
      navigate(`/marketplace/${strategy.id}/configure`);
    },
    [navigate]
  );

  // Loading state
  if (isLoadingTemplates) {
    return (
      <div className="space-y-6">
        {/* Page Header */}
        <div className="flex items-start justify-between">
          <div>
            <div className="flex items-center gap-3">
              <Store className="w-8 h-8 text-primary" />
              <h1 className="text-2xl font-bold text-text">Strategy Marketplace</h1>
            </div>
            <p className="text-text-muted mt-1">
              Browse and deploy pre-built trading strategies
            </p>
          </div>
        </div>
        <div className="flex items-center justify-center h-64">
          <Spinner size="lg" />
        </div>
      </div>
    );
  }

  // Error state
  if (templatesError) {
    return (
      <div className="space-y-6">
        <div className="flex items-start justify-between">
          <div>
            <div className="flex items-center gap-3">
              <Store className="w-8 h-8 text-primary" />
              <h1 className="text-2xl font-bold text-text">Strategy Marketplace</h1>
            </div>
          </div>
        </div>
        <Card className="text-center py-12">
          <p className="text-danger mb-4">Failed to load strategies</p>
          <p className="text-text-muted text-sm">
            {templatesError instanceof Error ? templatesError.message : 'Unknown error'}
          </p>
        </Card>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div className="flex items-start justify-between">
        <div>
          <div className="flex items-center gap-3">
            <Store className="w-8 h-8 text-primary" />
            <h1 className="text-2xl font-bold text-text">Strategy Marketplace</h1>
          </div>
          <p className="text-text-muted mt-1">
            Browse and deploy pre-built trading strategies
          </p>
        </div>
        <Button
          variant={isComparisonMode ? 'primary' : 'secondary'}
          onClick={toggleComparisonMode}
          leftIcon={<GitCompare className="w-4 h-4" />}
        >
          Compare
          {comparisonItems.length > 0 && (
            <span className="ml-2 px-1.5 py-0.5 text-xs bg-white/20 rounded">
              {comparisonItems.length}
            </span>
          )}
        </Button>
      </div>

      {/* Comparison View */}
      {isComparisonMode && (
        <StrategyComparison
          onAddStrategy={() => toggleComparisonMode()}
          onUseStrategy={handleUse}
        />
      )}

      {/* Main Content */}
      {!isComparisonMode && (
        <>
          {/* Search and Filters */}
          <SearchBar />

          {/* Filter Panel */}
          <FilterPanel isOpen={isFilterPanelOpen} onClose={toggleFilterPanel} />

          {/* Category Tabs */}
          <CategoryTabs />

          {/* Results Count */}
          <div className="flex items-center justify-between">
            <p className="text-sm text-text-muted">
              {strategies.length} strategies found
            </p>
          </div>

          {/* Strategy Grid/List */}
          {strategies.length === 0 ? (
            <Card className="text-center py-12">
              <p className="text-text-muted">
                No strategies match your filters. Try adjusting your search criteria.
              </p>
            </Card>
          ) : (
            <div
              className={
                viewMode === 'grid'
                  ? 'grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6'
                  : 'space-y-4'
              }
            >
              {strategies.map((strategy) => (
                <MarketplaceCard
                  key={strategy.id}
                  strategy={strategy}
                  onPreview={handlePreview}
                  onUse={handleUse}
                  className={viewMode === 'list' ? 'max-w-none' : ''}
                />
              ))}
            </div>
          )}

          {/* Load More */}
          {strategies.length > 0 && templatesData && templatesData.total > strategies.length && (
            <div className="text-center pt-4">
              <Button variant="secondary">
                Load More
              </Button>
              <p className="text-xs text-text-muted mt-2">
                Showing {strategies.length} of {templatesData.total} strategies
              </p>
            </div>
          )}
        </>
      )}
    </div>
  );
}
