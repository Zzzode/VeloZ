import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { BacktestResultsView } from './BacktestResultsView';
import type { BacktestResult } from '@/shared/api/types';
import * as exportUtils from '../utils/exportCsv';

// Mock the export utilities
vi.mock('../utils/exportCsv', () => ({
  exportTradesToCsv: vi.fn(),
  exportSummaryToCsv: vi.fn(),
  exportFullReportToCsv: vi.fn(),
}));

describe('BacktestResultsView', () => {
  const mockResult: BacktestResult = {
    id: 'bt-001',
    config: {
      symbol: 'BTCUSDT',
      strategy: 'MA Crossover',
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
    ],
    created_at: '2024-07-01T12:00:00Z',
  };

  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('rendering', () => {
    it('renders performance summary card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Performance Summary')).toBeInTheDocument();
    });

    it('renders trade statistics card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Trade Statistics')).toBeInTheDocument();
    });

    it('renders equity curve card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Equity Curve')).toBeInTheDocument();
    });

    it('renders drawdown card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Drawdown')).toBeInTheDocument();
    });

    it('renders trade history card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Trade History')).toBeInTheDocument();
    });

    it('renders backtest configuration card', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('Backtest Configuration')).toBeInTheDocument();
    });
  });

  describe('performance metrics', () => {
    it('displays total return with correct formatting', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('+25.50%')).toBeInTheDocument();
    });

    it('displays sharpe ratio', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('1.85')).toBeInTheDocument();
    });

    it('displays max drawdown', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('12.30%')).toBeInTheDocument();
    });

    it('displays win rate', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('58.50%')).toBeInTheDocument();
    });

    it('displays profit factor', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('1.65')).toBeInTheDocument();
    });
  });

  describe('trade statistics', () => {
    it('displays total trades', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('150')).toBeInTheDocument();
    });

    it('displays winning trades', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('87')).toBeInTheDocument();
    });

    it('displays losing trades', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('63')).toBeInTheDocument();
    });
  });

  describe('configuration display', () => {
    it('displays symbol', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
    });

    it('displays strategy name', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('MA Crossover')).toBeInTheDocument();
    });

    it('displays commission percentage', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('0.10%')).toBeInTheDocument();
    });

    it('displays slippage percentage', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByText('0.050%')).toBeInTheDocument();
    });
  });

  describe('export buttons', () => {
    it('renders export summary button', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByRole('button', { name: /export summary/i })).toBeInTheDocument();
    });

    it('renders export trades button', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByRole('button', { name: /export trades/i })).toBeInTheDocument();
    });

    it('renders export full report button', () => {
      render(<BacktestResultsView result={mockResult} />);
      expect(screen.getByRole('button', { name: /export full report/i })).toBeInTheDocument();
    });

    it('calls exportSummaryToCsv when export summary clicked', async () => {
      const user = userEvent.setup();
      render(<BacktestResultsView result={mockResult} />);

      await user.click(screen.getByRole('button', { name: /export summary/i }));

      expect(exportUtils.exportSummaryToCsv).toHaveBeenCalledWith(mockResult);
    });

    it('calls exportTradesToCsv when export trades clicked', async () => {
      const user = userEvent.setup();
      render(<BacktestResultsView result={mockResult} />);

      await user.click(screen.getByRole('button', { name: /export trades/i }));

      expect(exportUtils.exportTradesToCsv).toHaveBeenCalledWith(mockResult);
    });

    it('calls exportFullReportToCsv when export full report clicked', async () => {
      const user = userEvent.setup();
      render(<BacktestResultsView result={mockResult} />);

      await user.click(screen.getByRole('button', { name: /export full report/i }));

      expect(exportUtils.exportFullReportToCsv).toHaveBeenCalledWith(mockResult);
    });
  });

  describe('negative return handling', () => {
    it('displays negative return without plus sign', () => {
      const negativeResult: BacktestResult = {
        ...mockResult,
        total_return: -15.5,
      };
      render(<BacktestResultsView result={negativeResult} />);
      expect(screen.getByText('-15.50%')).toBeInTheDocument();
    });
  });

  describe('win/loss ratio calculation', () => {
    it('calculates win/loss ratio correctly', () => {
      render(<BacktestResultsView result={mockResult} />);
      // 87 / 63 = 1.38
      expect(screen.getByText('1.38')).toBeInTheDocument();
    });

    it('shows N/A when no losing trades', () => {
      const noLossResult: BacktestResult = {
        ...mockResult,
        losing_trades: 0,
      };
      render(<BacktestResultsView result={noLossResult} />);
      expect(screen.getByText('N/A')).toBeInTheDocument();
    });
  });
});
