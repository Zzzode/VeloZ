import { useState } from 'react';
import { OnboardingWizard } from './components/OnboardingWizard';
import { RiskDashboard } from './components/RiskDashboard';
import { SimulationProgressTracker } from './components/SimulationProgressTracker';
import { AnomalyAlertList } from './components/AnomalyAlert';
import { LossLimitWarning } from './components/LossLimitWarning';
import { useSecurityEducationStore } from './store';
import { useOnboardingGuard } from './hooks';

export default function SecurityEducation() {
  const [showOnboarding, setShowOnboarding] = useState(false);
  const { onboarding, tradingMode } = useSecurityEducationStore();
  const { shouldShowOnboarding, markOnboardingViewed } = useOnboardingGuard();

  // Show onboarding wizard if needed
  if (showOnboarding || (shouldShowOnboarding && !onboarding.isComplete)) {
    return (
      <OnboardingWizard
        onComplete={() => {
          setShowOnboarding(false);
          markOnboardingViewed();
        }}
        onSkip={() => {
          setShowOnboarding(false);
          markOnboardingViewed();
        }}
      />
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-text">Security & Risk</h1>
          <p className="text-text-muted mt-1">
            Manage your trading safety and risk settings
          </p>
        </div>
        {!onboarding.isComplete && (
          <button
            type="button"
            onClick={() => setShowOnboarding(true)}
            className="px-4 py-2 bg-primary text-white rounded-lg hover:bg-primary-dark"
          >
            Complete Security Guide
          </button>
        )}
      </div>

      {/* Alerts section */}
      <AnomalyAlertList maxAlerts={3} />

      {/* Loss limit warning */}
      <LossLimitWarning />

      {/* Main content grid */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Risk Dashboard */}
        <div className="lg:col-span-1">
          <RiskDashboard accountBalance={10000} />
        </div>

        {/* Simulation Progress (only show if not in live mode) */}
        {tradingMode !== 'live' && (
          <div className="lg:col-span-1">
            <SimulationProgressTracker />
          </div>
        )}
      </div>
    </div>
  );
}

// Re-export components and hooks for use in other features
export {
  OnboardingWizard,
  TradingModeBadge,
  ModeSwitchDialog,
  LossLimitWarning,
  EmergencyStopButton,
  RiskDashboard,
  SimulationProgressTracker,
} from './components';
export { AnomalyAlert, AnomalyAlertList } from './components/AnomalyAlert';
export * from './hooks';
export * from './store';
export * from './types';
