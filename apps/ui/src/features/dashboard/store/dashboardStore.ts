/**
 * Dashboard Zustand store for real-time SSE data
 */
import { create } from 'zustand';
import type { ConnectionState, MarketSSEEvent, OrderStateSSEEvent, AccountSSEEvent } from '@/shared/api';

interface MarketPrice {
  symbol: string;
  price: number;
  timestamp: number;
  change24h?: number;
}

interface DashboardState {
  // Connection state
  sseConnectionState: ConnectionState;
  lastEventId: number | null;

  // Real-time market data
  prices: Record<string, MarketPrice>;

  // Actions
  setConnectionState: (state: ConnectionState) => void;
  setLastEventId: (id: number) => void;
  updatePrice: (event: MarketSSEEvent) => void;
  handleOrderUpdate: (event: OrderStateSSEEvent) => void;
  handleAccountUpdate: (event: AccountSSEEvent) => void;
}

export const useDashboardStore = create<DashboardState>((set) => ({
  // Initial state
  sseConnectionState: 'disconnected',
  lastEventId: null,
  prices: {},

  // Actions
  setConnectionState: (state) => set({ sseConnectionState: state }),

  setLastEventId: (id) => set({ lastEventId: id }),

  updatePrice: (event) =>
    set((state) => ({
      prices: {
        ...state.prices,
        [event.symbol]: {
          symbol: event.symbol,
          price: event.price,
          timestamp: event.ts_ns,
        },
      },
    })),

  handleOrderUpdate: (_event) => {
    // Order updates are handled by TanStack Query invalidation
    // This is a placeholder for any additional real-time processing
  },

  handleAccountUpdate: (_event) => {
    // Account updates are handled by TanStack Query invalidation
    // This is a placeholder for any additional real-time processing
  },
}));
