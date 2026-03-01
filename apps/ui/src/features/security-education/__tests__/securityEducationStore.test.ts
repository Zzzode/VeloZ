import { describe, it, expect, beforeEach } from 'vitest';
import { act, renderHook } from '@testing-library/react';
import {
  useSecurityEducationStore,
  REQUIRED_SIMULATED_TRADES,
  REQUIRED_SIMULATED_DAYS,
} from '../store';

describe('securityEducationStore', () => {
  beforeEach(() => {
    // Reset store before each test
    const { result } = renderHook(() => useSecurityEducationStore());
    act(() => {
      result.current.resetOnboarding();
      result.current.clearAlerts();
      result.current.deactivateEmergencyStop();
      result.current.resetDailyLoss();
      result.current.resetWeeklyLoss();
    });
  });

  describe('onboarding', () => {
    it('should start with welcome step', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      expect(result.current.onboarding.currentStep).toBe('welcome');
      expect(result.current.onboarding.isComplete).toBe(false);
    });

    it('should track onboarding start time', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.startOnboarding();
      });

      expect(result.current.onboarding.startedAt).not.toBeNull();
    });

    it('should complete steps and track progress', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.completeStep('welcome');
      });

      expect(result.current.onboarding.completedSteps).toContain('welcome');
    });

    it('should navigate between steps', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.goToNextStep();
      });

      expect(result.current.onboarding.currentStep).toBe('api-security');

      act(() => {
        result.current.goToPreviousStep();
      });

      expect(result.current.onboarding.currentStep).toBe('welcome');
    });

    it('should mark onboarding complete when all steps done', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.completeStep('welcome');
        result.current.completeStep('api-security');
        result.current.completeStep('risk-awareness');
        result.current.completeStep('trading-modes');
        result.current.completeStep('safety-checklist');
      });

      expect(result.current.onboarding.isComplete).toBe(true);
      expect(result.current.onboarding.completedAt).not.toBeNull();
    });
  });

  describe('safety checklist', () => {
    it('should toggle checklist items', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      const firstItemId = result.current.safetyChecklist[0].id;

      act(() => {
        result.current.toggleChecklistItem(firstItemId);
      });

      expect(
        result.current.safetyChecklist.find((item) => item.id === firstItemId)?.checked
      ).toBe(true);
    });

    it('should report checklist completion status', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      expect(result.current.isChecklistComplete()).toBe(false);

      act(() => {
        result.current.safetyChecklist.forEach((item) => {
          if (item.required) {
            result.current.toggleChecklistItem(item.id);
          }
        });
      });

      expect(result.current.isChecklistComplete()).toBe(true);
    });
  });

  describe('trading mode', () => {
    it('should start in simulated mode', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      expect(result.current.tradingMode).toBe('simulated');
    });

    it('should not allow switching to live without requirements', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.setTradingMode('live');
      });

      expect(result.current.tradingMode).toBe('simulated');
    });

    it('should track simulated trades', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.recordSimulatedTrade();
        result.current.recordSimulatedTrade();
      });

      expect(result.current.simulatedTradesCompleted).toBe(2);
    });

    it('should track simulated days', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.recordSimulatedDay();
      });

      expect(result.current.simulatedDaysCompleted).toBe(1);
    });

    it('should allow live mode when all requirements met', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      // Complete onboarding
      act(() => {
        result.current.completeStep('welcome');
        result.current.completeStep('api-security');
        result.current.completeStep('risk-awareness');
        result.current.completeStep('trading-modes');
        result.current.completeStep('safety-checklist');
      });

      // Complete checklist
      act(() => {
        result.current.safetyChecklist.forEach((item) => {
          if (item.required && !item.checked) {
            result.current.toggleChecklistItem(item.id);
          }
        });
      });

      // Complete simulated trades
      act(() => {
        for (let i = 0; i < REQUIRED_SIMULATED_TRADES; i++) {
          result.current.recordSimulatedTrade();
        }
      });

      // Complete simulated days
      act(() => {
        for (let i = 0; i < REQUIRED_SIMULATED_DAYS; i++) {
          result.current.recordSimulatedDay();
        }
      });

      expect(result.current.canSwitchToLive()).toBe(true);

      act(() => {
        result.current.setTradingMode('live');
      });

      expect(result.current.tradingMode).toBe('live');
    });
  });

  describe('loss limits', () => {
    it('should track losses', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      const accountBalance = 10000;

      act(() => {
        result.current.recordLoss(100, accountBalance);
      });

      expect(result.current.lossTracking.dailyLoss).toBe(100);
      expect(result.current.lossTracking.dailyLossPercent).toBe(1);
    });

    it('should detect when daily limit is reached', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      const accountBalance = 10000;

      // Default daily limit is 5%
      act(() => {
        result.current.recordLoss(500, accountBalance);
      });

      const limits = result.current.isLossLimitReached();
      expect(limits.daily).toBe(true);
    });

    it('should calculate loss limit progress', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      const accountBalance = 10000;

      act(() => {
        result.current.recordLoss(250, accountBalance); // 2.5% of 5% limit = 50%
      });

      const progress = result.current.getLossLimitProgress();
      expect(progress.daily).toBe(50);
    });

    it('should reset daily loss', () => {
      const { result } = renderHook(() => useSecurityEducationStore());
      const accountBalance = 10000;

      act(() => {
        result.current.recordLoss(100, accountBalance);
        result.current.resetDailyLoss();
      });

      expect(result.current.lossTracking.dailyLoss).toBe(0);
    });
  });

  describe('anomaly alerts', () => {
    it('should add anomaly alerts', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.addAnomalyAlert(
          'unusual-volume',
          'warning',
          'Test message',
          'Test details'
        );
      });

      expect(result.current.anomalyAlerts.length).toBe(1);
      expect(result.current.anomalyAlerts[0].type).toBe('unusual-volume');
    });

    it('should acknowledge alerts', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.addAnomalyAlert(
          'unusual-volume',
          'warning',
          'Test message',
          'Test details'
        );
      });

      const alertId = result.current.anomalyAlerts[0].id;

      act(() => {
        result.current.acknowledgeAlert(alertId);
      });

      expect(result.current.anomalyAlerts[0].acknowledged).toBe(true);
    });

    it('should clear all alerts', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.addAnomalyAlert(
          'unusual-volume',
          'warning',
          'Test message',
          'Test details'
        );
        result.current.clearAlerts();
      });

      expect(result.current.anomalyAlerts.length).toBe(0);
    });
  });

  describe('emergency stop', () => {
    it('should activate emergency stop', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.activateEmergencyStop('Test reason', 5);
      });

      expect(result.current.emergencyStop.isActive).toBe(true);
      expect(result.current.emergencyStop.reason).toBe('Test reason');
      expect(result.current.emergencyStop.cancelledOrders).toBe(5);
    });

    it('should deactivate emergency stop', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      act(() => {
        result.current.activateEmergencyStop('Test reason', 5);
        result.current.deactivateEmergencyStop();
      });

      expect(result.current.emergencyStop.isActive).toBe(false);
      expect(result.current.emergencyStop.reason).toBeNull();
    });

    it('should report emergency stop status', () => {
      const { result } = renderHook(() => useSecurityEducationStore());

      expect(result.current.isEmergencyStopActive()).toBe(false);

      act(() => {
        result.current.activateEmergencyStop('Test reason');
      });

      expect(result.current.isEmergencyStopActive()).toBe(true);
    });
  });
});
