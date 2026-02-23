/**
 * SSE Hook
 * React hook for managing Server-Sent Events connection lifecycle
 */

import { useEffect, useRef, useCallback, useState } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { getSSEClient } from './client';
import { queryKeys } from './queryKeys';
import type { SSEEventHandlers, ConnectionState } from '../types';

export interface UseSSEOptions {
  /** Event handlers for SSE events */
  handlers?: SSEEventHandlers;
  /** Auto-connect on mount (default: true) */
  autoConnect?: boolean;
  /** Invalidate queries on events (default: true) */
  invalidateOnEvents?: boolean;
}

export interface UseSSEReturn {
  /** Current connection state */
  connectionState: ConnectionState;
  /** Connect to SSE stream */
  connect: () => void;
  /** Disconnect from SSE stream */
  disconnect: () => void;
  /** Reconnect to SSE stream */
  reconnect: () => void;
  /** Last event ID for resumption */
  lastEventId: string;
}

/**
 * Hook to manage SSE connection and automatically invalidate queries on events
 */
export function useSSE(options: UseSSEOptions = {}): UseSSEReturn {
  const { handlers, autoConnect = true, invalidateOnEvents = true } = options;
  const queryClient = useQueryClient();
  const connectedRef = useRef(false);
  const [connectionState, setConnectionState] = useState<ConnectionState>('disconnected');
  const [lastEventId, setLastEventId] = useState('');

  // Memoize handlers to avoid reconnection on every render
  const handlersRef = useRef(handlers);
  handlersRef.current = handlers;

  const setupClient = useCallback(() => {
    const client = getSSEClient();

    // Set up event handlers
    const eventHandlers: SSEEventHandlers = {
      market: (event) => {
        handlersRef.current?.market?.(event);
        if (invalidateOnEvents) {
          queryClient.invalidateQueries({ queryKey: queryKeys.market.all });
        }
      },
      order_update: (event) => {
        handlersRef.current?.order_update?.(event);
        if (invalidateOnEvents) {
          queryClient.invalidateQueries({ queryKey: queryKeys.orders.all });
        }
      },
      fill: (event) => {
        handlersRef.current?.fill?.(event);
        if (invalidateOnEvents) {
          queryClient.invalidateQueries({ queryKey: queryKeys.orders.all });
          queryClient.invalidateQueries({ queryKey: queryKeys.positions.all });
          queryClient.invalidateQueries({ queryKey: queryKeys.account.all });
        }
      },
      order_state: (event) => {
        handlersRef.current?.order_state?.(event);
        if (invalidateOnEvents) {
          queryClient.invalidateQueries({ queryKey: queryKeys.orders.all });
        }
      },
      account: (event) => {
        handlersRef.current?.account?.(event);
        if (invalidateOnEvents) {
          queryClient.invalidateQueries({ queryKey: queryKeys.account.all });
        }
      },
      error: (event) => {
        handlersRef.current?.error?.(event);
      },
    };

    client.setHandlers(eventHandlers);

    client.setClientEvents({
      onStateChange: (state) => {
        setConnectionState(state);
      },
      onConnect: () => {
        setLastEventId(client.getLastEventId());
      },
    });

    return client;
  }, [queryClient, invalidateOnEvents]);

  const connect = useCallback(() => {
    const client = setupClient();
    client.connect();
    connectedRef.current = true;
  }, [setupClient]);

  const disconnect = useCallback(() => {
    const client = getSSEClient();
    client.disconnect();
    connectedRef.current = false;
  }, []);

  const reconnect = useCallback(() => {
    const client = getSSEClient();
    client.reconnect();
  }, []);

  // Auto-connect on mount
  useEffect(() => {
    if (autoConnect && !connectedRef.current) {
      connect();
    }

    return () => {
      if (connectedRef.current) {
        disconnect();
      }
    };
  }, [autoConnect, connect, disconnect]);

  // Update last event ID periodically
  useEffect(() => {
    const interval = setInterval(() => {
      const client = getSSEClient();
      setLastEventId(client.getLastEventId());
    }, 1000);

    return () => clearInterval(interval);
  }, []);

  return {
    connectionState,
    connect,
    disconnect,
    reconnect,
    lastEventId,
  };
}

/**
 * Simplified hook that just provides SSE connection state
 */
export function useSSEConnectionState(): ConnectionState {
  const [state, setState] = useState<ConnectionState>('disconnected');

  useEffect(() => {
    const client = getSSEClient();
    client.setClientEvents({
      onStateChange: setState,
    });

    // Get initial state
    setState(client.getState());
  }, []);

  return state;
}
