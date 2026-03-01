/**
 * useStrategies Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor, act } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { useStrategies, useStrategy, useStartStrategy, useStopStrategy } from './useStrategies';
import { mockStrategies } from '../../../test/mocks/handlers';

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

describe('useStrategies', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch strategies successfully', async () => {
    const { result } = renderHook(() => useStrategies(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // useStrategies returns strategies array directly
    expect(result.current.data).toHaveLength(mockStrategies.length);
  });

  it('should return strategies with correct structure', async () => {
    const { result } = renderHook(() => useStrategies(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // useStrategies returns strategies array directly
    const strategies = result.current.data;
    const btcStrategy = strategies?.find(s => s.name === 'BTC Momentum');

    expect(btcStrategy).toBeDefined();
    expect(btcStrategy?.id).toBe('strat-001');
    expect(btcStrategy?.type).toBe('momentum');
    expect(btcStrategy?.state).toBe('Running');
    expect(btcStrategy?.pnl).toBeTypeOf('number');
    expect(btcStrategy?.trade_count).toBeTypeOf('number');
  });

  it('should include strategies with different states', async () => {
    const { result } = renderHook(() => useStrategies(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // useStrategies returns strategies array directly
    const strategies = result.current.data;
    const running = strategies?.find(s => s.state === 'Running');
    const paused = strategies?.find(s => s.state === 'Paused');
    const stopped = strategies?.find(s => s.state === 'Stopped');

    expect(running).toBeDefined();
    expect(paused).toBeDefined();
    expect(stopped).toBeDefined();
  });
});

describe('useStrategy', () => {
  it('should fetch single strategy with details', async () => {
    const { result } = renderHook(() => useStrategy('strat-001'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.id).toBe('strat-001');
    expect(result.current.data?.is_running).toBe(true);
    expect(result.current.data?.max_drawdown).toBeDefined();
    expect(result.current.data?.win_rate).toBeDefined();
    expect(result.current.data?.profit_factor).toBeDefined();
  });

  it('should handle non-existent strategy', async () => {
    const { result } = renderHook(() => useStrategy('non-existent'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isError).toBe(true);
    });
  });
});

describe('useStartStrategy', () => {
  it('should start strategy successfully', async () => {
    const { result } = renderHook(() => useStartStrategy(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate('strat-002');
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.message).toContain('started');
  });
});

describe('useStopStrategy', () => {
  it('should stop strategy successfully', async () => {
    const { result } = renderHook(() => useStopStrategy(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate('strat-001');
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.message).toContain('stopped');
  });
});
