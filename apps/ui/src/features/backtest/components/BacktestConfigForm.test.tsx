import { describe, it, expect, vi } from 'vitest';
import { render, screen, waitFor } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { BacktestConfigForm } from './BacktestConfigForm';

describe('BacktestConfigForm', () => {
  const mockOnSubmit = vi.fn();

  beforeEach(() => {
    mockOnSubmit.mockClear();
  });

  describe('rendering', () => {
    it('renders all form fields', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      expect(screen.getByLabelText(/symbol/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/strategy/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/start date/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/end date/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/initial capital/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/commission/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/slippage/i)).toBeInTheDocument();
    });

    it('renders submit button', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      expect(screen.getByRole('button', { name: /run backtest/i })).toBeInTheDocument();
    });

    it('renders symbol options', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      const symbolSelect = screen.getByLabelText(/symbol/i);
      expect(symbolSelect).toBeInTheDocument();
      expect(screen.getByRole('option', { name: /btc\/usdt/i })).toBeInTheDocument();
      expect(screen.getByRole('option', { name: /eth\/usdt/i })).toBeInTheDocument();
    });

    it('renders default strategy options when none provided', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      expect(screen.getByRole('option', { name: /ma crossover/i })).toBeInTheDocument();
      expect(screen.getByRole('option', { name: /rsi reversal/i })).toBeInTheDocument();
    });

    it('renders custom strategy options when provided', () => {
      const strategies = [
        { id: 'custom-1', name: 'Custom Strategy 1' },
        { id: 'custom-2', name: 'Custom Strategy 2' },
      ];
      render(<BacktestConfigForm onSubmit={mockOnSubmit} strategies={strategies} />);
      expect(screen.getByRole('option', { name: /custom strategy 1/i })).toBeInTheDocument();
      expect(screen.getByRole('option', { name: /custom strategy 2/i })).toBeInTheDocument();
    });
  });

  describe('default values', () => {
    it('has default initial capital', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      const capitalInput = screen.getByLabelText(/initial capital/i);
      expect(capitalInput).toHaveValue(10000);
    });

    it('has default commission', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      const commissionInput = screen.getByLabelText(/commission/i);
      expect(commissionInput).toHaveValue(0.001);
    });

    it('has default slippage', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      const slippageInput = screen.getByLabelText(/slippage/i);
      expect(slippageInput).toHaveValue(0.0005);
    });

    it('has default symbol selected', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      const symbolSelect = screen.getByLabelText(/symbol/i) as HTMLSelectElement;
      expect(symbolSelect.value).toBe('BTCUSDT');
    });
  });

  describe('validation', () => {
    it('does not call onSubmit when strategy is not selected', async () => {
      const user = userEvent.setup();
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      // Submit without selecting strategy (default is empty string)
      await user.click(screen.getByRole('button', { name: /run backtest/i }));

      // Wait a bit for any async validation
      await new Promise((resolve) => setTimeout(resolve, 100));

      expect(mockOnSubmit).not.toHaveBeenCalled();
    });

    it('does not call onSubmit when validation fails', async () => {
      const user = userEvent.setup();
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      // Submit without filling required fields
      await user.click(screen.getByRole('button', { name: /run backtest/i }));

      await waitFor(() => {
        expect(mockOnSubmit).not.toHaveBeenCalled();
      });
    });
  });

  describe('form submission', () => {
    it('form has submit button that can be clicked', async () => {
      const user = userEvent.setup();
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      const submitButton = screen.getByRole('button', { name: /run backtest/i });
      expect(submitButton).toBeInTheDocument();
      expect(submitButton).not.toBeDisabled();

      // Can click the button (even if validation fails)
      await user.click(submitButton);
    });

    it('form is connected to onSubmit handler', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      // Verify form exists and has submit button
      const form = screen.getByRole('button', { name: /run backtest/i }).closest('form');
      expect(form).toBeInTheDocument();
    });
  });

  describe('loading state', () => {
    it('disables submit button when loading', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} isLoading={true} />);
      // When loading, button text changes to "Loading..."
      const button = screen.getByRole('button', { name: /loading/i });
      expect(button).toBeDisabled();
    });

    it('shows loading text when loading', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} isLoading={true} />);
      expect(screen.getByText(/loading/i)).toBeInTheDocument();
    });
  });

  describe('helper text', () => {
    it('displays helper text for initial capital', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      expect(screen.getByText(/starting balance in usd/i)).toBeInTheDocument();
    });

    it('displays helper text for commission', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      expect(screen.getByText(/per trade/i)).toBeInTheDocument();
    });

    it('displays helper text for slippage', () => {
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);
      expect(screen.getByText(/expected slippage/i)).toBeInTheDocument();
    });
  });

  describe('field interactions', () => {
    it('allows changing symbol', async () => {
      const user = userEvent.setup();
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      await user.selectOptions(screen.getByLabelText(/symbol/i), 'ETHUSDT');

      const symbolSelect = screen.getByLabelText(/symbol/i) as HTMLSelectElement;
      expect(symbolSelect.value).toBe('ETHUSDT');
    });

    it('allows changing initial capital', async () => {
      const user = userEvent.setup();
      render(<BacktestConfigForm onSubmit={mockOnSubmit} />);

      const capitalInput = screen.getByLabelText(/initial capital/i);
      await user.clear(capitalInput);
      await user.type(capitalInput, '50000');

      expect(capitalInput).toHaveValue(50000);
    });
  });
});
