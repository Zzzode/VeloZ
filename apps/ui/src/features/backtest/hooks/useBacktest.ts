import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { createApiClient } from '@/shared/api';
import type {
  BacktestResult,
  RunBacktestRequest,
  RunBacktestResponse,
  ListBacktestsResponse,
} from '@/shared/api/types';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';
const apiClient = createApiClient(API_BASE);

/**
 * Query keys for backtest-related queries
 */
export const backtestKeys = {
  all: ['backtests'] as const,
  lists: () => [...backtestKeys.all, 'list'] as const,
  list: (filters?: Record<string, unknown>) =>
    [...backtestKeys.lists(), filters] as const,
  details: () => [...backtestKeys.all, 'detail'] as const,
  detail: (id: string) => [...backtestKeys.details(), id] as const,
};

/**
 * Hook to fetch list of all backtests
 */
export function useBacktests() {
  return useQuery({
    queryKey: backtestKeys.lists(),
    queryFn: async (): Promise<ListBacktestsResponse> => {
      return apiClient.listBacktests();
    },
  });
}

/**
 * Hook to fetch a single backtest result by ID
 */
export function useBacktest(backtestId: string | null) {
  return useQuery({
    queryKey: backtestKeys.detail(backtestId ?? ''),
    queryFn: async (): Promise<BacktestResult> => {
      if (!backtestId) throw new Error('Backtest ID is required');
      return apiClient.getBacktest(backtestId);
    },
    enabled: !!backtestId,
  });
}

/**
 * Hook to run a new backtest
 */
export function useRunBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (
      request: RunBacktestRequest
    ): Promise<RunBacktestResponse> => {
      return apiClient.runBacktest(request);
    },
    onSuccess: () => {
      // Invalidate backtests list to refetch
      queryClient.invalidateQueries({ queryKey: backtestKeys.lists() });
    },
  });
}

/**
 * Hook to delete a backtest
 */
export function useDeleteBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (backtestId: string): Promise<{ ok: true }> => {
      return apiClient.deleteBacktest(backtestId);
    },
    onSuccess: (_data, backtestId) => {
      // Remove from cache and invalidate list
      queryClient.removeQueries({ queryKey: backtestKeys.detail(backtestId) });
      queryClient.invalidateQueries({ queryKey: backtestKeys.lists() });
    },
  });
}

/**
 * Hook to cancel a running backtest
 */
export function useCancelBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (backtestId: string): Promise<{ ok: true }> => {
      return apiClient.cancelBacktest(backtestId);
    },
    onSuccess: (_data, backtestId) => {
      // Invalidate the specific backtest and list
      queryClient.invalidateQueries({ queryKey: backtestKeys.detail(backtestId) });
      queryClient.invalidateQueries({ queryKey: backtestKeys.lists() });
    },
  });
}
