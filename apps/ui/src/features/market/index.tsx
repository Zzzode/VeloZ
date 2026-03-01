/**
 * Market feature module - main page
 */
import { RefreshCw } from 'lucide-react';
import { Button } from '@/shared/components';
import {
  SymbolSelector,
  TimeframeSelector,
  MarketInfoPanel,
  RecentTrades,
  PriceChart,
} from './components';
import { useMarketWebSocket } from './hooks';
import { useMarketStore } from './store';

export default function Market() {
  const { reconnect } = useMarketWebSocket();
  const connectionState = useMarketStore((state) => state.wsConnectionState);

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div>
          <h1 className="text-2xl font-bold text-text">Market</h1>
          <p className="text-text-muted mt-1">Real-time market data and charts</p>
        </div>

        <div className="flex flex-wrap items-center gap-4">
          <SymbolSelector />
          <TimeframeSelector />
          {connectionState !== 'connected' && (
            <Button
              variant="secondary"
              size="sm"
              onClick={reconnect}
              leftIcon={<RefreshCw className="w-4 h-4" />}
            >
              Reconnect
            </Button>
          )}
        </div>
      </div>

      {/* Main Content */}
      <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
        {/* Chart - Takes 3 columns on large screens */}
        <div className="lg:col-span-3">
          <PriceChart height={500} />
        </div>

        {/* Sidebar - Market Info */}
        <div className="space-y-6">
          <MarketInfoPanel />
        </div>
      </div>

      {/* Recent Trades */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <RecentTrades />

        {/* Placeholder for additional panels */}
        <div className="hidden lg:block">
          {/* Could add order book depth chart or other widgets here */}
        </div>
      </div>
    </div>
  );
}
