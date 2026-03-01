import { AlertTriangle, Check, X } from 'lucide-react';

export function ApiSecurityStep() {
  return (
    <div>
      <h1 className="text-2xl font-bold text-text mb-4 text-center">
        API Key Security
      </h1>

      <p className="text-text-muted mb-8 text-center">
        Your API keys are like passwords to your exchange account.
      </p>

      {/* Critical warning */}
      <div className="bg-danger/10 border border-danger/20 rounded-xl p-4 mb-6">
        <div className="flex items-start gap-3">
          <AlertTriangle className="w-5 h-5 text-danger flex-shrink-0 mt-0.5" />
          <div>
            <h3 className="font-semibold text-danger">
              CRITICAL: Never share your API keys
            </h3>
            <ul className="mt-2 space-y-1 text-sm text-text-muted">
              <li>VeloZ staff will NEVER ask for your keys</li>
              <li>Do not post keys in forums or support tickets</li>
              <li>Do not store keys in plain text files</li>
            </ul>
          </div>
        </div>
      </div>

      {/* Best practices */}
      <div className="bg-background-secondary rounded-xl p-6">
        <h3 className="font-semibold text-text mb-4">Best Practices</h3>

        <div className="space-y-4">
          <div className="flex items-start gap-3">
            <div className="w-6 h-6 rounded-full bg-success/10 flex items-center justify-center flex-shrink-0">
              <Check className="w-4 h-4 text-success" />
            </div>
            <div>
              <h4 className="font-medium text-text">
                Enable IP whitelist on your exchange
              </h4>
              <p className="text-sm text-text-muted">
                Only allow connections from your IP
              </p>
            </div>
          </div>

          <div className="flex items-start gap-3">
            <div className="w-6 h-6 rounded-full bg-success/10 flex items-center justify-center flex-shrink-0">
              <Check className="w-4 h-4 text-success" />
            </div>
            <div>
              <h4 className="font-medium text-text">
                Disable withdrawal permissions
              </h4>
              <p className="text-sm text-text-muted">
                VeloZ never needs to withdraw funds
              </p>
            </div>
          </div>

          <div className="flex items-start gap-3">
            <div className="w-6 h-6 rounded-full bg-success/10 flex items-center justify-center flex-shrink-0">
              <Check className="w-4 h-4 text-success" />
            </div>
            <div>
              <h4 className="font-medium text-text">
                Use separate API keys for VeloZ
              </h4>
              <p className="text-sm text-text-muted">
                Don't reuse keys from other services
              </p>
            </div>
          </div>

          <div className="flex items-start gap-3">
            <div className="w-6 h-6 rounded-full bg-success/10 flex items-center justify-center flex-shrink-0">
              <Check className="w-4 h-4 text-success" />
            </div>
            <div>
              <h4 className="font-medium text-text">Rotate keys periodically</h4>
              <p className="text-sm text-text-muted">
                Change keys every 3-6 months
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Permission guide */}
      <div className="mt-6 bg-background-secondary rounded-xl p-6">
        <h3 className="font-semibold text-text mb-4">
          Required API Permissions
        </h3>

        <div className="space-y-3">
          <div className="flex items-center justify-between p-3 bg-background rounded-lg">
            <div className="flex items-center gap-2">
              <Check className="w-4 h-4 text-success" />
              <span className="text-text">Read Account</span>
            </div>
            <span className="text-xs text-success bg-success/10 px-2 py-1 rounded">
              Required
            </span>
          </div>

          <div className="flex items-center justify-between p-3 bg-background rounded-lg">
            <div className="flex items-center gap-2">
              <Check className="w-4 h-4 text-success" />
              <span className="text-text">Spot Trading</span>
            </div>
            <span className="text-xs text-success bg-success/10 px-2 py-1 rounded">
              Required
            </span>
          </div>

          <div className="flex items-center justify-between p-3 bg-background rounded-lg border border-danger/20">
            <div className="flex items-center gap-2">
              <X className="w-4 h-4 text-danger" />
              <span className="text-text">Withdrawal</span>
            </div>
            <span className="text-xs text-danger bg-danger/10 px-2 py-1 rounded">
              NEVER Enable
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}
