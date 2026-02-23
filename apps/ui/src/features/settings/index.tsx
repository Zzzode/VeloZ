import ConfigForm from './components/ConfigForm';

export default function Settings() {
  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-text">Settings</h1>
        <p className="text-text-muted mt-1">
          Configure your trading environment and exchange connections
        </p>
      </div>

      <ConfigForm />
    </div>
  );
}
