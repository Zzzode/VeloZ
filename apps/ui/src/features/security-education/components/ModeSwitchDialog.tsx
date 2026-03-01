import { useState } from 'react';
import { AlertTriangle, Check } from 'lucide-react';
import { Modal } from '@/shared/components';
import type { TradingMode } from '../types';
import { useSecurityEducationStore } from '../store';

interface ModeSwitchDialogProps {
  isOpen: boolean;
  onClose: () => void;
  targetMode: TradingMode;
  onConfirm: () => void;
  riskSettings?: {
    maxPositionPercent: number;
    dailyLossLimit: number;
    maxLeverage: number;
  };
}

export function ModeSwitchDialog({
  isOpen,
  onClose,
  targetMode,
  onConfirm,
  riskSettings = {
    maxPositionPercent: 10,
    dailyLossLimit: 5,
    maxLeverage: 5,
  },
}: ModeSwitchDialogProps) {
  const [acknowledged, setAcknowledged] = useState(false);
  const { canSwitchToLive } = useSecurityEducationStore();

  const isLiveMode = targetMode === 'live';
  const canSwitch = !isLiveMode || canSwitchToLive();

  const handleConfirm = () => {
    if (acknowledged || !isLiveMode) {
      onConfirm();
      onClose();
      setAcknowledged(false);
    }
  };

  const handleClose = () => {
    setAcknowledged(false);
    onClose();
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={handleClose}
      title={
        isLiveMode ? 'Switch to Live Trading?' : `Switch to ${targetMode} Mode?`
      }
      size="md"
      closeOnOverlayClick={false}
    >
      {isLiveMode ? (
        <>
          <p className="text-text-muted mb-4">
            You are about to enable LIVE trading mode.
          </p>

          {/* Warning box */}
          <div className="bg-warning/10 border border-warning/20 rounded-lg p-4 mb-4">
            <div className="flex items-start gap-3">
              <AlertTriangle className="w-5 h-5 text-warning flex-shrink-0 mt-0.5" />
              <div>
                <h4 className="font-semibold text-warning">WARNING</h4>
                <ul className="mt-2 space-y-1 text-sm text-text-muted">
                  <li>All orders will use REAL funds</li>
                  <li>Losses will affect your actual balance</li>
                  <li>Make sure your risk limits are set</li>
                </ul>
              </div>
            </div>
          </div>

          {/* Current risk settings */}
          <div className="bg-background-secondary rounded-lg p-4 mb-4">
            <h4 className="font-medium text-text mb-3">Current Risk Settings:</h4>
            <div className="space-y-2 text-sm">
              <div className="flex justify-between">
                <span className="text-text-muted">Max Position:</span>
                <span className="text-text">
                  {riskSettings.maxPositionPercent}% of balance
                </span>
              </div>
              <div className="flex justify-between">
                <span className="text-text-muted">Daily Loss Limit:</span>
                <span className="text-text">{riskSettings.dailyLossLimit}%</span>
              </div>
              <div className="flex justify-between">
                <span className="text-text-muted">Max Leverage:</span>
                <span className="text-text">{riskSettings.maxLeverage}x</span>
              </div>
            </div>
          </div>

          {/* Acknowledgment checkbox */}
          <label className="flex items-center gap-3 cursor-pointer mb-6">
            <div className="relative flex items-center justify-center">
              <input
                type="checkbox"
                checked={acknowledged}
                onChange={(e) => setAcknowledged(e.target.checked)}
                className="sr-only"
              />
              <div
                className={`
                  w-5 h-5 rounded border-2 flex items-center justify-center transition-colors
                  ${
                    acknowledged
                      ? 'bg-primary border-primary'
                      : 'border-gray-300'
                  }
                `}
              >
                {acknowledged && <Check className="w-3 h-3 text-white" />}
              </div>
            </div>
            <span className="text-sm text-text">
              I understand and accept the risks
            </span>
          </label>

          {!canSwitch && (
            <div className="bg-danger/10 border border-danger/20 rounded-lg p-3 mb-4">
              <p className="text-sm text-danger">
                You must complete the onboarding and simulated trading
                requirements before switching to live mode.
              </p>
            </div>
          )}
        </>
      ) : (
        <p className="text-text-muted mb-6">
          Switch to {targetMode} mode? You can switch back to live mode at any
          time.
        </p>
      )}

      {/* Action buttons */}
      <div className="flex justify-end gap-3">
        <button
          type="button"
          onClick={handleClose}
          className="px-4 py-2 text-text-muted hover:text-text border border-border rounded-lg"
        >
          {isLiveMode ? 'Stay in Simulated' : 'Cancel'}
        </button>
        <button
          type="button"
          onClick={handleConfirm}
          disabled={isLiveMode && (!acknowledged || !canSwitch)}
          className={`
            px-4 py-2 rounded-lg transition-colors
            ${
              isLiveMode
                ? 'bg-success text-white hover:bg-success/90 disabled:opacity-50 disabled:cursor-not-allowed'
                : 'bg-primary text-white hover:bg-primary-dark'
            }
          `}
        >
          {isLiveMode ? 'Switch to Live' : `Switch to ${targetMode}`}
        </button>
      </div>
    </Modal>
  );
}
