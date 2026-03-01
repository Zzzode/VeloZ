import { TestTube, Zap, Check } from 'lucide-react';

export function TradingModesStep() {
  return (
    <div>
      <h1 className="text-2xl font-bold text-text mb-4 text-center">
        Trading Modes
      </h1>

      <p className="text-text-muted mb-8 text-center">
        VeloZ offers two trading modes to help you learn safely.
      </p>

      {/* Simulated mode */}
      <div className="bg-background-secondary rounded-xl p-6 mb-4">
        <div className="flex items-center gap-3 mb-4">
          <div className="w-10 h-10 rounded-lg bg-simulated/10 flex items-center justify-center">
            <TestTube className="w-5 h-5 text-simulated" />
          </div>
          <div>
            <h3 className="font-semibold text-text">SIMULATED MODE</h3>
            <span className="inline-block mt-1 px-2 py-0.5 text-xs font-medium bg-simulated/10 text-simulated rounded">
              SIMULATED
            </span>
          </div>
        </div>

        <ul className="space-y-2 mb-4">
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Practice with virtual funds
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Real market data, fake orders
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            No risk to your actual balance
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Perfect for testing strategies
          </li>
        </ul>

        <div className="text-xs text-text-muted bg-background rounded-lg p-3">
          <span className="font-medium">Recommended for:</span> Beginners,
          strategy testing
        </div>
      </div>

      {/* Live mode */}
      <div className="bg-background-secondary rounded-xl p-6">
        <div className="flex items-center gap-3 mb-4">
          <div className="w-10 h-10 rounded-lg bg-success/10 flex items-center justify-center">
            <Zap className="w-5 h-5 text-success" />
          </div>
          <div>
            <h3 className="font-semibold text-text">LIVE MODE</h3>
            <span className="inline-block mt-1 px-2 py-0.5 text-xs font-medium bg-success/10 text-success rounded">
              LIVE
            </span>
          </div>
        </div>

        <ul className="space-y-2 mb-4">
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Trade with real funds
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Real orders on the exchange
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Real profits and losses
          </li>
          <li className="flex items-center gap-2 text-sm text-text-muted">
            <Check className="w-4 h-4 text-success" />
            Requires careful risk management
          </li>
        </ul>

        <div className="text-xs text-text-muted bg-background rounded-lg p-3">
          <span className="font-medium">Recommended for:</span> Experienced
          traders
        </div>
      </div>

      {/* Progression note */}
      <div className="mt-6 bg-primary/5 border border-primary/20 rounded-xl p-4">
        <p className="text-sm text-text-muted">
          <span className="font-medium text-primary">Note:</span> You must
          complete at least 10 simulated trades and 7 days of practice before
          you can switch to live trading.
        </p>
      </div>
    </div>
  );
}
