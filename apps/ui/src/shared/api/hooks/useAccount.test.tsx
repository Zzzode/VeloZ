/**
 * useAccount Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { useAccount } from './useAccount';

function createTestQueryClient() {
  return new QueryClient({
    defaultOptions: {
      queries: {
        retry: false,
        gcTime: 0,
        staleTime: 0,
      },
    },
  });
}

function createWrapper() {
  const queryClient = createTestQueryClient();
  return function Wrapper({ children }: { children: ReactNode }) {
    return (
      <QueryClientProvider client={queryClient}>
        {children}
      </QueryClientProvider>
    );
  };
}

describe('useAccount', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch account data successfully', async () => {
    const { result } = renderHook(() => useAccount(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data).toBeDefined();
    expect(result.current.data?.balances).toBeDefined();
    expect(result.current.data?.ts_ns).toBeTypeOf('number');
  });

  it('should return balances with correct structure', async () => {
    const { result } = renderHook(() => useAccount(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const balances = result.current.data?.balances;
    expect(balances).toBeInstanceOf(Array);
    expect(balances?.length).toBeGreaterThan(0);

    // Check balance structure
    const usdtBalance = balances?.find(b => b.asset === 'USDT');
    expect(usdtBalance).toBeDefined();
    expect(usdtBalance?.free).toBeTypeOf('number');
    expect(usdtBalance?.locked).toBeTypeOf('number');
  });

  it('should include BTC and ETH balances', async () => {
    const { result } = renderHook(() => useAccount(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const balances = result.current.data?.balances;
    const btcBalance = balances?.find(b => b.asset === 'BTC');
    const ethBalance = balances?.find(b => b.asset === 'ETH');

    expect(btcBalance).toBeDefined();
    expect(ethBalance).toBeDefined();
  });
});
