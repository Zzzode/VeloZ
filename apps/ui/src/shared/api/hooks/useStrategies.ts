/**
 * Strategies Hooks
 * TanStack Query hooks for strategy operations
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';

/**
 * Fetch all strategies
 */
export function useStrategies() {
  return useQuery({
    queryKey: queryKeys.strategies.list(),
    queryFn: async () => {
      const client = getApiClient();
      const response = await client.listStrategies();
      return response.strategies;
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch a single strategy by ID
 */
export function useStrategy(id: string | undefined) {
  return useQuery({
    queryKey: queryKeys.strategies.detail(id ?? ''),
    queryFn: async () => {
      if (!id) throw new Error('Strategy ID is required');
      const client = getApiClient();
      return client.getStrategy(id);
    },
    enabled: !!id,
    staleTime: 5000,
  });
}

/**
 * Start a strategy
 */
export function useStartStrategy() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const client = getApiClient();
      return client.startStrategy(id);
    },
    onSuccess: (_data, id) => {
      // Invalidate both list and detail queries
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.list() });
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.detail(id) });
    },
  });
}

/**
 * Stop a strategy
 */
export function useStopStrategy() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const client = getApiClient();
      return client.stopStrategy(id);
    },
    onSuccess: (_data, id) => {
      // Invalidate both list and detail queries
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.list() });
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.detail(id) });
    },
  });
}
