/**
 * Strategy performance metrics display component
 */
import type { StrategyDetail } from '@/shared/api';
import { formatCurrency, formatPercent } from '@/shared/utils';

interface StrategyMetricsProps {
  strategy: StrategyDetail;
  compact?: boolean;
}

interface MetricItemProps {
  label: string;
  value: string;
  valueClassName?: string;
}

function MetricItem({ label, value, valueClassName = '' }: MetricItemProps) {
  return (
    <div>
      <dt className="text-xs text-text-muted">{label}</dt>
      <dd className={`text-sm font-medium ${valueClassName}`}>{value}</dd>
    </div>
  );
}

export function StrategyMetrics({ strategy, compact = false }: StrategyMetricsProps) {
  const pnlClassName = strategy.pnl >= 0 ? 'text-success' : 'text-danger';
  const pnlValue = strategy.pnl >= 0
    ? `+${formatCurrency(strategy.pnl)}`
    : formatCurrency(strategy.pnl);

  if (compact) {
    return (
      <dl className="grid grid-cols-2 gap-3">
        <MetricItem
          label="P&L"
          value={pnlValue}
          valueClassName={pnlClassName}
        />
        <MetricItem
          label="Win Rate"
          value={formatPercent(strategy.win_rate)}
        />
        <MetricItem
          label="Trades"
          value={String(strategy.trade_count)}
        />
        <MetricItem
          label="Max DD"
          value={formatPercent(strategy.max_drawdown)}
          valueClassName="text-danger"
        />
      </dl>
    );
  }

  return (
    <div className="space-y-4">
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <div className="bg-background-secondary rounded-lg p-3">
          <dt className="text-xs text-text-muted">Total P&L</dt>
          <dd className={`text-lg font-semibold ${pnlClassName}`}>
            {pnlValue}
          </dd>
        </div>
        <div className="bg-background-secondary rounded-lg p-3">
          <dt className="text-xs text-text-muted">Win Rate</dt>
          <dd className="text-lg font-semibold">
            {formatPercent(strategy.win_rate)}
          </dd>
        </div>
        <div className="bg-background-secondary rounded-lg p-3">
          <dt className="text-xs text-text-muted">Profit Factor</dt>
          <dd className="text-lg font-semibold">
            {strategy.profit_factor.toFixed(2)}
          </dd>
        </div>
        <div className="bg-background-secondary rounded-lg p-3">
          <dt className="text-xs text-text-muted">Max Drawdown</dt>
          <dd className="text-lg font-semibold text-danger">
            {formatPercent(strategy.max_drawdown)}
          </dd>
        </div>
      </div>

      <div className="grid grid-cols-3 gap-4">
        <div className="text-center">
          <dt className="text-xs text-text-muted">Total Trades</dt>
          <dd className="text-xl font-bold">{strategy.trade_count}</dd>
        </div>
        <div className="text-center">
          <dt className="text-xs text-text-muted">Wins</dt>
          <dd className="text-xl font-bold text-success">{strategy.win_count}</dd>
        </div>
        <div className="text-center">
          <dt className="text-xs text-text-muted">Losses</dt>
          <dd className="text-xl font-bold text-danger">{strategy.lose_count}</dd>
        </div>
      </div>
    </div>
  );
}
