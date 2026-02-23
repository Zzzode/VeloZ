/**
 * Strategies Hooks Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor, act } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactNode } from 'react';
import {
  useStrategies,
  useStrategyDetail,
  useStartStrategy,
  useStopStrategy,
  getStrategyStateColor,
  getStrategyStateBadge,
  strategyKeys,
} from '../hooks/useStrategies';
import { mockStrategies } from '@/test/mocks/handlers';

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

describe('strategyKeys', () => {
  it('should generate correct query keys', () => {
    expect(strategyKeys.all).toEqual(['strategies']);
    expect(strategyKeys.lists()).toEqual(['strategies', 'list']);
    expect(strategyKeys.list('active')).toEqual(['strategies', 'list', { filters: 'active' }]);
    expect(strategyKeys.details()).toEqual(['strategies', 'detail']);
    expect(strategyKeys.detail('strat-001')).toEqual(['strategies', 'detail', 'strat-001']);
  });
});

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

    expect(result.current.data).toHaveLength(mockStrategies.length);
  });

  it('should return strategies with correct structure', async () => {
    const { result } = renderHook(() => useStrategies(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const strategy = result.current.data?.[0];
    expect(strategy).toHaveProperty('id');
    expect(strategy).toHaveProperty('name');
    expect(strategy).toHaveProperty('type');
    expect(strategy).toHaveProperty('state');
    expect(strategy).toHaveProperty('pnl');
    expect(strategy).toHaveProperty('trade_count');
  });
});

describe('useStrategyDetail', () => {
  it('should fetch strategy detail successfully', async () => {
    const { result } = renderHook(() => useStrategyDetail('strat-001'), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.id).toBe('strat-001');
    expect(result.current.data?.is_running).toBeDefined();
    expect(result.current.data?.max_drawdown).toBeDefined();
  });

  it('should not fetch when id is undefined', () => {
    const { result } = renderHook(() => useStrategyDetail(undefined), {
      wrapper: createWrapper(),
    });

    expect(result.current.fetchStatus).toBe('idle');
  });

  it('should handle non-existent strategy', async () => {
    const { result } = renderHook(() => useStrategyDetail('non-existent'), {
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

describe('getStrategyStateColor', () => {
  it('should return correct color for Running state', () => {
    expect(getStrategyStateColor('Running')).toBe('text-success');
  });

  it('should return correct color for Paused state', () => {
    expect(getStrategyStateColor('Paused')).toBe('text-warning');
  });

  it('should return correct color for Stopped state', () => {
    expect(getStrategyStateColor('Stopped')).toBe('text-text-muted');
  });
});

describe('getStrategyStateBadge', () => {
  it('should return badge classes for Running state', () => {
    const badge = getStrategyStateBadge('Running');
    expect(badge).toContain('bg-success/10');
    expect(badge).toContain('text-success');
    expect(badge).toContain('rounded-full');
  });

  it('should return badge classes for Paused state', () => {
    const badge = getStrategyStateBadge('Paused');
    expect(badge).toContain('bg-warning/10');
    expect(badge).toContain('text-warning');
  });

  it('should return badge classes for Stopped state', () => {
    const badge = getStrategyStateBadge('Stopped');
    expect(badge).toContain('bg-gray-100');
    expect(badge).toContain('text-text-muted');
  });
});
