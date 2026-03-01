import { useEffect, useState } from 'react';
import { useSecurityEducationStore } from '../store';

interface OnboardingGuardResult {
  shouldShowOnboarding: boolean;
  isOnboardingComplete: boolean;
  canAccessFeature: (feature: 'trading' | 'live-mode' | 'settings') => boolean;
  markOnboardingViewed: () => void;
}

export function useOnboardingGuard(): OnboardingGuardResult {
  const { onboarding, canSwitchToLive } = useSecurityEducationStore();
  const [hasViewedOnboarding, setHasViewedOnboarding] = useState(false);

  // Check if user has seen onboarding before
  useEffect(() => {
    const viewed = localStorage.getItem('veloz-onboarding-viewed');
    if (viewed === 'true') {
      setHasViewedOnboarding(true);
    }
  }, []);

  const shouldShowOnboarding =
    !onboarding.isComplete && !hasViewedOnboarding;

  const canAccessFeature = (
    feature: 'trading' | 'live-mode' | 'settings'
  ): boolean => {
    switch (feature) {
      case 'trading':
        // Allow trading in simulated mode even without completing onboarding
        return true;
      case 'live-mode':
        // Require full onboarding completion for live trading
        return canSwitchToLive();
      case 'settings':
        // Always allow access to settings
        return true;
      default:
        return true;
    }
  };

  const markOnboardingViewed = () => {
    localStorage.setItem('veloz-onboarding-viewed', 'true');
    setHasViewedOnboarding(true);
  };

  return {
    shouldShowOnboarding,
    isOnboardingComplete: onboarding.isComplete,
    canAccessFeature,
    markOnboardingViewed,
  };
}
