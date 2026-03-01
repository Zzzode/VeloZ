import { useCallback, useRef } from 'react';
import { useSecurityEducationStore } from '../store';

interface TradeRecord {
  timestamp: number;
  size: number;
  value: number;
  pnl: number;
}

interface AnomalyDetectionConfig {
  volumeThreshold: number; // Trades per hour threshold
  sizeMultiplier: number; // How many times larger than average to flag
  rapidLossPercent: number; // Percent loss in short time to flag
  revengeTradeMultiplier: number; // Size multiplier after loss to flag
  tradingHoursStart: number; // Hour (0-23)
  tradingHoursEnd: number; // Hour (0-23)
}

const defaultConfig: AnomalyDetectionConfig = {
  volumeThreshold: 50,
  sizeMultiplier: 5,
  rapidLossPercent: 20,
  revengeTradeMultiplier: 2,
  tradingHoursStart: 9,
  tradingHoursEnd: 17,
};

export function useAnomalyDetection(config: Partial<AnomalyDetectionConfig> = {}) {
  const { addAnomalyAlert, anomalyDetectionEnabled } = useSecurityEducationStore();

  const mergedConfig = { ...defaultConfig, ...config };
  const tradeHistory = useRef<TradeRecord[]>([]);
  const lastLoss = useRef<{ amount: number; timestamp: number } | null>(null);

  const recordTrade = useCallback(
    (trade: Omit<TradeRecord, 'timestamp'>) => {
      if (!anomalyDetectionEnabled) return;

      const record: TradeRecord = {
        ...trade,
        timestamp: Date.now(),
      };

      tradeHistory.current.push(record);

      // Keep only last 24 hours of trades
      const oneDayAgo = Date.now() - 24 * 60 * 60 * 1000;
      tradeHistory.current = tradeHistory.current.filter(
        (t) => t.timestamp > oneDayAgo
      );

      // Check for anomalies
      checkUnusualVolume();
      checkUnusualSize(trade.value);
      checkRapidLoss();
      checkRevengeTrade(trade.value);
      checkOffHours();

      // Track losses for revenge trading detection
      if (trade.pnl < 0) {
        lastLoss.current = {
          amount: Math.abs(trade.pnl),
          timestamp: Date.now(),
        };
      }
    },
    [anomalyDetectionEnabled]
  );

  const checkUnusualVolume = useCallback(() => {
    const oneHourAgo = Date.now() - 60 * 60 * 1000;
    const recentTrades = tradeHistory.current.filter(
      (t) => t.timestamp > oneHourAgo
    );

    if (recentTrades.length > mergedConfig.volumeThreshold) {
      addAnomalyAlert(
        'unusual-volume',
        'warning',
        'Unusual trading volume detected',
        `You have made ${recentTrades.length} trades in the last hour, which is above the threshold of ${mergedConfig.volumeThreshold}.`
      );
    }
  }, [addAnomalyAlert, mergedConfig.volumeThreshold]);

  const checkUnusualSize = useCallback(
    (tradeValue: number) => {
      if (tradeHistory.current.length < 10) return;

      const avgValue =
        tradeHistory.current.reduce((sum, t) => sum + t.value, 0) /
        tradeHistory.current.length;

      if (tradeValue > avgValue * mergedConfig.sizeMultiplier) {
        addAnomalyAlert(
          'unusual-size',
          'warning',
          'Unusual trade size detected',
          `This trade ($${tradeValue.toFixed(2)}) is ${(tradeValue / avgValue).toFixed(1)}x larger than your average trade size ($${avgValue.toFixed(2)}).`
        );
      }
    },
    [addAnomalyAlert, mergedConfig.sizeMultiplier]
  );

  const checkRapidLoss = useCallback(() => {
    const oneHourAgo = Date.now() - 60 * 60 * 1000;
    const recentTrades = tradeHistory.current.filter(
      (t) => t.timestamp > oneHourAgo
    );

    const totalLoss = recentTrades
      .filter((t) => t.pnl < 0)
      .reduce((sum, t) => sum + Math.abs(t.pnl), 0);

    const totalValue = recentTrades.reduce((sum, t) => sum + t.value, 0);

    if (totalValue > 0) {
      const lossPercent = (totalLoss / totalValue) * 100;
      if (lossPercent > mergedConfig.rapidLossPercent) {
        addAnomalyAlert(
          'rapid-loss',
          'critical',
          'Rapid loss detected',
          `You have lost ${lossPercent.toFixed(1)}% of your traded value in the last hour. Consider pausing to review your strategy.`
        );
      }
    }
  }, [addAnomalyAlert, mergedConfig.rapidLossPercent]);

  const checkRevengeTrade = useCallback(
    (tradeValue: number) => {
      if (!lastLoss.current) return;

      const timeSinceLoss = Date.now() - lastLoss.current.timestamp;
      const fiveMinutes = 5 * 60 * 1000;

      if (
        timeSinceLoss < fiveMinutes &&
        tradeValue > lastLoss.current.amount * mergedConfig.revengeTradeMultiplier
      ) {
        addAnomalyAlert(
          'revenge-trading',
          'critical',
          'Potential revenge trading detected',
          `You placed a large trade ($${tradeValue.toFixed(2)}) shortly after a loss. This pattern can lead to emotional trading decisions.`
        );
      }
    },
    [addAnomalyAlert, mergedConfig.revengeTradeMultiplier]
  );

  const checkOffHours = useCallback(() => {
    const now = new Date();
    const hour = now.getHours();

    if (
      hour < mergedConfig.tradingHoursStart ||
      hour >= mergedConfig.tradingHoursEnd
    ) {
      addAnomalyAlert(
        'off-hours',
        'warning',
        'Off-hours trading',
        `You are trading outside your configured trading hours (${mergedConfig.tradingHoursStart}:00 - ${mergedConfig.tradingHoursEnd}:00).`
      );
    }
  }, [
    addAnomalyAlert,
    mergedConfig.tradingHoursStart,
    mergedConfig.tradingHoursEnd,
  ]);

  const clearHistory = useCallback(() => {
    tradeHistory.current = [];
    lastLoss.current = null;
  }, []);

  return {
    recordTrade,
    clearHistory,
    tradeCount: tradeHistory.current.length,
  };
}
