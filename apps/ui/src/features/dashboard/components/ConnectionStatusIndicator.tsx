/**
 * Connection Status Indicator - shows SSE connection state
 */
import { Wifi, WifiOff, RefreshCw } from 'lucide-react';
import type { ConnectionState } from '@/shared/api';
import { useDashboardStore } from '../store/dashboardStore';

const statusConfig: Record<
  ConnectionState,
  { label: string; color: string; icon: typeof Wifi }
> = {
  connected: {
    label: 'Connected',
    color: 'text-success',
    icon: Wifi,
  },
  connecting: {
    label: 'Connecting...',
    color: 'text-warning',
    icon: RefreshCw,
  },
  reconnecting: {
    label: 'Reconnecting...',
    color: 'text-warning',
    icon: RefreshCw,
  },
  disconnected: {
    label: 'Disconnected',
    color: 'text-danger',
    icon: WifiOff,
  },
};

interface ConnectionStatusIndicatorProps {
  onReconnect?: () => void;
}

export function ConnectionStatusIndicator({
  onReconnect,
}: ConnectionStatusIndicatorProps) {
  const connectionState = useDashboardStore((state) => state.sseConnectionState);
  const config = statusConfig[connectionState];
  const Icon = config.icon;

  const isAnimating = connectionState === 'connecting' || connectionState === 'reconnecting';

  return (
    <div className="flex items-center gap-2">
      <Icon
        className={`w-4 h-4 ${config.color} ${isAnimating ? 'animate-spin' : ''}`}
      />
      <span className={`text-sm ${config.color}`}>{config.label}</span>
      {connectionState === 'disconnected' && onReconnect && (
        <button
          onClick={onReconnect}
          className="text-xs text-info hover:underline ml-2"
        >
          Reconnect
        </button>
      )}
    </div>
  );
}
