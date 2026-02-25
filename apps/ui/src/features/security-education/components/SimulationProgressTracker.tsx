import { Check, Clock } from 'lucide-react';
import {
  useSecurityEducationStore,
  REQUIRED_SIMULATED_TRADES,
  REQUIRED_SIMULATED_DAYS,
} from '../store';
import { TradingModeBadge } from './TradingModeBadge';

interface SimulationProgressTrackerProps {
  simulatedStats?: {
    trades: number;
    winRate: number;
    totalPnL: number;
    maxDrawdown: number;
  };
}

export function SimulationProgressTracker({
  simulatedStats = {
    trades: 8,
    winRate: 62,
    totalPnL: 245,
    maxDrawdown: -80,
  },
}: SimulationProgressTrackerProps) {
  const {
    tradingMode,
    onboarding,
    simulatedTradesCompleted,
    simulatedDaysCompleted,
    canSwitchToLive,
    isChecklistComplete,
  } = useSecurityEducationStore();

  const tradesProgress = Math.min(
    (simulatedTradesCompleted / REQUIRED_SIMULATED_TRADES) * 100,
    100
  );
  const daysProgress = Math.min(
    (simulatedDaysCompleted / REQUIRED_SIMULATED_DAYS) * 100,
    100
  );

  const overallProgress =
    (tradesProgress +
      daysProgress +
      (onboarding.isComplete ? 100 : 0) +
      (isChecklistComplete() ? 100 : 0)) /
    4;

  const canGoLive = canSwitchToLive();

  return (
    <div className="bg-background-secondary rounded-xl p-6">
      <div className="flex items-center justify-between mb-6">
        <h2 className="text-lg font-semibold text-text">Your Trading Journey</h2>
        <TradingModeBadge mode={tradingMode} />
      </div>

      {/* Overall progress */}
      <div className="mb-6">
        <div className="flex items-center justify-between mb-2">
          <span className="text-sm font-medium text-text">
            Progress to Live Trading
          </span>
          <span className="text-sm text-text-muted">
            {overallProgress.toFixed(0)}%
          </span>
        </div>
        <div className="h-3 bg-background rounded-full overflow-hidden">
          <div
            className={`h-full transition-all ${
              canGoLive ? 'bg-success' : 'bg-primary'
            }`}
            style={{ width: `${overallProgress}%` }}
          />
        </div>
      </div>

      {/* Requirements checklist */}
      <div className="space-y-4">
        <RequirementItem
          label="Complete security onboarding"
          completed={onboarding.isComplete}
        />

        <RequirementItem
          label="Configure exchange connection"
          completed={true}
        />

        <RequirementItem
          label={`Complete ${REQUIRED_SIMULATED_TRADES} simulated trades`}
          completed={simulatedTradesCompleted >= REQUIRED_SIMULATED_TRADES}
          progress={`${simulatedTradesCompleted}/${REQUIRED_SIMULATED_TRADES}`}
        />

        <RequirementItem
          label={`${REQUIRED_SIMULATED_DAYS} days of simulation experience`}
          completed={simulatedDaysCompleted >= REQUIRED_SIMULATED_DAYS}
          progress={`${simulatedDaysCompleted}/${REQUIRED_SIMULATED_DAYS} days`}
        />

        <RequirementItem
          label="Review and accept risk disclosure"
          completed={isChecklistComplete()}
        />
      </div>

      {/* Simulation stats */}
      <div className="mt-6 pt-6 border-t border-border">
        <h3 className="text-sm font-medium text-text-muted mb-3">
          Simulation Stats
        </h3>
        <div className="grid grid-cols-4 gap-4">
          <StatItem label="Trades" value={simulatedStats.trades.toString()} />
          <StatItem label="Win Rate" value={`${simulatedStats.winRate}%`} />
          <StatItem
            label="Total P&L"
            value={`${simulatedStats.totalPnL >= 0 ? '+' : ''}$${simulatedStats.totalPnL}`}
            valueColor={simulatedStats.totalPnL >= 0 ? 'success' : 'danger'}
          />
          <StatItem
            label="Max Drawdown"
            value={`$${simulatedStats.maxDrawdown}`}
            valueColor="danger"
          />
        </div>
      </div>

      {/* Status message */}
      <div
        className={`mt-6 p-3 rounded-lg ${
          canGoLive
            ? 'bg-success/10 border border-success/20'
            : 'bg-primary/5 border border-primary/20'
        }`}
      >
        <p className="text-sm text-text-muted">
          {canGoLive ? (
            <>
              <span className="font-medium text-success">
                Congratulations!
              </span>{' '}
              You've completed all requirements and can now switch to live
              trading.
            </>
          ) : (
            <>
              <span className="font-medium text-primary">Keep going!</span>{' '}
              Complete{' '}
              {REQUIRED_SIMULATED_TRADES - simulatedTradesCompleted > 0 &&
                `${REQUIRED_SIMULATED_TRADES - simulatedTradesCompleted} more trades`}
              {REQUIRED_SIMULATED_TRADES - simulatedTradesCompleted > 0 &&
                REQUIRED_SIMULATED_DAYS - simulatedDaysCompleted > 0 &&
                ' and '}
              {REQUIRED_SIMULATED_DAYS - simulatedDaysCompleted > 0 &&
                `wait ${REQUIRED_SIMULATED_DAYS - simulatedDaysCompleted} more days`}{' '}
              to unlock live trading.
            </>
          )}
        </p>
      </div>
    </div>
  );
}

interface RequirementItemProps {
  label: string;
  completed: boolean;
  progress?: string;
}

function RequirementItem({
  label,
  completed,
  progress,
}: RequirementItemProps) {
  return (
    <div className="flex items-center gap-3">
      <div
        className={`w-6 h-6 rounded-full flex items-center justify-center ${
          completed ? 'bg-success/10' : 'bg-background'
        }`}
      >
        {completed ? (
          <Check className="w-4 h-4 text-success" />
        ) : (
          <Clock className="w-4 h-4 text-text-muted" />
        )}
      </div>
      <div className="flex-1">
        <span
          className={`text-sm ${completed ? 'text-text' : 'text-text-muted'}`}
        >
          {label}
        </span>
      </div>
      {progress && (
        <span className="text-xs text-text-muted">{progress}</span>
      )}
    </div>
  );
}

interface StatItemProps {
  label: string;
  value: string;
  valueColor?: 'success' | 'danger' | 'default';
}

function StatItem({ label, value, valueColor = 'default' }: StatItemProps) {
  const colorClass =
    valueColor === 'success'
      ? 'text-success'
      : valueColor === 'danger'
      ? 'text-danger'
      : 'text-text';

  return (
    <div className="text-center">
      <div className={`text-lg font-semibold ${colorClass}`}>{value}</div>
      <div className="text-xs text-text-muted">{label}</div>
    </div>
  );
}
