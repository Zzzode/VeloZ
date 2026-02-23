import type { BacktestResult } from '@/shared/api/types';
import { formatCurrency } from '@/shared/utils';
import { Button } from '@/shared/components';

interface BacktestListProps {
  backtests: BacktestResult[];
  selectedId: string | null;
  onSelect: (id: string) => void;
  onDelete?: (id: string) => void;
  isDeleting?: boolean;
  comparisonIds?: Set<string>;
  onToggleComparison?: (id: string) => void;
}

export function BacktestList({
  backtests,
  selectedId,
  onSelect,
  onDelete,
  isDeleting = false,
  comparisonIds = new Set(),
  onToggleComparison,
}: BacktestListProps) {
  if (!backtests || backtests.length === 0) {
    return (
      <div className="py-8 text-center text-text-muted">
        <p>No backtests yet</p>
        <p className="text-sm mt-1">Run a backtest to see results here</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {backtests.map((backtest) => {
        const isSelected = backtest.id === selectedId;
        const isInComparison = comparisonIds.has(backtest.id);
        const isProfitable = backtest.total_return >= 0;

        return (
          <div
            key={backtest.id}
            onClick={() => onSelect(backtest.id)}
            className={`p-4 rounded-lg border cursor-pointer transition-colors ${
              isSelected
                ? 'border-primary bg-blue-50'
                : isInComparison
                  ? 'border-warning bg-warning/5'
                  : 'border-border hover:border-primary/50 hover:bg-background-secondary'
            }`}
          >
            <div className="flex items-start justify-between">
              <div className="flex-1">
                <div className="flex items-center gap-2">
                  {onToggleComparison && (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        onToggleComparison(backtest.id);
                      }}
                      className={`w-5 h-5 rounded border flex items-center justify-center text-xs transition-colors ${
                        isInComparison
                          ? 'bg-warning border-warning text-white'
                          : 'border-border hover:border-warning'
                      }`}
                      title={isInComparison ? 'Remove from comparison' : 'Add to comparison'}
                    >
                      {isInComparison ? '✓' : ''}
                    </button>
                  )}
                  <span className="font-semibold text-text">
                    {backtest.config.strategy}
                  </span>
                  <span className="text-xs px-2 py-0.5 rounded bg-background-secondary text-text-muted">
                    {backtest.config.symbol}
                  </span>
                </div>
                <div className="mt-2 grid grid-cols-4 gap-4 text-sm">
                  <div>
                    <span className="text-text-muted">Return:</span>
                    <span
                      className={`ml-1 font-semibold ${
                        isProfitable ? 'text-success' : 'text-danger'
                      }`}
                    >
                      {isProfitable ? '+' : ''}
                      {backtest.total_return.toFixed(2)}%
                    </span>
                  </div>
                  <div>
                    <span className="text-text-muted">Sharpe:</span>
                    <span className="ml-1 font-medium">
                      {backtest.sharpe_ratio.toFixed(2)}
                    </span>
                  </div>
                  <div>
                    <span className="text-text-muted">Max DD:</span>
                    <span className="ml-1 font-medium text-danger">
                      {backtest.max_drawdown.toFixed(2)}%
                    </span>
                  </div>
                  <div>
                    <span className="text-text-muted">Trades:</span>
                    <span className="ml-1 font-medium">
                      {backtest.total_trades}
                    </span>
                  </div>
                </div>
                <div className="mt-2 text-xs text-text-muted">
                  {new Date(backtest.config.start_date).toLocaleDateString()} -{' '}
                  {new Date(backtest.config.end_date).toLocaleDateString()}
                  <span className="mx-2">|</span>
                  Initial: {formatCurrency(backtest.config.initial_capital)}
                </div>
              </div>
              {onDelete && (
                <Button
                  variant="danger"
                  size="sm"
                  onClick={(e) => {
                    e.stopPropagation();
                    onDelete(backtest.id);
                  }}
                  disabled={isDeleting}
                >
                  Delete
                </Button>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
}
