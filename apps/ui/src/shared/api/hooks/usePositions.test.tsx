/**
 * usePositions Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { usePositions, usePosition } from './usePositions';
import { mockPositions } from '../../../test/mocks/handlers';

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

describe('usePositions', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch positions successfully', async () => {
    const { result } = renderHook(() => usePositions(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data).toBeDefined();
    // usePositions returns positions array directly
    expect(result.current.data).toHaveLength(mockPositions.length);
  });

  it('should return positions with correct structure', async () => {
    const { result } = renderHook(() => usePositions(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // usePositions returns positions array directly
    const positions = result.current.data;
    expect(positions).toBeInstanceOf(Array);

    const btcPosition = positions?.find(p => p.symbol === 'BTCUSDT');
    expect(btcPosition).toBeDefined();
    expect(btcPosition?.side).toBe('LONG');
    expect(btcPosition?.qty).toBeTypeOf('number');
    expect(btcPosition?.unrealized_pnl).toBeTypeOf('number');
  });

  it('should include both LONG and SHORT positions', async () => {
    const { result } = renderHook(() => usePositions(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // usePositions returns positions array directly
    const positions = result.current.data;
    const longPosition = positions?.find(p => p.side === 'LONG');
    const shortPosition = positions?.find(p => p.side === 'SHORT');

    expect(longPosition).toBeDefined();
    expect(shortPosition).toBeDefined();
  });
});

describe('usePosition', () => {
  it('should return specific position by symbol', async () => {
    const { result } = renderHook(() => usePosition('BTCUSDT'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.symbol).toBe('BTCUSDT');
    expect(result.current.data?.venue).toBe('binance');
  });

  it('should handle non-existent position with error', async () => {
    const { result } = renderHook(() => usePosition('XRPUSDT'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isError).toBe(true);
    });
  });
});
