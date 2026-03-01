import { useState, useMemo } from 'react';
import type { TradeRecord } from '@/shared/api/types';
import { formatCurrency, formatQuantity } from '@/shared/utils';
import { Button } from '@/shared/components';

interface TradeListTableProps {
  trades: TradeRecord[];
  pageSize?: number;
}

export function TradeListTable({ trades, pageSize = 10 }: TradeListTableProps) {
  const [currentPage, setCurrentPage] = useState(0);

  // Guard clause must come before accessing trades.length
  if (!trades || trades.length === 0) {
    return (
      <div className="py-8 text-center text-text-muted">
        No trades to display
      </div>
    );
  }

  const totalPages = Math.ceil(trades.length / pageSize);

  const paginatedTrades = useMemo(() => {
    const start = currentPage * pageSize;
    return trades.slice(start, start + pageSize);
  }, [trades, currentPage, pageSize]);

  const formatDateTime = (isoString: string) => {
    return new Date(isoString).toLocaleString('en-US', {
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
    });
  };

  return (
    <div>
      <div className="overflow-x-auto">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-border">
              <th className="text-left py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                #
              </th>
              <th className="text-left py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Entry
              </th>
              <th className="text-left py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Exit
              </th>
              <th className="text-left py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Side
              </th>
              <th className="text-right py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Entry Price
              </th>
              <th className="text-right py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Exit Price
              </th>
              <th className="text-right py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                Qty
              </th>
              <th className="text-right py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                PnL
              </th>
              <th className="text-right py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
                PnL %
              </th>
            </tr>
          </thead>
          <tbody>
            {paginatedTrades.map((trade, index) => {
              const tradeNumber = currentPage * pageSize + index + 1;
              const isProfitable = trade.pnl >= 0;

              return (
                <tr
                  key={`${trade.entry_time}-${index}`}
                  className="border-b border-border-light hover:bg-background-secondary"
                >
                  <td className="py-3 px-2 text-text-muted">{tradeNumber}</td>
                  <td className="py-3 px-2 font-mono text-xs">
                    {formatDateTime(trade.entry_time)}
                  </td>
                  <td className="py-3 px-2 font-mono text-xs">
                    {formatDateTime(trade.exit_time)}
                  </td>
                  <td className="py-3 px-2">
                    <span
                      className={`font-semibold ${
                        trade.side === 'BUY' ? 'text-success' : 'text-danger'
                      }`}
                    >
                      {trade.side}
                    </span>
                  </td>
                  <td className="py-3 px-2 text-right font-mono">
                    {formatCurrency(trade.entry_price)}
                  </td>
                  <td className="py-3 px-2 text-right font-mono">
                    {formatCurrency(trade.exit_price)}
                  </td>
                  <td className="py-3 px-2 text-right font-mono">
                    {formatQuantity(trade.quantity)}
                  </td>
                  <td
                    className={`py-3 px-2 text-right font-mono font-semibold ${
                      isProfitable ? 'text-success' : 'text-danger'
                    }`}
                  >
                    {isProfitable ? '+' : ''}
                    {formatCurrency(trade.pnl)}
                  </td>
                  <td
                    className={`py-3 px-2 text-right font-mono ${
                      isProfitable ? 'text-success' : 'text-danger'
                    }`}
                  >
                    {isProfitable ? '+' : ''}
                    {(trade.pnl_percent * 100).toFixed(2)}%
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {totalPages > 1 && (
        <div className="flex items-center justify-between mt-4 pt-4 border-t border-border">
          <span className="text-sm text-text-muted">
            Showing {currentPage * pageSize + 1} -{' '}
            {Math.min((currentPage + 1) * pageSize, trades.length)} of{' '}
            {trades.length} trades
          </span>
          <div className="flex gap-2">
            <Button
              variant="secondary"
              size="sm"
              onClick={() => setCurrentPage((p) => Math.max(0, p - 1))}
              disabled={currentPage === 0}
            >
              Previous
            </Button>
            <Button
              variant="secondary"
              size="sm"
              onClick={() => setCurrentPage((p) => Math.min(totalPages - 1, p + 1))}
              disabled={currentPage >= totalPages - 1}
            >
              Next
            </Button>
          </div>
        </div>
      )}
    </div>
  );
}
