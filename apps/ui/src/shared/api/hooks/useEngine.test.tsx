/**
 * useEngine Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor, act } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { useEngineStatus, useStartEngine, useStopEngine } from './useEngine';

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

describe('useEngineStatus', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch engine status successfully', async () => {
    const { result } = renderHook(() => useEngineStatus(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data).toBeDefined();
    expect(result.current.data?.state).toBe('Running');
  });

  it('should return engine status with correct structure', async () => {
    const { result } = renderHook(() => useEngineStatus(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const status = result.current.data;
    expect(status?.state).toBeTypeOf('string');
    expect(status?.uptime_ms).toBeTypeOf('number');
    expect(status?.strategies_loaded).toBeTypeOf('number');
    expect(status?.strategies_running).toBeTypeOf('number');
  });

  it('should have valid engine state', async () => {
    const { result } = renderHook(() => useEngineStatus(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    const validStates = ['Starting', 'Running', 'Stopping', 'Stopped'];
    expect(validStates).toContain(result.current.data?.state);
  });
});

describe('useStartEngine', () => {
  it('should start engine successfully', async () => {
    const { result } = renderHook(() => useStartEngine(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate();
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.message).toContain('started');
  });
});

describe('useStopEngine', () => {
  it('should stop engine successfully', async () => {
    const { result } = renderHook(() => useStopEngine(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate();
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.message).toContain('stopped');
  });
});
