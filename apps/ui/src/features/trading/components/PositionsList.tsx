/**
 * Positions List Component
 * Display current positions with real-time P&L
 */

import { useEffect } from 'react';
import { Card, Button, Spinner } from '@/shared/components';
import { useTradingStore, selectTotalUnrealizedPnL, selectTotalRealizedPnL } from '../store';
import { usePositions } from '../hooks';
import { formatPrice, formatQuantity, formatPnL } from '@/shared/utils/format';
import { SIDE_COLORS } from '@/shared/utils/constants';
import type { Position } from '@/shared/api/types';

interface PositionRowProps {
  position: Position;
}

function PositionRow({ position }: PositionRowProps) {
  const unrealizedPnL = formatPnL(position.unrealized_pnl);
  const realizedPnL = formatPnL(position.realized_pnl);
  const pnlPercent =
    position.avg_entry_price > 0
      ? ((position.current_price - position.avg_entry_price) / position.avg_entry_price) * 100
      : 0;
  const adjustedPnlPercent = position.side === 'SHORT' ? -pnlPercent : pnlPercent;

  return (
    <tr className="border-b border-border hover:bg-background-secondary/50">
      <td className="px-3 py-3 text-sm font-medium">{position.symbol}</td>
      <td className="px-3 py-3 text-sm">
        <span className={SIDE_COLORS[position.side]}>{position.side}</span>
      </td>
      <td className="px-3 py-3 text-sm font-mono">{formatQuantity(position.qty, 4)}</td>
      <td className="px-3 py-3 text-sm font-mono">
        {formatPrice(position.avg_entry_price, 2)}
      </td>
      <td className="px-3 py-3 text-sm font-mono">{formatPrice(position.current_price, 2)}</td>
      <td className="px-3 py-3 text-sm">
        <div className="flex flex-col">
          <span className={`font-mono ${unrealizedPnL.className}`}>
            {unrealizedPnL.value}
          </span>
          <span
            className={`text-xs ${adjustedPnlPercent >= 0 ? 'text-success' : 'text-danger'}`}
          >
            {adjustedPnlPercent >= 0 ? '+' : ''}
            {adjustedPnlPercent.toFixed(2)}%
          </span>
        </div>
      </td>
      <td className="px-3 py-3 text-sm">
        <span className={`font-mono ${realizedPnL.className}`}>{realizedPnL.value}</span>
      </td>
      <td className="px-3 py-3 text-sm text-text-muted">{position.venue}</td>
    </tr>
  );
}

export function PositionsList() {
  const { positions, isLoading, refetch } = usePositions();
  const totalUnrealizedPnL = useTradingStore(selectTotalUnrealizedPnL);
  const totalRealizedPnL = useTradingStore(selectTotalRealizedPnL);

  // Fetch positions on mount
  useEffect(() => {
    refetch();
  }, [refetch]);

  const totalUnrealized = formatPnL(totalUnrealizedPnL);
  const totalRealized = formatPnL(totalRealizedPnL);

  return (
    <Card
      title="Positions"
      subtitle={`${positions.length} open position${positions.length !== 1 ? 's' : ''}`}
      headerAction={
        <Button variant="secondary" size="sm" onClick={refetch} disabled={isLoading}>
          Refresh
        </Button>
      }
    >
      {/* Summary */}
      {positions.length > 0 && (
        <div className="grid grid-cols-2 gap-4 mb-4 p-3 bg-background-secondary rounded-lg">
          <div>
            <div className="text-xs text-text-muted mb-1">Unrealized P&L</div>
            <div className={`text-lg font-semibold font-mono ${totalUnrealized.className}`}>
              {totalUnrealized.value}
            </div>
          </div>
          <div>
            <div className="text-xs text-text-muted mb-1">Realized P&L</div>
            <div className={`text-lg font-semibold font-mono ${totalRealized.className}`}>
              {totalRealized.value}
            </div>
          </div>
        </div>
      )}

      {/* Table */}
      {isLoading && positions.length === 0 ? (
        <div className="flex items-center justify-center py-8">
          <Spinner size="lg" />
        </div>
      ) : positions.length === 0 ? (
        <div className="text-center py-8 text-text-muted">No open positions.</div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="border-b border-border">
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Symbol
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Side
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Qty
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Entry Price
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Mark Price
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Unrealized P&L
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Realized P&L
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Venue
                </th>
              </tr>
            </thead>
            <tbody>
              {positions.map((position) => (
                <PositionRow key={`${position.symbol}-${position.venue}`} position={position} />
              ))}
            </tbody>
          </table>
        </div>
      )}
    </Card>
  );
}
