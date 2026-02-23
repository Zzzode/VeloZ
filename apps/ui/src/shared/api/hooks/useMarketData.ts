/**
 * Market Data Hooks
 * TanStack Query hooks for market data
 */

import { useQuery } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';

/**
 * Fetch current market data
 * Auto-refreshes every 5 seconds
 */
export function useMarketData(symbol?: string) {
  return useQuery({
    queryKey: queryKeys.market.data(symbol),
    queryFn: async () => {
      const client = getApiClient();
      return client.getMarketData();
    },
    refetchInterval: 5000,
    staleTime: 2000,
  });
}
