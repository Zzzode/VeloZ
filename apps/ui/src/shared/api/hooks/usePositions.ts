/**
 * Positions Hooks
 * TanStack Query hooks for position data
 */

import { useQuery } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';

/**
 * Fetch all positions
 */
export function usePositions() {
  return useQuery({
    queryKey: queryKeys.positions.list(),
    queryFn: async () => {
      const client = getApiClient();
      const response = await client.listPositions();
      return response.positions;
    },
    staleTime: 5000,
  });
}

/**
 * Fetch a single position by symbol
 */
export function usePosition(symbol: string | undefined) {
  return useQuery({
    queryKey: queryKeys.positions.detail(symbol ?? ''),
    queryFn: async () => {
      if (!symbol) throw new Error('Symbol is required');
      const client = getApiClient();
      return client.getPosition(symbol);
    },
    enabled: !!symbol,
    staleTime: 5000,
  });
}
