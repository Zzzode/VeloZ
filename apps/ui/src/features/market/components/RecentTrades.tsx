/**
 * Recent trades list component
 */
import { Card } from '@/shared/components';
import { formatPrice, formatQuantity, formatTime } from '@/shared/utils';
import { useMarketStore } from '../store';

export function RecentTrades() {
  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const trades = useMarketStore((state) => state.recentTrades[selectedSymbol] ?? []);

  return (
    <Card title="Recent Trades" subtitle="Last 50 trades">
      <div className="max-h-80 overflow-y-auto">
        {trades.length === 0 ? (
          <p className="text-center text-text-muted py-8">
            No trades yet. Waiting for data...
          </p>
        ) : (
          <table className="w-full text-sm">
            <thead className="sticky top-0 bg-background">
              <tr className="text-text-muted text-xs">
                <th className="text-left py-2">Price</th>
                <th className="text-right py-2">Qty</th>
                <th className="text-right py-2">Time</th>
              </tr>
            </thead>
            <tbody>
              {trades.map((trade, index) => (
                <tr
                  key={`${trade.timestamp}-${index}`}
                  className="border-t border-border/50"
                >
                  <td className={`py-1.5 mono ${trade.isBuyerMaker ? 'text-danger' : 'text-success'}`}>
                    {formatPrice(trade.price, 2)}
                  </td>
                  <td className="text-right py-1.5 mono text-text-muted">
                    {formatQuantity(trade.qty)}
                  </td>
                  <td className="text-right py-1.5 text-text-muted text-xs">
                    {formatTime(trade.timestamp)}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </Card>
  );
}
