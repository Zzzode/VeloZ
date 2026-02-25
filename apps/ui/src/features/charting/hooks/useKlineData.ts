/**
 * useKlineData Hook
 * Fetches historical kline data and subscribes to real-time updates
 */
import { useEffect, useRef, useCallback } from 'react';
import { useQuery } from '@tanstack/react-query';
import { useChartStore } from '../store/chartStore';
import type { OHLCVData, Timeframe, KlineMessage } from '../types';
import { API_BASE } from '@/shared/utils/constants';

const API_BASE_URL = API_BASE;

// =============================================================================
// API Functions
// =============================================================================

interface KlineResponse {
  klines: Array<{
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  }>;
}

async function fetchKlines(
  symbol: string,
  exchange: string,
  timeframe: Timeframe,
  limit: number = 500
): Promise<OHLCVData[]> {
  const params = new URLSearchParams({
    symbol,
    exchange,
    timeframe,
    limit: limit.toString(),
  });

  const response = await fetch(`${API_BASE_URL}/api/market/klines?${params}`);

  if (!response.ok) {
    throw new Error(`Failed to fetch klines: ${response.statusText}`);
  }

  const data: KlineResponse = await response.json();
  return data.klines;
}

// =============================================================================
// WebSocket Connection
// =============================================================================

function createKlineWebSocket(
  symbol: string,
  _exchange: string,
  timeframe: Timeframe,
  onMessage: (data: KlineMessage) => void,
  onConnect: () => void,
  onDisconnect: () => void
): WebSocket {
  const wsUrl = API_BASE_URL.replace(/^http/, 'ws');
  const ws = new WebSocket(`${wsUrl}/ws/market`);

  ws.onopen = () => {
    // Subscribe to kline channel
    ws.send(
      JSON.stringify({
        action: 'subscribe',
        symbols: [symbol],
        channels: ['kline'],
        timeframe,
      })
    );
    onConnect();
  };

  ws.onmessage = (event) => {
    try {
      const message = JSON.parse(event.data);
      if (message.type === 'kline' && message.symbol === symbol) {
        onMessage(message as KlineMessage);
      }
    } catch (error) {
      console.error('Failed to parse kline message:', error);
    }
  };

  ws.onclose = () => {
    onDisconnect();
  };

  ws.onerror = (error) => {
    console.error('Kline WebSocket error:', error);
  };

  return ws;
}

// =============================================================================
// Hook
// =============================================================================

export function useKlineData() {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number | undefined>(undefined);

  const {
    symbol,
    exchange,
    timeframe,
    setCandles,
    updateCandle,
    setLoading,
    setError,
    setConnected,
  } = useChartStore();

  // Fetch historical data
  const { data, isLoading, error } = useQuery({
    queryKey: ['klines', symbol, exchange, timeframe],
    queryFn: () => fetchKlines(symbol, exchange, timeframe),
    staleTime: 60 * 1000, // 1 minute
    refetchOnWindowFocus: false,
  });

  // Update store when data is fetched
  useEffect(() => {
    setLoading(isLoading);
  }, [isLoading, setLoading]);

  useEffect(() => {
    if (error) {
      setError(error instanceof Error ? error.message : 'Failed to fetch kline data');
    }
  }, [error, setError]);

  useEffect(() => {
    if (data) {
      setCandles(data);
    }
  }, [data, setCandles]);

  // Handle incoming kline messages
  const handleKlineMessage = useCallback(
    (message: KlineMessage) => {
      const candle: OHLCVData = {
        time: message.data.time,
        open: message.data.open,
        high: message.data.high,
        low: message.data.low,
        close: message.data.close,
        volume: message.data.volume,
      };

      updateCandle(candle);
    },
    [updateCandle]
  );

  // WebSocket connection management
  useEffect(() => {
    const connect = () => {
      if (wsRef.current) {
        wsRef.current.close();
      }

      wsRef.current = createKlineWebSocket(
        symbol,
        exchange,
        timeframe,
        handleKlineMessage,
        () => setConnected(true),
        () => {
          setConnected(false);
          // Reconnect after delay
          reconnectTimeoutRef.current = setTimeout(connect, 3000);
        }
      );
    };

    connect();

    return () => {
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [symbol, exchange, timeframe, handleKlineMessage, setConnected]);

  return {
    isLoading,
    error,
  };
}
