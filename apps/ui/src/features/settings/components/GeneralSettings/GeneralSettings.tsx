import { useState, useEffect } from 'react';
import { Button, Card } from '@/shared/components';
import { useConfigStore } from '../../store/configStore';
import type { GeneralConfig } from '../../schemas/configSchemas';

const LANGUAGES = [
  { value: 'en', label: 'English' },
  { value: 'zh', label: 'Chinese' },
  { value: 'ja', label: 'Japanese' },
  { value: 'ko', label: 'Korean' },
];

const THEMES = [
  { value: 'light', label: 'Light' },
  { value: 'dark', label: 'Dark' },
  { value: 'system', label: 'System' },
];

const DATE_FORMATS = [
  { value: 'ISO', label: 'ISO (2026-02-25)' },
  { value: 'US', label: 'US (02/25/2026)' },
  { value: 'EU', label: 'EU (25/02/2026)' },
];

const TIMEZONES = [
  { value: 'UTC', label: 'UTC' },
  { value: 'America/New_York', label: 'Eastern Time (ET)' },
  { value: 'America/Los_Angeles', label: 'Pacific Time (PT)' },
  { value: 'Europe/London', label: 'London (GMT)' },
  { value: 'Europe/Paris', label: 'Central European (CET)' },
  { value: 'Asia/Tokyo', label: 'Tokyo (JST)' },
  { value: 'Asia/Shanghai', label: 'Shanghai (CST)' },
  { value: 'Asia/Singapore', label: 'Singapore (SGT)' },
];

export function GeneralSettings() {
  const { config, updateConfig, saveConfig, isDirty, isLoading } = useConfigStore();
  const [localConfig, setLocalConfig] = useState<GeneralConfig>(config.general);
  const [saveSuccess, setSaveSuccess] = useState(false);

  useEffect(() => {
    setLocalConfig(config.general);
  }, [config.general]);

  const handleChange = <K extends keyof GeneralConfig>(
    key: K,
    value: GeneralConfig[K]
  ) => {
    setLocalConfig((prev) => ({ ...prev, [key]: value }));
    setSaveSuccess(false);
  };

  const handleNotificationChange = (
    key: keyof GeneralConfig['notifications'],
    value: boolean
  ) => {
    setLocalConfig((prev) => ({
      ...prev,
      notifications: {
        ...prev.notifications,
        [key]: value,
      },
    }));
    setSaveSuccess(false);
  };

  const handleSave = async () => {
    updateConfig('general', localConfig);
    const success = await saveConfig();
    if (success) {
      setSaveSuccess(true);
      setTimeout(() => setSaveSuccess(false), 3000);
    }
  };

  const handleReset = () => {
    setLocalConfig(config.general);
    setSaveSuccess(false);
  };

  return (
    <div className="space-y-6">
      {/* Success Message */}
      {saveSuccess && (
        <div className="bg-green-500/10 border border-green-500/50 text-green-700 px-4 py-3 rounded">
          Settings saved successfully!
        </div>
      )}

      {/* Appearance */}
      <Card title="Appearance">
        <div className="space-y-4">
          {/* Theme */}
          <div>
            <label className="block text-sm font-medium text-text mb-2">Theme</label>
            <div className="flex gap-3">
              {THEMES.map((theme) => (
                <button
                  key={theme.value}
                  type="button"
                  onClick={() =>
                    handleChange('theme', theme.value as GeneralConfig['theme'])
                  }
                  className={`
                    flex-1 px-4 py-3 rounded-lg border-2 transition-all
                    ${
                      localConfig.theme === theme.value
                        ? 'border-primary bg-primary/5'
                        : 'border-border hover:border-gray-300'
                    }
                  `}
                >
                  <span className="text-sm font-medium text-text">{theme.label}</span>
                </button>
              ))}
            </div>
          </div>

          {/* Language */}
          <div>
            <label className="block text-sm font-medium text-text mb-2">Language</label>
            <select
              value={localConfig.language}
              onChange={(e) =>
                handleChange('language', e.target.value as GeneralConfig['language'])
              }
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            >
              {LANGUAGES.map((lang) => (
                <option key={lang.value} value={lang.value}>
                  {lang.label}
                </option>
              ))}
            </select>
          </div>
        </div>
      </Card>

      {/* Date & Time */}
      <Card title="Date & Time">
        <div className="space-y-4">
          {/* Timezone */}
          <div>
            <label className="block text-sm font-medium text-text mb-2">Timezone</label>
            <select
              value={localConfig.timezone}
              onChange={(e) => handleChange('timezone', e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            >
              {TIMEZONES.map((tz) => (
                <option key={tz.value} value={tz.value}>
                  {tz.label}
                </option>
              ))}
            </select>
          </div>

          {/* Date Format */}
          <div>
            <label className="block text-sm font-medium text-text mb-2">
              Date Format
            </label>
            <select
              value={localConfig.dateFormat}
              onChange={(e) =>
                handleChange('dateFormat', e.target.value as GeneralConfig['dateFormat'])
              }
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            >
              {DATE_FORMATS.map((format) => (
                <option key={format.value} value={format.value}>
                  {format.label}
                </option>
              ))}
            </select>
          </div>
        </div>
      </Card>

      {/* Notifications */}
      <Card title="Notifications">
        <div className="space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">Order Fills</label>
              <p className="text-xs text-text-muted">
                Notify when orders are filled
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.notifications.orderFills}
                onChange={(e) =>
                  handleNotificationChange('orderFills', e.target.checked)
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">Price Alerts</label>
              <p className="text-xs text-text-muted">
                Notify when price targets are reached
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.notifications.priceAlerts}
                onChange={(e) =>
                  handleNotificationChange('priceAlerts', e.target.checked)
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">System Alerts</label>
              <p className="text-xs text-text-muted">
                Important system notifications
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.notifications.systemAlerts}
                onChange={(e) =>
                  handleNotificationChange('systemAlerts', e.target.checked)
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">Sound</label>
              <p className="text-xs text-text-muted">
                Play sound for notifications
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.notifications.sound}
                onChange={(e) => handleNotificationChange('sound', e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>
        </div>
      </Card>

      {/* Actions */}
      <div className="flex justify-end gap-4">
        <Button variant="secondary" onClick={handleReset} disabled={!isDirty}>
          Reset
        </Button>
        <Button
          variant="primary"
          onClick={handleSave}
          isLoading={isLoading}
          disabled={!isDirty}
        >
          Save Changes
        </Button>
      </div>
    </div>
  );
}
