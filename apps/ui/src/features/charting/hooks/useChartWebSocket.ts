/**
 * useChartWebSocket Hook
 * Manages WebSocket connection for real-time chart data
 */
import { useEffect, useRef, useCallback } from 'react';
import type { Timeframe, OHLCVData } from '../types';

// =============================================================================
// Types
// =============================================================================

interface UseChartWebSocketOptions {
  symbol: string;
  timeframe: Timeframe;
  onKline: (kline: OHLCVData, isClosed: boolean) => void;
  onConnect?: () => void;
  onDisconnect?: () => void;
  onError?: (error: Event) => void;
  enabled?: boolean;
}

interface WebSocketState {
  ws: WebSocket | null;
  reconnectAttempts: number;
  isManualClose: boolean;
}

// =============================================================================
// Constants
// =============================================================================

const MAX_RECONNECT_ATTEMPTS = 10;
const INITIAL_RECONNECT_DELAY = 1000;
const MAX_RECONNECT_DELAY = 30000;

// =============================================================================
// Hook
// =============================================================================

export function useChartWebSocket({
  symbol,
  timeframe,
  onKline,
  onConnect,
  onDisconnect,
  onError,
  enabled = true,
}: UseChartWebSocketOptions) {
  const stateRef = useRef<WebSocketState>({
    ws: null,
    reconnectAttempts: 0,
    isManualClose: false,
  });
  const reconnectTimeoutRef = useRef<number | undefined>(undefined);

  // Calculate reconnect delay with exponential backoff
  const getReconnectDelay = useCallback(() => {
    const delay = Math.min(
      INITIAL_RECONNECT_DELAY * Math.pow(1.5, stateRef.current.reconnectAttempts),
      MAX_RECONNECT_DELAY
    );
    return delay;
  }, []);

  // Connect to WebSocket
  const connect = useCallback(() => {
    if (!enabled) return;

    const state = stateRef.current;

    // Close existing connection
    if (state.ws) {
      state.ws.close();
    }

    // Determine WebSocket URL
    const baseUrl = import.meta.env.VITE_API_BASE_URL || 'http://127.0.0.1:8080';
    const wsUrl = baseUrl.replace(/^http/, 'ws');

    try {
      const ws = new WebSocket(`${wsUrl}/ws/market`);

      ws.onopen = () => {
        state.reconnectAttempts = 0;

        // Subscribe to kline channel
        ws.send(
          JSON.stringify({
            action: 'subscribe',
            symbols: [symbol],
            channels: ['kline'],
            timeframe,
          })
        );

        onConnect?.();
      };

      ws.onmessage = (event) => {
        try {
          const message = JSON.parse(event.data);

          if (message.type === 'kline' && message.symbol === symbol) {
            const kline: OHLCVData = {
              time: message.data.time,
              open: message.data.open,
              high: message.data.high,
              low: message.data.low,
              close: message.data.close,
              volume: message.data.volume,
            };
            onKline(kline, message.isClosed ?? false);
          }
        } catch (error) {
          console.error('Failed to parse WebSocket message:', error);
        }
      };

      ws.onclose = () => {
        onDisconnect?.();

        // Attempt reconnection if not manually closed
        if (!state.isManualClose && state.reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
          const delay = getReconnectDelay();
          state.reconnectAttempts++;

          reconnectTimeoutRef.current = setTimeout(() => {
            connect();
          }, delay);
        }
      };

      ws.onerror = (error) => {
        onError?.(error);
      };

      state.ws = ws;
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
    }
  }, [enabled, symbol, timeframe, onKline, onConnect, onDisconnect, onError, getReconnectDelay]);

  // Disconnect from WebSocket
  const disconnect = useCallback(() => {
    const state = stateRef.current;
    state.isManualClose = true;

    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current);
    }

    if (state.ws) {
      state.ws.close();
      state.ws = null;
    }
  }, []);

  // Reconnect to WebSocket
  const reconnect = useCallback(() => {
    const state = stateRef.current;
    state.isManualClose = false;
    state.reconnectAttempts = 0;
    connect();
  }, [connect]);

  // Connect on mount, disconnect on unmount
  useEffect(() => {
    stateRef.current.isManualClose = false;
    connect();

    return () => {
      disconnect();
    };
  }, [connect, disconnect]);

  // Reconnect when symbol/timeframe changes
  useEffect(() => {
    if (stateRef.current.ws && stateRef.current.ws.readyState === WebSocket.OPEN) {
      // Unsubscribe from old symbol/timeframe
      stateRef.current.ws.send(
        JSON.stringify({
          action: 'unsubscribe',
          symbols: [symbol],
          channels: ['kline'],
        })
      );

      // Subscribe to new symbol/timeframe
      stateRef.current.ws.send(
        JSON.stringify({
          action: 'subscribe',
          symbols: [symbol],
          channels: ['kline'],
          timeframe,
        })
      );
    }
  }, [symbol, timeframe]);

  return {
    disconnect,
    reconnect,
    isConnected: stateRef.current.ws?.readyState === WebSocket.OPEN,
  };
}
