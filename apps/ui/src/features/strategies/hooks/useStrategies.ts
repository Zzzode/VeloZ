/**
 * Strategy TanStack Query hooks
 */
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { createApiClient, type StrategySummary } from '@/shared/api';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';
const apiClient = createApiClient(API_BASE);

// Query keys factory
export const strategyKeys = {
  all: ['strategies'] as const,
  lists: () => [...strategyKeys.all, 'list'] as const,
  list: (filters?: string) => [...strategyKeys.lists(), { filters }] as const,
  details: () => [...strategyKeys.all, 'detail'] as const,
  detail: (id: string) => [...strategyKeys.details(), id] as const,
};

/**
 * Fetch all strategies
 */
export function useStrategies() {
  return useQuery({
    queryKey: strategyKeys.lists(),
    queryFn: async () => {
      const response = await apiClient.listStrategies();
      return response.strategies;
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch a single strategy detail
 */
export function useStrategyDetail(id: string | undefined) {
  return useQuery({
    queryKey: strategyKeys.detail(id ?? ''),
    queryFn: async () => {
      if (!id) throw new Error('Strategy ID is required');
      return apiClient.getStrategy(id);
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
      return apiClient.startStrategy(id);
    },
    onSuccess: (_data, id) => {
      // Invalidate both list and detail queries
      queryClient.invalidateQueries({ queryKey: strategyKeys.lists() });
      queryClient.invalidateQueries({ queryKey: strategyKeys.detail(id) });
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
      return apiClient.stopStrategy(id);
    },
    onSuccess: (_data, id) => {
      // Invalidate both list and detail queries
      queryClient.invalidateQueries({ queryKey: strategyKeys.lists() });
      queryClient.invalidateQueries({ queryKey: strategyKeys.detail(id) });
    },
  });
}

/**
 * Get strategy state color class
 */
export function getStrategyStateColor(state: StrategySummary['state']): string {
  switch (state) {
    case 'Running':
      return 'text-success';
    case 'Paused':
      return 'text-warning';
    case 'Stopped':
      return 'text-text-muted';
    default:
      return 'text-text-muted';
  }
}

/**
 * Get strategy state badge classes
 */
export function getStrategyStateBadge(state: StrategySummary['state']): string {
  const base = 'inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium';
  switch (state) {
    case 'Running':
      return `${base} bg-success/10 text-success`;
    case 'Paused':
      return `${base} bg-warning/10 text-warning`;
    case 'Stopped':
      return `${base} bg-gray-100 text-text-muted`;
    default:
      return `${base} bg-gray-100 text-text-muted`;
  }
}
