import { Button } from '@/shared/components';
import { FlaskConical, FileText, Zap, Lock } from 'lucide-react';

interface TradingModeStepProps {
  tradingMode: 'simulated' | 'paper' | 'live';
  simulatedBalance: number;
  onTradingModeChange: (mode: 'simulated' | 'paper' | 'live') => void;
  onSimulatedBalanceChange: (balance: number) => void;
  onNext: () => void;
  onBack: () => void;
}

const TRADING_MODES = [
  {
    id: 'simulated' as const,
    name: 'Simulated Trading',
    description: 'Practice with virtual money in a simulated environment',
    icon: FlaskConical,
    recommended: true,
    locked: false,
    features: [
      'No real money at risk',
      'Realistic market simulation',
      'Perfect for learning strategies',
      'Instant order execution',
    ],
  },
  {
    id: 'paper' as const,
    name: 'Paper Trading',
    description: 'Trade with real market data but virtual money',
    icon: FileText,
    recommended: false,
    locked: false,
    features: [
      'Real market data',
      'Virtual balance',
      'Test strategies in real conditions',
      'No financial risk',
    ],
  },
  {
    id: 'live' as const,
    name: 'Live Trading',
    description: 'Trade with real money on the exchange',
    icon: Zap,
    recommended: false,
    locked: true,
    features: [
      'Real money trading',
      'Actual order execution',
      'For experienced traders',
      'Requires security verification',
    ],
  },
];

const BALANCE_PRESETS = [1000, 5000, 10000, 50000, 100000];

export function TradingModeStep({
  tradingMode,
  simulatedBalance,
  onTradingModeChange,
  onSimulatedBalanceChange,
  onNext,
  onBack,
}: TradingModeStepProps) {
  return (
    <div className="max-w-2xl mx-auto py-6">
      <h2 className="text-2xl font-bold text-text mb-2">Trading Mode</h2>
      <p className="text-text-muted mb-6">Choose how you want to trade.</p>

      {/* Trading Mode Selection */}
      <div className="space-y-4 mb-8">
        {TRADING_MODES.map((mode) => {
          const Icon = mode.icon;
          const isSelected = tradingMode === mode.id;
          const isDisabled = mode.locked;

          return (
            <button
              key={mode.id}
              type="button"
              onClick={() => !isDisabled && onTradingModeChange(mode.id)}
              disabled={isDisabled}
              className={`
                w-full p-4 rounded-lg border-2 transition-all text-left relative
                ${
                  isSelected
                    ? 'border-primary bg-primary/5'
                    : isDisabled
                      ? 'border-gray-200 bg-gray-50 opacity-60 cursor-not-allowed'
                      : 'border-border hover:border-gray-300'
                }
              `}
            >
              <div className="flex items-start gap-4">
                <div
                  className={`
                    w-12 h-12 rounded-lg flex items-center justify-center flex-shrink-0
                    ${isSelected ? 'bg-primary/10' : 'bg-gray-100'}
                  `}
                >
                  <Icon
                    className={`w-6 h-6 ${isSelected ? 'text-primary' : 'text-gray-500'}`}
                  />
                </div>

                <div className="flex-1">
                  <div className="flex items-center gap-2">
                    <h3 className="font-medium text-text">{mode.name}</h3>
                    {mode.recommended && (
                      <span className="px-2 py-0.5 bg-green-500/10 text-green-600 text-xs font-medium rounded">
                        Recommended
                      </span>
                    )}
                    {mode.locked && (
                      <span className="flex items-center gap-1 px-2 py-0.5 bg-gray-100 text-gray-500 text-xs font-medium rounded">
                        <Lock className="w-3 h-3" />
                        Locked
                      </span>
                    )}
                  </div>
                  <p className="text-sm text-text-muted mt-1">{mode.description}</p>

                  <ul className="mt-3 grid grid-cols-2 gap-1">
                    {mode.features.map((feature, i) => (
                      <li key={i} className="text-xs text-text-muted flex items-center gap-1">
                        <span className="w-1 h-1 bg-gray-400 rounded-full" />
                        {feature}
                      </li>
                    ))}
                  </ul>
                </div>

                {/* Selection indicator */}
                <div
                  className={`
                    w-5 h-5 rounded-full border-2 flex items-center justify-center flex-shrink-0
                    ${isSelected ? 'border-primary bg-primary' : 'border-gray-300'}
                  `}
                >
                  {isSelected && (
                    <svg className="w-3 h-3 text-white" fill="currentColor" viewBox="0 0 20 20">
                      <path
                        fillRule="evenodd"
                        d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z"
                        clipRule="evenodd"
                      />
                    </svg>
                  )}
                </div>
              </div>
            </button>
          );
        })}
      </div>

      {/* Simulated Balance (only for simulated mode) */}
      {tradingMode === 'simulated' && (
        <div className="bg-surface border border-border rounded-lg p-4 mb-8">
          <label className="block text-sm font-medium text-text mb-3">
            Starting Balance (USD)
          </label>

          {/* Preset buttons */}
          <div className="flex flex-wrap gap-2 mb-4">
            {BALANCE_PRESETS.map((preset) => (
              <button
                key={preset}
                type="button"
                onClick={() => onSimulatedBalanceChange(preset)}
                className={`
                  px-3 py-1.5 rounded text-sm font-medium transition-colors
                  ${
                    simulatedBalance === preset
                      ? 'bg-primary text-white'
                      : 'bg-gray-100 text-text hover:bg-gray-200'
                  }
                `}
              >
                ${preset.toLocaleString()}
              </button>
            ))}
          </div>

          {/* Custom input */}
          <div className="flex items-center gap-2">
            <span className="text-text-muted">$</span>
            <input
              type="number"
              value={simulatedBalance}
              onChange={(e) => onSimulatedBalanceChange(parseInt(e.target.value) || 0)}
              min={100}
              max={1000000}
              className="flex-1 px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
          </div>
          <p className="text-xs text-text-muted mt-2">
            This is your starting virtual balance for paper trading.
          </p>
        </div>
      )}

      {/* Live Trading Info */}
      {tradingMode === 'live' && (
        <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-4 mb-8">
          <div className="flex items-start gap-3">
            <Lock className="w-5 h-5 text-yellow-600 flex-shrink-0 mt-0.5" />
            <div className="text-sm">
              <p className="font-medium text-yellow-700">Live Trading Locked</p>
              <p className="text-yellow-600 mt-1">
                Live trading requires completing the security education module and
                verifying your identity. You can unlock it later from Settings.
              </p>
            </div>
          </div>
        </div>
      )}

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
