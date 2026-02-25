import { Button } from '@/shared/components';
import { ArrowLeftRight, ShieldCheck, BadgeCheck } from 'lucide-react';

interface WelcomeStepProps {
  termsAccepted: boolean;
  onTermsChange: (accepted: boolean) => void;
  onNext: () => void;
  onSkip: () => void;
}

export function WelcomeStep({
  termsAccepted,
  onTermsChange,
  onNext,
  onSkip,
}: WelcomeStepProps) {
  return (
    <div className="flex flex-col items-center text-center max-w-2xl mx-auto py-8">
      {/* Logo */}
      <div className="w-20 h-20 bg-primary rounded-2xl flex items-center justify-center mb-6">
        <span className="text-white text-3xl font-bold">V</span>
      </div>

      <h1 className="text-3xl font-bold text-text mb-2">Welcome to VeloZ</h1>
      <p className="text-text-muted mb-8">
        Let's get your trading platform configured. This wizard will guide you through
        the essential setup steps.
      </p>

      {/* What we'll configure */}
      <div className="w-full bg-surface border border-border rounded-lg p-6 mb-8">
        <h2 className="text-lg font-semibold text-text mb-4">What we'll configure:</h2>
        <div className="space-y-4">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <ArrowLeftRight className="w-5 h-5 text-primary" />
            </div>
            <div className="text-left">
              <h3 className="font-medium text-text">Connect Your Exchange</h3>
              <p className="text-sm text-text-muted">
                Link your Binance, OKX, Bybit, or Coinbase account
              </p>
            </div>
          </div>

          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <ShieldCheck className="w-5 h-5 text-primary" />
            </div>
            <div className="text-left">
              <h3 className="font-medium text-text">Set Risk Parameters</h3>
              <p className="text-sm text-text-muted">
                Configure position limits and safety controls
              </p>
            </div>
          </div>

          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <BadgeCheck className="w-5 h-5 text-primary" />
            </div>
            <div className="text-left">
              <h3 className="font-medium text-text">Test Connection</h3>
              <p className="text-sm text-text-muted">
                Verify everything works before you start trading
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Estimated time */}
      <p className="text-sm text-text-muted mb-6">
        Estimated time: <span className="font-medium">5 minutes</span>
      </p>

      {/* Terms acceptance */}
      <div className="w-full bg-blue-500/10 border border-blue-500/30 rounded-lg p-4 mb-8">
        <label className="flex items-start gap-3 cursor-pointer">
          <input
            type="checkbox"
            checked={termsAccepted}
            onChange={(e) => onTermsChange(e.target.checked)}
            className="mt-1 w-5 h-5 rounded border-gray-300 text-primary focus:ring-primary"
          />
          <span className="text-sm text-left text-text">
            I understand that VeloZ is a trading tool and that trading cryptocurrencies
            involves significant risk. I accept the{' '}
            <a href="#" className="text-primary hover:underline">
              Terms of Service
            </a>{' '}
            and{' '}
            <a href="#" className="text-primary hover:underline">
              Privacy Policy
            </a>
            .
          </span>
        </label>
      </div>

      {/* Actions */}
      <div className="flex items-center gap-4 w-full">
        <Button variant="secondary" onClick={onSkip} className="flex-1">
          Skip for Now
        </Button>
        <Button
          variant="primary"
          onClick={onNext}
          disabled={!termsAccepted}
          className="flex-1"
        >
          Get Started
        </Button>
      </div>

      {/* Skip info */}
      <p className="text-xs text-text-muted mt-4">
        You can skip this wizard and configure later from Settings
      </p>
    </div>
  );
}
