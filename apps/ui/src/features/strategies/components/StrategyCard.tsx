/**
 * Individual strategy card component
 */
import { Play, Square, Settings, TrendingUp, TrendingDown } from 'lucide-react';
import { Card, Button } from '@/shared/components';
import type { StrategySummary } from '@/shared/api';
import { formatCurrency } from '@/shared/utils';
import { getStrategyStateBadge, useStartStrategy, useStopStrategy } from '../hooks';

interface StrategyCardProps {
  strategy: StrategySummary;
  onConfigure?: (strategy: StrategySummary) => void;
  onSelect?: (strategy: StrategySummary) => void;
}

export function StrategyCard({ strategy, onConfigure, onSelect }: StrategyCardProps) {
  const startMutation = useStartStrategy();
  const stopMutation = useStopStrategy();

  const isRunning = strategy.state === 'Running';
  const isPending = startMutation.isPending || stopMutation.isPending;

  const handleToggle = (e: React.MouseEvent) => {
    e.stopPropagation();
    if (isRunning) {
      stopMutation.mutate(strategy.id);
    } else {
      startMutation.mutate(strategy.id);
    }
  };

  const handleConfigure = (e: React.MouseEvent) => {
    e.stopPropagation();
    onConfigure?.(strategy);
  };

  const pnlPositive = strategy.pnl >= 0;
  const pnlValue = pnlPositive
    ? `+${formatCurrency(strategy.pnl)}`
    : formatCurrency(strategy.pnl);

  return (
    <Card
      className="cursor-pointer hover:shadow-lg transition-shadow duration-200"
      onClick={() => onSelect?.(strategy)}
    >
      <div className="space-y-4">
        {/* Header */}
        <div className="flex items-start justify-between">
          <div>
            <h3 className="font-semibold text-text">{strategy.name}</h3>
            <p className="text-xs text-text-muted mt-0.5">{strategy.type}</p>
          </div>
          <span className={getStrategyStateBadge(strategy.state)}>
            {strategy.state}
          </span>
        </div>

        {/* Metrics */}
        <div className="grid grid-cols-2 gap-4">
          <div>
            <p className="text-xs text-text-muted">P&L</p>
            <div className="flex items-center gap-1">
              {pnlPositive ? (
                <TrendingUp className="w-4 h-4 text-success" />
              ) : (
                <TrendingDown className="w-4 h-4 text-danger" />
              )}
              <span className={`font-semibold ${pnlPositive ? 'text-success' : 'text-danger'}`}>
                {pnlValue}
              </span>
            </div>
          </div>
          <div>
            <p className="text-xs text-text-muted">Trades</p>
            <p className="font-semibold text-text">{strategy.trade_count}</p>
          </div>
        </div>

        {/* Actions */}
        <div className="flex gap-2 pt-2 border-t border-border">
          <Button
            variant={isRunning ? 'danger' : 'success'}
            size="sm"
            onClick={handleToggle}
            isLoading={isPending}
            leftIcon={isRunning ? <Square className="w-3 h-3" /> : <Play className="w-3 h-3" />}
            className="flex-1"
          >
            {isRunning ? 'Stop' : 'Start'}
          </Button>
          <Button
            variant="secondary"
            size="sm"
            onClick={handleConfigure}
            leftIcon={<Settings className="w-3 h-3" />}
          >
            Configure
          </Button>
        </div>
      </div>
    </Card>
  );
}
