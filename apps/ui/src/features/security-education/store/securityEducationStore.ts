import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import type {
  SecurityEducationState,
  OnboardingStep,
  TradingMode,
  SafetyChecklistItem,
  LossLimitConfig,
  AnomalyType,
} from '../types';

// Default safety checklist items
const defaultSafetyChecklist: SafetyChecklistItem[] = [
  {
    id: 'risk-understanding',
    label: 'I understand that trading involves risk and I may lose money',
    checked: false,
    required: true,
  },
  {
    id: 'no-withdrawal',
    label: 'I will not enable withdrawal permissions on my API keys',
    checked: false,
    required: true,
  },
  {
    id: 'start-simulated',
    label: 'I will start with simulated trading before using real funds',
    checked: false,
    required: true,
  },
  {
    id: 'risk-limits',
    label: 'I will set appropriate risk limits before trading live',
    checked: false,
    required: true,
  },
  {
    id: 'affordable-loss',
    label: 'I will only trade with money I can afford to lose',
    checked: false,
    required: true,
  },
];

// Default loss limits
const defaultLossLimits: LossLimitConfig = {
  dailyLimitPercent: 5,
  dailyLimitAmount: 500,
  weeklyLimitPercent: 15,
  weeklyLimitAmount: 1500,
  enabled: true,
};

// Onboarding step order
const ONBOARDING_STEPS: OnboardingStep[] = [
  'welcome',
  'api-security',
  'risk-awareness',
  'trading-modes',
  'safety-checklist',
];

// Requirements for live trading
const SIMULATED_TRADES_REQUIRED = 10;
const SIMULATED_DAYS_REQUIRED = 7;

interface SecurityEducationActions {
  // Onboarding actions
  startOnboarding: () => void;
  completeStep: (step: OnboardingStep) => void;
  goToStep: (step: OnboardingStep) => void;
  goToPreviousStep: () => void;
  goToNextStep: () => void;
  resetOnboarding: () => void;

  // Safety checklist actions
  toggleChecklistItem: (id: string) => void;
  isChecklistComplete: () => boolean;

  // Trading mode actions
  setTradingMode: (mode: TradingMode) => void;
  canSwitchToLive: () => boolean;
  recordSimulatedTrade: () => void;
  recordSimulatedDay: () => void;

  // Loss limit actions
  updateLossLimits: (limits: Partial<LossLimitConfig>) => void;
  recordLoss: (amount: number, accountBalance: number) => void;
  resetDailyLoss: () => void;
  resetWeeklyLoss: () => void;
  isLossLimitReached: () => { daily: boolean; weekly: boolean };
  getLossLimitProgress: () => { daily: number; weekly: number };

  // Anomaly detection actions
  addAnomalyAlert: (type: AnomalyType, severity: 'warning' | 'critical', message: string, details: string) => void;
  acknowledgeAlert: (id: string) => void;
  clearAlerts: () => void;
  setAnomalyDetectionEnabled: (enabled: boolean) => void;

  // Emergency stop actions
  activateEmergencyStop: (reason: string, cancelledOrders?: number) => void;
  deactivateEmergencyStop: () => void;
  isEmergencyStopActive: () => boolean;
}

type SecurityEducationStore = SecurityEducationState & SecurityEducationActions;

