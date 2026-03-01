/**
 * Orders Hooks
 * TanStack Query hooks for order operations
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';
import type { PlaceOrderRequest, CancelOrderRequest } from '../types';

/**
 * Fetch all orders (raw events)
 */
export function useOrders() {
  return useQuery({
    queryKey: queryKeys.orders.list(),
    queryFn: async () => {
      const client = getApiClient();
      const response = await client.listOrders();
      return response.items;
    },
    staleTime: 5000,
  });
}

/**
 * Fetch all order states (aggregated)
 */
export function useOrderStates() {
  return useQuery({
    queryKey: queryKeys.orders.states(),
    queryFn: async () => {
      const client = getApiClient();
      const response = await client.listOrderStates();
      return response.items;
    },
    staleTime: 5000,
  });
}

/**
 * Fetch a single order state by client order ID
 */
export function useOrderState(clientOrderId: string | undefined) {
  return useQuery({
    queryKey: queryKeys.orders.state(clientOrderId ?? ''),
    queryFn: async () => {
      if (!clientOrderId) throw new Error('Client order ID is required');
      const client = getApiClient();
      return client.getOrderState(clientOrderId);
    },
    enabled: !!clientOrderId,
    staleTime: 5000,
  });
}

/**
 * Place a new order
 */
export function usePlaceOrder() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: PlaceOrderRequest) => {
      const client = getApiClient();
      return client.placeOrder(request);
    },
    onSuccess: () => {
      // Invalidate order queries to refetch
      queryClient.invalidateQueries({ queryKey: queryKeys.orders.all });
      // Also invalidate positions as they may change
      queryClient.invalidateQueries({ queryKey: queryKeys.positions.all });
      // And account balance
      queryClient.invalidateQueries({ queryKey: queryKeys.account.all });
    },
  });
}

/**
 * Cancel an order
 */
export function useCancelOrder() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: CancelOrderRequest) => {
      const client = getApiClient();
      return client.cancelOrder(request);
    },
    onSuccess: () => {
      // Invalidate order queries to refetch
      queryClient.invalidateQueries({ queryKey: queryKeys.orders.all });
    },
  });
}
