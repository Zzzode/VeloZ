/**
 * SSE Connection hook for real-time dashboard updates
 */
import { useEffect, useRef, useCallback } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { createSSEClient, type VelozSSEClient } from '@/shared/api';
import { useDashboardStore } from '../store/dashboardStore';
import { dashboardKeys } from './useDashboardData';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';

export function useSSEConnection() {
  const clientRef = useRef<VelozSSEClient | null>(null);
  const queryClient = useQueryClient();

  const sseConnectionState = useDashboardStore((state) => state.sseConnectionState);
  const setConnectionState = useDashboardStore((state) => state.setConnectionState);
  const updatePrice = useDashboardStore((state) => state.updatePrice);

  const handleMarketEvent = useCallback(
    (event: { symbol: string; price: number; ts_ns: number }) => {
      updatePrice({
        type: 'market',
        symbol: event.symbol,
        price: event.price,
        ts_ns: event.ts_ns,
      });
    },
    [updatePrice]
  );

  const handleOrderUpdate = useCallback(() => {
    // Invalidate order queries to refetch
    queryClient.invalidateQueries({ queryKey: dashboardKeys.orders() });
    queryClient.invalidateQueries({ queryKey: dashboardKeys.orderStates() });
  }, [queryClient]);

  const handleAccountUpdate = useCallback(() => {
    // Invalidate account query to refetch
    queryClient.invalidateQueries({ queryKey: dashboardKeys.account() });
  }, [queryClient]);

  useEffect(() => {
    // Create SSE client
    clientRef.current = createSSEClient(API_BASE);

    // Set up event handlers
    clientRef.current.setHandlers({
      market: handleMarketEvent,
      order_update: handleOrderUpdate,
      fill: handleOrderUpdate,
      order_state: handleOrderUpdate,
      account: handleAccountUpdate,
      error: (event) => {
        console.error('SSE error:', event.message);
      },
    });

    // Set up connection state tracking
    clientRef.current.setClientEvents({
      onStateChange: setConnectionState,
    });

    // Connect
    clientRef.current.connect();

    // Cleanup on unmount
    return () => {
      clientRef.current?.disconnect();
    };
  }, [
    handleMarketEvent,
    handleOrderUpdate,
    handleAccountUpdate,
    setConnectionState,
  ]);

  const reconnect = useCallback(() => {
    clientRef.current?.connect();
  }, []);

  const disconnect = useCallback(() => {
    clientRef.current?.disconnect();
  }, []);

  return {
    connectionState: sseConnectionState,
    reconnect,
    disconnect,
  };
}
