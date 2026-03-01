import { AlertTriangle, TrendingUp, Repeat, Moon, X, Check } from 'lucide-react';
import type { AnomalyAlert as AnomalyAlertType, AnomalyType } from '../types';
import { useSecurityEducationStore } from '../store';

interface AnomalyAlertProps {
  alert: AnomalyAlertType;
  onDismiss?: () => void;
}

const anomalyConfig: Record<
  AnomalyType,
  {
    icon: typeof AlertTriangle;
    title: string;
    color: string;
  }
> = {
  'unusual-volume': {
    icon: TrendingUp,
    title: 'Unusual Trading Volume',
    color: 'warning',
  },
  'unusual-size': {
    icon: TrendingUp,
    title: 'Unusual Trade Size',
    color: 'warning',
  },
  'rapid-loss': {
    icon: AlertTriangle,
    title: 'Rapid Loss Detected',
    color: 'danger',
  },
  'revenge-trading': {
    icon: Repeat,
    title: 'Potential Revenge Trading',
    color: 'danger',
  },
  'off-hours': {
    icon: Moon,
    title: 'Off-Hours Trading',
    color: 'warning',
  },
};

export function AnomalyAlert({ alert, onDismiss }: AnomalyAlertProps) {
  const { acknowledgeAlert } = useSecurityEducationStore();

  const config = anomalyConfig[alert.type];
  const Icon = config.icon;

  const colorClasses = {
    warning: {
      bg: 'bg-warning/10',
      border: 'border-warning/20',
      icon: 'text-warning',
      title: 'text-warning',
    },
    danger: {
      bg: 'bg-danger/10',
      border: 'border-danger/20',
      icon: 'text-danger',
      title: 'text-danger',
    },
  };

  const colors = colorClasses[config.color as keyof typeof colorClasses];

  const handleAcknowledge = () => {
    acknowledgeAlert(alert.id);
    if (onDismiss) {
      onDismiss();
    }
  };

  if (alert.acknowledged) {
    return null;
  }

  return (
    <div className={`rounded-xl border p-4 ${colors.bg} ${colors.border}`}>
      <div className="flex items-start justify-between">
        <div className="flex items-start gap-3">
          <Icon className={`w-5 h-5 flex-shrink-0 mt-0.5 ${colors.icon}`} />
          <div>
            <h3 className={`font-semibold ${colors.title}`}>{config.title}</h3>
            <p className="mt-1 text-sm text-text-muted">{alert.message}</p>
            {alert.details && (
              <p className="mt-2 text-xs text-text-muted bg-background rounded p-2">
                {alert.details}
              </p>
            )}
            <p className="mt-2 text-xs text-text-muted">
              {new Date(alert.timestamp).toLocaleString()}
            </p>
          </div>
        </div>
        {onDismiss && (
          <button
            type="button"
            onClick={onDismiss}
            className="p-1 text-text-muted hover:text-text rounded"
          >
            <X className="w-4 h-4" />
          </button>
        )}
      </div>

      <div className="mt-4 flex items-center gap-2">
        <button
          type="button"
          onClick={handleAcknowledge}
          className="flex items-center gap-1.5 px-3 py-1.5 text-sm bg-primary text-white rounded-lg hover:bg-primary-dark"
        >
          <Check className="w-4 h-4" />
          Acknowledge
        </button>
      </div>
    </div>
  );
}

// Component to display multiple alerts
interface AnomalyAlertListProps {
  maxAlerts?: number;
}

export function AnomalyAlertList({ maxAlerts = 5 }: AnomalyAlertListProps) {
  const { anomalyAlerts } = useSecurityEducationStore();

  const unacknowledgedAlerts = anomalyAlerts
    .filter((alert) => !alert.acknowledged)
    .slice(0, maxAlerts);

  if (unacknowledgedAlerts.length === 0) {
    return null;
  }

  return (
    <div className="space-y-3">
      {unacknowledgedAlerts.map((alert) => (
        <AnomalyAlert key={alert.id} alert={alert} />
      ))}
    </div>
  );
}
