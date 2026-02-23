/**
 * Engine Status Card - displays engine and strategy status
 */
import { Link } from 'react-router-dom';
import { Activity, Play, Square, AlertCircle, ArrowRight } from 'lucide-react';
import { Card, Spinner, Button } from '@/shared/components';
import { useEngineStatus, useStrategies } from '../hooks/useDashboardData';

export function EngineStatusCard() {
  const { data: engineStatus, isLoading: engineLoading, error: engineError } = useEngineStatus();
  const { data: strategies, isLoading: strategiesLoading } = useStrategies();

  const isLoading = engineLoading || strategiesLoading;

  if (isLoading) {
    return (
      <Card title="Engine Status">
        <div className="flex items-center justify-center h-24">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (engineError) {
    return (
      <Card title="Engine Status">
        <div className="flex items-center gap-2 text-danger">
          <AlertCircle className="w-4 h-4" />
          <span className="text-sm">Engine unavailable</span>
        </div>
      </Card>
    );
  }

  const state = engineStatus?.state || 'Stopped';
  const isRunning = state === 'Running';
  const runningStrategies = strategies?.filter((s) => s.state === 'Running').length || 0;
  const totalStrategies = strategies?.length || 0;

  // Format uptime
  const uptimeMs = engineStatus?.uptime_ms || 0;
  const uptimeHours = Math.floor(uptimeMs / 3600000);
  const uptimeMinutes = Math.floor((uptimeMs % 3600000) / 60000);

  return (
    <Card
      title="Engine Status"
      headerAction={
        <Link to="/strategies">
          <Button variant="secondary" size="sm">
            Strategies
            <ArrowRight className="w-4 h-4 ml-1" />
          </Button>
        </Link>
      }
    >
      <div className="space-y-4">
        {/* Engine State */}
        <div className="flex items-center gap-3">
          <div
            className={`p-2 rounded-lg ${
              isRunning ? 'bg-success/10' : 'bg-gray-100'
            }`}
          >
            {isRunning ? (
              <Activity className="w-5 h-5 text-success" />
            ) : (
              <Square className="w-5 h-5 text-text-muted" />
            )}
          </div>
          <div>
            <div className="font-medium">{state}</div>
            {isRunning && (
              <div className="text-xs text-text-muted">
                Uptime: {uptimeHours}h {uptimeMinutes}m
              </div>
            )}
          </div>
        </div>

        {/* Strategy Stats */}
        <div className="grid grid-cols-2 gap-4 pt-2 border-t border-border">
          <div className="flex items-center gap-2">
            <Play className="w-4 h-4 text-success" />
            <div>
              <div className="text-xs text-text-muted">Running</div>
              <div className="text-lg font-semibold">{runningStrategies}</div>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <Activity className="w-4 h-4 text-text-muted" />
            <div>
              <div className="text-xs text-text-muted">Total</div>
              <div className="text-lg font-semibold">{totalStrategies}</div>
            </div>
          </div>
        </div>
      </div>
    </Card>
  );
}
