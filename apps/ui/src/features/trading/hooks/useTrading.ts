/**
 * Trading Hooks
 * Custom hooks for trading operations
 */

import { useEffect, useCallback, useRef } from 'react';
import { useTradingStore } from '../store';
import {
  createApiClient,
  createSSEClient,
  createMarketWsClient,
  type VelozApiClient,
  type VelozSSEClient,
  type VelozMarketWsClient,
} from '@/shared/api';
import type { PlaceOrderRequest, CancelOrderRequest } from '@/shared/api/types';
import { API_BASE } from '@/shared/utils/constants';

// =============================================================================
// API Client Singleton
// =============================================================================

let apiClient: VelozApiClient | null = null;
let sseClient: VelozSSEClient | null = null;
let marketWsClient: VelozMarketWsClient | null = null;

function getApiClient(): VelozApiClient {
  if (!apiClient) {
    apiClient = createApiClient(API_BASE);
  }
  return apiClient;
}

function getSSEClient(): VelozSSEClient {
  if (!sseClient) {
    sseClient = createSSEClient(API_BASE);
  }
  return sseClient;
}

function getMarketWsClient(): VelozMarketWsClient {
  if (!marketWsClient) {
    marketWsClient = createMarketWsClient(API_BASE);
  }
  return marketWsClient;
}

// =============================================================================
// useOrders Hook
// =============================================================================

export function useOrders() {
  const orders = useTradingStore((state) => state.orders);
  const isLoadingOrders = useTradingStore((state) => state.isLoadingOrders);
  const setOrders = useTradingStore((state) => state.setOrders);
  const setIsLoadingOrders = useTradingStore((state) => state.setIsLoadingOrders);
  const setError = useTradingStore((state) => state.setError);

  const fetchOrders = useCallback(async () => {
    setIsLoadingOrders(true);
    setError(null);

    try {
      const client = getApiClient();
      const response = await client.listOrderStates();
      setOrders(response.items);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch orders');
    } finally {
      setIsLoadingOrders(false);
    }
  }, [setOrders, setIsLoadingOrders, setError]);

  return {
    orders,
    isLoading: isLoadingOrders,
    refetch: fetchOrders,
  };
}

// =============================================================================
// usePositions Hook
// =============================================================================

export function usePositions() {
  const positions = useTradingStore((state) => state.positions);
  const isLoadingPositions = useTradingStore((state) => state.isLoadingPositions);
  const setPositions = useTradingStore((state) => state.setPositions);
  const setIsLoadingPositions = useTradingStore((state) => state.setIsLoadingPositions);
  const setError = useTradingStore((state) => state.setError);

  const fetchPositions = useCallback(async () => {
    setIsLoadingPositions(true);
    setError(null);

    try {
      const client = getApiClient();
      const response = await client.listPositions();
      setPositions(response.positions);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch positions');
    } finally {
      setIsLoadingPositions(false);
    }
  }, [setPositions, setIsLoadingPositions, setError]);

  return {
    positions,
    isLoading: isLoadingPositions,
    refetch: fetchPositions,
  };
}

// =============================================================================
// usePlaceOrder Hook
// =============================================================================

export function usePlaceOrder() {
  const isPlacingOrder = useTradingStore((state) => state.isPlacingOrder);
  const setIsPlacingOrder = useTradingStore((state) => state.setIsPlacingOrder);
  const setError = useTradingStore((state) => state.setError);
  const resetOrderForm = useTradingStore((state) => state.resetOrderForm);

  const placeOrder = useCallback(
    async (request: PlaceOrderRequest) => {
      setIsPlacingOrder(true);
      setError(null);

      try {
        const client = getApiClient();
        const response = await client.placeOrder(request);
        resetOrderForm();
        return response;
      } catch (err) {
        const message = err instanceof Error ? err.message : 'Failed to place order';
        setError(message);
        throw err;
      } finally {
        setIsPlacingOrder(false);
      }
    },
    [setIsPlacingOrder, setError, resetOrderForm]
  );

  return {
    placeOrder,
    isLoading: isPlacingOrder,
  };
}

// =============================================================================
// useCancelOrder Hook
// =============================================================================

export function useCancelOrder() {
  const setError = useTradingStore((state) => state.setError);

  const cancelOrder = useCallback(
    async (request: CancelOrderRequest) => {
      setError(null);

      try {
        const client = getApiClient();
        const response = await client.cancelOrder(request);
        return response;
      } catch (err) {
        const message = err instanceof Error ? err.message : 'Failed to cancel order';
        setError(message);
        throw err;
      }
    },
    [setError]
  );

  return { cancelOrder };
}

// =============================================================================
// useOrderBookStream Hook
// =============================================================================

export function useOrderBookStream() {
  const selectedSymbol = useTradingStore((state) => state.selectedSymbol);
  const wsConnectionState = useTradingStore((state) => state.wsConnectionState);
  const updateOrderBook = useTradingStore((state) => state.updateOrderBook);
  const setWSConnectionState = useTradingStore((state) => state.setWSConnectionState);

  const connectedRef = useRef(false);

  useEffect(() => {
    if (connectedRef.current) return;
    connectedRef.current = true;

    const client = getMarketWsClient();

    client.setHandlers({
      onBookTop: (data) => {
        updateOrderBook(data);
      },
    });

    client.setClientEvents({
      onStateChange: (state) => {
        setWSConnectionState(state);
      },
      onConnect: () => {
        // Subscribe to book_top for selected symbol
        client.subscribe([selectedSymbol], ['book_top']);
      },
    });

    client.connect();

    return () => {
      client.unsubscribe([selectedSymbol], ['book_top']);
      client.disconnect();
      connectedRef.current = false;
    };
  }, [selectedSymbol, updateOrderBook, setWSConnectionState]);

  return {
    connectionState: wsConnectionState,
  };
}

// =============================================================================
// useOrderUpdates Hook
// =============================================================================

export function useOrderUpdates() {
  const sseConnectionState = useTradingStore((state) => state.sseConnectionState);
  const updateOrder = useTradingStore((state) => state.updateOrder);
  const setSSEConnectionState = useTradingStore((state) => state.setSSEConnectionState);
  const setOrders = useTradingStore((state) => state.setOrders);

  const connectedRef = useRef(false);

  useEffect(() => {
    if (connectedRef.current) return;
    connectedRef.current = true;

    const client = getSSEClient();

    client.setHandlers({
      order_update: (event) => {
        updateOrder({
          client_order_id: event.client_order_id,
          venue_order_id: event.venue_order_id,
          status: event.status,
          symbol: event.symbol,
          side: event.side,
          order_qty: event.qty,
          limit_price: event.price,
          reason: event.reason,
        });
      },
      order_state: (event) => {
        updateOrder({
          client_order_id: event.client_order_id,
          venue_order_id: event.venue_order_id,
          status: event.status,
          symbol: event.symbol,
          side: event.side,
          order_qty: event.order_qty,
          limit_price: event.limit_price,
          executed_qty: event.executed_qty,
          avg_price: event.avg_price,
          reason: event.reason,
          last_ts_ns: event.last_ts_ns,
        });
      },
      fill: () => {
        // Fills trigger order state updates, refetch orders
        getApiClient()
          .listOrderStates()
          .then((response) => setOrders(response.items))
          .catch(console.error);
      },
    });

    client.setClientEvents({
      onStateChange: (state) => {
        setSSEConnectionState(state);
      },
    });

    client.connect();

    return () => {
      client.disconnect();
      connectedRef.current = false;
    };
  }, [updateOrder, setSSEConnectionState, setOrders]);

  return {
    connectionState: sseConnectionState,
  };
}
