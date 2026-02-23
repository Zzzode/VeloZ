/**
 * Trading Store Tests
 * Unit tests for Zustand trading store
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { useTradingStore, selectOpenOrders, selectFilledOrders, selectTotalUnrealizedPnL, selectTotalRealizedPnL } from '../store';
import type { OrderState, Position, BookTopData } from '@/shared/api/types';

describe('tradingStore', () => {
  beforeEach(() => {
    // Reset store to initial state before each test
    useTradingStore.setState({
      selectedSymbol: 'BTCUSDT',
      orderBook: {
        bids: [],
        asks: [],
        spread: 0,
        spreadPercent: 0,
        lastUpdate: 0,
      },
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
    });
  });

  describe('setSelectedSymbol', () => {
    it('should update selected symbol', () => {
      const { setSelectedSymbol } = useTradingStore.getState();

      setSelectedSymbol('ETHUSDT');

      expect(useTradingStore.getState().selectedSymbol).toBe('ETHUSDT');
    });
  });

  describe('updateOrderBook', () => {
    it('should update order book from BookTopData', () => {
      const { updateOrderBook } = useTradingStore.getState();

      const bookTopData: BookTopData = {
        type: 'book_top',
        symbol: 'BTCUSDT',
        bid_price: 42000,
        bid_qty: 1.5,
        ask_price: 42010,
        ask_qty: 2.0,
        ts_ns: Date.now() * 1_000_000,
      };

      updateOrderBook(bookTopData);

      const { orderBook } = useTradingStore.getState();
      expect(orderBook.bids).toHaveLength(1);
      expect(orderBook.bids[0].price).toBe(42000);
      expect(orderBook.bids[0].qty).toBe(1.5);
      expect(orderBook.asks).toHaveLength(1);
      expect(orderBook.asks[0].price).toBe(42010);
      expect(orderBook.asks[0].qty).toBe(2.0);
      expect(orderBook.spread).toBe(10);
    });

    it('should calculate spread percentage correctly', () => {
      const { updateOrderBook } = useTradingStore.getState();

      const bookTopData: BookTopData = {
        type: 'book_top',
        symbol: 'BTCUSDT',
        bid_price: 40000,
        bid_qty: 1.0,
        ask_price: 40100,
        ask_qty: 1.0,
        ts_ns: Date.now() * 1_000_000,
      };

      updateOrderBook(bookTopData);

      const { orderBook } = useTradingStore.getState();
      expect(orderBook.spreadPercent).toBeCloseTo(0.25, 2);
    });
  });

  describe('setOrders', () => {
    it('should set orders array', () => {
      const { setOrders } = useTradingStore.getState();

      const orders: OrderState[] = [
        {
          client_order_id: 'order-001',
          symbol: 'BTCUSDT',
          side: 'BUY',
          order_qty: 0.1,
          limit_price: 42000,
          venue_order_id: 'venue-001',
          status: 'FILLED',
          executed_qty: 0.1,
          avg_price: 41950,
          last_ts_ns: Date.now() * 1_000_000,
        },
      ];

      setOrders(orders);

      expect(useTradingStore.getState().orders).toEqual(orders);
    });
  });

  describe('updateOrder', () => {
    it('should update existing order', () => {
      const { setOrders, updateOrder } = useTradingStore.getState();

      const orders: OrderState[] = [
        {
          client_order_id: 'order-001',
          symbol: 'BTCUSDT',
          side: 'BUY',
          order_qty: 0.1,
          limit_price: 42000,
          venue_order_id: 'venue-001',
          status: 'ACCEPTED',
          executed_qty: 0,
          avg_price: 0,
          last_ts_ns: Date.now() * 1_000_000,
        },
      ];

      setOrders(orders);

      updateOrder({
        client_order_id: 'order-001',
        status: 'FILLED',
        executed_qty: 0.1,
        avg_price: 41950,
      });

      const updatedOrders = useTradingStore.getState().orders;
      expect(updatedOrders[0].status).toBe('FILLED');
      expect(updatedOrders[0].executed_qty).toBe(0.1);
      expect(updatedOrders[0].avg_price).toBe(41950);
    });

    it('should add new order if not found and has required fields', () => {
      const { updateOrder } = useTradingStore.getState();

      updateOrder({
        client_order_id: 'order-new',
        symbol: 'BTCUSDT',
        side: 'BUY',
        status: 'ACCEPTED',
        order_qty: 0.5,
        limit_price: 43000,
      });

      const orders = useTradingStore.getState().orders;
      expect(orders).toHaveLength(1);
      expect(orders[0].client_order_id).toBe('order-new');
    });
  });

  describe('setPositions', () => {
    it('should set positions array', () => {
      const { setPositions } = useTradingStore.getState();

      const positions: Position[] = [
        {
          symbol: 'BTCUSDT',
          venue: 'binance',
          side: 'LONG',
          qty: 0.5,
          avg_entry_price: 41000,
          current_price: 42500,
          unrealized_pnl: 750,
          realized_pnl: 1200,
          last_update_ts_ns: Date.now() * 1_000_000,
        },
      ];

      setPositions(positions);

      expect(useTradingStore.getState().positions).toEqual(positions);
    });
  });

  describe('order form actions', () => {
    it('should set order form side', () => {
      const { setOrderFormSide } = useTradingStore.getState();

      setOrderFormSide('SELL');

      expect(useTradingStore.getState().orderFormSide).toBe('SELL');
    });

    it('should set order form price', () => {
      const { setOrderFormPrice } = useTradingStore.getState();

      setOrderFormPrice('42500.50');

      expect(useTradingStore.getState().orderFormPrice).toBe('42500.50');
    });

    it('should set order form quantity', () => {
      const { setOrderFormQty } = useTradingStore.getState();

      setOrderFormQty('0.1234');

      expect(useTradingStore.getState().orderFormQty).toBe('0.1234');
    });

    it('should reset order form', () => {
      const { setOrderFormSide, setOrderFormPrice, setOrderFormQty, resetOrderForm } = useTradingStore.getState();

      setOrderFormSide('SELL');
      setOrderFormPrice('42500');
      setOrderFormQty('0.5');

      resetOrderForm();

      const state = useTradingStore.getState();
      expect(state.orderFormSide).toBe('BUY');
      expect(state.orderFormPrice).toBe('');
      expect(state.orderFormQty).toBe('');
    });
  });

  describe('connection state actions', () => {
    it('should set SSE connection state', () => {
      const { setSSEConnectionState } = useTradingStore.getState();

      setSSEConnectionState('connected');

      expect(useTradingStore.getState().sseConnectionState).toBe('connected');
    });

    it('should set WebSocket connection state', () => {
      const { setWSConnectionState } = useTradingStore.getState();

      setWSConnectionState('reconnecting');

      expect(useTradingStore.getState().wsConnectionState).toBe('reconnecting');
    });
  });

  describe('loading state actions', () => {
    it('should set loading orders state', () => {
      const { setIsLoadingOrders } = useTradingStore.getState();

      setIsLoadingOrders(true);
      expect(useTradingStore.getState().isLoadingOrders).toBe(true);

      setIsLoadingOrders(false);
      expect(useTradingStore.getState().isLoadingOrders).toBe(false);
    });

    it('should set loading positions state', () => {
      const { setIsLoadingPositions } = useTradingStore.getState();

      setIsLoadingPositions(true);
      expect(useTradingStore.getState().isLoadingPositions).toBe(true);
    });

    it('should set placing order state', () => {
      const { setIsPlacingOrder } = useTradingStore.getState();

      setIsPlacingOrder(true);
      expect(useTradingStore.getState().isPlacingOrder).toBe(true);
    });
  });

  describe('error state', () => {
    it('should set error message', () => {
      const { setError } = useTradingStore.getState();

      setError('Failed to place order');
      expect(useTradingStore.getState().error).toBe('Failed to place order');
    });

    it('should clear error', () => {
      const { setError } = useTradingStore.getState();

      setError('Some error');
      setError(null);

      expect(useTradingStore.getState().error).toBeNull();
    });
  });

  describe('selectors', () => {
    it('selectOpenOrders should return only open orders', () => {
      const orders: OrderState[] = [
        { client_order_id: '1', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v1', status: 'ACCEPTED', executed_qty: 0, avg_price: 0, last_ts_ns: 0 },
        { client_order_id: '2', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v2', status: 'PARTIALLY_FILLED', executed_qty: 0.05, avg_price: 42000, last_ts_ns: 0 },
        { client_order_id: '3', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v3', status: 'FILLED', executed_qty: 0.1, avg_price: 42000, last_ts_ns: 0 },
        { client_order_id: '4', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v4', status: 'CANCELLED', executed_qty: 0, avg_price: 0, last_ts_ns: 0 },
      ];

      useTradingStore.setState({ orders });

      const openOrders = selectOpenOrders(useTradingStore.getState());
      expect(openOrders).toHaveLength(2);
      expect(openOrders.map(o => o.status)).toEqual(['ACCEPTED', 'PARTIALLY_FILLED']);
    });

    it('selectFilledOrders should return only filled orders', () => {
      const orders: OrderState[] = [
        { client_order_id: '1', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v1', status: 'ACCEPTED', executed_qty: 0, avg_price: 0, last_ts_ns: 0 },
        { client_order_id: '2', symbol: 'BTCUSDT', side: 'BUY', order_qty: 0.1, limit_price: 42000, venue_order_id: 'v2', status: 'FILLED', executed_qty: 0.1, avg_price: 42000, last_ts_ns: 0 },
        { client_order_id: '3', symbol: 'BTCUSDT', side: 'SELL', order_qty: 0.2, limit_price: 43000, venue_order_id: 'v3', status: 'FILLED', executed_qty: 0.2, avg_price: 43000, last_ts_ns: 0 },
      ];

      useTradingStore.setState({ orders });

      const filledOrders = selectFilledOrders(useTradingStore.getState());
      expect(filledOrders).toHaveLength(2);
      expect(filledOrders.every(o => o.status === 'FILLED')).toBe(true);
    });

    it('selectTotalUnrealizedPnL should sum unrealized P&L', () => {
      const positions: Position[] = [
        { symbol: 'BTCUSDT', venue: 'binance', side: 'LONG', qty: 0.5, avg_entry_price: 41000, current_price: 42000, unrealized_pnl: 500, realized_pnl: 0, last_update_ts_ns: 0 },
        { symbol: 'ETHUSDT', venue: 'binance', side: 'SHORT', qty: 2.0, avg_entry_price: 2300, current_price: 2200, unrealized_pnl: 200, realized_pnl: 0, last_update_ts_ns: 0 },
      ];

      useTradingStore.setState({ positions });

      const totalUnrealized = selectTotalUnrealizedPnL(useTradingStore.getState());
      expect(totalUnrealized).toBe(700);
    });

    it('selectTotalRealizedPnL should sum realized P&L', () => {
      const positions: Position[] = [
        { symbol: 'BTCUSDT', venue: 'binance', side: 'LONG', qty: 0.5, avg_entry_price: 41000, current_price: 42000, unrealized_pnl: 0, realized_pnl: 1200, last_update_ts_ns: 0 },
        { symbol: 'ETHUSDT', venue: 'binance', side: 'SHORT', qty: 2.0, avg_entry_price: 2300, current_price: 2200, unrealized_pnl: 0, realized_pnl: -300, last_update_ts_ns: 0 },
      ];

      useTradingStore.setState({ positions });

      const totalRealized = selectTotalRealizedPnL(useTradingStore.getState());
      expect(totalRealized).toBe(900);
    });
  });
});
