/**
 * Market data hooks for API and WebSocket integration
 */
import { useEffect, useCallback } from 'react';
import { useQuery } from '@tanstack/react-query';
import {
  createApiClient,
  type VelozMarketWsClient,
} from '@/shared/api';
import { useMarketStore, selectCurrentPrice, selectCurrentBookTop, selectCurrentTrades } from '../store';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';
const apiClient = createApiClient(API_BASE);

// Query keys
export const marketKeys = {
  all: ['market'] as const,
  data: () => [...marketKeys.all, 'data'] as const,
  symbol: (symbol: string) => [...marketKeys.all, 'symbol', symbol] as const,
};

/**
 * Fetch current market data from REST API
 */
export function useMarketData() {
  return useQuery({
    queryKey: marketKeys.data(),
    queryFn: async () => {
      return apiClient.getMarketData();
    },
    staleTime: 1000,
    refetchInterval: 5000,
  });
}

/**
 * WebSocket connection for real-time market data
 * NOTE: WebSocket endpoint not implemented in gateway yet.
 * Using SSE as fallback. See /api/stream for real-time updates.
 */
export function useMarketWebSocket() {
  const setWsConnectionState = useMarketStore((state) => state.setWsConnectionState);

  // Use SSE as fallback for now (WebSocket endpoint not implemented)
  useEffect(() => {
    setWsConnectionState('connected'); // Mark as connected since SSE works

    // TODO: When WebSocket endpoint is implemented, uncomment below
    // const ws = createMarketWsClient(API_BASE);
    // ws.setHandlers({...});
    // ws.connect();
    // return () => ws.disconnect();

    return () => {
      setWsConnectionState('disconnected');
    };
  }, [setWsConnectionState]);

  const reconnect = useCallback(() => {
    console.log('WebSocket reconnect not implemented - using SSE fallback');
  }, []);

  const disconnect = useCallback(() => {
    console.log('WebSocket disconnect not implemented - using SSE fallback');
  }, []);

  return {
    reconnect,
    disconnect,
  };
}

/**
 * Combined hook for market data (REST + WebSocket)
 */
export function useMarket() {
  const { data: restData, isLoading, error } = useMarketData();
  const { reconnect, disconnect } = useMarketWebSocket();

  const currentPrice = useMarketStore(selectCurrentPrice);
  const currentBookTop = useMarketStore(selectCurrentBookTop);
  const recentTrades = useMarketStore(selectCurrentTrades);
  const connectionState = useMarketStore((state) => state.wsConnectionState);

  return {
    // REST data
    restData,
    isLoading,
    error,

    // Real-time data
    currentPrice,
    currentBookTop,
    recentTrades,

    // Connection
    connectionState,
    reconnect,
    disconnect,
  };
}
