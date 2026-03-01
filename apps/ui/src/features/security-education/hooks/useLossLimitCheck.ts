import { useCallback, useEffect } from 'react';
import { useSecurityEducationStore } from '../store';

interface LossLimitCheckResult {
  canTrade: boolean;
  dailyLimitReached: boolean;
  weeklyLimitReached: boolean;
  dailyProgress: number;
  weeklyProgress: number;
  warningLevel: 'none' | 'warning' | 'critical' | 'blocked';
}

export function useLossLimitCheck(accountBalance: number) {
  const {
    lossLimits,
    lossTracking,
    getLossLimitProgress,
    isLossLimitReached,
    recordLoss,
    resetDailyLoss,
    resetWeeklyLoss,
  } = useSecurityEducationStore();

  // Check if we need to reset daily/weekly limits
  useEffect(() => {
    const today = new Date().toISOString().split('T')[0];
    if (lossTracking.lastResetDate !== today) {
      resetDailyLoss();
    }

    // Check weekly reset (Monday)
    const now = new Date();
    const day = now.getDay();
    const diff = now.getDate() - day + (day === 0 ? -6 : 1);
    const monday = new Date(now.setDate(diff));
    const weekStart = monday.toISOString().split('T')[0];

    if (lossTracking.weekStartDate !== weekStart) {
      resetWeeklyLoss();
    }
  }, [lossTracking.lastResetDate, lossTracking.weekStartDate, resetDailyLoss, resetWeeklyLoss]);

  const checkLimits = useCallback((): LossLimitCheckResult => {
    const limits = isLossLimitReached();
    const progress = getLossLimitProgress();

    let warningLevel: LossLimitCheckResult['warningLevel'] = 'none';

    if (limits.daily || limits.weekly) {
      warningLevel = 'blocked';
    } else if (progress.daily >= 90 || progress.weekly >= 90) {
      warningLevel = 'critical';
    } else if (progress.daily >= 80 || progress.weekly >= 80) {
      warningLevel = 'warning';
    }

    return {
      canTrade: !limits.daily && !limits.weekly,
      dailyLimitReached: limits.daily,
      weeklyLimitReached: limits.weekly,
      dailyProgress: progress.daily,
      weeklyProgress: progress.weekly,
      warningLevel,
    };
  }, [getLossLimitProgress, isLossLimitReached]);

  const recordTradeLoss = useCallback(
    (lossAmount: number) => {
      if (lossAmount > 0) {
        recordLoss(lossAmount, accountBalance);
      }
    },
    [accountBalance, recordLoss]
  );

  const getRemainingLoss = useCallback(() => {
    const dailyRemaining =
      (lossLimits.dailyLimitPercent - lossTracking.dailyLossPercent) / 100 *
      accountBalance;
    const weeklyRemaining =
      (lossLimits.weeklyLimitPercent - lossTracking.weeklyLossPercent) / 100 *
      accountBalance;

    return {
      daily: Math.max(0, dailyRemaining),
      weekly: Math.max(0, weeklyRemaining),
    };
  }, [
    accountBalance,
    lossLimits.dailyLimitPercent,
    lossLimits.weeklyLimitPercent,
    lossTracking.dailyLossPercent,
    lossTracking.weeklyLossPercent,
  ]);

  return {
    checkLimits,
    recordTradeLoss,
    getRemainingLoss,
    lossLimits,
    lossTracking,
  };
}
