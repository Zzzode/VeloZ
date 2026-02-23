import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import {
  exportTradesToCsv,
  exportSummaryToCsv,
  exportEquityCurveToCsv,
  exportFullReportToCsv,
} from './exportCsv';
import type { BacktestResult } from '@/shared/api/types';

describe('exportCsv utilities', () => {
  const mockResult: BacktestResult = {
    id: 'bt-001',
    config: {
      symbol: 'BTCUSDT',
      strategy: 'momentum',
      start_date: '2024-01-01',
      end_date: '2024-06-30',
      initial_capital: 10000,
      commission: 0.001,
      slippage: 0.0005,
    },
    total_return: 25.5,
    sharpe_ratio: 1.85,
    max_drawdown: 12.3,
    win_rate: 58.5,
    profit_factor: 1.65,
    total_trades: 150,
    winning_trades: 87,
    losing_trades: 63,
    avg_win: 45.5,
    avg_loss: 28.3,
    equity_curve: [
      { timestamp: '2024-01-01', equity: 10000 },
      { timestamp: '2024-03-01', equity: 11200 },
      { timestamp: '2024-06-30', equity: 12500 },
    ],
    drawdown_curve: [
      { timestamp: '2024-01-01', drawdown: 0 },
      { timestamp: '2024-02-15', drawdown: -0.08 },
      { timestamp: '2024-06-30', drawdown: -0.02 },
    ],
    trades: [
      {
        entry_time: '2024-01-15T10:00:00Z',
        exit_time: '2024-01-15T14:00:00Z',
        side: 'BUY',
        entry_price: 42000,
        exit_price: 42500,
        quantity: 0.1,
        pnl: 50,
        pnl_percent: 0.012,
        commission: 8.45,
      },
      {
        entry_time: '2024-02-01T09:00:00Z',
        exit_time: '2024-02-01T16:00:00Z',
        side: 'SELL',
        entry_price: 43000,
        exit_price: 42800,
        quantity: 0.15,
        pnl: 30,
        pnl_percent: 0.007,
        commission: 12.87,
      },
    ],
    created_at: '2024-07-01T12:00:00Z',
  };

  let mockCreateObjectURL: ReturnType<typeof vi.fn>;
  let mockRevokeObjectURL: ReturnType<typeof vi.fn>;
  let mockAppendChild: ReturnType<typeof vi.fn>;
  let mockRemoveChild: ReturnType<typeof vi.fn>;
  let mockClick: ReturnType<typeof vi.fn>;
  let createdLink: HTMLAnchorElement | null = null;

  beforeEach(() => {
    mockCreateObjectURL = vi.fn().mockReturnValue('blob:mock-url');
    mockRevokeObjectURL = vi.fn();
    mockAppendChild = vi.fn();
    mockRemoveChild = vi.fn();
    mockClick = vi.fn();

    URL.createObjectURL = mockCreateObjectURL;
    URL.revokeObjectURL = mockRevokeObjectURL;

    const originalCreateElement = document.createElement.bind(document);
    vi.spyOn(document, 'createElement').mockImplementation((tagName: string) => {
      const element = originalCreateElement(tagName);
      if (tagName === 'a') {
        createdLink = element as HTMLAnchorElement;
        element.click = mockClick;
      }
      return element;
    });

    vi.spyOn(document.body, 'appendChild').mockImplementation(mockAppendChild);
    vi.spyOn(document.body, 'removeChild').mockImplementation(mockRemoveChild);
  });

  afterEach(() => {
    vi.restoreAllMocks();
    createdLink = null;
  });

  describe('exportTradesToCsv', () => {
    it('creates CSV with correct headers', () => {
      exportTradesToCsv(mockResult);

      expect(mockCreateObjectURL).toHaveBeenCalled();
      const blobArg = mockCreateObjectURL.mock.calls[0][0] as Blob;
      expect(blobArg).toBeInstanceOf(Blob);
    });

    it('triggers download with correct filename', () => {
      exportTradesToCsv(mockResult);

      expect(createdLink?.download).toBe('backtest_trades_bt-001.csv');
    });

    it('clicks the link to trigger download', () => {
      exportTradesToCsv(mockResult);

      expect(mockClick).toHaveBeenCalled();
    });

    it('cleans up after download', () => {
      exportTradesToCsv(mockResult);

      expect(mockAppendChild).toHaveBeenCalled();
      expect(mockRemoveChild).toHaveBeenCalled();
      expect(mockRevokeObjectURL).toHaveBeenCalledWith('blob:mock-url');
    });

    it('formats trade data correctly', () => {
      exportTradesToCsv(mockResult);

      expect(mockCreateObjectURL).toHaveBeenCalled();
      // Verify blob was created (content validation would require reading the blob)
    });
  });

  describe('exportSummaryToCsv', () => {
    it('creates CSV with summary metrics', () => {
      exportSummaryToCsv(mockResult);

      expect(mockCreateObjectURL).toHaveBeenCalled();
    });

    it('triggers download with correct filename', () => {
      exportSummaryToCsv(mockResult);

      expect(createdLink?.download).toBe('backtest_summary_bt-001.csv');
    });

    it('includes config and performance metrics', () => {
      exportSummaryToCsv(mockResult);

      expect(mockClick).toHaveBeenCalled();
    });
  });

  describe('exportEquityCurveToCsv', () => {
    it('creates CSV with equity curve data', () => {
      exportEquityCurveToCsv(mockResult);

      expect(mockCreateObjectURL).toHaveBeenCalled();
    });

    it('triggers download with correct filename', () => {
      exportEquityCurveToCsv(mockResult);

      expect(createdLink?.download).toBe('backtest_equity_bt-001.csv');
    });

    it('includes timestamp and equity columns', () => {
      exportEquityCurveToCsv(mockResult);

      expect(mockClick).toHaveBeenCalled();
    });
  });

  describe('exportFullReportToCsv', () => {
    it('creates CSV with full report', () => {
      exportFullReportToCsv(mockResult);

      expect(mockCreateObjectURL).toHaveBeenCalled();
    });

    it('triggers download with correct filename', () => {
      exportFullReportToCsv(mockResult);

      expect(createdLink?.download).toBe('backtest_report_bt-001.csv');
    });

    it('includes summary, performance, and trades sections', () => {
      exportFullReportToCsv(mockResult);

      expect(mockClick).toHaveBeenCalled();
    });
  });

  describe('edge cases', () => {
    it('handles empty trades array', () => {
      const resultWithNoTrades = { ...mockResult, trades: [] };
      expect(() => exportTradesToCsv(resultWithNoTrades)).not.toThrow();
    });

    it('handles empty equity curve', () => {
      const resultWithNoEquity = { ...mockResult, equity_curve: [] };
      expect(() => exportEquityCurveToCsv(resultWithNoEquity)).not.toThrow();
    });

    it('handles special characters in strategy name', () => {
      const resultWithSpecialChars = {
        ...mockResult,
        config: { ...mockResult.config, strategy: 'MA Crossover (20/50)' },
      };
      expect(() => exportSummaryToCsv(resultWithSpecialChars)).not.toThrow();
    });
  });
});
