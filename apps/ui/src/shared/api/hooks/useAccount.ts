/**
 * Account Hooks
 * TanStack Query hooks for account data
 */

import { useQuery } from '@tanstack/react-query';
import { getApiClient } from './client';
import { queryKeys } from './queryKeys';

/**
 * Fetch account balance and info
 */
export function useAccount() {
  return useQuery({
    queryKey: queryKeys.account.balance(),
    queryFn: async () => {
      const client = getApiClient();
      return client.getAccount();
    },
    staleTime: 10000,
  });
}
