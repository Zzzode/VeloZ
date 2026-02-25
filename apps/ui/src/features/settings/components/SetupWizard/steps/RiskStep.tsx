import { Button } from '@/shared/components';
import { ShieldCheck, Info } from 'lucide-react';

interface RiskStepProps {
  maxPositionSize: number;
  dailyLossLimit: number;
  circuitBreakerEnabled: boolean;
  circuitBreakerThreshold: number;
  onMaxPositionSizeChange: (value: number) => void;
  onDailyLossLimitChange: (value: number) => void;
  onCircuitBreakerEnabledChange: (enabled: boolean) => void;
  onCircuitBreakerThresholdChange: (value: number) => void;
  onNext: () => void;
  onBack: () => void;
}

const RISK_PRESETS = [
  {
    id: 'conservative',
    name: 'Conservative',
    description: 'Lower risk, smaller positions',
    maxPositionSize: 0.05,
    dailyLossLimit: 0.02,
    circuitBreakerThreshold: 0.05,
  },
  {
    id: 'moderate',
    name: 'Moderate',
    description: 'Balanced risk approach',
    maxPositionSize: 0.1,
    dailyLossLimit: 0.05,
    circuitBreakerThreshold: 0.1,
  },
  {
    id: 'aggressive',
    name: 'Aggressive',
    description: 'Higher risk, larger positions',
    maxPositionSize: 0.25,
    dailyLossLimit: 0.1,
    circuitBreakerThreshold: 0.15,
  },
];

