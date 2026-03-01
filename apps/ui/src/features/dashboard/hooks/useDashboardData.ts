/**
 * Dashboard data hooks using TanStack Query
 */
import { useQuery } from '@tanstack/react-query';
import { createApiClient } from '@/shared/api';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';
const apiClient = createApiClient(API_BASE);

// Query keys
export const dashboardKeys = {
  all: ['dashboard'] as const,
  market: () => [...dashboardKeys.all, 'market'] as const,
  account: () => [...dashboardKeys.all, 'account'] as const,
  positions: () => [...dashboardKeys.all, 'positions'] as const,
  orders: () => [...dashboardKeys.all, 'orders'] as const,
  orderStates: () => [...dashboardKeys.all, 'orderStates'] as const,
  strategies: () => [...dashboardKeys.all, 'strategies'] as const,
  engineStatus: () => [...dashboardKeys.all, 'engineStatus'] as const,
  config: () => [...dashboardKeys.all, 'config'] as const,
};

/**
 * Fetch current market data
 */
export function useMarketData() {
  return useQuery({
    queryKey: dashboardKeys.market(),
    queryFn: () => apiClient.getMarketData(),
    staleTime: 1000, // 1 second - market data updates frequently
    refetchInterval: 5000, // Refetch every 5 seconds
  });
}

/**
 * Fetch account balance
 */
export function useAccount() {
  return useQuery({
    queryKey: dashboardKeys.account(),
    queryFn: () => apiClient.getAccount(),
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch open positions
 */
export function usePositions() {
  return useQuery({
    queryKey: dashboardKeys.positions(),
    queryFn: async () => {
      const response = await apiClient.listPositions();
      return response.positions;
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch order states (aggregated orders)
 */
export function useOrderStates() {
  return useQuery({
    queryKey: dashboardKeys.orderStates(),
    queryFn: async () => {
      const response = await apiClient.listOrderStates();
      return response.items;
    },
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch strategies
 */
export function useStrategies() {
  return useQuery({
    queryKey: dashboardKeys.strategies(),
    queryFn: async () => {
      const response = await apiClient.listStrategies();
      return response.strategies;
    },
    staleTime: 5000,
    refetchInterval: 15000,
  });
}

/**
 * Fetch engine status
 */
export function useEngineStatus() {
  return useQuery({
    queryKey: dashboardKeys.engineStatus(),
    queryFn: () => apiClient.getEngineStatus(),
    staleTime: 5000,
    refetchInterval: 10000,
  });
}

/**
 * Fetch gateway config
 */
export function useConfig() {
  return useQuery({
    queryKey: dashboardKeys.config(),
    queryFn: () => apiClient.getConfig(),
    staleTime: 30000, // Config doesn't change often
  });
}
