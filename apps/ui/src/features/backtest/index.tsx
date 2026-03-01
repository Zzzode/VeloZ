import { useState, useMemo } from 'react';
import { Card, Spinner, Button } from '@/shared/components';
import {
  useBacktests,
  useBacktest,
  useRunBacktest,
  useDeleteBacktest,
} from './hooks';
import {
  BacktestConfigForm,
  BacktestResultsView,
  BacktestList,
  BacktestComparison,
} from './components';
import type { BacktestConfigFormData } from './schemas';

type ViewMode = 'results' | 'compare';

export default function Backtest() {
  const [selectedBacktestId, setSelectedBacktestId] = useState<string | null>(null);
  const [comparisonIds, setComparisonIds] = useState<Set<string>>(new Set());
  const [viewMode, setViewMode] = useState<ViewMode>('results');

  // Queries
  const { data: backtestsData, isLoading: isLoadingList } = useBacktests();
  const { data: selectedResult, isLoading: isLoadingResult } = useBacktest(selectedBacktestId);

  // Mutations
  const runBacktest = useRunBacktest();
  const deleteBacktest = useDeleteBacktest();

  const backtests = backtestsData?.backtests ?? [];

  // Get backtests selected for comparison
  const comparisonBacktests = useMemo(() => {
    return backtests.filter((bt) => comparisonIds.has(bt.id));
  }, [backtests, comparisonIds]);

  const handleRunBacktest = (formData: BacktestConfigFormData) => {
    // Convert form data to API request format with ISO 8601 dates
    const config = {
      ...formData,
      start_date: new Date(formData.start_date).toISOString(),
      end_date: new Date(formData.end_date).toISOString(),
    };

    runBacktest.mutate(
      { config },
      {
        onSuccess: (response) => {
          // Select the newly created backtest
          setSelectedBacktestId(response.backtest_id);
          setViewMode('results');
        },
      }
    );
  };

  const handleDeleteBacktest = (id: string) => {
    deleteBacktest.mutate(id, {
      onSuccess: () => {
        if (selectedBacktestId === id) {
          setSelectedBacktestId(null);
        }
        // Also remove from comparison if present
        setComparisonIds((prev) => {
          const next = new Set(prev);
          next.delete(id);
          return next;
        });
      },
    });
  };

  const handleToggleComparison = (id: string) => {
    setComparisonIds((prev) => {
      const next = new Set(prev);
      if (next.has(id)) {
        next.delete(id);
      } else {
        next.add(id);
      }
      return next;
    });
  };

  const handleRemoveFromComparison = (id: string) => {
    setComparisonIds((prev) => {
      const next = new Set(prev);
      next.delete(id);
      return next;
    });
  };

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-text">Backtest</h1>
          <p className="text-text-muted mt-1">
            Test strategies against historical data
          </p>
        </div>
        <div className="flex gap-2">
          <Button
            variant={viewMode === 'results' ? 'primary' : 'secondary'}
            size="sm"
            onClick={() => setViewMode('results')}
          >
            Results View
          </Button>
          <Button
            variant={viewMode === 'compare' ? 'primary' : 'secondary'}
            size="sm"
            onClick={() => setViewMode('compare')}
          >
            Compare ({comparisonIds.size})
          </Button>
        </div>
      </div>

      {/* Configuration Form */}
      <Card title="Run New Backtest">
        <BacktestConfigForm
          onSubmit={handleRunBacktest}
          isLoading={runBacktest.isPending}
        />
        {runBacktest.isError && (
          <div className="mt-4 p-3 bg-danger-50 border border-danger rounded-md text-danger text-sm">
            Error running backtest: {runBacktest.error?.message}
          </div>
        )}
      </Card>

      {/* Past Backtests List */}
      <Card
        title="Past Backtests"
        subtitle={`${backtests.length} backtest${backtests.length !== 1 ? 's' : ''}`}
        headerAction={
          comparisonIds.size > 0 && (
            <Button
              variant="secondary"
              size="sm"
              onClick={() => setComparisonIds(new Set())}
            >
              Clear Selection
            </Button>
          )
        }
      >
        {isLoadingList ? (
          <div className="py-8 flex justify-center">
            <Spinner size="lg" />
          </div>
        ) : (
          <BacktestList
            backtests={backtests}
            selectedId={selectedBacktestId}
            onSelect={(id) => {
              setSelectedBacktestId(id);
              setViewMode('results');
            }}
            onDelete={handleDeleteBacktest}
            isDeleting={deleteBacktest.isPending}
            comparisonIds={comparisonIds}
            onToggleComparison={handleToggleComparison}
          />
        )}
      </Card>

      {/* View Mode: Results or Compare */}
      {viewMode === 'results' && selectedBacktestId && (
        <>
          {isLoadingResult ? (
            <Card title="Loading Results...">
              <div className="py-8 flex justify-center">
                <Spinner size="lg" />
              </div>
            </Card>
          ) : selectedResult ? (
            <BacktestResultsView result={selectedResult} />
          ) : null}
        </>
      )}

      {viewMode === 'compare' && (
        <BacktestComparison
          backtests={comparisonBacktests}
          onRemove={handleRemoveFromComparison}
        />
      )}
    </div>
  );
}
