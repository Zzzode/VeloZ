/**
 * Market Zustand store for client-side state
 */
import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type { ConnectionState, TradeData, BookTopData } from '@/shared/api';

export type Timeframe = '1m' | '5m' | '15m' | '1h' | '4h' | '1d';

export interface MarketPrice {
  price: number;
  timestamp: number;
}

export interface BookTop {
  bidPrice: number;
  bidQty: number;
  askPrice: number;
  askQty: number;
  timestamp: number;
}

export interface RecentTrade {
  price: number;
  qty: number;
  isBuyerMaker: boolean;
  timestamp: number;
}

interface MarketState {
  // Selected symbol and timeframe
  selectedSymbol: string;
  selectedTimeframe: Timeframe;

  // Available symbols
  availableSymbols: string[];

  // Real-time data
  prices: Record<string, MarketPrice>;
  bookTops: Record<string, BookTop>;
  recentTrades: Record<string, RecentTrade[]>;

  // Connection state
  wsConnectionState: ConnectionState;

  // Actions
  setSelectedSymbol: (symbol: string) => void;
  setSelectedTimeframe: (timeframe: Timeframe) => void;
  setAvailableSymbols: (symbols: string[]) => void;
  updatePrice: (symbol: string, price: number, timestamp: number) => void;
  updateBookTop: (symbol: string, data: BookTopData) => void;
  addTrade: (symbol: string, data: TradeData) => void;
  setWsConnectionState: (state: ConnectionState) => void;
}

const MAX_RECENT_TRADES = 50;

export const useMarketStore = create<MarketState>()(
  subscribeWithSelector((set) => ({
    // Initial state
    selectedSymbol: 'BTCUSDT',
    selectedTimeframe: '1h',
    availableSymbols: ['BTCUSDT', 'ETHUSDT', 'BNBUSDT', 'SOLUSDT', 'XRPUSDT'],
    prices: {},
    bookTops: {},
    recentTrades: {},
    wsConnectionState: 'disconnected',

    // Actions
    setSelectedSymbol: (symbol) => set({ selectedSymbol: symbol }),

    setSelectedTimeframe: (timeframe) => set({ selectedTimeframe: timeframe }),

    setAvailableSymbols: (symbols) => set({ availableSymbols: symbols }),

    updatePrice: (symbol, price, timestamp) => set((state) => ({
      prices: {
        ...state.prices,
        [symbol]: { price, timestamp },
      },
    })),

    updateBookTop: (symbol, data) => set((state) => ({
      bookTops: {
        ...state.bookTops,
        [symbol]: {
          bidPrice: data.bid_price,
          bidQty: data.bid_qty,
          askPrice: data.ask_price,
          askQty: data.ask_qty,
          timestamp: data.ts_ns,
        },
      },
    })),

    addTrade: (symbol, data) => set((state) => {
      const existing = state.recentTrades[symbol] ?? [];
      const newTrade: RecentTrade = {
        price: data.price,
        qty: data.qty,
        isBuyerMaker: data.is_buyer_maker,
        timestamp: data.ts_ns,
      };

      return {
        recentTrades: {
          ...state.recentTrades,
          [symbol]: [newTrade, ...existing].slice(0, MAX_RECENT_TRADES),
        },
        // Also update price from trade
        prices: {
          ...state.prices,
          [symbol]: { price: data.price, timestamp: data.ts_ns },
        },
      };
    }),

    setWsConnectionState: (state) => set({ wsConnectionState: state }),
  }))
);

// Selectors
export const selectCurrentPrice = (state: MarketState) =>
  state.prices[state.selectedSymbol];

export const selectCurrentBookTop = (state: MarketState) =>
  state.bookTops[state.selectedSymbol];

export const selectCurrentTrades = (state: MarketState) =>
  state.recentTrades[state.selectedSymbol] ?? [];
