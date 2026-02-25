/**
 * Engine Hooks
 * TanStack Query hooks for engine status and control
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';

/**
 * Fetch engine status
 */
export function useEngineStatus() {
  return useQuery({
    queryKey: queryKeys.engine.status(),
    queryFn: async () => {
      const client = getApiClient();
      return client.getEngineStatus();
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

export function useEngineHealth() {
  return useQuery({
    queryKey: queryKeys.engine.health(),
    queryFn: async () => {
      const client = getApiClient();
      return client.health();
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Start the engine
 */
export function useStartEngine() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async () => {
      const client = getApiClient();
      return client.startEngine();
    },
    onSuccess: () => {
      // Invalidate engine status to refetch
      queryClient.invalidateQueries({ queryKey: queryKeys.engine.status() });
      // Also invalidate strategies as they may change state
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.all });
    },
  });
}

/**
 * Stop the engine
 */
export function useStopEngine() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async () => {
      const client = getApiClient();
      return client.stopEngine();
    },
    onSuccess: () => {
      // Invalidate engine status to refetch
      queryClient.invalidateQueries({ queryKey: queryKeys.engine.status() });
      // Also invalidate strategies as they may change state
      queryClient.invalidateQueries({ queryKey: queryKeys.strategies.all });
    },
  });
}
