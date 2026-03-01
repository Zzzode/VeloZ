import { useState, useEffect } from 'react';
import { Button, Card } from '@/shared/components';
import { useConfigStore } from '../../store/configStore';
import type { RiskConfig } from '../../schemas/configSchemas';
import { riskConfigSchema } from '../../schemas/configSchemas';
import { Info, ShieldCheck } from 'lucide-react';

export function RiskSettings() {
  const { config, updateConfig, saveConfig, isDirty, isLoading } = useConfigStore();
  const [localConfig, setLocalConfig] = useState<RiskConfig>(config.risk);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [saveSuccess, setSaveSuccess] = useState(false);

  useEffect(() => {
    setLocalConfig(config.risk);
  }, [config.risk]);

  const handleChange = <K extends keyof RiskConfig>(
    key: K,
    value: RiskConfig[K]
  ) => {
    setLocalConfig((prev) => ({ ...prev, [key]: value }));
    setErrors({});
    setSaveSuccess(false);
  };

  const handleNestedChange = <
    K extends keyof RiskConfig,
    NK extends keyof RiskConfig[K]
  >(
    key: K,
    nestedKey: NK,
    value: RiskConfig[K][NK]
  ) => {
    setLocalConfig((prev) => ({
      ...prev,
      [key]: {
        ...(prev[key] as object),
        [nestedKey]: value,
      },
    }));
    setErrors({});
    setSaveSuccess(false);
  };

  const handleSave = async () => {
    // Validate
    const result = riskConfigSchema.safeParse(localConfig);
    if (!result.success) {
      const newErrors: Record<string, string> = {};
      result.error.issues.forEach((err: any) => {
        newErrors[err.path.join('.')] = err.message;
      });
      setErrors(newErrors);
      return;
    }

    updateConfig('risk', localConfig);
    const success = await saveConfig();
    if (success) {
      setSaveSuccess(true);
      setTimeout(() => setSaveSuccess(false), 3000);
    }
  };

  const handleReset = () => {
    setLocalConfig(config.risk);
    setErrors({});
    setSaveSuccess(false);
  };

  // Example portfolio for preview
  const examplePortfolio = 10000;
  const maxPositionValue = examplePortfolio * localConfig.maxPositionSize;
  const maxDailyLoss = examplePortfolio * localConfig.dailyLossLimit;

  return (
    <div className="space-y-6">
      {/* Success Message */}
      {saveSuccess && (
        <div className="bg-green-500/10 border border-green-500/50 text-green-700 px-4 py-3 rounded">
          Risk settings saved successfully!
        </div>
      )}

      {/* Position Limits */}
      <Card title="Position Limits">
        <div className="space-y-6">
          {/* Max Position Size */}
          <div>
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <label className="text-sm font-medium text-text">
                  Maximum Position Size (% of portfolio)
                </label>
                <div className="group relative">
                  <Info className="w-4 h-4 text-text-muted cursor-help" />
                  <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-3 py-2 bg-gray-900 text-white text-xs rounded shadow-lg opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                    Maximum size of any single position as percentage of total portfolio
                  </div>
                </div>
              </div>
              <span className="text-sm font-semibold text-primary">
                {(localConfig.maxPositionSize * 100).toFixed(0)}%
              </span>
            </div>
            <input
              type="range"
              min="0.01"
              max="1"
              step="0.01"
              value={localConfig.maxPositionSize}
              onChange={(e) =>
                handleChange('maxPositionSize', parseFloat(e.target.value))
              }
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
            />
            <div className="flex justify-between text-xs text-text-muted mt-1">
              <span>1%</span>
              <span>10%</span>
              <span>25%</span>
              <span>50%</span>
              <span>100%</span>
            </div>
          </div>

          {/* Max Position Value */}
          <div>
            <div className="flex items-center gap-2 mb-2">
              <label className="text-sm font-medium text-text">
                Maximum Position Value (USD)
              </label>
              <div className="group relative">
                <Info className="w-4 h-4 text-text-muted cursor-help" />
                <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-3 py-2 bg-gray-900 text-white text-xs rounded shadow-lg opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                  Maximum value of any single position in USD
                </div>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <span className="text-text-muted">$</span>
              <input
                type="number"
                min="0"
                max="1000000"
                value={localConfig.maxPositionValue}
                onChange={(e) =>
                  handleChange('maxPositionValue', parseInt(e.target.value) || 0)
                }
                className="flex-1 px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
              />
            </div>
            {errors['maxPositionValue'] && (
              <p className="text-xs text-red-500 mt-1">{errors['maxPositionValue']}</p>
            )}
          </div>

          {/* Max Open Orders */}
          <div>
            <div className="flex items-center gap-2 mb-2">
              <label className="text-sm font-medium text-text">
                Maximum Open Orders
              </label>
              <div className="group relative">
                <Info className="w-4 h-4 text-text-muted cursor-help" />
                <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-3 py-2 bg-gray-900 text-white text-xs rounded shadow-lg opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                  Maximum number of open orders at any time
                </div>
              </div>
            </div>
            <input
              type="number"
              min="1"
              max="100"
              value={localConfig.maxOpenOrders}
              onChange={(e) =>
                handleChange('maxOpenOrders', parseInt(e.target.value) || 1)
              }
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
            {errors['maxOpenOrders'] && (
              <p className="text-xs text-red-500 mt-1">{errors['maxOpenOrders']}</p>
            )}
          </div>
        </div>
      </Card>

      {/* Loss Limits */}
      <Card title="Loss Limits">
        <div className="space-y-6">
          {/* Daily Loss Limit */}
          <div>
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <label className="text-sm font-medium text-text">
                  Daily Loss Limit (% of portfolio)
                </label>
                <div className="group relative">
                  <Info className="w-4 h-4 text-text-muted cursor-help" />
                  <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-3 py-2 bg-gray-900 text-white text-xs rounded shadow-lg opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                    Trading will be paused if daily loss exceeds this percentage
                  </div>
                </div>
              </div>
              <span className="text-sm font-semibold text-primary">
                {(localConfig.dailyLossLimit * 100).toFixed(0)}%
              </span>
            </div>
            <input
              type="range"
              min="0.01"
              max="0.5"
              step="0.01"
              value={localConfig.dailyLossLimit}
              onChange={(e) =>
                handleChange('dailyLossLimit', parseFloat(e.target.value))
              }
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
            />
            <div className="flex justify-between text-xs text-text-muted mt-1">
              <span>1%</span>
              <span>5%</span>
              <span>10%</span>
              <span>25%</span>
              <span>50%</span>
            </div>
          </div>

          {/* Info Alert */}
          <div className="bg-blue-500/10 border border-blue-500/30 rounded-lg p-3 text-sm text-blue-700">
            <div className="flex items-start gap-2">
              <ShieldCheck className="w-5 h-5 flex-shrink-0 mt-0.5" />
              <p>
                When daily loss limit is reached, all trading will be paused until the
                next day or manual reset.
              </p>
            </div>
          </div>
        </div>
      </Card>

      {/* Circuit Breaker */}
      <Card title="Circuit Breaker">
        <div className="space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">
                Enable Circuit Breaker
              </label>
              <p className="text-xs text-text-muted">
                Automatically pause trading on rapid losses
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.circuitBreaker.enabled}
                onChange={(e) =>
                  handleNestedChange('circuitBreaker', 'enabled', e.target.checked)
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          {localConfig.circuitBreaker.enabled && (
            <>
              {/* Threshold */}
              <div>
                <div className="flex items-center justify-between mb-2">
                  <label className="text-sm font-medium text-text">
                    Trigger Threshold (% loss)
                  </label>
                  <span className="text-sm font-semibold text-primary">
                    {(localConfig.circuitBreaker.threshold * 100).toFixed(0)}%
                  </span>
                </div>
                <input
                  type="range"
                  min="0.01"
                  max="0.5"
                  step="0.01"
                  value={localConfig.circuitBreaker.threshold}
                  onChange={(e) =>
                    handleNestedChange(
                      'circuitBreaker',
                      'threshold',
                      parseFloat(e.target.value)
                    )
                  }
                  className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
                />
              </div>

              {/* Cooldown */}
              <div>
                <div className="flex items-center gap-2 mb-2">
                  <label className="text-sm font-medium text-text">
                    Cooldown Period (minutes)
                  </label>
                </div>
                <input
                  type="number"
                  min="1"
                  max="1440"
                  value={localConfig.circuitBreaker.cooldownMinutes}
                  onChange={(e) =>
                    handleNestedChange(
                      'circuitBreaker',
                      'cooldownMinutes',
                      parseInt(e.target.value) || 1
                    )
                  }
                  className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
                />
                <p className="text-xs text-text-muted mt-1">
                  Time before trading can resume after circuit breaker triggers
                </p>
              </div>
            </>
          )}
        </div>
      </Card>

      {/* Order Confirmation */}
      <Card title="Order Confirmation">
        <div className="space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">
                Require confirmation for large orders
              </label>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.requireConfirmation.largeOrders}
                onChange={(e) =>
                  handleNestedChange(
                    'requireConfirmation',
                    'largeOrders',
                    e.target.checked
                  )
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          {localConfig.requireConfirmation.largeOrders && (
            <div>
              <label className="text-sm font-medium text-text mb-2 block">
                Large Order Threshold (USD)
              </label>
              <div className="flex items-center gap-2">
                <span className="text-text-muted">$</span>
                <input
                  type="number"
                  min="0"
                  value={localConfig.requireConfirmation.largeOrderThreshold}
                  onChange={(e) =>
                    handleNestedChange(
                      'requireConfirmation',
                      'largeOrderThreshold',
                      parseInt(e.target.value) || 0
                    )
                  }
                  className="flex-1 px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
                />
              </div>
            </div>
          )}

          <div className="flex items-center justify-between">
            <div>
              <label className="text-sm font-medium text-text">
                Require confirmation for market orders
              </label>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={localConfig.requireConfirmation.marketOrders}
                onChange={(e) =>
                  handleNestedChange(
                    'requireConfirmation',
                    'marketOrders',
                    e.target.checked
                  )
                }
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>
        </div>
      </Card>

      {/* Preview */}
      <Card title="Preview">
        <div className="bg-surface border border-border rounded-lg p-4">
          <div className="flex items-center gap-2 mb-3">
            <Info className="w-5 h-5 text-text-muted" />
            <span className="text-sm font-medium text-text">
              With ${examplePortfolio.toLocaleString()} portfolio
            </span>
          </div>
          <div className="grid grid-cols-2 gap-4 text-sm">
            <div>
              <span className="text-text-muted">Max position:</span>
              <span className="ml-2 font-medium text-text">
                ${maxPositionValue.toLocaleString()}
              </span>
            </div>
            <div>
              <span className="text-text-muted">Max daily loss:</span>
              <span className="ml-2 font-medium text-text">
                ${maxDailyLoss.toLocaleString()}
              </span>
            </div>
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
