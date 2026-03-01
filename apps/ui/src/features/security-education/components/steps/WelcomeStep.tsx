import { Shield, Key, AlertTriangle, TestTube } from 'lucide-react';

export function WelcomeStep() {
  return (
    <div className="text-center">
      <div className="flex justify-center mb-6">
        <div className="w-20 h-20 rounded-full bg-primary/10 flex items-center justify-center">
          <Shield className="w-10 h-10 text-primary" />
        </div>
      </div>

      <h1 className="text-2xl font-bold text-text mb-4">
        Your Security Matters
      </h1>

      <p className="text-text-muted mb-8 max-w-lg mx-auto">
        Before you start trading, let's make sure you understand how to keep
        your account and funds safe.
      </p>

      <div className="bg-background-secondary rounded-xl p-6 mb-8">
        <p className="text-sm text-text-muted mb-4">This quick guide covers:</p>

        <div className="space-y-4">
          <div className="flex items-start gap-4 text-left">
            <div className="w-10 h-10 rounded-lg bg-primary/10 flex items-center justify-center flex-shrink-0">
              <Key className="w-5 h-5 text-primary" />
            </div>
            <div>
              <h3 className="font-medium text-text">API Key Security</h3>
              <p className="text-sm text-text-muted">
                Protect your exchange credentials
              </p>
            </div>
          </div>

          <div className="flex items-start gap-4 text-left">
            <div className="w-10 h-10 rounded-lg bg-warning/10 flex items-center justify-center flex-shrink-0">
              <AlertTriangle className="w-5 h-5 text-warning" />
            </div>
            <div>
              <h3 className="font-medium text-text">Risk Awareness</h3>
              <p className="text-sm text-text-muted">
                Understand trading risks
              </p>
            </div>
          </div>

          <div className="flex items-start gap-4 text-left">
            <div className="w-10 h-10 rounded-lg bg-simulated/10 flex items-center justify-center flex-shrink-0">
              <TestTube className="w-5 h-5 text-simulated" />
            </div>
            <div>
              <h3 className="font-medium text-text">Trading Modes</h3>
              <p className="text-sm text-text-muted">
                Simulated vs. Live trading
              </p>
            </div>
          </div>
        </div>
      </div>

      <div className="text-sm text-text-muted">
        Estimated time: 3 minutes
      </div>
    </div>
  );
}
