import { Activity, Gauge } from 'lucide-react';
import { Card, Spinner } from '@/shared/components';
import { useEngineHealth } from '@/shared/api/hooks/useEngine';

export function OperationalMetricsCard() {
  const { data, isLoading, error } = useEngineHealth();

  if (isLoading) {
    return (
      <Card title="Operational KPIs">
        <div className="flex items-center justify-center h-24">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (error) {
    return (
      <Card title="Operational KPIs">
        <div className="text-danger text-sm">Failed to load health status</div>
      </Card>
    );
  }

  const engineConnected = data?.engine.connected ?? false;

  return (
    <Card title="Operational KPIs">
      <div className="space-y-4">
        <div className="flex items-center gap-3">
          <div className={`p-2 rounded-lg ${engineConnected ? 'bg-success/10' : 'bg-danger/10'}`}>
            <Activity className={`w-5 h-5 ${engineConnected ? 'text-success' : 'text-danger'}`} />
          </div>
          <div>
            <div className="text-xs text-text-muted">Engine Connected</div>
            <div className="text-lg font-semibold">
              {engineConnected ? 'Yes' : 'No'}
            </div>
          </div>
        </div>
        <div className="flex items-center gap-3 pt-2 border-t border-border">
          <div className="p-2 rounded-lg bg-primary/10">
            <Gauge className="w-5 h-5 text-primary" />
          </div>
          <div>
            <div className="text-xs text-text-muted">Order Latency</div>
            <div className="text-lg font-semibold text-text-muted">N/A</div>
          </div>
        </div>
      </div>
    </Card>
  );
}
