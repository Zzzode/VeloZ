import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { BacktestComparison } from './BacktestComparison';
import type { BacktestResult } from '@/shared/api/types';

describe('BacktestComparison', () => {
  const createMockBacktest = (overrides: Partial<BacktestResult> = {}): BacktestResult => ({
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
    equity_curve: [],
    drawdown_curve: [],
    trades: [],
    created_at: '2024-07-01T12:00:00Z',
    ...overrides,
  });

  const mockBacktest1 = createMockBacktest({
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
  });

  const mockBacktest2 = createMockBacktest({
    id: 'bt-002',
    config: {
      symbol: 'ETHUSDT',
      strategy: 'RSI Reversal',
      start_date: '2024-01-01',
      end_date: '2024-06-30',
      initial_capital: 15000,
      commission: 0.002,
      slippage: 0.001,
    },
    total_return: 18.2,
    sharpe_ratio: 2.1,
    max_drawdown: 8.5,
  });

  const mockBacktest3 = createMockBacktest({
    id: 'bt-003',
    config: {
      symbol: 'BTCUSDT',
      strategy: 'Breakout',
      start_date: '2024-01-01',
      end_date: '2024-06-30',
      initial_capital: 20000,
      commission: 0.0015,
      slippage: 0.0008,
    },
    total_return: 32.1,
    sharpe_ratio: 1.5,
    max_drawdown: 15.2,
  });

  describe('empty state', () => {
    it('shows message when no backtests selected', () => {
      render(<BacktestComparison backtests={[]} />);
      expect(screen.getByText(/no backtests selected for comparison/i)).toBeInTheDocument();
    });

    it('shows instruction text when empty', () => {
      render(<BacktestComparison backtests={[]} />);
      expect(screen.getByText(/select backtests from the list/i)).toBeInTheDocument();
    });
  });

  describe('single backtest', () => {
    it('shows message when only one backtest selected', () => {
      render(<BacktestComparison backtests={[mockBacktest1]} />);
      expect(screen.getByText(/select at least 2 backtests to compare/i)).toBeInTheDocument();
    });
  });

  describe('comparison table', () => {
    it('renders comparison table with two backtests', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Backtest Comparison')).toBeInTheDocument();
    });

    it('displays strategy names in headers', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('MA Crossover')).toBeInTheDocument();
      expect(screen.getByText('RSI Reversal')).toBeInTheDocument();
    });

    it('displays symbols in headers', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
      expect(screen.getByText('ETHUSDT')).toBeInTheDocument();
    });

    it('shows comparison count in subtitle', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText(/comparing 2 backtests/i)).toBeInTheDocument();
    });
  });

  describe('metric sections', () => {
    it('renders performance section header', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Performance')).toBeInTheDocument();
    });

    it('renders trade statistics section header', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Trade Statistics')).toBeInTheDocument();
    });

    it('renders configuration section header', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Configuration')).toBeInTheDocument();
    });
  });

  describe('metric rows', () => {
    it('displays total return metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Total Return')).toBeInTheDocument();
    });

    it('displays sharpe ratio metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Sharpe Ratio')).toBeInTheDocument();
    });

    it('displays max drawdown metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Max Drawdown')).toBeInTheDocument();
    });

    it('displays profit factor metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Profit Factor')).toBeInTheDocument();
    });

    it('displays total trades metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Total Trades')).toBeInTheDocument();
    });

    it('displays win rate metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Win Rate')).toBeInTheDocument();
    });

    it('displays initial capital metric', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      expect(screen.getByText('Initial Capital')).toBeInTheDocument();
    });
  });

  describe('remove functionality', () => {
    it('renders remove buttons when onRemove provided', () => {
      const onRemove = vi.fn();
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} onRemove={onRemove} />);

      const removeButtons = screen.getAllByText('Remove');
      expect(removeButtons).toHaveLength(2);
    });

    it('does not render remove buttons when onRemove not provided', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);

      expect(screen.queryByText('Remove')).not.toBeInTheDocument();
    });

    it('calls onRemove with correct id when remove clicked', async () => {
      const user = userEvent.setup();
      const onRemove = vi.fn();
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} onRemove={onRemove} />);

      const removeButtons = screen.getAllByText('Remove');
      await user.click(removeButtons[0]);

      expect(onRemove).toHaveBeenCalledWith('bt-001');
    });
  });

  describe('three backtests comparison', () => {
    it('renders three columns', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2, mockBacktest3]} />);

      expect(screen.getByText('MA Crossover')).toBeInTheDocument();
      expect(screen.getByText('RSI Reversal')).toBeInTheDocument();
      expect(screen.getByText('Breakout')).toBeInTheDocument();
    });

    it('shows correct comparison count', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2, mockBacktest3]} />);
      expect(screen.getByText(/comparing 3 backtests/i)).toBeInTheDocument();
    });
  });

  describe('value formatting', () => {
    it('formats currency values correctly', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      // Initial capital values should be formatted as currency
      expect(screen.getByText('$10,000.00')).toBeInTheDocument();
      expect(screen.getByText('$15,000.00')).toBeInTheDocument();
    });

    it('formats percent values with sign', () => {
      render(<BacktestComparison backtests={[mockBacktest1, mockBacktest2]} />);
      // Total return values
      expect(screen.getByText('+25.50%')).toBeInTheDocument();
      expect(screen.getByText('+18.20%')).toBeInTheDocument();
    });
  });
});
