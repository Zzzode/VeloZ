/**
 * Account Summary Card - displays balance and P&L
 */
import { Wallet, TrendingUp, TrendingDown } from 'lucide-react';
import { Card, Spinner } from '@/shared/components';
import { useAccount } from '../hooks/useDashboardData';
import { formatCurrency } from '@/shared/utils';

export function AccountSummaryCard() {
  const { data: account, isLoading, error } = useAccount();

  if (isLoading) {
    return (
      <Card title="Account Summary">
        <div className="flex items-center justify-center h-32">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (error) {
    return (
      <Card title="Account Summary">
        <div className="text-danger text-sm">Failed to load account data</div>
      </Card>
    );
  }

  // Calculate totals from balances
  const balances = account?.balances || [];
  const usdtBalance = balances.find((b) => b.asset === 'USDT');
  const totalFree = usdtBalance?.free ?? 0;
  const totalLocked = usdtBalance?.locked ?? 0;
  const totalBalance = totalFree + totalLocked;

  // Mock P&L values (would come from positions in production)
  const unrealizedPnl = 0;
  const realizedPnl = 0;

  return (
    <Card title="Account Summary">
      <div className="space-y-4">
        {/* Total Balance */}
        <div className="flex items-start gap-3">
          <div className="p-2 bg-primary/10 rounded-lg">
            <Wallet className="w-5 h-5 text-primary" />
          </div>
          <div>
            <div className="text-sm text-text-muted">Total Balance</div>
            <div className="text-2xl font-bold font-tabular">
              {formatCurrency(totalBalance)}
            </div>
          </div>
        </div>

        {/* Balance Breakdown */}
        <div className="grid grid-cols-2 gap-4 pt-2 border-t border-border">
          <div>
            <div className="text-xs text-text-muted">Available</div>
            <div className="text-lg font-semibold font-tabular text-success">
              {formatCurrency(totalFree)}
            </div>
          </div>
          <div>
            <div className="text-xs text-text-muted">In Orders</div>
            <div className="text-lg font-semibold font-tabular text-warning">
              {formatCurrency(totalLocked)}
            </div>
          </div>
        </div>

        {/* P&L */}
        <div className="grid grid-cols-2 gap-4 pt-2 border-t border-border">
          <div className="flex items-center gap-2">
            {unrealizedPnl >= 0 ? (
              <TrendingUp className="w-4 h-4 text-success" />
            ) : (
              <TrendingDown className="w-4 h-4 text-danger" />
            )}
            <div>
              <div className="text-xs text-text-muted">Unrealized P&L</div>
              <div
                className={`text-sm font-semibold font-tabular ${
                  unrealizedPnl >= 0 ? 'text-success' : 'text-danger'
                }`}
              >
                {unrealizedPnl >= 0 ? '+' : ''}
                {formatCurrency(unrealizedPnl)}
              </div>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {realizedPnl >= 0 ? (
              <TrendingUp className="w-4 h-4 text-success" />
            ) : (
              <TrendingDown className="w-4 h-4 text-danger" />
            )}
            <div>
              <div className="text-xs text-text-muted">Realized P&L</div>
              <div
                className={`text-sm font-semibold font-tabular ${
                  realizedPnl >= 0 ? 'text-success' : 'text-danger'
                }`}
              >
                {realizedPnl >= 0 ? '+' : ''}
                {formatCurrency(realizedPnl)}
              </div>
            </div>
          </div>
        </div>
      </div>
    </Card>
  );
}
