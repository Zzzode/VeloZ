import { useState, useEffect } from 'react';
import { useConfigStore } from './store/configStore';
import {
  SetupWizard,
  APIKeyManager,
  RiskSettings,
  GeneralSettings,
  ImportExport,
} from './components';
import ConfigForm from './components/ConfigForm';
import { Settings as SettingsIcon, Key, ShieldCheck, ArrowLeftRight, SlidersHorizontal, RefreshCw } from 'lucide-react';

type SettingsTab = 'general' | 'exchange' | 'apikeys' | 'risk' | 'advanced' | 'backup';

const TABS: Array<{ id: SettingsTab; label: string; icon: React.ComponentType<{ className?: string }> }> = [
  { id: 'general', label: 'General', icon: SettingsIcon },
  { id: 'exchange', label: 'Exchange', icon: ArrowLeftRight },
  { id: 'apikeys', label: 'API Keys', icon: Key },
  { id: 'risk', label: 'Risk', icon: ShieldCheck },
  { id: 'advanced', label: 'Advanced', icon: SlidersHorizontal },
  { id: 'backup', label: 'Backup', icon: RefreshCw },
];

export default function Settings() {
  const [activeTab, setActiveTab] = useState<SettingsTab>('general');
  const { wizardCompleted, loadConfig, loadAPIKeys } = useConfigStore();
  const [showWizard, setShowWizard] = useState(false);

  useEffect(() => {
    // Load config and API keys on mount
    loadConfig();
    loadAPIKeys();

    // Show wizard if not completed
    if (!wizardCompleted) {
      setShowWizard(true);
    }
  }, [loadConfig, loadAPIKeys, wizardCompleted]);

  const renderTabContent = () => {
    switch (activeTab) {
      case 'general':
        return <GeneralSettings />;
      case 'exchange':
        return <ConfigForm />;
      case 'apikeys':
        return <APIKeyManager />;
      case 'risk':
        return <RiskSettings />;
      case 'advanced':
        return <AdvancedSettings />;
      case 'backup':
        return <ImportExport />;
      default:
        return null;
    }
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-text">Settings</h1>
          <p className="text-text-muted mt-1">
            Configure your trading environment and exchange connections
          </p>
        </div>
        {wizardCompleted && (
          <button
            onClick={() => setShowWizard(true)}
            className="text-sm text-primary hover:underline"
          >
            Run Setup Wizard
          </button>
        )}
      </div>

      {/* Tabs */}
      <div className="border-b border-border">
        <nav className="flex space-x-1 overflow-x-auto">
          {TABS.map((tab) => {
            const Icon = tab.icon;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={`
                  flex items-center gap-2 px-4 py-3 text-sm font-medium whitespace-nowrap
                  border-b-2 transition-colors
                  ${
                    activeTab === tab.id
                      ? 'border-primary text-primary'
                      : 'border-transparent text-text-muted hover:text-text hover:border-gray-300'
                  }
                `}
              >
                <Icon className="w-4 h-4" />
                {tab.label}
              </button>
            );
          })}
        </nav>
      </div>

      {/* Tab Content */}
      <div className="min-h-[400px]">{renderTabContent()}</div>

      {/* Setup Wizard Modal */}
      <SetupWizard isOpen={showWizard} onClose={() => setShowWizard(false)} />
    </div>
  );
}

// Advanced Settings Component (inline for simplicity)
function AdvancedSettings() {
  const { config, updateConfig, saveConfig, isDirty, isLoading } = useConfigStore();
  const [localConfig, setLocalConfig] = useState(config.advanced);
  const [saveSuccess, setSaveSuccess] = useState(false);

  useEffect(() => {
    setLocalConfig(config.advanced);
  }, [config.advanced]);

  const handleChange = (key: keyof typeof localConfig, value: typeof localConfig[keyof typeof localConfig]) => {
    setLocalConfig((prev) => ({ ...prev, [key]: value }));
    setSaveSuccess(false);
  };

  const handleSave = async () => {
    updateConfig('advanced', localConfig);
    const success = await saveConfig();
    if (success) {
      setSaveSuccess(true);
      setTimeout(() => setSaveSuccess(false), 3000);
    }
  };

  return (
    <div className="space-y-6">
      {saveSuccess && (
        <div className="bg-green-500/10 border border-green-500/50 text-green-700 px-4 py-3 rounded">
          Advanced settings saved successfully!
        </div>
      )}

      {/* Warning */}
      <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-4 text-sm text-yellow-700">
        <p className="font-medium">Advanced Settings</p>
        <p className="text-yellow-600 mt-1">
          These settings are for experienced users. Incorrect values may affect
          performance or stability.
        </p>
      </div>

      {/* API Settings */}
      <div className="bg-surface border border-border rounded-lg p-6">
        <h3 className="font-medium text-text mb-4">API Settings</h3>
        <div className="space-y-4">
          <div>
            <label className="block text-sm font-medium text-text mb-2">
              API Rate Limit (requests/second)
            </label>
            <input
              type="number"
              min="1"
              max="100"
              value={localConfig.apiRateLimit}
              onChange={(e) => handleChange('apiRateLimit', parseInt(e.target.value) || 1)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
            <p className="text-xs text-text-muted mt-1">
              Maximum number of API requests per second
            </p>
          </div>

          <div>
            <label className="block text-sm font-medium text-text mb-2">
              WebSocket Reconnect Delay (ms)
            </label>
            <input
              type="number"
              min="1000"
              max="60000"
              step="1000"
              value={localConfig.websocketReconnectDelay}
              onChange={(e) =>
                handleChange('websocketReconnectDelay', parseInt(e.target.value) || 5000)
              }
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
            <p className="text-xs text-text-muted mt-1">
              Delay before attempting to reconnect WebSocket
            </p>
          </div>

          <div>
            <label className="block text-sm font-medium text-text mb-2">
              Order Book Depth
            </label>
            <input
              type="number"
              min="5"
              max="100"
              value={localConfig.orderBookDepth}
              onChange={(e) => handleChange('orderBookDepth', parseInt(e.target.value) || 20)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
            <p className="text-xs text-text-muted mt-1">
              Number of price levels to display in order book
            </p>
          </div>
        </div>
      </div>

      {/* Logging */}
      <div className="bg-surface border border-border rounded-lg p-6">
        <h3 className="font-medium text-text mb-4">Logging</h3>
        <div className="space-y-4">
          <div>
            <label className="block text-sm font-medium text-text mb-2">Log Level</label>
            <select
              value={localConfig.logLevel}
              onChange={(e) =>
                handleChange('logLevel', e.target.value as typeof localConfig.logLevel)
              }
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            >
              <option value="debug">Debug</option>
              <option value="info">Info</option>
              <option value="warn">Warning</option>
              <option value="error">Error</option>
            </select>
          </div>

          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">Telemetry</label>
              <p className="text-xs text-text-muted">
                Help improve VeloZ by sending anonymous usage data
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.telemetry}
                onChange={(e) => handleChange('telemetry', e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>
        </div>
      </div>

      {/* Actions */}
      <div className="flex justify-end gap-4">
        <button
          onClick={() => setLocalConfig(config.advanced)}
          disabled={!isDirty}
          className="px-4 py-2 text-text-muted hover:text-text transition-colors disabled:opacity-50"
        >
          Reset
        </button>
        <button
          onClick={handleSave}
          disabled={!isDirty || isLoading}
          className="px-6 py-2 bg-primary text-white rounded hover:bg-primary-dark transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
        >
          {isLoading ? 'Saving...' : 'Save Changes'}
        </button>
      </div>
    </div>
  );
}
