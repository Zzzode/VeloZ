/**
 * Backtest Hooks
 * TanStack Query hooks for backtest operations
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';
import type { RunBacktestRequest } from '../types';

/**
 * Fetch all backtests
 */
export function useBacktests() {
  return useQuery({
    queryKey: queryKeys.backtests.list(),
    queryFn: async () => {
      const client = getApiClient();
      return client.listBacktests();
    },
    staleTime: 10000,
  });
}

/**
 * Fetch a single backtest by ID
 */
export function useBacktest(backtestId: string | null | undefined) {
  return useQuery({
    queryKey: queryKeys.backtests.detail(backtestId ?? ''),
    queryFn: async () => {
      if (!backtestId) throw new Error('Backtest ID is required');
      const client = getApiClient();
      return client.getBacktest(backtestId);
    },
    enabled: !!backtestId,
    staleTime: 30000, // Backtest results don't change
  });
}

/**
 * Run a new backtest
 */
export function useRunBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: RunBacktestRequest) => {
      const client = getApiClient();
      return client.runBacktest(request);
    },
    onSuccess: () => {
      // Invalidate backtests list to refetch
      queryClient.invalidateQueries({ queryKey: queryKeys.backtests.list() });
    },
  });
}

/**
 * Cancel a running backtest
 */
export function useCancelBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (backtestId: string) => {
      const client = getApiClient();
      return client.cancelBacktest(backtestId);
    },
    onSuccess: (_data, backtestId) => {
      // Invalidate the specific backtest and list
      queryClient.invalidateQueries({ queryKey: queryKeys.backtests.detail(backtestId) });
      queryClient.invalidateQueries({ queryKey: queryKeys.backtests.list() });
    },
  });
}

/**
 * Delete a backtest
 */
export function useDeleteBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (backtestId: string) => {
      const client = getApiClient();
      return client.deleteBacktest(backtestId);
    },
    onSuccess: (_data, backtestId) => {
      // Remove from cache and invalidate list
      queryClient.removeQueries({ queryKey: queryKeys.backtests.detail(backtestId) });
      queryClient.invalidateQueries({ queryKey: queryKeys.backtests.list() });
    },
  });
}
