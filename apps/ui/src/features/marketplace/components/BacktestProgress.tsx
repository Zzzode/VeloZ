/**
 * Backtest Progress Component
 * Shows real-time progress during backtest execution
 */

import { Loader2, CheckCircle, XCircle, Clock } from 'lucide-react';
import { Card } from '@/shared/components';
import type { BacktestProgress as BacktestProgressType } from '../types';

interface BacktestProgressProps {
  progress: BacktestProgressType | null;
  className?: string;
}

export function BacktestProgress({ progress, className = '' }: BacktestProgressProps) {
  if (!progress) {
    return null;
  }

  const getStatusIcon = () => {
    switch (progress.status) {
      case 'queued':
        return <Clock className="w-5 h-5 text-text-muted animate-pulse" />;
      case 'running':
        return <Loader2 className="w-5 h-5 text-primary animate-spin" />;
      case 'completed':
        return <CheckCircle className="w-5 h-5 text-success" />;
      case 'failed':
        return <XCircle className="w-5 h-5 text-danger" />;
    }
  };

  const getStatusText = () => {
    switch (progress.status) {
      case 'queued':
        return 'Waiting in queue...';
      case 'running':
        return progress.message ?? `Processing ${progress.current_date ?? ''}...`;
      case 'completed':
        return 'Backtest completed!';
      case 'failed':
        return progress.message ?? 'Backtest failed';
    }
  };

  const getStatusColor = () => {
    switch (progress.status) {
      case 'queued':
        return 'bg-surface-hover';
      case 'running':
        return 'bg-primary/10';
      case 'completed':
        return 'bg-success/10';
      case 'failed':
        return 'bg-danger/10';
    }
  };

  return (
    <Card className={`${getStatusColor()} ${className}`}>
      <div className="space-y-3">
        {/* Status Header */}
        <div className="flex items-center gap-3">
          {getStatusIcon()}
          <div className="flex-1">
            <p className="font-medium text-text">{getStatusText()}</p>
            {progress.status === 'running' && progress.current_date && (
              <p className="text-xs text-text-muted">
                Processing date: {progress.current_date}
              </p>
            )}
          </div>
          <span className="text-lg font-bold text-text">
            {progress.progress_pct.toFixed(0)}%
          </span>
        </div>

        {/* Progress Bar */}
        <div className="h-2 bg-border rounded-full overflow-hidden">
          <div
            className={`h-full transition-all duration-300 ${
              progress.status === 'failed'
                ? 'bg-danger'
                : progress.status === 'completed'
                ? 'bg-success'
                : 'bg-primary'
            }`}
            style={{ width: `${progress.progress_pct}%` }}
          />
        </div>

        {/* Time Estimate (for running) */}
        {progress.status === 'running' && progress.progress_pct > 0 && (
          <p className="text-xs text-text-muted text-right">
            Estimated time remaining: calculating...
          </p>
        )}
      </div>
    </Card>
  );
}
