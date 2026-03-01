/**
 * useBacktest Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor, act } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { useBacktests, useBacktest, useRunBacktest, useCancelBacktest, useDeleteBacktest } from './useBacktest';
import { mockBacktests } from '../../../test/mocks/handlers';

function createTestQueryClient() {
  return new QueryClient({
    defaultOptions: {
      queries: {
        retry: false,
        gcTime: 0,
        staleTime: 0,
      },
      mutations: {
        retry: false,
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

describe('useBacktests', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch backtests successfully', async () => {
    const { result } = renderHook(() => useBacktests(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.backtests).toHaveLength(mockBacktests.length);
    expect(result.current.data?.total).toBe(mockBacktests.length);
  });

  it('should return backtests with correct structure', async () => {
    const { result } = renderHook(() => useBacktests(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const backtest = result.current.data?.backtests[0];
    expect(backtest?.id).toBe('bt-001');
    expect(backtest?.config).toBeDefined();
    expect(backtest?.total_return).toBeTypeOf('number');
    expect(backtest?.sharpe_ratio).toBeTypeOf('number');
    expect(backtest?.max_drawdown).toBeTypeOf('number');
    expect(backtest?.equity_curve).toBeInstanceOf(Array);
    expect(backtest?.trades).toBeInstanceOf(Array);
  });
});

describe('useBacktest', () => {
  it('should fetch single backtest with full details', async () => {
    const { result } = renderHook(() => useBacktest('bt-001'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.id).toBe('bt-001');
    expect(result.current.data?.config.symbol).toBe('BTCUSDT');
    expect(result.current.data?.config.strategy).toBe('momentum');
    expect(result.current.data?.config.initial_capital).toBe(10000);
  });

  it('should handle non-existent backtest', async () => {
    const { result } = renderHook(() => useBacktest('non-existent'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isError).toBe(true);
    });
  });

  it('should not fetch when id is undefined', async () => {
    const { result } = renderHook(() => useBacktest(undefined), {
      wrapper: createWrapper(),
    });

    // Should not be loading since query is disabled
    expect(result.current.fetchStatus).toBe('idle');
  });
});

describe('useRunBacktest', () => {
  it('should run backtest successfully', async () => {
    const { result } = renderHook(() => useRunBacktest(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate({
        config: {
          symbol: 'BTCUSDT',
          strategy: 'momentum',
          start_date: '2024-01-01',
          end_date: '2024-06-30',
          initial_capital: 10000,
          commission: 0.001,
          slippage: 0.0005,
        },
      });
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.backtest_id).toBeDefined();
    expect(result.current.data?.status).toBe('queued');
  });
});

describe('useCancelBacktest', () => {
  it('should cancel backtest successfully', async () => {
    const { result } = renderHook(() => useCancelBacktest(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate('bt-001');
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
  });
});

describe('useDeleteBacktest', () => {
  it('should delete backtest successfully', async () => {
    const { result } = renderHook(() => useDeleteBacktest(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate('bt-001');
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
  });
});
