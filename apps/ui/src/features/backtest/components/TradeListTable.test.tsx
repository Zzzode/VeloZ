import { describe, it, expect } from 'vitest';
import { render, screen } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { TradeListTable } from './TradeListTable';
import type { TradeRecord } from '@/shared/api/types';

describe('TradeListTable', () => {
  const mockTrades: TradeRecord[] = [
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
    {
      entry_time: '2024-02-15T11:00:00Z',
      exit_time: '2024-02-15T15:00:00Z',
      side: 'BUY',
      entry_price: 44000,
      exit_price: 43500,
      quantity: 0.2,
      pnl: -100,
      pnl_percent: -0.023,
      commission: 17.5,
    },
  ];

  describe('rendering', () => {
    it('renders table with headers', () => {
      render(<TradeListTable trades={mockTrades} />);

      expect(screen.getByText('#')).toBeInTheDocument();
      expect(screen.getByText('Entry')).toBeInTheDocument();
      expect(screen.getByText('Exit')).toBeInTheDocument();
      expect(screen.getByText('Side')).toBeInTheDocument();
      expect(screen.getByText('Entry Price')).toBeInTheDocument();
      expect(screen.getByText('Exit Price')).toBeInTheDocument();
      expect(screen.getByText('Qty')).toBeInTheDocument();
      expect(screen.getByText('PnL')).toBeInTheDocument();
      expect(screen.getByText('PnL %')).toBeInTheDocument();
    });

    it('renders trade rows', () => {
      render(<TradeListTable trades={mockTrades} />);

      // Check for side indicators (there are 2 BUY trades and 1 SELL trade)
      expect(screen.getAllByText('BUY')).toHaveLength(2);
      expect(screen.getByText('SELL')).toBeInTheDocument();
    });

    it('renders trade numbers', () => {
      render(<TradeListTable trades={mockTrades} />);

      expect(screen.getByText('1')).toBeInTheDocument();
      expect(screen.getByText('2')).toBeInTheDocument();
      expect(screen.getByText('3')).toBeInTheDocument();
    });
  });

  describe('empty state', () => {
    it('shows message when trades array is empty', () => {
      render(<TradeListTable trades={[]} />);
      expect(screen.getByText(/no trades to display/i)).toBeInTheDocument();
    });
  });

  describe('pagination', () => {
    const manyTrades: TradeRecord[] = Array.from({ length: 25 }, (_, i) => ({
      entry_time: `2024-01-${String(i + 1).padStart(2, '0')}T10:00:00Z`,
      exit_time: `2024-01-${String(i + 1).padStart(2, '0')}T14:00:00Z`,
      side: i % 2 === 0 ? 'BUY' : 'SELL',
      entry_price: 40000 + i * 100,
      exit_price: 40500 + i * 100,
      quantity: 0.1,
      pnl: 50 + i,
      pnl_percent: 0.01 + i * 0.001,
      commission: 8,
    }));

    it('shows pagination controls when trades exceed page size', () => {
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      expect(screen.getByRole('button', { name: /previous/i })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: /next/i })).toBeInTheDocument();
    });

    it('shows correct count text', () => {
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      expect(screen.getByText(/showing 1 - 10 of 25 trades/i)).toBeInTheDocument();
    });

    it('disables previous button on first page', () => {
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      expect(screen.getByRole('button', { name: /previous/i })).toBeDisabled();
    });

    it('enables next button on first page', () => {
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      expect(screen.getByRole('button', { name: /next/i })).not.toBeDisabled();
    });

    it('navigates to next page', async () => {
      const user = userEvent.setup();
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      await user.click(screen.getByRole('button', { name: /next/i }));

      expect(screen.getByText(/showing 11 - 20 of 25 trades/i)).toBeInTheDocument();
    });

    it('navigates back to previous page', async () => {
      const user = userEvent.setup();
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      await user.click(screen.getByRole('button', { name: /next/i }));
      await user.click(screen.getByRole('button', { name: /previous/i }));

      expect(screen.getByText(/showing 1 - 10 of 25 trades/i)).toBeInTheDocument();
    });

    it('disables next button on last page', async () => {
      const user = userEvent.setup();
      render(<TradeListTable trades={manyTrades} pageSize={10} />);

      await user.click(screen.getByRole('button', { name: /next/i }));
      await user.click(screen.getByRole('button', { name: /next/i }));

      expect(screen.getByRole('button', { name: /next/i })).toBeDisabled();
    });

    it('does not show pagination when trades fit on one page', () => {
      render(<TradeListTable trades={mockTrades} pageSize={10} />);

      expect(screen.queryByRole('button', { name: /previous/i })).not.toBeInTheDocument();
      expect(screen.queryByRole('button', { name: /next/i })).not.toBeInTheDocument();
    });
  });

  describe('trade display', () => {
    it('displays profitable trades with positive styling', () => {
      render(<TradeListTable trades={mockTrades} />);

      // Check that positive PnL values have + prefix
      const pnlCells = screen.getAllByText(/\+\$50\.00/);
      expect(pnlCells.length).toBeGreaterThan(0);
    });

    it('displays losing trades with negative styling', () => {
      render(<TradeListTable trades={mockTrades} />);

      // Check that negative PnL values are displayed
      expect(screen.getByText('-$100.00')).toBeInTheDocument();
    });

    it('formats PnL percentage correctly', () => {
      render(<TradeListTable trades={mockTrades} />);

      // Positive percentage
      expect(screen.getByText('+1.20%')).toBeInTheDocument();
      // Negative percentage
      expect(screen.getByText('-2.30%')).toBeInTheDocument();
    });
  });

  describe('custom page size', () => {
    it('respects custom page size', () => {
      const trades: TradeRecord[] = Array.from({ length: 15 }, (_, i) => ({
        entry_time: `2024-01-${String(i + 1).padStart(2, '0')}T10:00:00Z`,
        exit_time: `2024-01-${String(i + 1).padStart(2, '0')}T14:00:00Z`,
        side: 'BUY',
        entry_price: 40000,
        exit_price: 40500,
        quantity: 0.1,
        pnl: 50,
        pnl_percent: 0.01,
        commission: 8,
      }));

      render(<TradeListTable trades={trades} pageSize={5} />);

      expect(screen.getByText(/showing 1 - 5 of 15 trades/i)).toBeInTheDocument();
    });
  });
});
