import { AlertCircle, X, Settings, Pause } from 'lucide-react';
import { useSecurityEducationStore } from '../store';

interface LossLimitWarningProps {
  onDismiss?: () => void;
  onPauseTrading?: () => void;
  onOpenSettings?: () => void;
}

export function LossLimitWarning({
  onDismiss,
  onPauseTrading,
  onOpenSettings,
}: LossLimitWarningProps) {
  const { lossLimits, lossTracking, getLossLimitProgress } =
    useSecurityEducationStore();

  const progress = getLossLimitProgress();
  const dailyRemaining = lossLimits.dailyLimitPercent - lossTracking.dailyLossPercent;

  // Only show if approaching limit (>= 80%)
  if (progress.daily < 80) {
    return null;
  }

  const isAtLimit = progress.daily >= 100;

  return (
    <div
      className={`
        rounded-xl border p-4
        ${
          isAtLimit
            ? 'bg-danger/10 border-danger/20'
            : 'bg-warning/10 border-warning/20'
        }
      `}
    >
      <div className="flex items-start justify-between">
        <div className="flex items-start gap-3">
          <AlertCircle
            className={`w-5 h-5 flex-shrink-0 mt-0.5 ${
              isAtLimit ? 'text-danger' : 'text-warning'
            }`}
          />
          <div>
            <h3
              className={`font-semibold ${
                isAtLimit ? 'text-danger' : 'text-warning'
              }`}
            >
              {isAtLimit
                ? 'Daily Loss Limit Reached'
                : 'Approaching Daily Loss Limit'}
            </h3>
            <p className="mt-1 text-sm text-text-muted">
              {isAtLimit
                ? `You have lost ${lossTracking.dailyLossPercent.toFixed(1)}% today. Trading is paused until tomorrow.`
                : `You have lost ${lossTracking.dailyLossPercent.toFixed(1)}% today (${progress.daily.toFixed(0)}% of your ${lossLimits.dailyLimitPercent}% daily limit).`}
            </p>
          </div>
        </div>
        {onDismiss && !isAtLimit && (
          <button
            type="button"
            onClick={onDismiss}
            className="p-1 text-text-muted hover:text-text rounded"
          >
            <X className="w-4 h-4" />
          </button>
        )}
      </div>

      {/* Progress bar */}
      <div className="mt-4">
        <div className="flex justify-between text-xs text-text-muted mb-1">
          <span>Daily Loss Limit: {lossLimits.dailyLimitPercent}%</span>
          <span>
            Current: {lossTracking.dailyLossPercent.toFixed(1)}%
          </span>
        </div>
        <div className="h-2 bg-background rounded-full overflow-hidden">
          <div
            className={`h-full transition-all ${
              isAtLimit ? 'bg-danger' : progress.daily >= 90 ? 'bg-warning' : 'bg-warning/70'
            }`}
            style={{ width: `${Math.min(progress.daily, 100)}%` }}
          />
        </div>
        {!isAtLimit && (
          <p className="mt-1 text-xs text-text-muted">
            Remaining: {dailyRemaining.toFixed(1)}%
          </p>
        )}
      </div>

      {/* Actions */}
      {!isAtLimit && (
        <div className="mt-4 flex items-center gap-2">
          {onPauseTrading && (
            <button
              type="button"
              onClick={onPauseTrading}
              className="flex items-center gap-1.5 px-3 py-1.5 text-sm bg-warning text-white rounded-lg hover:bg-warning/90"
            >
              <Pause className="w-4 h-4" />
              Pause Trading
            </button>
          )}
          {onOpenSettings && (
            <button
              type="button"
              onClick={onOpenSettings}
              className="flex items-center gap-1.5 px-3 py-1.5 text-sm border border-border rounded-lg hover:bg-background-secondary"
            >
              <Settings className="w-4 h-4" />
              Settings
            </button>
          )}
        </div>
      )}
    </div>
  );
}
