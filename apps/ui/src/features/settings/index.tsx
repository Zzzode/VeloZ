import { Card } from '@/shared/components';

export default function Settings() {
  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-text">Settings</h1>
        <p className="text-text-muted mt-1">Configure your trading environment</p>
      </div>

      <Card title="General Settings">
        <p className="text-text-muted">General settings will be implemented here.</p>
      </Card>

      <Card title="API Configuration">
        <p className="text-text-muted">API configuration will be implemented here.</p>
      </Card>

      <Card title="Notifications">
        <p className="text-text-muted">Notification settings will be implemented here.</p>
      </Card>
    </div>
  );
}
