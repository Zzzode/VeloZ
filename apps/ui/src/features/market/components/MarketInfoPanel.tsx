/**
 * Market info panel showing symbol stats
 */
import { TrendingUp, TrendingDown, Wifi, WifiOff } from 'lucide-react';
import { Card } from '@/shared/components';
import { formatPrice, formatQuantity } from '@/shared/utils';
import { useMarketStore, selectCurrentPrice, selectCurrentBookTop } from '../store';

export function MarketInfoPanel() {
  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const currentPrice = useMarketStore(selectCurrentPrice);
  const bookTop = useMarketStore(selectCurrentBookTop);
  const connectionState = useMarketStore((state) => state.wsConnectionState);

  const isConnected = connectionState === 'connected';

  return (
    <Card>
      <div className="space-y-4">
        {/* Symbol and Connection Status */}
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-2xl font-bold text-text">{selectedSymbol}</h2>
            <p className="text-sm text-text-muted">Spot Trading</p>
          </div>
          <div className="flex items-center gap-2">
            {isConnected ? (
              <Wifi className="w-4 h-4 text-success" />
            ) : (
              <WifiOff className="w-4 h-4 text-danger" />
            )}
            <span className={`text-xs ${isConnected ? 'text-success' : 'text-danger'}`}>
              {connectionState}
            </span>
          </div>
        </div>

        {/* Current Price */}
        <div>
          <p className="text-sm text-text-muted">Last Price</p>
          <p className="text-3xl font-bold text-text mono">
            {currentPrice ? formatPrice(currentPrice.price, 2) : '--'}
          </p>
        </div>

        {/* Bid/Ask */}
        {bookTop && (
          <div className="grid grid-cols-2 gap-4 pt-3 border-t border-border">
            <div>
              <p className="text-xs text-text-muted">Bid</p>
              <div className="flex items-center gap-1">
                <TrendingUp className="w-3 h-3 text-success" />
                <span className="font-medium text-success mono">
                  {formatPrice(bookTop.bidPrice, 2)}
                </span>
              </div>
              <p className="text-xs text-text-muted">
                Qty: {formatQuantity(bookTop.bidQty)}
              </p>
            </div>
            <div>
              <p className="text-xs text-text-muted">Ask</p>
              <div className="flex items-center gap-1">
                <TrendingDown className="w-3 h-3 text-danger" />
                <span className="font-medium text-danger mono">
                  {formatPrice(bookTop.askPrice, 2)}
                </span>
              </div>
              <p className="text-xs text-text-muted">
                Qty: {formatQuantity(bookTop.askQty)}
              </p>
            </div>
          </div>
        )}

        {/* Spread */}
        {bookTop && (
          <div className="pt-3 border-t border-border">
            <div className="flex items-center justify-between">
              <span className="text-sm text-text-muted">Spread</span>
              <span className="font-medium mono">
                {formatPrice(bookTop.askPrice - bookTop.bidPrice, 2)}
              </span>
            </div>
            <div className="flex items-center justify-between mt-1">
              <span className="text-sm text-text-muted">Spread %</span>
              <span className="font-medium mono">
                {((bookTop.askPrice - bookTop.bidPrice) / bookTop.bidPrice * 100).toFixed(4)}%
              </span>
            </div>
          </div>
        )}
      </div>
    </Card>
  );
}
