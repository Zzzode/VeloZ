import { useState } from 'react';
import { AlertOctagon, X } from 'lucide-react';
import { Modal } from '@/shared/components';
import { useSecurityEducationStore } from '../store';

interface EmergencyStopButtonProps {
  onStop?: (cancelledOrders: number) => Promise<number> | number;
  size?: 'sm' | 'md' | 'lg';
  className?: string;
}

export function EmergencyStopButton({
  onStop,
  size = 'md',
  className = '',
}: EmergencyStopButtonProps) {
  const [isDialogOpen, setIsDialogOpen] = useState(false);
  const [confirmText, setConfirmText] = useState('');
  const [isProcessing, setIsProcessing] = useState(false);

  const { activateEmergencyStop, emergencyStop } = useSecurityEducationStore();

  const sizeClasses = {
    sm: 'px-3 py-1.5 text-sm',
    md: 'px-4 py-2 text-base',
    lg: 'px-6 py-3 text-lg',
  };

  const iconSizes = {
    sm: 'w-4 h-4',
    md: 'w-5 h-5',
    lg: 'w-6 h-6',
  };

  const handleOpenDialog = () => {
    if (!emergencyStop.isActive) {
      setIsDialogOpen(true);
    }
  };

  const handleConfirm = async () => {
    if (confirmText.toUpperCase() !== 'STOP') {
      return;
    }

    setIsProcessing(true);

    try {
      let cancelledOrders = 0;
      if (onStop) {
        cancelledOrders = await onStop(0);
      }
      activateEmergencyStop('User initiated emergency stop', cancelledOrders);
    } finally {
      setIsProcessing(false);
      setIsDialogOpen(false);
      setConfirmText('');
    }
  };

  const handleClose = () => {
    setIsDialogOpen(false);
    setConfirmText('');
  };

  return (
    <>
      <button
        type="button"
        onClick={handleOpenDialog}
        disabled={emergencyStop.isActive}
        className={`
          inline-flex items-center gap-2 font-semibold rounded-lg
          bg-danger text-white hover:bg-danger/90
          disabled:opacity-50 disabled:cursor-not-allowed
          ${sizeClasses[size]}
          ${className}
        `}
      >
        <AlertOctagon className={iconSizes[size]} />
        {emergencyStop.isActive ? 'STOPPED' : 'Emergency Stop'}
      </button>

      <Modal
        isOpen={isDialogOpen}
        onClose={handleClose}
        title="Emergency Stop"
        size="md"
        closeOnOverlayClick={false}
      >
        <div className="bg-danger/10 border border-danger/20 rounded-lg p-4 mb-4">
          <div className="flex items-start gap-3">
            <AlertOctagon className="w-5 h-5 text-danger flex-shrink-0 mt-0.5" />
            <div>
              <h4 className="font-semibold text-danger">
                EMERGENCY STOP
              </h4>
              <p className="mt-1 text-sm text-text-muted">
                This will immediately:
              </p>
            </div>
          </div>
        </div>

        <ul className="space-y-2 mb-6 text-sm text-text-muted">
          <li className="flex items-center gap-2">
            <X className="w-4 h-4 text-danger" />
            Cancel ALL open orders
          </li>
          <li className="flex items-center gap-2">
            <X className="w-4 h-4 text-danger" />
            Disable ALL running strategies
          </li>
          <li className="flex items-center gap-2">
            <X className="w-4 h-4 text-danger" />
            Prevent any new orders
          </li>
        </ul>

        <div className="bg-background-secondary rounded-lg p-3 mb-6">
          <p className="text-sm text-text-muted">
            Your open positions will NOT be closed automatically.
          </p>
        </div>

        <div className="mb-6">
          <label className="block text-sm font-medium text-text mb-2">
            Type "STOP" to confirm:
          </label>
          <input
            type="text"
            value={confirmText}
            onChange={(e) => setConfirmText(e.target.value)}
            placeholder="STOP"
            className="w-full px-3 py-2 border border-border rounded-lg focus:outline-none focus:ring-2 focus:ring-danger/50"
          />
        </div>

        <div className="flex justify-end gap-3">
          <button
            type="button"
            onClick={handleClose}
            className="px-4 py-2 text-text-muted hover:text-text border border-border rounded-lg"
          >
            Cancel
          </button>
          <button
            type="button"
            onClick={handleConfirm}
            disabled={confirmText.toUpperCase() !== 'STOP' || isProcessing}
            className="px-4 py-2 bg-danger text-white rounded-lg hover:bg-danger/90 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {isProcessing ? 'Stopping...' : 'Emergency Stop'}
          </button>
        </div>
      </Modal>
    </>
  );
}
