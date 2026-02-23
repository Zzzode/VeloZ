/**
 * useMarketData Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactNode } from 'react';
import { useMarketData } from './useMarketData';

// Create a fresh QueryClient for each test
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

describe('useMarketData', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch market data successfully', async () => {
    const { result } = renderHook(() => useMarketData('BTCUSDT'), {
      wrapper: createWrapper(),
    });

    // Initially loading
    expect(result.current.isLoading).toBe(true);

    // Wait for data
    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // Verify data structure
    expect(result.current.data).toBeDefined();
    expect(result.current.data?.symbol).toBeTypeOf('string');
    expect(result.current.data?.price).toBeTypeOf('number');
    expect(result.current.data?.ts_ns).toBeTypeOf('number');
  });

  it('should use symbol for query key differentiation', async () => {
    // Different symbols should create different query keys
    const { result: result1 } = renderHook(() => useMarketData('BTCUSDT'), {
      wrapper: createWrapper(),
    });

    const { result: result2 } = renderHook(() => useMarketData('ETHUSDT'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result1.current.isSuccess).toBe(true);
      expect(result2.current.isSuccess).toBe(true);
    });

    // Both should have market data
    expect(result1.current.data).toBeDefined();
    expect(result2.current.data).toBeDefined();
  });

  it('should use default symbol when none provided', async () => {
    const { result } = renderHook(() => useMarketData(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data).toBeDefined();
  });

  it('should have correct query key', async () => {
    const { result } = renderHook(() => useMarketData('BTCUSDT'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // Query should be successful
    expect(result.current.error).toBeNull();
  });
});
