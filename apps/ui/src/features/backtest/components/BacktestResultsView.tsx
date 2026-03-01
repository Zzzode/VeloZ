import { Card, Button } from '@/shared/components';
import type { BacktestResult } from '@/shared/api/types';
import { formatCurrency, formatPercent } from '@/shared/utils';
import { EquityCurveChart } from './EquityCurveChart';
import { DrawdownChart } from './DrawdownChart';
import { TradeListTable } from './TradeListTable';
import {
  exportTradesToCsv,
  exportSummaryToCsv,
  exportFullReportToCsv,
} from '../utils';

interface BacktestResultsViewProps {
  result: BacktestResult;
}

interface MetricItemProps {
  label: string;
  value: string | number;
  className?: string;
}

function MetricItem({ label, value, className = '' }: MetricItemProps) {
  return (
    <div className="flex flex-col">
      <span className="text-xs text-text-muted uppercase tracking-wide">
        {label}
      </span>
      <span className={`text-lg font-semibold font-mono ${className}`}>
        {value}
      </span>
    </div>
  );
}

export function BacktestResultsView({ result }: BacktestResultsViewProps) {
  const isProfitable = result.total_return >= 0;
  const returnClass = isProfitable ? 'text-success' : 'text-danger';

  // Calculate final balance from initial capital and return
  const finalBalance =
    result.config.initial_capital * (1 + result.total_return / 100);

  return (
    <div className="space-y-6">
      {/* Export Actions */}
      <div className="flex justify-end gap-2">
        <Button
          variant="secondary"
          size="sm"
          onClick={() => exportSummaryToCsv(result)}
        >
          Export Summary
        </Button>
        <Button
          variant="secondary"
          size="sm"
          onClick={() => exportTradesToCsv(result)}
        >
          Export Trades
        </Button>
        <Button
          variant="primary"
          size="sm"
          onClick={() => exportFullReportToCsv(result)}
        >
          Export Full Report
        </Button>
      </div>

      {/* Summary Metrics */}
      <Card title="Performance Summary">
        <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-6">
          <MetricItem
            label="Total Return"
            value={`${isProfitable ? '+' : ''}${result.total_return.toFixed(2)}%`}
            className={returnClass}
          />
          <MetricItem
            label="Final Balance"
            value={formatCurrency(finalBalance)}
            className={returnClass}
          />
          <MetricItem
            label="Sharpe Ratio"
            value={result.sharpe_ratio.toFixed(2)}
          />
          <MetricItem
            label="Max Drawdown"
            value={`${result.max_drawdown.toFixed(2)}%`}
            className="text-danger"
          />
          <MetricItem
            label="Win Rate"
            value={formatPercent(result.win_rate / 100)}
          />
          <MetricItem
            label="Profit Factor"
            value={result.profit_factor.toFixed(2)}
          />
        </div>
      </Card>

      {/* Trade Statistics */}
      <Card title="Trade Statistics">
        <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-6">
          <MetricItem label="Total Trades" value={result.total_trades} />
          <MetricItem
            label="Winning Trades"
            value={result.winning_trades}
            className="text-success"
          />
          <MetricItem
            label="Losing Trades"
            value={result.losing_trades}
            className="text-danger"
          />
          <MetricItem
            label="Avg Win"
            value={formatCurrency(result.avg_win)}
            className="text-success"
          />
          <MetricItem
            label="Avg Loss"
            value={formatCurrency(result.avg_loss)}
            className="text-danger"
          />
          <MetricItem
            label="Win/Loss Ratio"
            value={
              result.losing_trades > 0
                ? (result.winning_trades / result.losing_trades).toFixed(2)
                : 'N/A'
            }
          />
        </div>
      </Card>

      {/* Equity Curve */}
      <Card title="Equity Curve">
        <EquityCurveChart
          data={result.equity_curve}
          initialCapital={result.config.initial_capital}
        />
      </Card>

      {/* Drawdown Chart */}
      <Card title="Drawdown">
        <DrawdownChart data={result.drawdown_curve} />
      </Card>

      {/* Trade List */}
      <Card title="Trade History">
        <TradeListTable trades={result.trades} pageSize={10} />
      </Card>

      {/* Configuration Used */}
      <Card title="Backtest Configuration">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
          <div>
            <span className="text-text-muted">Symbol:</span>
            <span className="ml-2 font-medium">{result.config.symbol}</span>
          </div>
          <div>
            <span className="text-text-muted">Strategy:</span>
            <span className="ml-2 font-medium">{result.config.strategy}</span>
          </div>
          <div>
            <span className="text-text-muted">Period:</span>
            <span className="ml-2 font-medium">
              {new Date(result.config.start_date).toLocaleDateString()} -{' '}
              {new Date(result.config.end_date).toLocaleDateString()}
            </span>
          </div>
          <div>
            <span className="text-text-muted">Initial Capital:</span>
            <span className="ml-2 font-medium">
              {formatCurrency(result.config.initial_capital)}
            </span>
          </div>
          <div>
            <span className="text-text-muted">Commission:</span>
            <span className="ml-2 font-medium">
              {(result.config.commission * 100).toFixed(2)}%
            </span>
          </div>
          <div>
            <span className="text-text-muted">Slippage:</span>
            <span className="ml-2 font-medium">
              {(result.config.slippage * 100).toFixed(3)}%
            </span>
          </div>
          <div>
            <span className="text-text-muted">Created:</span>
            <span className="ml-2 font-medium">
              {new Date(result.created_at).toLocaleString()}
            </span>
          </div>
        </div>
      </Card>
    </div>
  );
}
