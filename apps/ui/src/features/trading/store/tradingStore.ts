/**
 * Trading Store
 * Zustand store for trading state management
 */

import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type {
  OrderState,
  Position,
  BookTopData,
  OrderSide,
  ConnectionState,
} from '@/shared/api/types';

// =============================================================================
// Types
// =============================================================================

export interface OrderBookLevel {
  price: number;
  qty: number;
  total: number;
}

export interface OrderBookState {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  spread: number;
  spreadPercent: number;
  lastUpdate: number;
}

export interface TradingState {
  // Selected symbol
  selectedSymbol: string;

  // Order book data
  orderBook: OrderBookState;

  // Orders
  orders: OrderState[];

  // Positions
  positions: Position[];

  // Connection states
  sseConnectionState: ConnectionState;
  wsConnectionState: ConnectionState;

  // Order form state
  orderFormSide: OrderSide;
  orderFormPrice: string;
  orderFormQty: string;

  // Loading states
  isLoadingOrders: boolean;
  isLoadingPositions: boolean;
  isPlacingOrder: boolean;

  // Error state
  error: string | null;

  // Actions
  setSelectedSymbol: (symbol: string) => void;
  updateOrderBook: (data: BookTopData) => void;
  setOrders: (orders: OrderState[]) => void;
  updateOrder: (order: Partial<OrderState> & { client_order_id: string }) => void;
  setPositions: (positions: Position[]) => void;
  setSSEConnectionState: (state: ConnectionState) => void;
  setWSConnectionState: (state: ConnectionState) => void;
  setOrderFormSide: (side: OrderSide) => void;
  setOrderFormPrice: (price: string) => void;
  setOrderFormQty: (qty: string) => void;
  setIsLoadingOrders: (loading: boolean) => void;
  setIsLoadingPositions: (loading: boolean) => void;
  setIsPlacingOrder: (loading: boolean) => void;
  setError: (error: string | null) => void;
  resetOrderForm: () => void;
}

// =============================================================================
// Initial State
// =============================================================================

const initialOrderBook: OrderBookState = {
  bids: [],
  asks: [],
  spread: 0,
  spreadPercent: 0,
  lastUpdate: 0,
};

// =============================================================================
// Store
// =============================================================================

export const useTradingStore = create<TradingState>()(
  subscribeWithSelector((set, get) => ({
    // Initial state
    selectedSymbol: 'BTCUSDT',
    orderBook: initialOrderBook,
    orders: [],
    positions: [],
    sseConnectionState: 'disconnected',
    wsConnectionState: 'disconnected',
    orderFormSide: 'BUY',
    orderFormPrice: '',
    orderFormQty: '',
    isLoadingOrders: false,
    isLoadingPositions: false,
    isPlacingOrder: false,
    error: null,

    // Actions
    setSelectedSymbol: (symbol) => set({ selectedSymbol: symbol }),

    updateOrderBook: (data) => {
      const spread = data.ask_price - data.bid_price;
      const spreadPercent = data.bid_price > 0 ? (spread / data.bid_price) * 100 : 0;

      set({
        orderBook: {
          bids: [{ price: data.bid_price, qty: data.bid_qty, total: data.bid_qty }],
          asks: [{ price: data.ask_price, qty: data.ask_qty, total: data.ask_qty }],
          spread,
          spreadPercent,
          lastUpdate: data.ts_ns,
        },
      });
    },

    setOrders: (orders) => set({ orders }),

    updateOrder: (orderUpdate) => {
      const { orders } = get();
      const index = orders.findIndex(
        (o) => o.client_order_id === orderUpdate.client_order_id
      );

      if (index >= 0) {
        const updatedOrders = [...orders];
        updatedOrders[index] = { ...updatedOrders[index], ...orderUpdate };
        set({ orders: updatedOrders });
      } else {
        // New order - add to list if we have enough info
        if (orderUpdate.symbol && orderUpdate.side && orderUpdate.status) {
          set({
            orders: [
              {
                client_order_id: orderUpdate.client_order_id,
                symbol: orderUpdate.symbol,
                side: orderUpdate.side,
                order_qty: orderUpdate.order_qty ?? 0,
                limit_price: orderUpdate.limit_price ?? 0,
                venue_order_id: orderUpdate.venue_order_id ?? '',
                status: orderUpdate.status,
                executed_qty: orderUpdate.executed_qty ?? 0,
                avg_price: orderUpdate.avg_price ?? 0,
                last_ts_ns: orderUpdate.last_ts_ns ?? Date.now() * 1_000_000,
                reason: orderUpdate.reason,
              },
              ...orders,
            ],
          });
        }
      }
    },

    setPositions: (positions) => set({ positions }),

    setSSEConnectionState: (state) => set({ sseConnectionState: state }),

    setWSConnectionState: (state) => set({ wsConnectionState: state }),

    setOrderFormSide: (side) => set({ orderFormSide: side }),

    setOrderFormPrice: (price) => set({ orderFormPrice: price }),

    setOrderFormQty: (qty) => set({ orderFormQty: qty }),

    setIsLoadingOrders: (loading) => set({ isLoadingOrders: loading }),

    setIsLoadingPositions: (loading) => set({ isLoadingPositions: loading }),

    setIsPlacingOrder: (loading) => set({ isPlacingOrder: loading }),

    setError: (error) => set({ error }),

    resetOrderForm: () =>
      set({
        orderFormSide: 'BUY',
        orderFormPrice: '',
        orderFormQty: '',
      }),
  }))
);

// =============================================================================
// Selectors
// =============================================================================

export const selectOpenOrders = (state: TradingState) =>
  state.orders.filter(
    (o) => o.status === 'ACCEPTED' || o.status === 'PARTIALLY_FILLED'
  );

export const selectFilledOrders = (state: TradingState) =>
  state.orders.filter((o) => o.status === 'FILLED');

export const selectTotalUnrealizedPnL = (state: TradingState) =>
  state.positions.reduce((sum, p) => sum + p.unrealized_pnl, 0);

export const selectTotalRealizedPnL = (state: TradingState) =>
  state.positions.reduce((sum, p) => sum + p.realized_pnl, 0);
