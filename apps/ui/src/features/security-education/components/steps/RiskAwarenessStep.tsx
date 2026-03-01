import { AlertTriangle, TrendingDown, Zap, Clock, Wifi, Info } from 'lucide-react';

export function RiskAwarenessStep() {
  return (
    <div>
      <h1 className="text-2xl font-bold text-text mb-4 text-center">
        Understanding Trading Risks
      </h1>

      {/* Important disclaimer */}
      <div className="bg-warning/10 border border-warning/20 rounded-xl p-4 mb-6">
        <div className="flex items-start gap-3">
          <AlertTriangle className="w-5 h-5 text-warning flex-shrink-0 mt-0.5" />
          <div>
            <h3 className="font-semibold text-warning">Important Disclaimer</h3>
            <p className="mt-1 text-sm text-text-muted">
              Cryptocurrency trading involves significant risk. You can lose some
              or all of your investment.
            </p>
          </div>
        </div>
      </div>

      <p className="text-text-muted mb-6 text-center">
        Key Risks to Understand:
      </p>

      {/* Risk cards */}
      <div className="bg-background-secondary rounded-xl p-6 space-y-4">
        <div className="flex items-start gap-4">
          <div className="w-10 h-10 rounded-lg bg-danger/10 flex items-center justify-center flex-shrink-0">
            <TrendingDown className="w-5 h-5 text-danger" />
          </div>
          <div>
            <h3 className="font-medium text-text">Market Risk</h3>
            <p className="text-sm text-text-muted">
              Prices can move rapidly against your position
            </p>
          </div>
        </div>

        <div className="flex items-start gap-4">
          <div className="w-10 h-10 rounded-lg bg-warning/10 flex items-center justify-center flex-shrink-0">
            <Zap className="w-5 h-5 text-warning" />
          </div>
          <div>
            <h3 className="font-medium text-text">Leverage Risk</h3>
            <p className="text-sm text-text-muted">
              Leverage amplifies both gains AND losses
            </p>
          </div>
        </div>

        <div className="flex items-start gap-4">
          <div className="w-10 h-10 rounded-lg bg-danger/10 flex items-center justify-center flex-shrink-0">
            <Clock className="w-5 h-5 text-danger" />
          </div>
          <div>
            <h3 className="font-medium text-text">Liquidation Risk</h3>
            <p className="text-sm text-text-muted">
              Positions can be forcibly closed at a loss
            </p>
          </div>
        </div>

        <div className="flex items-start gap-4">
          <div className="w-10 h-10 rounded-lg bg-text-muted/10 flex items-center justify-center flex-shrink-0">
            <Wifi className="w-5 h-5 text-text-muted" />
          </div>
          <div>
            <h3 className="font-medium text-text">Technical Risk</h3>
            <p className="text-sm text-text-muted">
              System failures can prevent order execution
            </p>
          </div>
        </div>
      </div>

      {/* Info box */}
      <div className="mt-6 bg-primary/5 border border-primary/20 rounded-xl p-4">
        <div className="flex items-start gap-3">
          <Info className="w-5 h-5 text-primary flex-shrink-0 mt-0.5" />
          <p className="text-sm text-text-muted">
            Only trade with money you can afford to lose. Never invest your
            emergency funds or money needed for essential expenses.
          </p>
        </div>
      </div>
    </div>
  );
}
