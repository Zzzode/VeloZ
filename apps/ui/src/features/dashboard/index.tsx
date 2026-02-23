/**
 * Dashboard Page - main trading dashboard with real-time updates
 */
import { Link } from 'react-router-dom';
import { Button } from '@/shared/components';
import {
  MarketOverviewCard,
  AccountSummaryCard,
  ActivePositionsCard,
  RecentOrdersCard,
  ConnectionStatusIndicator,
  EngineStatusCard,
} from './components';
import { useSSEConnection } from './hooks';

export default function Dashboard() {
  const { reconnect } = useSSEConnection();

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-text">Dashboard</h1>
          <p className="text-text-muted mt-1">Welcome to VeloZ Trading Platform</p>
        </div>
        <ConnectionStatusIndicator onReconnect={reconnect} />
      </div>

      {/* Top Row - Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        <MarketOverviewCard />
        <AccountSummaryCard />
        <EngineStatusCard />
      </div>

      {/* Bottom Row - Activity */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
        <ActivePositionsCard />
        <RecentOrdersCard />
      </div>

      {/* Quick Actions */}
      <div className="bg-background-secondary rounded-lg p-4">
        <h3 className="text-sm font-medium text-text-muted mb-3">Quick Actions</h3>
        <div className="flex flex-wrap gap-3">
          <Link to="/trading">
            <Button variant="primary">Place Order</Button>
          </Link>
          <Link to="/strategies">
            <Button variant="secondary">Manage Strategies</Button>
          </Link>
          <Link to="/backtest">
            <Button variant="secondary">Run Backtest</Button>
          </Link>
          <Link to="/market">
            <Button variant="secondary">Market Data</Button>
          </Link>
        </div>
      </div>
    </div>
  );
}
