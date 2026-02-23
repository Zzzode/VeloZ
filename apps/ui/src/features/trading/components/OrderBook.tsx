/**
 * Order Book Component
 * Real-time order book display with bid/ask levels
 */

import { Card } from '@/shared/components';
import { useTradingStore } from '../store';
import { useOrderBookStream } from '../hooks';
import { formatPrice, formatQuantity } from '@/shared/utils/format';

interface OrderBookRowProps {
  price: number;
  qty: number;
  total: number;
  side: 'bid' | 'ask';
  maxTotal: number;
}

function OrderBookRow({ price, qty, total, side, maxTotal }: OrderBookRowProps) {
  const depthPercent = maxTotal > 0 ? (total / maxTotal) * 100 : 0;
  const bgColor = side === 'bid' ? 'bg-success/10' : 'bg-danger/10';
  const textColor = side === 'bid' ? 'text-success' : 'text-danger';

  return (
    <div className="relative flex items-center py-1 px-2 text-sm font-mono">
      {/* Depth bar */}
      <div
        className={`absolute inset-y-0 ${side === 'bid' ? 'right-0' : 'left-0'} ${bgColor}`}
        style={{ width: `${depthPercent}%` }}
      />

      {/* Content */}
      <div className="relative flex w-full items-center justify-between">
        <span className={textColor}>{formatPrice(price, 2)}</span>
        <span className="text-text">{formatQuantity(qty, 4)}</span>
        <span className="text-text-muted">{formatQuantity(total, 4)}</span>
      </div>
    </div>
  );
}

export function OrderBook() {
  const { selectedSymbol, orderBook, wsConnectionState } = useTradingStore();

  // Start WebSocket stream
  useOrderBookStream();

  const maxBidTotal = orderBook.bids.reduce((max, b) => Math.max(max, b.total), 0);
  const maxAskTotal = orderBook.asks.reduce((max, a) => Math.max(max, a.total), 0);
  const maxTotal = Math.max(maxBidTotal, maxAskTotal);

  const connectionIndicator = (
    <div className="flex items-center gap-2">
      <div
        className={`w-2 h-2 rounded-full ${
          wsConnectionState === 'connected'
            ? 'bg-success'
            : wsConnectionState === 'connecting' || wsConnectionState === 'reconnecting'
              ? 'bg-warning animate-pulse'
              : 'bg-danger'
        }`}
      />
      <span className="text-xs text-text-muted capitalize">{wsConnectionState}</span>
    </div>
  );

  return (
    <Card title="Order Book" subtitle={selectedSymbol} headerAction={connectionIndicator}>
      <div className="space-y-2">
        {/* Header */}
        <div className="flex items-center justify-between px-2 text-xs font-medium text-text-muted">
          <span>Price (USDT)</span>
          <span>Amount</span>
          <span>Total</span>
        </div>

        {/* Asks (reversed to show lowest ask at bottom) */}
        <div className="border-b border-border">
          {orderBook.asks.length > 0 ? (
            [...orderBook.asks].reverse().map((level, i) => (
              <OrderBookRow
                key={`ask-${i}`}
                price={level.price}
                qty={level.qty}
                total={level.total}
                side="ask"
                maxTotal={maxTotal}
              />
            ))
          ) : (
            <div className="py-4 text-center text-sm text-text-muted">No asks</div>
          )}
        </div>

        {/* Spread */}
        <div className="flex items-center justify-center py-2 bg-background-secondary rounded">
          <span className="text-sm font-medium">
            Spread: {formatPrice(orderBook.spread, 2)} ({orderBook.spreadPercent.toFixed(3)}%)
          </span>
        </div>

        {/* Bids */}
        <div className="border-t border-border">
          {orderBook.bids.length > 0 ? (
            orderBook.bids.map((level, i) => (
              <OrderBookRow
                key={`bid-${i}`}
                price={level.price}
                qty={level.qty}
                total={level.total}
                side="bid"
                maxTotal={maxTotal}
              />
            ))
          ) : (
            <div className="py-4 text-center text-sm text-text-muted">No bids</div>
          )}
        </div>
      </div>
    </Card>
  );
}
