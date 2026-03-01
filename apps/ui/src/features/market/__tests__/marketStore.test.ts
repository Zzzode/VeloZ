/**
 * Tests for Market Zustand store
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { useMarketStore } from '../store/marketStore';
import type { TradeData, BookTopData } from '@/shared/api';

describe('marketStore', () => {
  beforeEach(() => {
    // Reset store to initial state before each test
    useMarketStore.setState({
      selectedSymbol: 'BTCUSDT',
      selectedTimeframe: '1h',
      availableSymbols: ['BTCUSDT', 'ETHUSDT', 'BNBUSDT', 'SOLUSDT', 'XRPUSDT'],
      prices: {},
      bookTops: {},
      recentTrades: {},
      wsConnectionState: 'disconnected',
    });
  });

  describe('initial state', () => {
    it('has correct default values', () => {
      const state = useMarketStore.getState();

      expect(state.selectedSymbol).toBe('BTCUSDT');
      expect(state.selectedTimeframe).toBe('1h');
      expect(state.availableSymbols).toContain('BTCUSDT');
      expect(state.availableSymbols).toContain('ETHUSDT');
      expect(state.wsConnectionState).toBe('disconnected');
    });
  });

  describe('setSelectedSymbol', () => {
    it('updates selected symbol', () => {
      useMarketStore.getState().setSelectedSymbol('ETHUSDT');

      expect(useMarketStore.getState().selectedSymbol).toBe('ETHUSDT');
    });
  });

  describe('setSelectedTimeframe', () => {
    it('updates selected timeframe', () => {
      useMarketStore.getState().setSelectedTimeframe('15m');

      expect(useMarketStore.getState().selectedTimeframe).toBe('15m');
    });

    it('accepts all valid timeframes', () => {
      const timeframes = ['1m', '5m', '15m', '1h', '4h', '1d'] as const;

      timeframes.forEach((tf) => {
        useMarketStore.getState().setSelectedTimeframe(tf);
        expect(useMarketStore.getState().selectedTimeframe).toBe(tf);
      });
    });
  });

  describe('setAvailableSymbols', () => {
    it('updates available symbols list', () => {
      const newSymbols = ['BTCUSDT', 'ETHUSDT', 'DOGEUSDT'];
      useMarketStore.getState().setAvailableSymbols(newSymbols);

      expect(useMarketStore.getState().availableSymbols).toEqual(newSymbols);
    });
  });

  describe('updatePrice', () => {
    it('updates price for a symbol', () => {
      const timestamp = Date.now() * 1_000_000;
      useMarketStore.getState().updatePrice('BTCUSDT', 45000, timestamp);

      const state = useMarketStore.getState();
      expect(state.prices['BTCUSDT']).toEqual({
        price: 45000,
        timestamp,
      });
    });

    it('can update prices for multiple symbols', () => {
      const timestamp = Date.now() * 1_000_000;
      useMarketStore.getState().updatePrice('BTCUSDT', 45000, timestamp);
      useMarketStore.getState().updatePrice('ETHUSDT', 2500, timestamp);

      const state = useMarketStore.getState();
      expect(state.prices['BTCUSDT'].price).toBe(45000);
      expect(state.prices['ETHUSDT'].price).toBe(2500);
    });
  });

  describe('updateBookTop', () => {
    it('updates book top data for a symbol', () => {
      const bookTopData: BookTopData = {
        type: 'book_top',
        symbol: 'BTCUSDT',
        bid_price: 44990,
        bid_qty: 1.5,
        ask_price: 45010,
        ask_qty: 2.0,
        ts_ns: Date.now() * 1_000_000,
      };

      useMarketStore.getState().updateBookTop('BTCUSDT', bookTopData);

      const state = useMarketStore.getState();
      expect(state.bookTops['BTCUSDT']).toEqual({
        bidPrice: 44990,
        bidQty: 1.5,
        askPrice: 45010,
        askQty: 2.0,
        timestamp: bookTopData.ts_ns,
      });
    });
  });

  describe('addTrade', () => {
    it('adds a trade to recent trades', () => {
      const tradeData: TradeData = {
        type: 'trade',
        symbol: 'BTCUSDT',
        price: 45000,
        qty: 0.1,
        is_buyer_maker: false,
        ts_ns: Date.now() * 1_000_000,
      };

      useMarketStore.getState().addTrade('BTCUSDT', tradeData);

      const state = useMarketStore.getState();
      expect(state.recentTrades['BTCUSDT']).toHaveLength(1);
      expect(state.recentTrades['BTCUSDT'][0]).toEqual({
        price: 45000,
        qty: 0.1,
        isBuyerMaker: false,
        timestamp: tradeData.ts_ns,
      });
    });

    it('prepends new trades to the list', () => {
      const trade1: TradeData = {
        type: 'trade',
        symbol: 'BTCUSDT',
        price: 45000,
        qty: 0.1,
        is_buyer_maker: false,
        ts_ns: Date.now() * 1_000_000,
      };

      const trade2: TradeData = {
        type: 'trade',
        symbol: 'BTCUSDT',
        price: 45100,
        qty: 0.2,
        is_buyer_maker: true,
        ts_ns: Date.now() * 1_000_000 + 1000,
      };

      useMarketStore.getState().addTrade('BTCUSDT', trade1);
      useMarketStore.getState().addTrade('BTCUSDT', trade2);

      const state = useMarketStore.getState();
      expect(state.recentTrades['BTCUSDT'][0].price).toBe(45100); // Most recent first
      expect(state.recentTrades['BTCUSDT'][1].price).toBe(45000);
    });

    it('limits trades to 50 per symbol', () => {
      // Add 60 trades
      for (let i = 0; i < 60; i++) {
        const trade: TradeData = {
          type: 'trade',
          symbol: 'BTCUSDT',
          price: 45000 + i,
          qty: 0.1,
          is_buyer_maker: false,
          ts_ns: Date.now() * 1_000_000 + i,
        };
        useMarketStore.getState().addTrade('BTCUSDT', trade);
      }

      const state = useMarketStore.getState();
      expect(state.recentTrades['BTCUSDT']).toHaveLength(50);
    });

    it('also updates price when adding a trade', () => {
      const tradeData: TradeData = {
        type: 'trade',
        symbol: 'BTCUSDT',
        price: 45000,
        qty: 0.1,
        is_buyer_maker: false,
        ts_ns: Date.now() * 1_000_000,
      };

      useMarketStore.getState().addTrade('BTCUSDT', tradeData);

      const state = useMarketStore.getState();
      expect(state.prices['BTCUSDT'].price).toBe(45000);
    });
  });

  describe('setWsConnectionState', () => {
    it('updates WebSocket connection state', () => {
      useMarketStore.getState().setWsConnectionState('connected');

      expect(useMarketStore.getState().wsConnectionState).toBe('connected');
    });

    it('handles all connection states', () => {
      const states = ['disconnected', 'connecting', 'connected', 'reconnecting'] as const;

      states.forEach((state) => {
        useMarketStore.getState().setWsConnectionState(state);
        expect(useMarketStore.getState().wsConnectionState).toBe(state);
      });
    });
  });
});