export const useSecurityEducationStore = create<SecurityEducationStore>()(
  persist(
    (set, get) => ({
      // Initial state
      onboarding: {
        currentStep: 'welcome',
        completedSteps: [],
        isComplete: false,
        startedAt: null,
        completedAt: null,
      },
      safetyChecklist: defaultSafetyChecklist,
      tradingMode: 'simulated',
      tradingModeLockedUntil: null,
      simulatedTradesCompleted: 0,
      simulatedDaysCompleted: 0,
      lossLimits: defaultLossLimits,
      lossTracking: {
        dailyLoss: 0,
        dailyLossPercent: 0,
        weeklyLoss: 0,
        weeklyLossPercent: 0,
        lastResetDate: new Date().toISOString().split('T')[0],
        weekStartDate: getWeekStartDate(),
      },
      anomalyAlerts: [],
      anomalyDetectionEnabled: true,
      emergencyStop: {
        isActive: false,
        activatedAt: null,
        activatedBy: null,
        reason: null,
        cancelledOrders: 0,
      },

      // Onboarding actions
      startOnboarding: () =>
        set((state) => ({
          onboarding: {
            ...state.onboarding,
            startedAt: new Date().toISOString(),
          },
        })),

      completeStep: (step) =>
        set((state) => {
          const completedSteps = state.onboarding.completedSteps.includes(step)
            ? state.onboarding.completedSteps
            : [...state.onboarding.completedSteps, step];

          const isComplete = ONBOARDING_STEPS.every((s) =>
            completedSteps.includes(s)
          );

          return {
            onboarding: {
              ...state.onboarding,
              completedSteps,
              isComplete,
              completedAt: isComplete ? new Date().toISOString() : null,
            },
          };
        }),

      goToStep: (step) =>
        set((state) => ({
          onboarding: {
            ...state.onboarding,
            currentStep: step,
          },
        })),

      goToPreviousStep: () =>
        set((state) => {
          const currentIndex = ONBOARDING_STEPS.indexOf(
            state.onboarding.currentStep
          );
          if (currentIndex > 0) {
            return {
              onboarding: {
                ...state.onboarding,
                currentStep: ONBOARDING_STEPS[currentIndex - 1],
              },
            };
          }
          return state;
        }),

      goToNextStep: () =>
        set((state) => {
          const currentIndex = ONBOARDING_STEPS.indexOf(
            state.onboarding.currentStep
          );
          if (currentIndex < ONBOARDING_STEPS.length - 1) {
            return {
              onboarding: {
                ...state.onboarding,
                currentStep: ONBOARDING_STEPS[currentIndex + 1],
              },
            };
          }
          return state;
        }),

      resetOnboarding: () =>
        set({
          onboarding: {
            currentStep: 'welcome',
            completedSteps: [],
            isComplete: false,
            startedAt: null,
            completedAt: null,
          },
          safetyChecklist: defaultSafetyChecklist,
        }),

      // Safety checklist actions
      toggleChecklistItem: (id) =>
        set((state) => ({
          safetyChecklist: state.safetyChecklist.map((item) =>
            item.id === id ? { ...item, checked: !item.checked } : item
          ),
        })),

      isChecklistComplete: () => {
        const state = get();
        return state.safetyChecklist
          .filter((item) => item.required)
          .every((item) => item.checked);
      },

      // Trading mode actions
      setTradingMode: (mode) =>
        set((state) => {
          // Cannot switch to live if requirements not met
          if (mode === 'live' && !get().canSwitchToLive()) {
            return state;
          }
          return { tradingMode: mode };
        }),

      canSwitchToLive: () => {
        const state = get();
        return (
          state.onboarding.isComplete &&
          state.simulatedTradesCompleted >= SIMULATED_TRADES_REQUIRED &&
          state.simulatedDaysCompleted >= SIMULATED_DAYS_REQUIRED &&
          get().isChecklistComplete()
        );
      },

      recordSimulatedTrade: () =>
        set((state) => ({
          simulatedTradesCompleted: state.simulatedTradesCompleted + 1,
        })),

      recordSimulatedDay: () =>
        set((state) => ({
          simulatedDaysCompleted: Math.min(
            state.simulatedDaysCompleted + 1,
            SIMULATED_DAYS_REQUIRED
          ),
        })),

      // Loss limit actions
      updateLossLimits: (limits) =>
        set((state) => ({
          lossLimits: { ...state.lossLimits, ...limits },
        })),

      recordLoss: (amount, accountBalance) =>
        set((state) => {
          const newDailyLoss = state.lossTracking.dailyLoss + amount;
          const newWeeklyLoss = state.lossTracking.weeklyLoss + amount;
          const dailyPercent = (newDailyLoss / accountBalance) * 100;
          const weeklyPercent = (newWeeklyLoss / accountBalance) * 100;

          return {
            lossTracking: {
              ...state.lossTracking,
              dailyLoss: newDailyLoss,
              dailyLossPercent: dailyPercent,
              weeklyLoss: newWeeklyLoss,
              weeklyLossPercent: weeklyPercent,
            },
          };
        }),

      resetDailyLoss: () =>
        set((state) => ({
          lossTracking: {
            ...state.lossTracking,
            dailyLoss: 0,
            dailyLossPercent: 0,
            lastResetDate: new Date().toISOString().split('T')[0],
          },
        })),

      resetWeeklyLoss: () =>
        set((state) => ({
          lossTracking: {
            ...state.lossTracking,
            weeklyLoss: 0,
            weeklyLossPercent: 0,
            weekStartDate: getWeekStartDate(),
          },
        })),

      isLossLimitReached: () => {
        const state = get();
        return {
          daily:
            state.lossLimits.enabled &&
            state.lossTracking.dailyLossPercent >= state.lossLimits.dailyLimitPercent,
          weekly:
            state.lossLimits.enabled &&
            state.lossTracking.weeklyLossPercent >= state.lossLimits.weeklyLimitPercent,
        };
      },

      getLossLimitProgress: () => {
        const state = get();
        return {
          daily: Math.min(
            (state.lossTracking.dailyLossPercent / state.lossLimits.dailyLimitPercent) * 100,
            100
          ),
          weekly: Math.min(
            (state.lossTracking.weeklyLossPercent / state.lossLimits.weeklyLimitPercent) * 100,
            100
          ),
        };
      },

      // Anomaly detection actions
      addAnomalyAlert: (type, severity, message, details) =>
        set((state) => ({
          anomalyAlerts: [
            ...state.anomalyAlerts,
            {
              id: `${type}-${Date.now()}`,
              type,
              severity,
              message,
              details,
              timestamp: new Date().toISOString(),
              acknowledged: false,
            },
          ],
        })),

      acknowledgeAlert: (id) =>
        set((state) => ({
          anomalyAlerts: state.anomalyAlerts.map((alert) =>
            alert.id === id ? { ...alert, acknowledged: true } : alert
          ),
        })),

      clearAlerts: () => set({ anomalyAlerts: [] }),

      setAnomalyDetectionEnabled: (enabled) =>
        set({ anomalyDetectionEnabled: enabled }),

      // Emergency stop actions
      activateEmergencyStop: (reason, cancelledOrders = 0) =>
        set({
          emergencyStop: {
            isActive: true,
            activatedAt: new Date().toISOString(),
            activatedBy: 'user',
            reason,
            cancelledOrders,
          },
        }),

      deactivateEmergencyStop: () =>
        set({
          emergencyStop: {
            isActive: false,
            activatedAt: null,
            activatedBy: null,
            reason: null,
            cancelledOrders: 0,
          },
        }),

      isEmergencyStopActive: () => get().emergencyStop.isActive,
    }),
    {
      name: 'veloz-security-education',
      partialize: (state) => ({
        onboarding: state.onboarding,
        safetyChecklist: state.safetyChecklist,
        tradingMode: state.tradingMode,
        tradingModeLockedUntil: state.tradingModeLockedUntil,
        simulatedTradesCompleted: state.simulatedTradesCompleted,
        simulatedDaysCompleted: state.simulatedDaysCompleted,
        lossLimits: state.lossLimits,
        lossTracking: state.lossTracking,
        anomalyDetectionEnabled: state.anomalyDetectionEnabled,
      }),
    }
  )
);

// Helper function to get the start of the current week (Monday)
function getWeekStartDate(): string {
  const now = new Date();
  const day = now.getDay();
  const diff = now.getDate() - day + (day === 0 ? -6 : 1);
  const monday = new Date(now.setDate(diff));
  return monday.toISOString().split('T')[0];
}

// Export constants for use in components
export const ONBOARDING_STEP_ORDER = ONBOARDING_STEPS;
export const REQUIRED_SIMULATED_TRADES = SIMULATED_TRADES_REQUIRED;
export const REQUIRED_SIMULATED_DAYS = SIMULATED_DAYS_REQUIRED;
