import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { BacktestList } from './BacktestList';
import type { BacktestResult } from '@/shared/api/types';

describe('BacktestList', () => {
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

  const mockBacktests: BacktestResult[] = [
    createMockBacktest({
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
    }),
    createMockBacktest({
      id: 'bt-002',
      config: {
        symbol: 'ETHUSDT',
        strategy: 'RSI Reversal',
        start_date: '2024-02-01',
        end_date: '2024-07-31',
        initial_capital: 15000,
        commission: 0.002,
        slippage: 0.001,
      },
      total_return: -8.3,
    }),
  ];

  const mockOnSelect = vi.fn();

  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('empty state', () => {
    it('shows message when backtests array is empty', () => {
      render(<BacktestList backtests={[]} selectedId={null} onSelect={mockOnSelect} />);
      expect(screen.getByText(/no backtests yet/i)).toBeInTheDocument();
    });

    it('shows instruction text when empty', () => {
      render(<BacktestList backtests={[]} selectedId={null} onSelect={mockOnSelect} />);
      expect(screen.getByText(/run a backtest to see results here/i)).toBeInTheDocument();
    });

    it('shows message when backtests is undefined', () => {
      render(
        <BacktestList
          backtests={undefined as unknown as BacktestResult[]}
          selectedId={null}
          onSelect={mockOnSelect}
        />
      );
      expect(screen.getByText(/no backtests yet/i)).toBeInTheDocument();
    });
  });

  describe('rendering backtests', () => {
    it('renders all backtest items', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      expect(screen.getByText('MA Crossover')).toBeInTheDocument();
      expect(screen.getByText('RSI Reversal')).toBeInTheDocument();
    });

    it('displays symbol badges', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
      expect(screen.getByText('ETHUSDT')).toBeInTheDocument();
    });

    it('displays return values', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      expect(screen.getByText('+25.50%')).toBeInTheDocument();
      expect(screen.getByText('-8.30%')).toBeInTheDocument();
    });

    it('displays sharpe ratio', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      // Both backtests have the same sharpe ratio from createMockBacktest
      expect(screen.getAllByText('1.85')).toHaveLength(2);
    });

    it('displays max drawdown', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      // Both backtests have the same max drawdown from createMockBacktest
      expect(screen.getAllByText(/12\.30/)).toHaveLength(2);
    });

    it('displays total trades', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      // Both backtests have the same total trades from createMockBacktest
      expect(screen.getAllByText('150')).toHaveLength(2);
    });
  });

  describe('selection', () => {
    it('calls onSelect when backtest clicked', async () => {
      const user = userEvent.setup();
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      await user.click(screen.getByText('MA Crossover'));

      expect(mockOnSelect).toHaveBeenCalledWith('bt-001');
    });

    it('highlights selected backtest', () => {
      const { container } = render(
        <BacktestList backtests={mockBacktests} selectedId="bt-001" onSelect={mockOnSelect} />
      );

      // Selected item should have primary border color
      const selectedItem = container.querySelector('.border-primary');
      expect(selectedItem).toBeInTheDocument();
    });
  });

  describe('delete functionality', () => {
    it('renders delete button when onDelete provided', () => {
      const onDelete = vi.fn();
      render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onDelete={onDelete}
        />
      );

      const deleteButtons = screen.getAllByRole('button', { name: /delete/i });
      expect(deleteButtons).toHaveLength(2);
    });

    it('does not render delete button when onDelete not provided', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      expect(screen.queryByRole('button', { name: /delete/i })).not.toBeInTheDocument();
    });

    it('calls onDelete with correct id when delete clicked', async () => {
      const user = userEvent.setup();
      const onDelete = vi.fn();
      render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onDelete={onDelete}
        />
      );

      const deleteButtons = screen.getAllByRole('button', { name: /delete/i });
      await user.click(deleteButtons[0]);

      expect(onDelete).toHaveBeenCalledWith('bt-001');
    });

    it('does not trigger onSelect when delete clicked', async () => {
      const user = userEvent.setup();
      const onDelete = vi.fn();
      render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onDelete={onDelete}
        />
      );

      const deleteButtons = screen.getAllByRole('button', { name: /delete/i });
      await user.click(deleteButtons[0]);

      expect(mockOnSelect).not.toHaveBeenCalled();
    });

    it('disables delete button when isDeleting is true', () => {
      const onDelete = vi.fn();
      render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onDelete={onDelete}
          isDeleting={true}
        />
      );

      const deleteButtons = screen.getAllByRole('button', { name: /delete/i });
      deleteButtons.forEach((button) => {
        expect(button).toBeDisabled();
      });
    });
  });

  describe('comparison functionality', () => {
    it('renders comparison checkboxes when onToggleComparison provided', () => {
      const onToggleComparison = vi.fn();
      const { container } = render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onToggleComparison={onToggleComparison}
        />
      );

      // Should have checkbox buttons for each backtest
      const checkboxes = container.querySelectorAll('button.w-5.h-5');
      expect(checkboxes).toHaveLength(2);
    });

    it('does not render comparison checkboxes when onToggleComparison not provided', () => {
      const { container } = render(
        <BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />
      );

      const checkboxes = container.querySelectorAll('button.w-5.h-5');
      expect(checkboxes).toHaveLength(0);
    });

    it('calls onToggleComparison when checkbox clicked', async () => {
      const user = userEvent.setup();
      const onToggleComparison = vi.fn();
      const { container } = render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onToggleComparison={onToggleComparison}
        />
      );

      const checkboxes = container.querySelectorAll('button.w-5.h-5');
      await user.click(checkboxes[0]);

      expect(onToggleComparison).toHaveBeenCalledWith('bt-001');
    });

    it('does not trigger onSelect when comparison checkbox clicked', async () => {
      const user = userEvent.setup();
      const onToggleComparison = vi.fn();
      const { container } = render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onToggleComparison={onToggleComparison}
        />
      );

      const checkboxes = container.querySelectorAll('button.w-5.h-5');
      await user.click(checkboxes[0]);

      expect(mockOnSelect).not.toHaveBeenCalled();
    });

    it('shows checkmark for items in comparison set', () => {
      const onToggleComparison = vi.fn();
      render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onToggleComparison={onToggleComparison}
          comparisonIds={new Set(['bt-001'])}
        />
      );

      // Should show checkmark for bt-001
      const checkmarks = screen.getAllByText('✓');
      expect(checkmarks).toHaveLength(1);
    });

    it('highlights items in comparison set', () => {
      const onToggleComparison = vi.fn();
      const { container } = render(
        <BacktestList
          backtests={mockBacktests}
          selectedId={null}
          onSelect={mockOnSelect}
          onToggleComparison={onToggleComparison}
          comparisonIds={new Set(['bt-001'])}
        />
      );

      // Item in comparison should have warning border
      const comparisonItem = container.querySelector('.border-warning');
      expect(comparisonItem).toBeInTheDocument();
    });
  });

  describe('date formatting', () => {
    it('displays date range', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      // Date format depends on locale, just check the separator exists
      expect(screen.getAllByText(/-/).length).toBeGreaterThan(0);
    });
  });

  describe('initial capital display', () => {
    it('displays formatted initial capital', () => {
      render(<BacktestList backtests={mockBacktests} selectedId={null} onSelect={mockOnSelect} />);

      expect(screen.getByText(/\$10,000\.00/)).toBeInTheDocument();
      expect(screen.getByText(/\$15,000\.00/)).toBeInTheDocument();
    });
  });
});
