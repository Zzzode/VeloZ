/**
 * VeloZ Order Book Module Tests
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import {
  OrderBook,
  OrderBookConfig,
  PriceLevel,
  renderPriceLevel,
  renderOrderBook,
  renderOrderBookHeader,
} from './orderbook.js';

describe('PriceLevel', () => {
  it('should create a price level with correct properties', () => {
    const level = new PriceLevel(42000.50, 1.5, 'bid');
    expect(level.price).toBe(42000.50);
    expect(level.qty).toBe(1.5);
    expect(level.side).toBe('bid');
    expect(level.total).toBe(1.5);
    expect(level.depthPercent).toBe(0);
    expect(level.isNew).toBe(false);
    expect(level.isUpdated).toBe(false);
  });
});

describe('OrderBook', () => {
  let orderBook;

  beforeEach(() => {
    orderBook = new OrderBook({ throttleMs: 0 }); // Disable throttling for tests
  });

  describe('applyUpdate - snapshot', () => {
    it('should apply a snapshot correctly', () => {
      const snapshot = {
        is_snapshot: true,
        symbol: 'BTCUSDT',
        sequence: 12345,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
          { price: 41998, qty: 1.5 },
        ],
        asks: [
          { price: 42001, qty: 0.5 },
          { price: 42002, qty: 1.0 },
          { price: 42003, qty: 2.0 },
        ],
      };

      orderBook.applyUpdate(snapshot);

      expect(orderBook.symbol).toBe('BTCUSDT');
      expect(orderBook.sequence).toBe(12345);
      expect(orderBook.bids.length).toBe(3);
      expect(orderBook.asks.length).toBe(3);
    });

    it('should sort bids in descending order', () => {
      const snapshot = {
        is_snapshot: true,
        bids: [
          { price: 41998, qty: 1.5 },
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
        ],
        asks: [],
      };

      orderBook.applyUpdate(snapshot);

      expect(orderBook.bids[0].price).toBe(42000);
      expect(orderBook.bids[1].price).toBe(41999);
      expect(orderBook.bids[2].price).toBe(41998);
    });

    it('should sort asks in ascending order', () => {
      const snapshot = {
        is_snapshot: true,
        bids: [],
        asks: [
          { price: 42003, qty: 2.0 },
          { price: 42001, qty: 0.5 },
          { price: 42002, qty: 1.0 },
        ],
      };

      orderBook.applyUpdate(snapshot);

      expect(orderBook.asks[0].price).toBe(42001);
      expect(orderBook.asks[1].price).toBe(42002);
      expect(orderBook.asks[2].price).toBe(42003);
    });

    it('should filter out zero quantity levels', () => {
      const snapshot = {
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 0 },
        ],
        asks: [
          { price: 42001, qty: 0 },
          { price: 42002, qty: 1.0 },
        ],
      };

      orderBook.applyUpdate(snapshot);

      expect(orderBook.bids.length).toBe(1);
      expect(orderBook.asks.length).toBe(1);
    });

    it('should handle array format [price, qty]', () => {
      const snapshot = {
        is_snapshot: true,
        bids: [
          [42000, 1.0],
          [41999, 2.0],
        ],
        asks: [
          [42001, 0.5],
          [42002, 1.0],
        ],
      };

      orderBook.applyUpdate(snapshot);

      expect(orderBook.bids[0].price).toBe(42000);
      expect(orderBook.bids[0].qty).toBe(1.0);
      expect(orderBook.asks[0].price).toBe(42001);
      expect(orderBook.asks[0].qty).toBe(0.5);
    });
  });

  describe('applyUpdate - delta', () => {
    beforeEach(() => {
      // Start with a snapshot
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
        ],
        asks: [
          { price: 42001, qty: 0.5 },
          { price: 42002, qty: 1.0 },
        ],
      });
    });

    it('should update existing level', () => {
      orderBook.applyUpdate({
        bids: [{ price: 42000, qty: 1.5 }],
        asks: [],
      });

      expect(orderBook.bids[0].qty).toBe(1.5);
    });

    it('should add new level', () => {
      orderBook.applyUpdate({
        bids: [{ price: 42001.5, qty: 0.5 }],
        asks: [],
      });

      // New level should be inserted and sorted
      expect(orderBook.bids.length).toBe(3);
      expect(orderBook.bids[0].price).toBe(42001.5);
    });

    it('should remove level with zero quantity', () => {
      // Verify initial state
      expect(orderBook.bids.length).toBe(2);
      expect(orderBook.bids[0].price).toBe(42000);

      orderBook.applyUpdate({
        bids: [{ price: 42000, qty: 0 }],
        asks: [],
      });

      // After removing 42000, only 41999 should remain
      expect(orderBook.bids.length).toBe(1);
      expect(orderBook.bids[0].price).toBe(41999);
    });

    it('should mark updated levels', () => {
      orderBook.applyUpdate({
        bids: [{ price: 42000, qty: 1.5 }],
        asks: [],
      });

      expect(orderBook.bids[0].isUpdated).toBe(true);
    });

    it('should mark new levels', () => {
      orderBook.applyUpdate({
        bids: [{ price: 42001.5, qty: 0.5 }],
        asks: [],
      });

      expect(orderBook.bids[0].isNew).toBe(true);
    });
  });

  describe('cumulative totals', () => {
    it('should calculate cumulative totals for bids', () => {
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
          { price: 41998, qty: 1.5 },
        ],
        asks: [],
      });

      expect(orderBook.bids[0].total).toBe(1.0);
      expect(orderBook.bids[1].total).toBe(3.0);
      expect(orderBook.bids[2].total).toBe(4.5);
    });

    it('should calculate cumulative totals for asks', () => {
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [],
        asks: [
          { price: 42001, qty: 0.5 },
          { price: 42002, qty: 1.0 },
          { price: 42003, qty: 2.0 },
        ],
      });

      expect(orderBook.asks[0].total).toBe(0.5);
      expect(orderBook.asks[1].total).toBe(1.5);
      expect(orderBook.asks[2].total).toBe(3.5);
    });

    it('should calculate depth percentages', () => {
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 1.0 },
        ],
        asks: [
          { price: 42001, qty: 1.0 },
          { price: 42002, qty: 1.0 },
        ],
      });

      // Max depth is 2.0 (cumulative of either side)
      expect(orderBook.bids[0].depthPercent).toBe(50);
      expect(orderBook.bids[1].depthPercent).toBe(100);
      expect(orderBook.asks[0].depthPercent).toBe(50);
      expect(orderBook.asks[1].depthPercent).toBe(100);
    });
  });

  describe('bestBid / bestAsk / spread / midPrice', () => {
    beforeEach(() => {
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
        ],
        asks: [
          { price: 42001, qty: 0.5 },
          { price: 42002, qty: 1.0 },
        ],
      });
    });

    it('should return best bid', () => {
      expect(orderBook.bestBid()).toBe(42000);
    });

    it('should return best ask', () => {
      expect(orderBook.bestAsk()).toBe(42001);
    });

    it('should return spread', () => {
      const spread = orderBook.spread();
      expect(spread).not.toBeNull();
      expect(spread.absolute).toBe(1);
      expect(spread.percent).toBeCloseTo(0.00238, 4);
    });

    it('should return mid price', () => {
      expect(orderBook.midPrice()).toBe(42000.5);
    });

    it('should return null for empty book', () => {
      orderBook.clear();
      expect(orderBook.bestBid()).toBeNull();
      expect(orderBook.bestAsk()).toBeNull();
      expect(orderBook.spread()).toBeNull();
      expect(orderBook.midPrice()).toBeNull();
    });
  });

  describe('listeners', () => {
    it('should notify listeners on update', () => {
      const listener = vi.fn();
      orderBook.addListener(listener);

      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [{ price: 42000, qty: 1.0 }],
        asks: [],
      });

      expect(listener).toHaveBeenCalledWith(orderBook);
    });

    it('should remove listener', () => {
      const listener = vi.fn();
      orderBook.addListener(listener);
      orderBook.removeListener(listener);

      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [{ price: 42000, qty: 1.0 }],
        asks: [],
      });

      expect(listener).not.toHaveBeenCalled();
    });
  });

  describe('clearAnimationFlags', () => {
    it('should clear animation flags', () => {
      orderBook.applyUpdate({
        is_snapshot: true,
        bids: [{ price: 42000, qty: 1.0 }],
        asks: [{ price: 42001, qty: 0.5 }],
      });

      expect(orderBook.bids[0].isNew).toBe(true);
      expect(orderBook.asks[0].isNew).toBe(true);

      orderBook.clearAnimationFlags();

      expect(orderBook.bids[0].isNew).toBe(false);
      expect(orderBook.asks[0].isNew).toBe(false);
    });
  });

  describe('maxLevels', () => {
    it('should limit number of levels', () => {
      const customBook = new OrderBook({ maxLevels: 2, throttleMs: 0 });

      customBook.applyUpdate({
        is_snapshot: true,
        bids: [
          { price: 42000, qty: 1.0 },
          { price: 41999, qty: 2.0 },
          { price: 41998, qty: 1.5 },
          { price: 41997, qty: 1.0 },
        ],
        asks: [],
      });

      expect(customBook.bids.length).toBe(2);
      expect(customBook.bids[0].price).toBe(42000);
      expect(customBook.bids[1].price).toBe(41999);
    });
  });
});

describe('renderPriceLevel', () => {
  it('should render bid level with correct classes', () => {
    const level = new PriceLevel(42000.50, 1.5, 'bid');
    level.total = 1.5;
    level.depthPercent = 50;

    const html = renderPriceLevel(level, 2, 4);

    expect(html).toContain('bid-price');
    expect(html).toContain('bid-depth');
    expect(html).toContain('42000.50');
    expect(html).toContain('1.5000');
    expect(html).toContain('width: 50.0%');
  });

  it('should render ask level with correct classes', () => {
    const level = new PriceLevel(42001.00, 0.5, 'ask');
    level.total = 0.5;
    level.depthPercent = 25;

    const html = renderPriceLevel(level, 2, 4);

    expect(html).toContain('ask-price');
    expect(html).toContain('ask-depth');
    expect(html).toContain('42001.00');
  });

  it('should add animation class for new level', () => {
    const level = new PriceLevel(42000, 1.0, 'bid');
    level.isNew = true;

    const html = renderPriceLevel(level);

    expect(html).toContain('level-new');
  });

  it('should add animation class for updated level', () => {
    const level = new PriceLevel(42000, 1.0, 'bid');
    level.isUpdated = true;

    const html = renderPriceLevel(level);

    expect(html).toContain('level-updated');
  });
});

describe('renderOrderBookHeader', () => {
  it('should render symbol and stats', () => {
    const orderBook = new OrderBook({ throttleMs: 0 });
    orderBook.applyUpdate({
      is_snapshot: true,
      symbol: 'BTCUSDT',
      bids: [{ price: 42000, qty: 1.0 }],
      asks: [{ price: 42001, qty: 0.5 }],
    });

    const html = renderOrderBookHeader(orderBook);

    expect(html).toContain('BTCUSDT');
    expect(html).toContain('Spread');
    expect(html).toContain('Mid');
  });

  it('should handle empty book', () => {
    const orderBook = new OrderBook();

    const html = renderOrderBookHeader(orderBook);

    expect(html).toContain('-');
  });
});

describe('renderOrderBook', () => {
  it('should render full order book', () => {
    const orderBook = new OrderBook({ throttleMs: 0 });
    orderBook.applyUpdate({
      is_snapshot: true,
      symbol: 'BTCUSDT',
      bids: [
        { price: 42000, qty: 1.0 },
        { price: 41999, qty: 2.0 },
      ],
      asks: [
        { price: 42001, qty: 0.5 },
        { price: 42002, qty: 1.0 },
      ],
    });

    const html = renderOrderBook(orderBook);

    expect(html).toContain('orderbook-header');
    expect(html).toContain('orderbook-asks');
    expect(html).toContain('orderbook-bids');
    expect(html).toContain('orderbook-spread');
    expect(html).toContain('42000');
    expect(html).toContain('42001');
  });

  it('should render without header when showHeader is false', () => {
    const orderBook = new OrderBook({ throttleMs: 0 });
    orderBook.applyUpdate({
      is_snapshot: true,
      bids: [{ price: 42000, qty: 1.0 }],
      asks: [{ price: 42001, qty: 0.5 }],
    });

    const html = renderOrderBook(orderBook, { showHeader: false });

    expect(html).not.toContain('orderbook-header');
  });

  it('should show empty state when no levels', () => {
    const orderBook = new OrderBook();

    const html = renderOrderBook(orderBook);

    expect(html).toContain('No asks');
    expect(html).toContain('No bids');
  });
});