export function RiskStep({
  maxPositionSize,
  dailyLossLimit,
  circuitBreakerEnabled,
  circuitBreakerThreshold,
  onMaxPositionSizeChange,
  onDailyLossLimitChange,
  onCircuitBreakerEnabledChange,
  onCircuitBreakerThresholdChange,
  onNext,
  onBack,
}: RiskStepProps) {
  const applyPreset = (presetId: string) => {
    const preset = RISK_PRESETS.find((p) => p.id === presetId);
    if (preset) {
      onMaxPositionSizeChange(preset.maxPositionSize);
      onDailyLossLimitChange(preset.dailyLossLimit);
      onCircuitBreakerThresholdChange(preset.circuitBreakerThreshold);
    }
  };

  // Example portfolio for preview
  const examplePortfolio = 10000;
  const maxPositionValue = examplePortfolio * maxPositionSize;
  const maxDailyLoss = examplePortfolio * dailyLossLimit;

  return (
    <div className="max-w-2xl mx-auto py-6">
      <h2 className="text-2xl font-bold text-text mb-2">Risk Parameters</h2>
      <p className="text-text-muted mb-6">
        Configure safety limits to protect your capital.
      </p>

      {/* Recommendation Banner */}
      <div className="bg-blue-500/10 border border-blue-500/30 rounded-lg p-4 mb-6">
        <div className="flex items-start gap-3">
          <ShieldCheck className="w-5 h-5 text-blue-500 flex-shrink-0 mt-0.5" />
          <div className="text-sm text-blue-700">
            <p className="font-medium">Recommended Settings for Beginners</p>
            <p className="text-blue-600">
              Start conservative - you can adjust later as you gain experience.
            </p>
          </div>
        </div>
      </div>

      {/* Quick Presets */}
      <div className="mb-8">
        <label className="block text-sm font-medium text-text mb-3">Quick Setup</label>
        <div className="grid grid-cols-3 gap-3">
          {RISK_PRESETS.map((preset) => (
            <button
              key={preset.id}
              type="button"
              onClick={() => applyPreset(preset.id)}
              className={`
                p-4 rounded-lg border-2 transition-all text-left
                ${
                  maxPositionSize === preset.maxPositionSize &&
                  dailyLossLimit === preset.dailyLossLimit
                    ? 'border-primary bg-primary/5'
                    : 'border-border hover:border-gray-300'
                }
              `}
            >
              <div className="font-medium text-text">{preset.name}</div>
              <div className="text-xs text-text-muted mt-1">{preset.description}</div>
              <div className="text-xs text-primary mt-2">
                Max {(preset.maxPositionSize * 100).toFixed(0)}% position
              </div>
            </button>
          ))}
        </div>
      </div>

      {/* Custom Settings */}
      <div className="space-y-6 mb-8">
        {/* Max Position Size */}
        <div>
          <div className="flex items-center justify-between mb-2">
            <label className="text-sm font-medium text-text">
              Maximum Position Size (% of portfolio)
            </label>
            <span className="text-sm font-semibold text-primary">
              {(maxPositionSize * 100).toFixed(0)}%
            </span>
          </div>
          <input
            type="range"
            min="0.01"
            max="0.5"
            step="0.01"
            value={maxPositionSize}
            onChange={(e) => onMaxPositionSizeChange(parseFloat(e.target.value))}
            className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
          />
          <div className="flex justify-between text-xs text-text-muted mt-1">
            <span>1%</span>
            <span>10%</span>
            <span>25%</span>
            <span>50%</span>
          </div>
          <p className="text-xs text-text-muted mt-2">
            Limits each position to {(maxPositionSize * 100).toFixed(0)}% of your account
            balance
          </p>
        </div>

        {/* Daily Loss Limit */}
        <div>
          <div className="flex items-center justify-between mb-2">
            <label className="text-sm font-medium text-text">
              Daily Loss Limit (% of portfolio)
            </label>
            <span className="text-sm font-semibold text-primary">
              {(dailyLossLimit * 100).toFixed(0)}%
            </span>
          </div>
          <input
            type="range"
            min="0.01"
            max="0.25"
            step="0.01"
            value={dailyLossLimit}
            onChange={(e) => onDailyLossLimitChange(parseFloat(e.target.value))}
            className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
          />
          <div className="flex justify-between text-xs text-text-muted mt-1">
            <span>1%</span>
            <span>5%</span>
            <span>10%</span>
            <span>25%</span>
          </div>
          <p className="text-xs text-text-muted mt-2">
            Trading stops if daily losses exceed {(dailyLossLimit * 100).toFixed(0)}%
          </p>
        </div>

        {/* Circuit Breaker */}
        <div className="border border-border rounded-lg p-4">
          <div className="flex items-center justify-between mb-4">
            <div>
              <label className="text-sm font-medium text-text">Circuit Breaker</label>
              <p className="text-xs text-text-muted">
                Automatically pause trading on rapid losses
              </p>
            </div>
            <label className="relative inline-flex items-center cursor-pointer">
              <input
                type="checkbox"
                checked={circuitBreakerEnabled}
                onChange={(e) => onCircuitBreakerEnabledChange(e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
            </label>
          </div>

          {circuitBreakerEnabled && (
            <div>
              <div className="flex items-center justify-between mb-2">
                <label className="text-xs text-text-muted">Trigger Threshold</label>
                <span className="text-xs font-semibold text-primary">
                  {(circuitBreakerThreshold * 100).toFixed(0)}%
                </span>
              </div>
              <input
                type="range"
                min="0.02"
                max="0.2"
                step="0.01"
                value={circuitBreakerThreshold}
                onChange={(e) =>
                  onCircuitBreakerThresholdChange(parseFloat(e.target.value))
                }
                className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-primary"
              />
            </div>
          )}
        </div>
      </div>

      {/* Preview */}
      <div className="bg-surface border border-border rounded-lg p-4 mb-8">
        <div className="flex items-center gap-2 mb-3">
          <Info className="w-5 h-5 text-text-muted" />
          <span className="text-sm font-medium text-text">
            Preview: With ${examplePortfolio.toLocaleString()} portfolio
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
        <p className="text-xs text-text-muted mt-3">
          Trading will pause if daily loss exceeds ${maxDailyLoss.toLocaleString()}
        </p>
      </div>

      {/* Navigation */}
      <div className="flex items-center gap-4">
        <Button variant="secondary" onClick={onBack} className="flex-1">
          Back
        </Button>
        <Button variant="primary" onClick={onNext} className="flex-1">
          Continue
        </Button>
      </div>
    </div>
  );
}
