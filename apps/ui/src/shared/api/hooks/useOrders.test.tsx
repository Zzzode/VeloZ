/**
 * useOrders Hook Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, waitFor, act } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { useOrders, useOrderStates, usePlaceOrder, useCancelOrder } from './useOrders';
import { mockOrders, mockOrderStates } from '../../../test/mocks/handlers';

// Create a fresh QueryClient for each test
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

describe('useOrders', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should fetch orders successfully', async () => {
    const { result } = renderHook(() => useOrders(), {
      wrapper: createWrapper(),
    });

    expect(result.current.isLoading).toBe(true);

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data).toBeDefined();
    // useOrders returns items array directly
    expect(result.current.data).toHaveLength(mockOrders.length);
    expect(result.current.data?.[0].client_order_id).toBe('order-001');
  });
});

describe('useOrderStates', () => {
  it('should fetch order states successfully', async () => {
    const { result } = renderHook(() => useOrderStates(), {
      wrapper: createWrapper(),
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    // useOrderStates returns items array directly
    expect(result.current.data).toHaveLength(mockOrderStates.length);
    expect(result.current.data?.[0].status).toBe('FILLED');
  });
});

describe('usePlaceOrder', () => {
  it('should place order successfully', async () => {
    const { result } = renderHook(() => usePlaceOrder(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate({
        side: 'BUY',
        symbol: 'BTCUSDT',
        qty: 0.1,
        price: 42000,
      });
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.client_order_id).toBeDefined();
    expect(result.current.data?.venue_order_id).toBeDefined();
  });

  it('should use provided client_order_id', async () => {
    const { result } = renderHook(() => usePlaceOrder(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate({
        side: 'SELL',
        symbol: 'ETHUSDT',
        qty: 1.0,
        price: 2200,
        client_order_id: 'my-custom-order-id',
      });
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.client_order_id).toBe('my-custom-order-id');
  });
});

describe('useCancelOrder', () => {
  it('should cancel order successfully', async () => {
    const { result } = renderHook(() => useCancelOrder(), {
      wrapper: createWrapper(),
    });

    await act(async () => {
      result.current.mutate({ client_order_id: 'order-001' });
    });

    await waitFor(() => {
      expect(result.current.isSuccess).toBe(true);
    });

    expect(result.current.data?.ok).toBe(true);
    expect(result.current.data?.client_order_id).toBe('order-001');
  });
});
