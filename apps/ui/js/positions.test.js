/**
 * Unit tests for positions.js module
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import {
  PositionSide,
  PositionConfig,
  PositionData,
  PositionManager,
  renderPositionRow,
  renderPositionsTable,
  renderPnlSummary,
  fetchPositions,
  fetchAccount,
} from './positions.js';

describe('PositionData', () => {
  describe('constructor', () => {
    it('should create position from object data', () => {
      const pos = new PositionData({
        symbol: 'BTCUSDT',
        size: 1.5,
        avg_price: 45000,
        realized_pnl: 100,
        unrealized_pnl: 50,
        current_price: 45500,
      });

      expect(pos.symbol).toBe('BTCUSDT');
      expect(pos.size).toBe(1.5);
      expect(pos.avgPrice).toBe(45000);
      expect(pos.realizedPnl).toBe(100);
      expect(pos.unrealizedPnl).toBe(50);
      expect(pos.currentPrice).toBe(45500);
    });

    it('should handle camelCase field names', () => {
      const pos = new PositionData({
        symbol: 'ETHUSDT',
        size: -2.0,
        avgPrice: 2500,
        realizedPnl: -50,
        unrealizedPnl: 25,
        currentPrice: 2480,
      });

      expect(pos.avgPrice).toBe(2500);
      expect(pos.realizedPnl).toBe(-50);
      expect(pos.unrealizedPnl).toBe(25);
      expect(pos.currentPrice).toBe(2480);
    });

    it('should default missing values to zero', () => {
      const pos = new PositionData({ symbol: 'BTCUSDT' });

      expect(pos.size).toBe(0);
      expect(pos.avgPrice).toBe(0);
      expect(pos.realizedPnl).toBe(0);
      expect(pos.unrealizedPnl).toBe(0);
      expect(pos.currentPrice).toBe(0);
    });

    it('should parse string values to numbers', () => {
      const pos = new PositionData({
        symbol: 'BTCUSDT',
        size: '1.5',
        avg_price: '45000',
      });

      expect(pos.size).toBe(1.5);
      expect(pos.avgPrice).toBe(45000);
    });
  });

  describe('side determination', () => {
    it('should be LONG for positive size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: 1.0 });
      expect(pos.side).toBe(PositionSide.LONG);
    });

    it('should be SHORT for negative size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: -1.0 });
      expect(pos.side).toBe(PositionSide.SHORT);
    });

    it('should be NONE for zero size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: 0 });
      expect(pos.side).toBe(PositionSide.NONE);
    });
  });

  describe('totalPnl', () => {
    it('should sum realized and unrealized PnL', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        realized_pnl: 100,
        unrealized_pnl: 50,
      });
      expect(pos.totalPnl).toBe(150);
    });

    it('should handle negative PnL', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        realized_pnl: -100,
        unrealized_pnl: 30,
      });
      expect(pos.totalPnl).toBe(-70);
    });
  });

  describe('notionalValue', () => {
    it('should calculate notional for long position', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        size: 2.0,
        current_price: 45000,
      });
      expect(pos.notionalValue).toBe(90000);
    });

    it('should use absolute size for short position', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        size: -2.0,
        current_price: 45000,
      });
      expect(pos.notionalValue).toBe(90000);
    });
  });

  describe('pnlPercent', () => {
    it('should calculate PnL percentage', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        size: 1.0,
        avg_price: 40000,
        realized_pnl: 0,
        unrealized_pnl: 5000,
      });
      // 5000 / 40000 * 100 = 12.5%
      expect(pos.pnlPercent).toBeCloseTo(12.5, 2);
    });

    it('should return 0 for zero cost', () => {
      const pos = new PositionData({
        symbol: 'BTC',
        size: 0,
        avg_price: 0,
      });
      expect(pos.pnlPercent).toBe(0);
    });
  });

  describe('isFlat', () => {
    it('should be true for zero size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: 0 });
      expect(pos.isFlat).toBe(true);
    });

    it('should be true for very small size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: 1e-10 });
      expect(pos.isFlat).toBe(true);
    });

    it('should be false for non-zero size', () => {
      const pos = new PositionData({ symbol: 'BTC', size: 0.001 });
      expect(pos.isFlat).toBe(false);
    });
  });
});

describe('PositionManager', () => {
  let manager;

  beforeEach(() => {
    manager = new PositionManager({ throttleMs: 0 }); // Disable throttling for tests
  });

  describe('applyUpdate', () => {
    it('should add new position', () => {
      manager.applyUpdate({
        symbol: 'BTCUSDT',
        size: 1.0,
        avg_price: 45000,
      });

      const pos = manager.getPosition('BTCUSDT');
      expect(pos).not.toBeNull();
      expect(pos.size).toBe(1.0);
    });

    it('should update existing position', () => {
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0 });
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 2.0 });

      const pos = manager.getPosition('BTCUSDT');
      expect(pos.size).toBe(2.0);
    });

    it('should handle array of positions', () => {
      manager.applyUpdate([
        { symbol: 'BTCUSDT', size: 1.0 },
        { symbol: 'ETHUSDT', size: 2.0 },
      ]);

      expect(manager.getPosition('BTCUSDT').size).toBe(1.0);
      expect(manager.getPosition('ETHUSDT').size).toBe(2.0);
    });

    it('should handle { positions: [...] } format', () => {
      manager.applyUpdate({
        positions: [
          { symbol: 'BTCUSDT', size: 1.0 },
          { symbol: 'ETHUSDT', size: 2.0 },
        ],
      });

      expect(manager.getPosition('BTCUSDT').size).toBe(1.0);
      expect(manager.getPosition('ETHUSDT').size).toBe(2.0);
    });

    it('should remove flat positions', () => {
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0 });
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 0 });

      expect(manager.getPosition('BTCUSDT')).toBeNull();
    });
  });

  describe('getAllPositions', () => {
    it('should return all positions as array', () => {
      manager.applyUpdate([
        { symbol: 'BTCUSDT', size: 1.0 },
        { symbol: 'ETHUSDT', size: 2.0 },
      ]);

      const positions = manager.getAllPositions();
      expect(positions).toHaveLength(2);
      expect(positions.map(p => p.symbol).sort()).toEqual(['BTCUSDT', 'ETHUSDT']);
    });

    it('should return empty array when no positions', () => {
      expect(manager.getAllPositions()).toEqual([]);
    });
  });

  describe('getTotals', () => {
    it('should sum PnL across all positions', () => {
      manager.applyUpdate([
        { symbol: 'BTCUSDT', size: 1.0, realized_pnl: 100, unrealized_pnl: 50, current_price: 45000 },
        { symbol: 'ETHUSDT', size: 2.0, realized_pnl: 200, unrealized_pnl: -30, current_price: 2500 },
      ]);

      const totals = manager.getTotals();
      expect(totals.realizedPnl).toBe(300);
      expect(totals.unrealizedPnl).toBe(20);
      expect(totals.totalPnl).toBe(320);
    });

    it('should calculate total notional value', () => {
      manager.applyUpdate([
        { symbol: 'BTCUSDT', size: 1.0, current_price: 45000 },
        { symbol: 'ETHUSDT', size: 2.0, current_price: 2500 },
      ]);

      const totals = manager.getTotals();
      expect(totals.notionalValue).toBe(50000); // 45000 + 5000
    });

    it('should return zeros when no positions', () => {
      const totals = manager.getTotals();
      expect(totals.totalPnl).toBe(0);
      expect(totals.unrealizedPnl).toBe(0);
      expect(totals.realizedPnl).toBe(0);
      expect(totals.notionalValue).toBe(0);
    });
  });

  describe('pnlHistory', () => {
    it('should record PnL history on updates', () => {
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0, realized_pnl: 100, unrealized_pnl: 50 });
      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0, realized_pnl: 100, unrealized_pnl: 100 });

      const history = manager.getPnlHistory();
      expect(history).toHaveLength(2);
      expect(history[0].totalPnl).toBe(150);
      expect(history[1].totalPnl).toBe(200);
    });

    it('should limit history to maxHistoryPoints', () => {
      const smallManager = new PositionManager({ throttleMs: 0, maxHistoryPoints: 3 });

      for (let i = 0; i < 5; i++) {
        smallManager.applyUpdate({ symbol: 'BTC', size: 1.0, unrealized_pnl: i * 10 });
      }

      const history = smallManager.getPnlHistory();
      expect(history).toHaveLength(3);
    });
  });

  describe('listeners', () => {
    it('should notify listeners on update', () => {
      const listener = vi.fn();
      manager.addListener(listener);

      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0 });

      expect(listener).toHaveBeenCalledTimes(1);
      expect(listener).toHaveBeenCalledWith(manager);
    });

    it('should remove listener', () => {
      const listener = vi.fn();
      manager.addListener(listener);
      manager.removeListener(listener);

      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0 });

      expect(listener).not.toHaveBeenCalled();
    });

    it('should handle listener errors gracefully', () => {
      const badListener = vi.fn(() => { throw new Error('test'); });
      const goodListener = vi.fn();

      manager.addListener(badListener);
      manager.addListener(goodListener);

      manager.applyUpdate({ symbol: 'BTCUSDT', size: 1.0 });

      expect(goodListener).toHaveBeenCalled();
    });
  });

  describe('clear', () => {
    it('should remove all positions', () => {
      manager.applyUpdate([
        { symbol: 'BTCUSDT', size: 1.0 },
        { symbol: 'ETHUSDT', size: 2.0 },
      ]);

      manager.clear();

      expect(manager.getAllPositions()).toEqual([]);
      expect(manager.getPnlHistory()).toEqual([]);
    });
  });
});

describe('renderPositionRow', () => {
  it('should render long position row', () => {
    const pos = new PositionData({
      symbol: 'BTCUSDT',
      size: 1.5,
      avg_price: 45000,
      current_price: 46000,
      realized_pnl: 100,
      unrealized_pnl: 1500,
    });

    const html = renderPositionRow(pos);

    expect(html).toContain('BTCUSDT');
    expect(html).toContain('LONG');
    expect(html).toContain('side-long');
    expect(html).toContain('1.5000');
    expect(html).toContain('45000.00');
    expect(html).toContain('46000.00');
  });

  it('should render short position row', () => {
    const pos = new PositionData({
      symbol: 'ETHUSDT',
      size: -2.0,
      avg_price: 2500,
      current_price: 2400,
    });

    const html = renderPositionRow(pos);

    expect(html).toContain('ETHUSDT');
    expect(html).toContain('SHORT');
    expect(html).toContain('side-short');
  });

  it('should apply positive PnL class', () => {
    const pos = new PositionData({
      symbol: 'BTC',
      size: 1.0,
      realized_pnl: 100,
      unrealized_pnl: 50,
    });

    const html = renderPositionRow(pos);

    expect(html).toContain('pnl-positive');
  });

  it('should apply negative PnL class', () => {
    const pos = new PositionData({
      symbol: 'BTC',
      size: 1.0,
      realized_pnl: -100,
      unrealized_pnl: -50,
    });

    const html = renderPositionRow(pos);

    expect(html).toContain('pnl-negative');
  });
});

describe('renderPositionsTable', () => {
  it('should render empty message when no positions', () => {
    const manager = new PositionManager();
    const html = renderPositionsTable(manager);

    expect(html).toContain('No open positions');
    expect(html).toContain('positions-empty');
  });

  it('should render table with positions', () => {
    const manager = new PositionManager({ throttleMs: 0 });
    manager.applyUpdate([
      { symbol: 'BTCUSDT', size: 1.0 },
      { symbol: 'ETHUSDT', size: 2.0 },
    ]);

    const html = renderPositionsTable(manager);

    expect(html).toContain('<table');
    expect(html).toContain('BTCUSDT');
    expect(html).toContain('ETHUSDT');
    expect(html).toContain('<thead>');
    expect(html).toContain('<tbody>');
  });
});

describe('renderPnlSummary', () => {
  it('should render PnL summary', () => {
    const manager = new PositionManager({ throttleMs: 0 });
    manager.applyUpdate([
      { symbol: 'BTCUSDT', size: 1.0, realized_pnl: 100, unrealized_pnl: 50, current_price: 45000 },
    ]);

    const html = renderPnlSummary(manager);

    expect(html).toContain('Total PnL');
    expect(html).toContain('Unrealized');
    expect(html).toContain('Realized');
    expect(html).toContain('Notional');
    expect(html).toContain('pnl-summary');
  });

  it('should apply correct PnL classes', () => {
    const manager = new PositionManager({ throttleMs: 0 });
    manager.applyUpdate([
      { symbol: 'BTCUSDT', size: 1.0, realized_pnl: -100, unrealized_pnl: 50 },
    ]);

    const html = renderPnlSummary(manager);

    // Total is -50, should be negative
    expect(html).toContain('pnl-negative');
  });
});

describe('fetchPositions', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('should fetch positions from API', async () => {
    const mockPositions = [
      { symbol: 'BTCUSDT', size: 1.0 },
      { symbol: 'ETHUSDT', size: 2.0 },
    ];

    global.fetch.mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ positions: mockPositions }),
    });

    const result = await fetchPositions('http://localhost:8080');

    expect(global.fetch).toHaveBeenCalledWith(
      'http://localhost:8080/api/positions',
      { cache: 'no-store' }
    );
    expect(result).toEqual(mockPositions);
  });

  it('should handle array response', async () => {
    const mockPositions = [{ symbol: 'BTCUSDT', size: 1.0 }];

    global.fetch.mockResolvedValue({
      ok: true,
      json: () => Promise.resolve(mockPositions),
    });

    const result = await fetchPositions('http://localhost:8080');
    expect(result).toEqual(mockPositions);
  });

  it('should throw on HTTP error', async () => {
    global.fetch.mockResolvedValue({
      ok: false,
      status: 500,
    });

    await expect(fetchPositions('http://localhost:8080'))
      .rejects.toThrow('Failed to fetch positions: HTTP 500');
  });
});

describe('fetchAccount', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('should fetch account from API', async () => {
    const mockAccount = {
      balance: 10000,
      equity: 10500,
      positions: [],
    };

    global.fetch.mockResolvedValue({
      ok: true,
      json: () => Promise.resolve(mockAccount),
    });

    const result = await fetchAccount('http://localhost:8080');

    expect(global.fetch).toHaveBeenCalledWith(
      'http://localhost:8080/api/account',
      { cache: 'no-store' }
    );
    expect(result).toEqual(mockAccount);
  });

  it('should throw on HTTP error', async () => {
    global.fetch.mockResolvedValue({
      ok: false,
      status: 401,
    });

    await expect(fetchAccount('http://localhost:8080'))
      .rejects.toThrow('Failed to fetch account: HTTP 401');
  });
});
