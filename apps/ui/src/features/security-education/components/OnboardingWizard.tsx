import { useEffect } from 'react';
import { Shield, Key, AlertTriangle, TestTube, CheckSquare } from 'lucide-react';
import { useSecurityEducationStore, ONBOARDING_STEP_ORDER } from '../store';
import type { OnboardingStep } from '../types';
import { WelcomeStep } from './steps/WelcomeStep';
import { ApiSecurityStep } from './steps/ApiSecurityStep';
import { RiskAwarenessStep } from './steps/RiskAwarenessStep';
import { TradingModesStep } from './steps/TradingModesStep';
import { SafetyChecklistStep } from './steps/SafetyChecklistStep';

interface OnboardingWizardProps {
  onComplete?: () => void;
  onSkip?: () => void;
}

const stepConfig: Record<OnboardingStep, { icon: typeof Shield; label: string }> = {
  welcome: { icon: Shield, label: 'Welcome' },
  'api-security': { icon: Key, label: 'API' },
  'risk-awareness': { icon: AlertTriangle, label: 'Risk' },
  'trading-modes': { icon: TestTube, label: 'Modes' },
  'safety-checklist': { icon: CheckSquare, label: 'Checklist' },
};

export function OnboardingWizard({ onComplete, onSkip }: OnboardingWizardProps) {
  const {
    onboarding,
    startOnboarding,
    completeStep,
    goToNextStep,
    goToPreviousStep,
  } = useSecurityEducationStore();

  useEffect(() => {
    if (!onboarding.startedAt) {
      startOnboarding();
    }
  }, [onboarding.startedAt, startOnboarding]);

  useEffect(() => {
    if (onboarding.isComplete && onComplete) {
      onComplete();
    }
  }, [onboarding.isComplete, onComplete]);

  const handleNext = () => {
    completeStep(onboarding.currentStep);
    goToNextStep();
  };

  const handleBack = () => {
    goToPreviousStep();
  };

  const handleComplete = () => {
    completeStep(onboarding.currentStep);
    if (onComplete) {
      onComplete();
    }
  };

  const currentStepIndex = ONBOARDING_STEP_ORDER.indexOf(onboarding.currentStep);
  const isFirstStep = currentStepIndex === 0;
  const isLastStep = currentStepIndex === ONBOARDING_STEP_ORDER.length - 1;

  const renderStep = () => {
    switch (onboarding.currentStep) {
      case 'welcome':
        return <WelcomeStep />;
      case 'api-security':
        return <ApiSecurityStep />;
      case 'risk-awareness':
        return <RiskAwarenessStep />;
      case 'trading-modes':
        return <TradingModesStep />;
      case 'safety-checklist':
        return <SafetyChecklistStep />;
      default:
        return null;
    }
  };

  return (
    <div className="min-h-screen bg-background flex flex-col">
      {/* Progress indicator */}
      <div className="border-b border-border bg-background-secondary px-6 py-4">
        <div className="max-w-3xl mx-auto">
          <div className="flex items-center justify-between">
            {ONBOARDING_STEP_ORDER.map((step, index) => {
              const config = stepConfig[step];
              const Icon = config.icon;
              const isCompleted = onboarding.completedSteps.includes(step);
              const isCurrent = step === onboarding.currentStep;
              const isPast = index < currentStepIndex;

              return (
                <div key={step} className="flex items-center">
                  <div className="flex flex-col items-center">
                    <div
                      className={`
                        w-10 h-10 rounded-full flex items-center justify-center
                        ${isCompleted || isPast
                          ? 'bg-success text-white'
                          : isCurrent
                          ? 'bg-primary text-white'
                          : 'bg-gray-200 text-text-muted'
                        }
                      `}
                    >
                      <Icon className="w-5 h-5" />
                    </div>
                    <span
                      className={`
                        mt-1 text-xs font-medium
                        ${isCurrent ? 'text-primary' : 'text-text-muted'}
                      `}
                    >
                      {config.label}
                    </span>
                  </div>
                  {index < ONBOARDING_STEP_ORDER.length - 1 && (
                    <div
                      className={`
                        w-16 h-0.5 mx-2
                        ${isPast || isCompleted ? 'bg-success' : 'bg-gray-200'}
                      `}
                    />
                  )}
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* Step content */}
      <div className="flex-1 overflow-y-auto">
        <div className="max-w-3xl mx-auto px-6 py-8">
          {renderStep()}
        </div>
      </div>

      {/* Navigation buttons */}
      <div className="border-t border-border bg-background px-6 py-4">
        <div className="max-w-3xl mx-auto flex items-center justify-between">
          <div>
            {isFirstStep && onSkip && (
              <button
                type="button"
                onClick={onSkip}
                className="text-text-muted hover:text-text text-sm"
              >
                Skip for Now
              </button>
            )}
            {!isFirstStep && (
              <button
                type="button"
                onClick={handleBack}
                className="px-4 py-2 text-text-muted hover:text-text border border-border rounded-lg"
              >
                Back
              </button>
            )}
          </div>
          <div>
            {isLastStep ? (
              <button
                type="button"
                onClick={handleComplete}
                className="px-6 py-2 bg-primary text-white rounded-lg hover:bg-primary-dark transition-colors"
              >
                Complete
              </button>
            ) : (
              <button
                type="button"
                onClick={handleNext}
                className="px-6 py-2 bg-primary text-white rounded-lg hover:bg-primary-dark transition-colors"
              >
                Continue
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
