/**
 * Unit tests for backtest.js module
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import {
  TradeRecord,
  EquityPoint,
  DrawdownPoint,
  BacktestResult,
  RankedResult,
  BacktestManager,
  renderResultCard,
  renderResultsList,
  renderMetricsPanel,
  renderTradesTable,
  getEquityCurveData,
  getDrawdownData,
  renderParameterComparison,
  fetchBacktestResults,
  fetchBacktestResult,
  runBacktest,
} from './backtest.js';

describe('TradeRecord', () => {
  it('should parse trade data', () => {
    const trade = new TradeRecord({
      timestamp: 1708444800000,
      symbol: 'BTCUSDT',
      side: 'buy',
      price: 45000,
      quantity: 0.1,
      fee: 0.45,
      pnl: 100,
    });

    expect(trade.timestamp).toBe(1708444800000);
    expect(trade.symbol).toBe('BTCUSDT');
    expect(trade.side).toBe('buy');
    expect(trade.price).toBe(45000);
    expect(trade.quantity).toBe(0.1);
    expect(trade.fee).toBe(0.45);
    expect(trade.pnl).toBe(100);
  });

  it('should determine win/loss', () => {
    const winTrade = new TradeRecord({ pnl: 100 });
    const loseTrade = new TradeRecord({ pnl: -50 });

    expect(winTrade.isWin).toBe(true);
    expect(loseTrade.isWin).toBe(false);
  });

  it('should determine buy/sell', () => {
    const buyTrade = new TradeRecord({ side: 'buy' });
    const sellTrade = new TradeRecord({ side: 'SELL' });

    expect(buyTrade.isBuy).toBe(true);
    expect(sellTrade.isBuy).toBe(false);
  });
});

describe('EquityPoint', () => {
  it('should parse equity data', () => {
    const point = new EquityPoint({
      timestamp: 1708444800000,
      equity: 10500,
      cumulative_return: 5.0,
    });

    expect(point.timestamp).toBe(1708444800000);
    expect(point.equity).toBe(10500);
    expect(point.cumulativeReturn).toBe(5.0);
  });

  it('should handle camelCase fields', () => {
    const point = new EquityPoint({
      timestamp: 1708444800000,
      equity: 10500,
      cumulativeReturn: 5.0,
    });

    expect(point.cumulativeReturn).toBe(5.0);
  });
});

describe('DrawdownPoint', () => {
  it('should parse drawdown data', () => {
    const point = new DrawdownPoint({
      timestamp: 1708444800000,
      drawdown: -5.5,
    });

    expect(point.timestamp).toBe(1708444800000);
    expect(point.drawdown).toBe(-5.5);
  });
});

describe('BacktestResult', () => {
  const sampleData = {
    id: 'bt-123',
    strategy_name: 'TrendFollowing',
    symbol: 'BTCUSDT',
    start_time: 1704067200000, // 2024-01-01
    end_time: 1706745600000,   // 2024-02-01
    initial_balance: 10000,
    final_balance: 11500,
    total_return: 15.0,
    max_drawdown: 8.5,
    sharpe_ratio: 1.5,
    win_rate: 55.0,
    profit_factor: 1.8,
    trade_count: 50,
    win_count: 28,
    lose_count: 22,
    avg_win: 150,
    avg_lose: 80,
    trades: [
      { timestamp: 1704153600000, symbol: 'BTCUSDT', side: 'buy', price: 42000, quantity: 0.1, pnl: 100 },
    ],
    equity_curve: [
      { timestamp: 1704067200000, equity: 10000, cumulative_return: 0 },
      { timestamp: 1706745600000, equity: 11500, cumulative_return: 15 },
    ],
    drawdown_curve: [
      { timestamp: 1704067200000, drawdown: 0 },
      { timestamp: 1705276800000, drawdown: -8.5 },
    ],
    parameters: { period: 20, threshold: 0.5 },
  };

  it('should parse backtest result data', () => {
    const result = new BacktestResult(sampleData);

    expect(result.id).toBe('bt-123');
    expect(result.strategyName).toBe('TrendFollowing');
    expect(result.symbol).toBe('BTCUSDT');
    expect(result.initialBalance).toBe(10000);
    expect(result.finalBalance).toBe(11500);
    expect(result.totalReturn).toBe(15.0);
    expect(result.maxDrawdown).toBe(8.5);
    expect(result.sharpeRatio).toBe(1.5);
    expect(result.winRate).toBe(55.0);
    expect(result.profitFactor).toBe(1.8);
    expect(result.tradeCount).toBe(50);
    expect(result.winCount).toBe(28);
    expect(result.loseCount).toBe(22);
  });

  it('should parse trades array', () => {
    const result = new BacktestResult(sampleData);

    expect(result.trades).toHaveLength(1);
    expect(result.trades[0]).toBeInstanceOf(TradeRecord);
    expect(result.trades[0].price).toBe(42000);
  });

  it('should parse equity curve', () => {
    const result = new BacktestResult(sampleData);

    expect(result.equityCurve).toHaveLength(2);
    expect(result.equityCurve[0]).toBeInstanceOf(EquityPoint);
    expect(result.equityCurve[1].equity).toBe(11500);
  });

  it('should parse drawdown curve', () => {
    const result = new BacktestResult(sampleData);

    expect(result.drawdownCurve).toHaveLength(2);
    expect(result.drawdownCurve[0]).toBeInstanceOf(DrawdownPoint);
    expect(result.drawdownCurve[1].drawdown).toBe(-8.5);
  });

  it('should calculate profit/loss', () => {
    const result = new BacktestResult(sampleData);
    expect(result.profitLoss).toBe(1500);
  });

  it('should calculate return percentage', () => {
    const result = new BacktestResult(sampleData);
    expect(result.returnPct).toBe(15);
  });

  it('should calculate average trade PnL', () => {
    const result = new BacktestResult(sampleData);
    expect(result.avgTradePnl).toBe(30); // 1500 / 50
  });

  it('should generate ID if not provided', () => {
    const result = new BacktestResult({ strategy_name: 'Test' });
    expect(result.id).toMatch(/^bt-\d+$/);
  });
});

describe('RankedResult', () => {
  it('should parse ranked result', () => {
    const ranked = new RankedResult({
      rank: 1,
      fitness: 1.5,
      parameters: { period: 20 },
      result: { strategy_name: 'Test', total_return: 10 },
    });

    expect(ranked.rank).toBe(1);
    expect(ranked.fitness).toBe(1.5);
    expect(ranked.parameters.period).toBe(20);
    expect(ranked.result).toBeInstanceOf(BacktestResult);
    expect(ranked.result.totalReturn).toBe(10);
  });
});

describe('BacktestManager', () => {
  let manager;

  beforeEach(() => {
    manager = new BacktestManager();
  });

  describe('addResult', () => {
    it('should add result and return BacktestResult', () => {
      const result = manager.addResult({
        strategy_name: 'Test',
        total_return: 10,
      });

      expect(result).toBeInstanceOf(BacktestResult);
      expect(result.strategyName).toBe('Test');
      expect(manager.getAllResults()).toHaveLength(1);
    });
  });

  describe('getResult', () => {
    it('should get result by ID', () => {
      const added = manager.addResult({ id: 'test-1', strategy_name: 'Test' });
      const retrieved = manager.getResult('test-1');

      expect(retrieved).toBe(added);
    });

    it('should return null for unknown ID', () => {
      expect(manager.getResult('unknown')).toBeNull();
    });
  });

  describe('getSortedResults', () => {
    it('should sort results by metric descending', () => {
      manager.addResult({ id: '1', total_return: 5 });
      manager.addResult({ id: '2', total_return: 15 });
      manager.addResult({ id: '3', total_return: 10 });

      const sorted = manager.getSortedResults('totalReturn', true);

      expect(sorted[0].totalReturn).toBe(15);
      expect(sorted[1].totalReturn).toBe(10);
      expect(sorted[2].totalReturn).toBe(5);
    });

    it('should sort results ascending', () => {
      manager.addResult({ id: '1', total_return: 5 });
      manager.addResult({ id: '2', total_return: 15 });

      const sorted = manager.getSortedResults('totalReturn', false);

      expect(sorted[0].totalReturn).toBe(5);
      expect(sorted[1].totalReturn).toBe(15);
    });
  });

  describe('selectResult', () => {
    it('should select result for detailed view', () => {
      manager.addResult({ id: 'test-1' });
      manager.selectResult('test-1');

      expect(manager.selectedId).toBe('test-1');
      expect(manager.getSelectedResult().id).toBe('test-1');
    });
  });

  describe('comparison', () => {
    it('should toggle comparison selection', () => {
      manager.addResult({ id: 'test-1' });
      manager.addResult({ id: 'test-2' });

      manager.toggleComparison('test-1');
      expect(manager.comparisonIds.has('test-1')).toBe(true);

      manager.toggleComparison('test-1');
      expect(manager.comparisonIds.has('test-1')).toBe(false);
    });

    it('should get comparison results', () => {
      manager.addResult({ id: 'test-1' });
      manager.addResult({ id: 'test-2' });
      manager.toggleComparison('test-1');
      manager.toggleComparison('test-2');

      const comparison = manager.getComparisonResults();
      expect(comparison).toHaveLength(2);
    });
  });

  describe('deleteResult', () => {
    it('should delete result', () => {
      manager.addResult({ id: 'test-1' });
      manager.deleteResult('test-1');

      expect(manager.getResult('test-1')).toBeNull();
    });

    it('should clear selection if deleted', () => {
      manager.addResult({ id: 'test-1' });
      manager.selectResult('test-1');
      manager.deleteResult('test-1');

      expect(manager.selectedId).toBeNull();
    });
  });

  describe('clear', () => {
    it('should clear all results', () => {
      manager.addResult({ id: 'test-1' });
      manager.addResult({ id: 'test-2' });
      manager.selectResult('test-1');
      manager.toggleComparison('test-2');

      manager.clear();

      expect(manager.getAllResults()).toHaveLength(0);
      expect(manager.selectedId).toBeNull();
      expect(manager.comparisonIds.size).toBe(0);
    });
  });

  describe('listeners', () => {
    it('should notify listeners on changes', () => {
      const listener = vi.fn();
      manager.addListener(listener);

      manager.addResult({ id: 'test-1' });

      expect(listener).toHaveBeenCalledWith(manager);
    });

    it('should remove listener', () => {
      const listener = vi.fn();
      manager.addListener(listener);
      manager.removeListener(listener);

      manager.addResult({ id: 'test-1' });

      expect(listener).not.toHaveBeenCalled();
    });
  });
});

describe('renderResultCard', () => {
  const sampleResult = new BacktestResult({
    id: 'bt-123',
    strategy_name: 'TrendFollowing',
    symbol: 'BTCUSDT',
    total_return: 15.0,
    sharpe_ratio: 1.5,
    max_drawdown: 8.5,
    win_rate: 55.0,
    trade_count: 50,
    start_time: 1704067200000,
    end_time: 1706745600000,
  });

  it('should render result card', () => {
    const html = renderResultCard(sampleResult);

    expect(html).toContain('TrendFollowing');
    expect(html).toContain('BTCUSDT');
    expect(html).toContain('+15.00%');
    expect(html).toContain('1.50');
    expect(html).toContain('8.50%');
    expect(html).toContain('55.0%');
    expect(html).toContain('50 trades');
  });

  it('should apply selected class', () => {
    const html = renderResultCard(sampleResult, { selected: true });
    expect(html).toContain('selected');
  });

  it('should apply comparing class', () => {
    const html = renderResultCard(sampleResult, { comparing: true });
    expect(html).toContain('comparing');
  });

  it('should show negative return correctly', () => {
    const negativeResult = new BacktestResult({
      strategy_name: 'Test',
      total_return: -10.0,
    });
    const html = renderResultCard(negativeResult);

    expect(html).toContain('-10.00%');
    expect(html).toContain('negative');
  });
});

describe('renderResultsList', () => {
  it('should render empty message when no results', () => {
    const manager = new BacktestManager();
    const html = renderResultsList(manager);

    expect(html).toContain('No backtest results');
    expect(html).toContain('backtest-empty');
  });

  it('should render list of result cards', () => {
    const manager = new BacktestManager();
    manager.addResult({ id: '1', strategy_name: 'Strategy1' });
    manager.addResult({ id: '2', strategy_name: 'Strategy2' });

    const html = renderResultsList(manager);

    expect(html).toContain('Strategy1');
    expect(html).toContain('Strategy2');
    expect(html).toContain('backtest-results-list');
  });
});

describe('renderMetricsPanel', () => {
  it('should render empty message when no result', () => {
    const html = renderMetricsPanel(null);
    expect(html).toContain('Select a backtest');
  });

  it('should render metrics panel', () => {
    const result = new BacktestResult({
      strategy_name: 'Test',
      total_return: 15.0,
      initial_balance: 10000,
      final_balance: 11500,
      sharpe_ratio: 1.5,
      max_drawdown: 8.5,
      profit_factor: 1.8,
      trade_count: 50,
      win_rate: 55.0,
      win_count: 28,
      lose_count: 22,
    });

    const html = renderMetricsPanel(result);

    expect(html).toContain('Performance');
    expect(html).toContain('Risk Metrics');
    expect(html).toContain('Trade Statistics');
    expect(html).toContain('+15.00%');
    expect(html).toContain('1.500');
    expect(html).toContain('8.50%');
  });
});

describe('renderTradesTable', () => {
  it('should render empty message when no trades', () => {
    const result = new BacktestResult({ trades: [] });
    const html = renderTradesTable(result);

    expect(html).toContain('No trades to display');
  });

  it('should render trades table', () => {
    const result = new BacktestResult({
      trades: [
        { timestamp: 1704153600000, symbol: 'BTCUSDT', side: 'buy', price: 42000, quantity: 0.1, fee: 0.42, pnl: 100 },
        { timestamp: 1704240000000, symbol: 'BTCUSDT', side: 'sell', price: 43000, quantity: 0.1, fee: 0.43, pnl: -50 },
      ],
    });

    const html = renderTradesTable(result);

    expect(html).toContain('trades-table');
    expect(html).toContain('BTCUSDT');
    expect(html).toContain('BUY');
    expect(html).toContain('SELL');
    expect(html).toContain('42000.00');
    expect(html).toContain('+100.00');
    expect(html).toContain('-50.00');
  });

  it('should paginate trades', () => {
    const trades = Array.from({ length: 50 }, (_, i) => ({
      timestamp: 1704153600000 + i * 86400000,
      symbol: 'BTCUSDT',
      side: 'buy',
      price: 42000,
      quantity: 0.1,
      pnl: 10,
    }));

    const result = new BacktestResult({ trades });
    const html = renderTradesTable(result, { page: 0, pageSize: 20 });

    expect(html).toContain('Page 1 of 3');
    expect(html).toContain('50 total trades');
  });
});

describe('getEquityCurveData', () => {
  it('should return empty data when no curve', () => {
    const result = new BacktestResult({});
    const data = getEquityCurveData(result);

    expect(data.labels).toEqual([]);
    expect(data.values).toEqual([]);
    expect(data.returns).toEqual([]);
  });

  it('should extract equity curve data', () => {
    const result = new BacktestResult({
      equity_curve: [
        { timestamp: 1704067200000, equity: 10000, cumulative_return: 0 },
        { timestamp: 1706745600000, equity: 11500, cumulative_return: 15 },
      ],
    });

    const data = getEquityCurveData(result);

    expect(data.values).toEqual([10000, 11500]);
    expect(data.returns).toEqual([0, 15]);
    expect(data.labels).toHaveLength(2);
  });
});

describe('getDrawdownData', () => {
  it('should return empty data when no curve', () => {
    const result = new BacktestResult({});
    const data = getDrawdownData(result);

    expect(data.labels).toEqual([]);
    expect(data.values).toEqual([]);
  });

  it('should extract drawdown data', () => {
    const result = new BacktestResult({
      drawdown_curve: [
        { timestamp: 1704067200000, drawdown: 0 },
        { timestamp: 1705276800000, drawdown: -8.5 },
      ],
    });

    const data = getDrawdownData(result);

    expect(data.values).toEqual([0, -8.5]);
    expect(data.labels).toHaveLength(2);
  });
});

describe('renderParameterComparison', () => {
  it('should render empty message when no results', () => {
    const html = renderParameterComparison([]);
    expect(html).toContain('Select results to compare');
  });

  it('should render comparison table', () => {
    const results = [
      new BacktestResult({
        strategy_name: 'Strategy1',
        symbol: 'BTCUSDT',
        total_return: 15,
        sharpe_ratio: 1.5,
        max_drawdown: 8,
        win_rate: 55,
        trade_count: 50,
        parameters: { period: 20, threshold: 0.5 },
      }),
      new BacktestResult({
        strategy_name: 'Strategy2',
        symbol: 'ETHUSDT',
        total_return: 10,
        sharpe_ratio: 1.2,
        max_drawdown: 10,
        win_rate: 50,
        trade_count: 40,
        parameters: { period: 30, threshold: 0.3 },
      }),
    ];

    const html = renderParameterComparison(results);

    expect(html).toContain('Strategy1');
    expect(html).toContain('Strategy2');
    expect(html).toContain('Return');
    expect(html).toContain('Sharpe');
    expect(html).toContain('period');
    expect(html).toContain('threshold');
    expect(html).toContain('20');
    expect(html).toContain('30');
  });
});

describe('API functions', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  describe('fetchBacktestResults', () => {
    it('should fetch results from API', async () => {
      const mockResults = [
        { id: '1', strategy_name: 'Test1' },
        { id: '2', strategy_name: 'Test2' },
      ];

      global.fetch.mockResolvedValue({
        ok: true,
        json: () => Promise.resolve({ results: mockResults }),
      });

      const results = await fetchBacktestResults('http://localhost:8080');

      expect(global.fetch).toHaveBeenCalledWith(
        'http://localhost:8080/api/backtest/results',
        { cache: 'no-store' }
      );
      expect(results).toEqual(mockResults);
    });

    it('should throw on HTTP error', async () => {
      global.fetch.mockResolvedValue({ ok: false, status: 500 });

      await expect(fetchBacktestResults('http://localhost:8080'))
        .rejects.toThrow('Failed to fetch backtest results: HTTP 500');
    });
  });

  describe('fetchBacktestResult', () => {
    it('should fetch single result', async () => {
      const mockResult = { id: 'bt-123', strategy_name: 'Test' };

      global.fetch.mockResolvedValue({
        ok: true,
        json: () => Promise.resolve(mockResult),
      });

      const result = await fetchBacktestResult('http://localhost:8080', 'bt-123');

      expect(global.fetch).toHaveBeenCalledWith(
        'http://localhost:8080/api/backtest/results/bt-123',
        { cache: 'no-store' }
      );
      expect(result).toEqual(mockResult);
    });
  });

  describe('runBacktest', () => {
    it('should run backtest via API', async () => {
      const config = { strategy_name: 'Test', symbol: 'BTCUSDT' };
      const mockResult = { id: 'bt-new', total_return: 10 };

      global.fetch.mockResolvedValue({
        ok: true,
        json: () => Promise.resolve(mockResult),
      });

      const result = await runBacktest('http://localhost:8080', config);

      expect(global.fetch).toHaveBeenCalledWith(
        'http://localhost:8080/api/backtest/run',
        {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config),
        }
      );
      expect(result).toEqual(mockResult);
    });

    it('should throw on HTTP error', async () => {
      global.fetch.mockResolvedValue({ ok: false, status: 400 });

      await expect(runBacktest('http://localhost:8080', {}))
        .rejects.toThrow('Failed to run backtest: HTTP 400');
    });
  });
});
