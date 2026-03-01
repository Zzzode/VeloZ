/**
 * Dashboard Store Tests
 * Tests for Zustand dashboard store: SSE state, price updates
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { useDashboardStore } from '../store/dashboardStore';
import type { MarketSSEEvent, OrderStateSSEEvent, AccountSSEEvent } from '@/shared/api';

describe('dashboardStore', () => {
  // Reset store before each test
  beforeEach(() => {
    useDashboardStore.setState({
      sseConnectionState: 'disconnected',
      lastEventId: null,
      prices: {},
    });
  });

  describe('initial state', () => {
    it('should have disconnected SSE state initially', () => {
      const state = useDashboardStore.getState();
      expect(state.sseConnectionState).toBe('disconnected');
    });

    it('should have null lastEventId initially', () => {
      const state = useDashboardStore.getState();
      expect(state.lastEventId).toBeNull();
    });

    it('should have empty prices initially', () => {
      const state = useDashboardStore.getState();
      expect(state.prices).toEqual({});
    });
  });

  describe('setConnectionState', () => {
    it('should update connection state to connecting', () => {
      useDashboardStore.getState().setConnectionState('connecting');
      expect(useDashboardStore.getState().sseConnectionState).toBe('connecting');
    });

    it('should update connection state to connected', () => {
      useDashboardStore.getState().setConnectionState('connected');
      expect(useDashboardStore.getState().sseConnectionState).toBe('connected');
    });

    it('should update connection state to reconnecting', () => {
      useDashboardStore.getState().setConnectionState('reconnecting');
      expect(useDashboardStore.getState().sseConnectionState).toBe('reconnecting');
    });

    it('should update connection state to disconnected', () => {
      useDashboardStore.getState().setConnectionState('connected');
      useDashboardStore.getState().setConnectionState('disconnected');
      expect(useDashboardStore.getState().sseConnectionState).toBe('disconnected');
    });
  });

  describe('setLastEventId', () => {
    it('should set last event ID', () => {
      useDashboardStore.getState().setLastEventId(12345);
      expect(useDashboardStore.getState().lastEventId).toBe(12345);
    });

    it('should update last event ID on subsequent calls', () => {
      useDashboardStore.getState().setLastEventId(100);
      useDashboardStore.getState().setLastEventId(200);
      expect(useDashboardStore.getState().lastEventId).toBe(200);
    });
  });

  describe('updatePrice', () => {
    it('should add a new price entry', () => {
      const event: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42500.50,
        ts_ns: 1700000000000000000,
      };

      useDashboardStore.getState().updatePrice(event);

      const prices = useDashboardStore.getState().prices;
      expect(prices['BTCUSDT']).toBeDefined();
      expect(prices['BTCUSDT'].symbol).toBe('BTCUSDT');
      expect(prices['BTCUSDT'].price).toBe(42500.50);
      expect(prices['BTCUSDT'].timestamp).toBe(1700000000000000000);
    });

    it('should update existing price entry', () => {
      const event1: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42500.50,
        ts_ns: 1700000000000000000,
      };
      const event2: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42600.00,
        ts_ns: 1700000001000000000,
      };

      useDashboardStore.getState().updatePrice(event1);
      useDashboardStore.getState().updatePrice(event2);

      const prices = useDashboardStore.getState().prices;
      expect(prices['BTCUSDT'].price).toBe(42600.00);
      expect(prices['BTCUSDT'].timestamp).toBe(1700000001000000000);
    });

    it('should handle multiple symbols', () => {
      const btcEvent: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42500.50,
        ts_ns: 1700000000000000000,
      };
      const ethEvent: MarketSSEEvent = {
        type: 'market',
        symbol: 'ETHUSDT',
        price: 2250.75,
        ts_ns: 1700000000000000000,
      };

      useDashboardStore.getState().updatePrice(btcEvent);
      useDashboardStore.getState().updatePrice(ethEvent);

      const prices = useDashboardStore.getState().prices;
      expect(Object.keys(prices)).toHaveLength(2);
      expect(prices['BTCUSDT'].price).toBe(42500.50);
      expect(prices['ETHUSDT'].price).toBe(2250.75);
    });

    it('should preserve other prices when updating one', () => {
      const btcEvent: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42500.50,
        ts_ns: 1700000000000000000,
      };
      const ethEvent: MarketSSEEvent = {
        type: 'market',
        symbol: 'ETHUSDT',
        price: 2250.75,
        ts_ns: 1700000000000000000,
      };
      const btcUpdateEvent: MarketSSEEvent = {
        type: 'market',
        symbol: 'BTCUSDT',
        price: 42600.00,
        ts_ns: 1700000001000000000,
      };

      useDashboardStore.getState().updatePrice(btcEvent);
      useDashboardStore.getState().updatePrice(ethEvent);
      useDashboardStore.getState().updatePrice(btcUpdateEvent);

      const prices = useDashboardStore.getState().prices;
      expect(prices['BTCUSDT'].price).toBe(42600.00);
      expect(prices['ETHUSDT'].price).toBe(2250.75); // Unchanged
    });
  });

  describe('handleOrderUpdate', () => {
    it('should accept order update events without error', () => {
      const event: OrderStateSSEEvent = {
        type: 'order_state',
        client_order_id: 'order-001',
        symbol: 'BTCUSDT',
        side: 'BUY',
        order_qty: 0.1,
        limit_price: 42000,
        status: 'FILLED',
        executed_qty: 0.1,
        avg_price: 41950,
        last_ts_ns: Date.now() * 1_000_000,
      };

      // Should not throw
      expect(() => {
        useDashboardStore.getState().handleOrderUpdate(event);
      }).not.toThrow();
    });
  });

  describe('handleAccountUpdate', () => {
    it('should accept account update events without error', () => {
      const event: AccountSSEEvent = {
        type: 'account',
        ts_ns: Date.now() * 1_000_000,
        balances: [
          { asset: 'USDT', free: 10000, locked: 500 },
          { asset: 'BTC', free: 0.5, locked: 0.1 },
        ],
      };

      // Should not throw
      expect(() => {
        useDashboardStore.getState().handleAccountUpdate(event);
      }).not.toThrow();
    });
  });
});
