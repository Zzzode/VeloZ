/**
 * Strategy detail panel component for viewing full strategy information
 */
import { ArrowLeft, Play, Square, Settings } from 'lucide-react';
import { Card, Button, Spinner } from '@/shared/components';
import type { StrategySummary } from '@/shared/api';
import { useStrategyDetail, useStartStrategy, useStopStrategy, getStrategyStateBadge } from '../hooks';
import { StrategyMetrics } from './StrategyMetrics';

interface StrategyDetailPanelProps {
  strategy: StrategySummary;
  onBack: () => void;
  onConfigure: () => void;
}

export function StrategyDetailPanel({ strategy, onBack, onConfigure }: StrategyDetailPanelProps) {
  const { data: detail, isLoading, error } = useStrategyDetail(strategy.id);
  const startMutation = useStartStrategy();
  const stopMutation = useStopStrategy();

  const isRunning = strategy.state === 'Running';
  const isPending = startMutation.isPending || stopMutation.isPending;

  const handleToggle = () => {
    if (isRunning) {
      stopMutation.mutate(strategy.id);
    } else {
      startMutation.mutate(strategy.id);
    }
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <Button
            variant="secondary"
            size="sm"
            onClick={onBack}
            leftIcon={<ArrowLeft className="w-4 h-4" />}
          >
            Back
          </Button>
          <div>
            <div className="flex items-center gap-3">
              <h2 className="text-xl font-bold text-text">{strategy.name}</h2>
              <span className={getStrategyStateBadge(strategy.state)}>
                {strategy.state}
              </span>
            </div>
            <p className="text-sm text-text-muted">{strategy.type}</p>
          </div>
        </div>
        <div className="flex gap-2">
          <Button
            variant={isRunning ? 'danger' : 'success'}
            onClick={handleToggle}
            isLoading={isPending}
            leftIcon={isRunning ? <Square className="w-4 h-4" /> : <Play className="w-4 h-4" />}
          >
            {isRunning ? 'Stop Strategy' : 'Start Strategy'}
          </Button>
          <Button
            variant="secondary"
            onClick={onConfigure}
            leftIcon={<Settings className="w-4 h-4" />}
          >
            Configure
          </Button>
        </div>
      </div>

      {/* Content */}
      {isLoading ? (
        <div className="flex items-center justify-center py-12">
          <Spinner size="lg" />
        </div>
      ) : error ? (
        <Card>
          <div className="text-center py-8">
            <p className="text-danger">Failed to load strategy details</p>
            <p className="text-sm text-text-muted mt-1">
              {error instanceof Error ? error.message : 'Unknown error'}
            </p>
          </div>
        </Card>
      ) : detail ? (
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Main Metrics */}
          <div className="lg:col-span-2">
            <Card title="Performance Metrics">
              <StrategyMetrics strategy={detail} />
            </Card>
          </div>

          {/* Strategy Info */}
          <div className="space-y-6">
            <Card title="Strategy Info">
              <dl className="space-y-3">
                <div>
                  <dt className="text-xs text-text-muted">Strategy ID</dt>
                  <dd className="text-sm font-mono">{detail.id}</dd>
                </div>
                <div>
                  <dt className="text-xs text-text-muted">Type</dt>
                  <dd className="text-sm">{detail.type}</dd>
                </div>
                <div>
                  <dt className="text-xs text-text-muted">Status</dt>
                  <dd className="text-sm">
                    {detail.is_running ? 'Active' : 'Inactive'}
                  </dd>
                </div>
              </dl>
            </Card>

            <Card title="Quick Stats">
              <dl className="space-y-3">
                <div className="flex justify-between">
                  <dt className="text-text-muted">Win/Loss</dt>
                  <dd>
                    <span className="text-success">{detail.win_count}</span>
                    {' / '}
                    <span className="text-danger">{detail.lose_count}</span>
                  </dd>
                </div>
                <div className="flex justify-between">
                  <dt className="text-text-muted">Total Trades</dt>
                  <dd className="font-medium">{detail.trade_count}</dd>
                </div>
              </dl>
            </Card>
          </div>
        </div>
      ) : null}
    </div>
  );
}
