/**
 * Market data hooks for API and WebSocket integration
 */
import { useEffect, useRef, useCallback } from 'react';
import { useQuery } from '@tanstack/react-query';
import {
  createApiClient,
  createMarketWsClient,
  type VelozMarketWsClient,
} from '@/shared/api';
import { useMarketStore } from '../store';

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
 */
export function useMarketWebSocket() {
  const wsRef = useRef<VelozMarketWsClient | null>(null);
  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const updateBookTop = useMarketStore((state) => state.updateBookTop);
  const addTrade = useMarketStore((state) => state.addTrade);
  const setWsConnectionState = useMarketStore((state) => state.setWsConnectionState);

  // Initialize WebSocket client
  useEffect(() => {
    const ws = createMarketWsClient(API_BASE);

    ws.setHandlers({
      onTrade: (data) => {
        addTrade(data.symbol, data);
      },
      onBookTop: (data) => {
        updateBookTop(data.symbol, data);
      },
    });

    ws.setClientEvents({
      onStateChange: (state) => {
        setWsConnectionState(state);
      },
      onConnect: () => {
        console.log('Market WebSocket connected');
      },
      onDisconnect: () => {
        console.log('Market WebSocket disconnected');
      },
      onError: (error) => {
        console.error('Market WebSocket error:', error);
      },
    });

    wsRef.current = ws;
    ws.connect();

    return () => {
      ws.disconnect();
      wsRef.current = null;
    };
  }, [addTrade, updateBookTop, setWsConnectionState]);

  // Subscribe to selected symbol
  useEffect(() => {
    const ws = wsRef.current;
    if (!ws || !selectedSymbol) return;

    // Subscribe to trade and book_top channels
    ws.subscribe([selectedSymbol], ['trade', 'book_top']);

    return () => {
      ws.unsubscribe([selectedSymbol], ['trade', 'book_top']);
    };
  }, [selectedSymbol]);

  const reconnect = useCallback(() => {
    wsRef.current?.reconnect();
  }, []);

  const disconnect = useCallback(() => {
    wsRef.current?.disconnect();
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

  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const currentPrice = useMarketStore((state) => state.prices[selectedSymbol]);
  const currentBookTop = useMarketStore((state) => state.bookTops[selectedSymbol]);
  const recentTrades = useMarketStore((state) => state.recentTrades[selectedSymbol] ?? []);
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
