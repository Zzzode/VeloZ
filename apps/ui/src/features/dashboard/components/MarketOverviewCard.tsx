/**
 * Market Overview Card - displays current market price and status
 */
import { TrendingUp, TrendingDown, Minus } from 'lucide-react';
import { Card, Spinner } from '@/shared/components';
import { useMarketData, useConfig } from '../hooks/useDashboardData';
import { useDashboardStore } from '../store/dashboardStore';
import { formatPrice } from '@/shared/utils';

export function MarketOverviewCard() {
  const { data: marketData, isLoading: marketLoading, error: marketError } = useMarketData();
  const { data: config, isLoading: configLoading } = useConfig();
  const prices = useDashboardStore((state) => state.prices);

  // Use SSE price if available, otherwise fall back to REST API
  const symbol = config?.market_symbol || 'BTCUSDT';
  const ssePrice = prices[symbol];
  const currentPrice = ssePrice?.price ?? marketData?.price ?? 0;

  const isLoading = marketLoading || configLoading;

  if (isLoading) {
    return (
      <Card title="Market Overview">
        <div className="flex items-center justify-center h-24">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (marketError) {
    return (
      <Card title="Market Overview">
        <div className="text-danger text-sm">Failed to load market data</div>
      </Card>
    );
  }

  // Mock 24h change for now (would come from API in production)
  const change24h = 0;
  const changePercent = 0;

  return (
    <Card title="Market Overview">
      <div className="space-y-4">
        {/* Symbol and Price */}
        <div>
          <div className="text-sm text-text-muted mb-1">{symbol}</div>
          <div className="flex items-baseline gap-2">
            <span className="text-3xl font-bold font-tabular">
              ${formatPrice(currentPrice, 2)}
            </span>
            {ssePrice && (
              <span className="text-xs text-success animate-pulse">LIVE</span>
            )}
          </div>
        </div>

        {/* 24h Change */}
        <div className="flex items-center gap-2">
          {change24h > 0 ? (
            <TrendingUp className="w-4 h-4 text-success" />
          ) : change24h < 0 ? (
            <TrendingDown className="w-4 h-4 text-danger" />
          ) : (
            <Minus className="w-4 h-4 text-text-muted" />
          )}
          <span
            className={`text-sm font-medium ${
              change24h > 0
                ? 'text-success'
                : change24h < 0
                ? 'text-danger'
                : 'text-text-muted'
            }`}
          >
            {change24h >= 0 ? '+' : ''}
            {changePercent.toFixed(2)}%
          </span>
          <span className="text-sm text-text-muted">24h</span>
        </div>

        {/* Market Source */}
        <div className="pt-2 border-t border-border">
          <div className="flex justify-between text-xs">
            <span className="text-text-muted">Source</span>
            <span className="font-medium">
              {config?.market_source === 'binance_rest' ? 'Binance' : 'Simulator'}
            </span>
          </div>
        </div>
      </div>
    </Card>
  );
}
