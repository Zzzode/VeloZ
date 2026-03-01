import type { BacktestResult } from '@/shared/api/types';
import { Card } from '@/shared/components';
import { formatCurrency } from '@/shared/utils';

interface BacktestComparisonProps {
  backtests: BacktestResult[];
  onRemove?: (id: string) => void;
}

interface MetricRowProps {
  label: string;
  values: (string | number)[];
  highlight?: 'highest' | 'lowest';
  format?: 'currency' | 'percent' | 'number';
}

function MetricRow({ label, values, highlight, format }: MetricRowProps) {
  const numericValues = values.map((v) =>
    typeof v === 'string' ? parseFloat(v) || 0 : v
  );

  const bestIndex =
    highlight === 'highest'
      ? numericValues.indexOf(Math.max(...numericValues))
      : highlight === 'lowest'
        ? numericValues.indexOf(Math.min(...numericValues))
        : -1;

  const formatValue = (value: string | number): string => {
    const num = typeof value === 'string' ? parseFloat(value) || 0 : value;
    switch (format) {
      case 'currency':
        return formatCurrency(num);
      case 'percent':
        return `${num >= 0 ? '+' : ''}${num.toFixed(2)}%`;
      case 'number':
      default:
        return typeof num === 'number' ? num.toFixed(2) : String(value);
    }
  };

  return (
    <tr className="border-b border-border-light">
      <td className="py-3 px-4 font-medium text-text-muted">{label}</td>
      {values.map((value, index) => (
        <td
          key={index}
          className={`py-3 px-4 text-center font-mono ${
            index === bestIndex ? 'bg-success/10 text-success font-semibold' : ''
          }`}
        >
          {formatValue(value)}
        </td>
      ))}
    </tr>
  );
}

export function BacktestComparison({ backtests, onRemove }: BacktestComparisonProps) {
  if (backtests.length === 0) {
    return (
      <Card title="Backtest Comparison">
        <div className="py-8 text-center text-text-muted">
          <p>No backtests selected for comparison</p>
          <p className="text-sm mt-1">
            Select backtests from the list to compare them side by side
          </p>
        </div>
      </Card>
    );
  }

  if (backtests.length === 1) {
    return (
      <Card title="Backtest Comparison">
        <div className="py-8 text-center text-text-muted">
          <p>Select at least 2 backtests to compare</p>
        </div>
      </Card>
    );
  }

  return (
    <Card title="Backtest Comparison" subtitle={`Comparing ${backtests.length} backtests`}>
      <div className="overflow-x-auto">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-border bg-background-secondary">
              <th className="py-3 px-4 text-left font-semibold text-text-muted">
                Metric
              </th>
              {backtests.map((bt) => (
                <th key={bt.id} className="py-3 px-4 text-center">
                  <div className="flex flex-col items-center gap-1">
                    <span className="font-semibold text-text">
                      {bt.config.strategy}
                    </span>
                    <span className="text-xs text-text-muted">
                      {bt.config.symbol}
                    </span>
                    {onRemove && (
                      <button
                        onClick={() => onRemove(bt.id)}
                        className="text-xs text-danger hover:underline"
                      >
                        Remove
                      </button>
                    )}
                  </div>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {/* Performance Metrics */}
            <tr className="bg-background-secondary">
              <td
                colSpan={backtests.length + 1}
                className="py-2 px-4 text-xs font-semibold text-text-muted uppercase tracking-wide"
              >
                Performance
              </td>
            </tr>
            <MetricRow
              label="Total Return"
              values={backtests.map((bt) => bt.total_return)}
              highlight="highest"
              format="percent"
            />
            <MetricRow
              label="Sharpe Ratio"
              values={backtests.map((bt) => bt.sharpe_ratio)}
              highlight="highest"
              format="number"
            />
            <MetricRow
              label="Max Drawdown"
              values={backtests.map((bt) => bt.max_drawdown)}
              highlight="lowest"
              format="percent"
            />
            <MetricRow
              label="Profit Factor"
              values={backtests.map((bt) => bt.profit_factor)}
              highlight="highest"
              format="number"
            />

            {/* Trade Statistics */}
            <tr className="bg-background-secondary">
              <td
                colSpan={backtests.length + 1}
                className="py-2 px-4 text-xs font-semibold text-text-muted uppercase tracking-wide"
              >
                Trade Statistics
              </td>
            </tr>
            <MetricRow
              label="Total Trades"
              values={backtests.map((bt) => bt.total_trades)}
              format="number"
            />
            <MetricRow
              label="Win Rate"
              values={backtests.map((bt) => bt.win_rate)}
              highlight="highest"
              format="percent"
            />
            <MetricRow
              label="Winning Trades"
              values={backtests.map((bt) => bt.winning_trades)}
              format="number"
            />
            <MetricRow
              label="Losing Trades"
              values={backtests.map((bt) => bt.losing_trades)}
              format="number"
            />
            <MetricRow
              label="Avg Win"
              values={backtests.map((bt) => bt.avg_win)}
              highlight="highest"
              format="currency"
            />
            <MetricRow
              label="Avg Loss"
              values={backtests.map((bt) => bt.avg_loss)}
              highlight="lowest"
              format="currency"
            />

            {/* Configuration */}
            <tr className="bg-background-secondary">
              <td
                colSpan={backtests.length + 1}
                className="py-2 px-4 text-xs font-semibold text-text-muted uppercase tracking-wide"
              >
                Configuration
              </td>
            </tr>
            <MetricRow
              label="Initial Capital"
              values={backtests.map((bt) => bt.config.initial_capital)}
              format="currency"
            />
            <MetricRow
              label="Commission"
              values={backtests.map((bt) => bt.config.commission * 100)}
              format="percent"
            />
            <MetricRow
              label="Slippage"
              values={backtests.map((bt) => bt.config.slippage * 100)}
              format="percent"
            />
          </tbody>
        </table>
      </div>
    </Card>
  );
}
