/**
 * Performance Metrics Component
 * Displays strategy performance statistics in a grid layout
 */

import { TrendingUp, TrendingDown, Activity, Target, Award, BarChart3 } from 'lucide-react';
import { Card } from '@/shared/components';
import type { StrategyPerformanceMetrics } from '../types';

interface PerformanceMetricsProps {
  metrics: StrategyPerformanceMetrics;
  className?: string;
}

interface MetricCardProps {
  label: string;
  value: string;
  subValue?: string;
  icon: React.ReactNode;
  trend?: 'positive' | 'negative' | 'neutral';
}

function MetricCard({ label, value, subValue, icon, trend = 'neutral' }: MetricCardProps) {
  const trendColors = {
    positive: 'text-success',
    negative: 'text-danger',
    neutral: 'text-text',
  };

  return (
    <div className="flex items-start gap-3 p-3 bg-surface-hover rounded-lg">
      <div className="p-2 bg-surface rounded-lg text-text-muted">{icon}</div>
      <div>
        <p className="text-xs text-text-muted">{label}</p>
        <p className={`text-lg font-bold ${trendColors[trend]}`}>{value}</p>
        {subValue && <p className="text-xs text-text-muted">{subValue}</p>}
      </div>
    </div>
  );
}

export function PerformanceMetrics({ metrics, className = '' }: PerformanceMetricsProps) {
  const formatPercent = (value: number, showSign = true) => {
    const sign = showSign && value >= 0 ? '+' : '';
    return `${sign}${value.toFixed(1)}%`;
  };

  const formatNumber = (value: number, decimals = 2) => {
    return value.toFixed(decimals);
  };

  return (
    <div className={`space-y-6 ${className}`}>
      {/* Performance Section */}
      <Card title="Performance Metrics" subtitle="Historical returns and risk-adjusted metrics">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <MetricCard
            label="12M Return"
            value={formatPercent(metrics.return_12m)}
            icon={<TrendingUp className="w-5 h-5" />}
            trend={metrics.return_12m >= 0 ? 'positive' : 'negative'}
          />
          <MetricCard
            label="6M Return"
            value={formatPercent(metrics.return_6m)}
            icon={<TrendingUp className="w-5 h-5" />}
            trend={metrics.return_6m >= 0 ? 'positive' : 'negative'}
          />
          <MetricCard
            label="3M Return"
            value={formatPercent(metrics.return_3m)}
            icon={<TrendingUp className="w-5 h-5" />}
            trend={metrics.return_3m >= 0 ? 'positive' : 'negative'}
          />
          <MetricCard
            label="1M Return"
            value={formatPercent(metrics.return_1m)}
            icon={<TrendingUp className="w-5 h-5" />}
            trend={metrics.return_1m >= 0 ? 'positive' : 'negative'}
          />
        </div>
      </Card>

      {/* Risk Section */}
      <Card title="Risk Metrics" subtitle="Drawdown and volatility analysis">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <MetricCard
            label="Max Drawdown"
            value={formatPercent(metrics.max_drawdown, false)}
            icon={<TrendingDown className="w-5 h-5" />}
            trend="negative"
          />
          <MetricCard
            label="Avg Drawdown"
            value={formatPercent(metrics.avg_drawdown, false)}
            icon={<TrendingDown className="w-5 h-5" />}
            trend="negative"
          />
          <MetricCard
            label="Recovery Time"
            value={`${metrics.recovery_time_days} days`}
            icon={<Activity className="w-5 h-5" />}
          />
          <MetricCard
            label="Volatility"
            value={formatPercent(metrics.volatility, false)}
            icon={<BarChart3 className="w-5 h-5" />}
          />
        </div>
      </Card>

      {/* Trading Section */}
      <Card title="Trading Statistics" subtitle="Win rate and trade analysis">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <MetricCard
            label="Win Rate"
            value={formatPercent(metrics.win_rate, false)}
            icon={<Target className="w-5 h-5" />}
            trend={metrics.win_rate >= 50 ? 'positive' : 'negative'}
          />
          <MetricCard
            label="Profit Factor"
            value={formatNumber(metrics.profit_factor)}
            icon={<Award className="w-5 h-5" />}
            trend={metrics.profit_factor >= 1.5 ? 'positive' : metrics.profit_factor >= 1 ? 'neutral' : 'negative'}
          />
          <MetricCard
            label="Sharpe Ratio"
            value={formatNumber(metrics.sharpe_ratio)}
            icon={<Activity className="w-5 h-5" />}
            trend={metrics.sharpe_ratio >= 1.5 ? 'positive' : metrics.sharpe_ratio >= 1 ? 'neutral' : 'negative'}
          />
          <MetricCard
            label="Total Trades"
            value={metrics.total_trades.toLocaleString()}
            subValue={`${metrics.trades_per_month}/month`}
            icon={<BarChart3 className="w-5 h-5" />}
          />
        </div>

        {/* Win/Loss Details */}
        <div className="mt-4 pt-4 border-t border-border">
          <div className="grid grid-cols-2 gap-4">
            <div className="p-3 bg-success/10 rounded-lg">
              <p className="text-xs text-success">Average Win</p>
              <p className="text-lg font-bold text-success">+{metrics.avg_win.toFixed(1)}%</p>
            </div>
            <div className="p-3 bg-danger/10 rounded-lg">
              <p className="text-xs text-danger">Average Loss</p>
              <p className="text-lg font-bold text-danger">-{metrics.avg_loss.toFixed(1)}%</p>
            </div>
          </div>
        </div>
      </Card>
    </div>
  );
}
