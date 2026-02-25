import { Check, X, Shield, AlertTriangle, Settings } from 'lucide-react';
import { useSecurityEducationStore } from '../store';
import { TradingModeBadge } from './TradingModeBadge';
import { EmergencyStopButton } from './EmergencyStopButton';

interface RiskDashboardProps {
  accountBalance?: number;
  onOpenSettings?: () => void;
  onEmergencyStop?: () => Promise<number> | number;
}

export function RiskDashboard({
  accountBalance = 10000,
  onOpenSettings,
  onEmergencyStop,
}: RiskDashboardProps) {
  const {
    tradingMode,
    lossLimits,
    lossTracking,
    getLossLimitProgress,
    emergencyStop,
    onboarding,
    anomalyDetectionEnabled,
  } = useSecurityEducationStore();

  const progress = getLossLimitProgress();
  const dailyRemaining = (lossLimits.dailyLimitPercent - lossTracking.dailyLossPercent) / 100 * accountBalance;
  const positionExposure = 65; // This would come from actual position data

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold text-text">Risk Dashboard</h2>
        <TradingModeBadge mode={tradingMode} size="md" />
      </div>

      {/* Status cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* Daily Loss Status */}
        <div className="bg-background-secondary rounded-xl p-4">
          <h3 className="text-sm font-medium text-text-muted mb-3">
            Daily Loss Status
          </h3>
          <div className="space-y-2">
            <div className="flex justify-between text-sm">
              <span className="text-text-muted">Limit:</span>
              <span className="text-text">{lossLimits.dailyLimitPercent}%</span>
            </div>
            <div className="flex justify-between text-sm">
              <span className="text-text-muted">Current:</span>
              <span
                className={
                  progress.daily >= 80
                    ? 'text-danger'
                    : progress.daily >= 50
                    ? 'text-warning'
                    : 'text-text'
                }
              >
                -{lossTracking.dailyLossPercent.toFixed(1)}%
              </span>
            </div>
            <div className="h-2 bg-background rounded-full overflow-hidden">
              <div
                className={`h-full transition-all ${
                  progress.daily >= 80
                    ? 'bg-danger'
                    : progress.daily >= 50
                    ? 'bg-warning'
                    : 'bg-success'
                }`}
                style={{ width: `${Math.min(progress.daily, 100)}%` }}
              />
            </div>
            <div className="flex justify-between text-xs text-text-muted">
              <span>{progress.daily.toFixed(0)}%</span>
              <span>Remaining: ${dailyRemaining.toFixed(0)}</span>
            </div>
          </div>
        </div>

        {/* Position Exposure */}
        <div className="bg-background-secondary rounded-xl p-4">
          <h3 className="text-sm font-medium text-text-muted mb-3">
            Position Exposure
          </h3>
          <div className="space-y-2">
            <div className="flex justify-between text-sm">
              <span className="text-text-muted">Max Allowed:</span>
              <span className="text-text">10%</span>
            </div>
            <div className="flex justify-between text-sm">
              <span className="text-text-muted">Current:</span>
              <span className="text-text">{positionExposure / 10}%</span>
            </div>
            <div className="h-2 bg-background rounded-full overflow-hidden">
              <div
                className={`h-full transition-all ${
                  positionExposure >= 80
                    ? 'bg-danger'
                    : positionExposure >= 50
                    ? 'bg-warning'
                    : 'bg-success'
                }`}
                style={{ width: `${positionExposure}%` }}
              />
            </div>
            <div className="flex justify-between text-xs text-text-muted">
              <span>{positionExposure}%</span>
              <span>Available: ${((100 - positionExposure) / 100 * accountBalance * 0.1).toFixed(0)}</span>
            </div>
          </div>
        </div>
      </div>

      {/* Active Protections */}
      <div className="bg-background-secondary rounded-xl p-4">
        <h3 className="text-sm font-medium text-text-muted mb-3">
          Active Protections
        </h3>
        <div className="space-y-2">
          <ProtectionItem
            label={`Daily loss limit (${lossLimits.dailyLimitPercent}%)`}
            active={lossLimits.enabled}
          />
          <ProtectionItem
            label="Maximum position size (10%)"
            active={true}
          />
          <ProtectionItem
            label="Maximum leverage (5x)"
            active={true}
          />
          <ProtectionItem
            label="Order confirmation > $500"
            active={true}
          />
          <ProtectionItem
            label="Anomaly detection"
            active={anomalyDetectionEnabled}
          />
          <ProtectionItem
            label="Emergency stop"
            active={emergencyStop.isActive}
            status={emergencyStop.isActive ? 'Active' : 'Standby'}
            statusColor={emergencyStop.isActive ? 'danger' : 'text-muted'}
          />
        </div>
      </div>

      {/* Security Checklist */}
      <div className="bg-background-secondary rounded-xl p-4">
        <div className="flex items-center gap-2 mb-3">
          <Shield className="w-4 h-4 text-primary" />
          <h3 className="text-sm font-medium text-text-muted">
            Security Status
          </h3>
        </div>
        <div className="space-y-2">
          <SecurityItem
            label="Security onboarding completed"
            completed={onboarding.isComplete}
          />
          <SecurityItem
            label="API key without withdrawal permissions"
            completed={true}
          />
          <SecurityItem
            label="IP restriction enabled"
            completed={true}
          />
          <SecurityItem
            label="Daily loss limit configured"
            completed={lossLimits.enabled}
          />
          <SecurityItem
            label="Circuit breakers enabled"
            completed={true}
          />
        </div>
      </div>

      {/* Action buttons */}
      <div className="flex items-center justify-between">
        <EmergencyStopButton onStop={onEmergencyStop} />
        {onOpenSettings && (
          <button
            type="button"
            onClick={onOpenSettings}
            className="flex items-center gap-2 px-4 py-2 border border-border rounded-lg hover:bg-background-secondary"
          >
            <Settings className="w-4 h-4" />
            Edit Settings
          </button>
        )}
      </div>
    </div>
  );
}

interface ProtectionItemProps {
  label: string;
  active: boolean;
  status?: string;
  statusColor?: string;
}

function ProtectionItem({
  label,
  active,
  status,
  statusColor = 'success',
}: ProtectionItemProps) {
  return (
    <div className="flex items-center justify-between py-1">
      <div className="flex items-center gap-2">
        {active ? (
          <Check className="w-4 h-4 text-success" />
        ) : (
          <X className="w-4 h-4 text-text-muted" />
        )}
        <span className="text-sm text-text">{label}</span>
      </div>
      <span
        className={`text-xs ${
          statusColor === 'danger'
            ? 'text-danger'
            : statusColor === 'text-muted'
            ? 'text-text-muted'
            : 'text-success'
        }`}
      >
        {status || (active ? 'Active' : 'Inactive')}
      </span>
    </div>
  );
}

interface SecurityItemProps {
  label: string;
  completed: boolean;
}

function SecurityItem({ label, completed }: SecurityItemProps) {
  return (
    <div className="flex items-center gap-2 py-1">
      {completed ? (
        <Check className="w-4 h-4 text-success" />
      ) : (
        <AlertTriangle className="w-4 h-4 text-warning" />
      )}
      <span className="text-sm text-text">{label}</span>
    </div>
  );
}
