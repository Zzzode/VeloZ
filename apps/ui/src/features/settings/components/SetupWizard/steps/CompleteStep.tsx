import { Button } from '@/shared/components';
import { CheckCircle, FlaskConical, Zap, BookOpen, Settings } from 'lucide-react';

interface CompleteStepProps {
  tradingMode: 'simulated' | 'paper' | 'live';
  onStartSimulated: () => void;
  onStartLive: () => void;
  onExploreStrategies: () => void;
  onViewDocs: () => void;
}

export function CompleteStep({
  tradingMode,
  onStartSimulated,
  onStartLive,
  onExploreStrategies,
  onViewDocs,
}: CompleteStepProps) {
  return (
    <div className="flex flex-col items-center text-center max-w-2xl mx-auto py-8">
      {/* Success Icon */}
      <div className="w-20 h-20 bg-green-500/10 rounded-full flex items-center justify-center mb-6">
        <CheckCircle className="w-12 h-12 text-green-500" />
      </div>

      <h1 className="text-3xl font-bold text-text mb-2">You're All Set!</h1>
      <p className="text-text-muted mb-8">
        Your VeloZ trading platform is ready to use.
      </p>

      {/* Start Options */}
      <div className="w-full space-y-4 mb-8">
        {/* Simulated Mode */}
        <button
          onClick={onStartSimulated}
          className="w-full p-6 rounded-lg border-2 border-primary bg-primary/5 hover:bg-primary/10 transition-all text-left"
        >
          <div className="flex items-start gap-4">
            <div className="w-12 h-12 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <FlaskConical className="w-6 h-6 text-primary" />
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2">
                <h3 className="font-semibold text-text">Start in Simulated Mode</h3>
                <span className="px-2 py-0.5 bg-green-500/10 text-green-600 text-xs font-medium rounded">
                  Recommended
                </span>
              </div>
              <p className="text-sm text-text-muted mt-1">
                Practice with paper trading first. Perfect for beginners and testing
                strategies.
              </p>
            </div>
            <Button variant="primary" size="sm">
              Start Simulated
            </Button>
          </div>
        </button>

        {/* Live Mode */}
        <button
          onClick={onStartLive}
          disabled={tradingMode !== 'live'}
          className={`
            w-full p-6 rounded-lg border-2 transition-all text-left
            ${
              tradingMode === 'live'
                ? 'border-border hover:border-gray-300'
                : 'border-gray-200 bg-gray-50 opacity-60 cursor-not-allowed'
            }
          `}
        >
          <div className="flex items-start gap-4">
            <div className="w-12 h-12 bg-gray-100 rounded-lg flex items-center justify-center flex-shrink-0">
              <Zap className="w-6 h-6 text-gray-500" />
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2">
                <h3 className="font-semibold text-text">Start in Live Mode</h3>
                {tradingMode !== 'live' && (
                  <span className="px-2 py-0.5 bg-gray-100 text-gray-500 text-xs font-medium rounded">
                    Locked
                  </span>
                )}
              </div>
              <p className="text-sm text-text-muted mt-1">
                Trade with real funds immediately. For experienced traders only.
              </p>
            </div>
            <Button variant="secondary" size="sm" disabled={tradingMode !== 'live'}>
              Start Live
            </Button>
          </div>
        </button>
      </div>

      {/* Quick Links */}
      <div className="w-full grid grid-cols-2 gap-4 mb-8">
        <button
          onClick={onExploreStrategies}
          className="p-4 rounded-lg border border-border hover:border-gray-300 transition-all text-left"
        >
          <div className="flex items-center gap-3">
            <Settings className="w-5 h-5 text-text-muted" />
            <span className="text-sm font-medium text-text">Explore Strategies</span>
          </div>
        </button>

        <button
          onClick={onViewDocs}
          className="p-4 rounded-lg border border-border hover:border-gray-300 transition-all text-left"
        >
          <div className="flex items-center gap-3">
            <BookOpen className="w-5 h-5 text-text-muted" />
            <span className="text-sm font-medium text-text">View Documentation</span>
          </div>
        </button>
      </div>

      {/* Setup Summary */}
      <div className="w-full bg-surface border border-border rounded-lg p-4">
        <h3 className="font-medium text-text mb-3 text-left">Setup Summary</h3>
        <div className="space-y-2 text-sm">
          <div className="flex items-center justify-between">
            <span className="text-text-muted">Exchange Connected</span>
            <span className="flex items-center gap-1 text-green-600">
              <CheckCircle className="w-4 h-4" />
              Yes
            </span>
          </div>
          <div className="flex items-center justify-between">
            <span className="text-text-muted">Risk Limits Configured</span>
            <span className="flex items-center gap-1 text-green-600">
              <CheckCircle className="w-4 h-4" />
              Yes
            </span>
          </div>
          <div className="flex items-center justify-between">
            <span className="text-text-muted">Trading Mode</span>
            <span className="text-text font-medium capitalize">{tradingMode}</span>
          </div>
        </div>
      </div>

      {/* Help Text */}
      <p className="text-xs text-text-muted mt-6">
        You can change these settings anytime from the Settings page.
      </p>
    </div>
  );
}
