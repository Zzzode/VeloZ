import type { BacktestResult, TradeRecord } from '@/shared/api/types';

/**
 * Convert backtest trades to CSV format and trigger download
 */
export function exportTradesToCsv(result: BacktestResult): void {
  const headers = [
    'Entry Time',
    'Exit Time',
    'Side',
    'Entry Price',
    'Exit Price',
    'Quantity',
    'PnL',
    'PnL %',
    'Commission',
  ];

  const rows = result.trades.map((trade: TradeRecord) => [
    trade.entry_time,
    trade.exit_time,
    trade.side,
    trade.entry_price.toFixed(2),
    trade.exit_price.toFixed(2),
    trade.quantity.toFixed(6),
    trade.pnl.toFixed(2),
    (trade.pnl_percent * 100).toFixed(2),
    trade.commission.toFixed(4),
  ]);

  const csvContent = [
    headers.join(','),
    ...rows.map((row) => row.join(',')),
  ].join('\n');

  downloadCsv(csvContent, `backtest_trades_${result.id}.csv`);
}

/**
 * Export backtest summary metrics to CSV
 */
export function exportSummaryToCsv(result: BacktestResult): void {
  const metrics = [
    ['Metric', 'Value'],
    ['Backtest ID', result.id],
    ['Strategy', result.config.strategy],
    ['Symbol', result.config.symbol],
    ['Start Date', result.config.start_date],
    ['End Date', result.config.end_date],
    ['Initial Capital', result.config.initial_capital.toFixed(2)],
    ['Commission', (result.config.commission * 100).toFixed(3) + '%'],
    ['Slippage', (result.config.slippage * 100).toFixed(3) + '%'],
    [''],
    ['Performance Metrics', ''],
    ['Total Return', result.total_return.toFixed(2) + '%'],
    ['Sharpe Ratio', result.sharpe_ratio.toFixed(3)],
    ['Max Drawdown', result.max_drawdown.toFixed(2) + '%'],
    ['Profit Factor', result.profit_factor.toFixed(2)],
    ['Win Rate', result.win_rate.toFixed(2) + '%'],
    [''],
    ['Trade Statistics', ''],
    ['Total Trades', result.total_trades.toString()],
    ['Winning Trades', result.winning_trades.toString()],
    ['Losing Trades', result.losing_trades.toString()],
    ['Avg Win', '$' + result.avg_win.toFixed(2)],
    ['Avg Loss', '$' + result.avg_loss.toFixed(2)],
    [''],
    ['Created At', result.created_at],
  ];

  const csvContent = metrics.map((row) => row.join(',')).join('\n');
  downloadCsv(csvContent, `backtest_summary_${result.id}.csv`);
}

/**
 * Export equity curve to CSV
 */
export function exportEquityCurveToCsv(result: BacktestResult): void {
  const headers = ['Timestamp', 'Equity'];
  const rows = result.equity_curve.map((point) => [
    point.timestamp,
    point.equity.toFixed(2),
  ]);

  const csvContent = [
    headers.join(','),
    ...rows.map((row) => row.join(',')),
  ].join('\n');

  downloadCsv(csvContent, `backtest_equity_${result.id}.csv`);
}

/**
 * Export full backtest report (summary + trades + equity curve)
 */
export function exportFullReportToCsv(result: BacktestResult): void {
  const sections: string[] = [];

  // Summary section
  sections.push('=== BACKTEST SUMMARY ===');
  sections.push(`Strategy,${result.config.strategy}`);
  sections.push(`Symbol,${result.config.symbol}`);
  sections.push(`Period,${result.config.start_date} to ${result.config.end_date}`);
  sections.push(`Initial Capital,$${result.config.initial_capital.toFixed(2)}`);
  sections.push('');
  sections.push('=== PERFORMANCE ===');
  sections.push(`Total Return,${result.total_return.toFixed(2)}%`);
  sections.push(`Sharpe Ratio,${result.sharpe_ratio.toFixed(3)}`);
  sections.push(`Max Drawdown,${result.max_drawdown.toFixed(2)}%`);
  sections.push(`Win Rate,${result.win_rate.toFixed(2)}%`);
  sections.push(`Profit Factor,${result.profit_factor.toFixed(2)}`);
  sections.push(`Total Trades,${result.total_trades}`);
  sections.push('');

  // Trades section
  sections.push('=== TRADES ===');
  sections.push('Entry Time,Exit Time,Side,Entry Price,Exit Price,Quantity,PnL,PnL %');
  result.trades.forEach((trade) => {
    sections.push(
      [
        trade.entry_time,
        trade.exit_time,
        trade.side,
        trade.entry_price.toFixed(2),
        trade.exit_price.toFixed(2),
        trade.quantity.toFixed(6),
        trade.pnl.toFixed(2),
        (trade.pnl_percent * 100).toFixed(2),
      ].join(',')
    );
  });

  const csvContent = sections.join('\n');
  downloadCsv(csvContent, `backtest_report_${result.id}.csv`);
}

/**
 * Helper to trigger CSV download
 */
function downloadCsv(content: string, filename: string): void {
  const blob = new Blob([content], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.style.display = 'none';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}
