/**
 * Tests for StrategyCard component
 */
import { describe, it, expect, vi } from 'vitest';
import { render, screen, userEvent, waitFor } from '@/test/test-utils';
import { StrategyCard } from '../components/StrategyCard';
import type { StrategySummary } from '@/shared/api';

const mockStrategy: StrategySummary = {
  id: 'strat-001',
  name: 'BTC Momentum',
  type: 'momentum',
  state: 'Running',
  pnl: 2500,
  trade_count: 45,
};

const mockStoppedStrategy: StrategySummary = {
  id: 'strat-002',
  name: 'ETH Grid',
  type: 'grid',
  state: 'Stopped',
  pnl: -300,
  trade_count: 15,
};

describe('StrategyCard', () => {
  describe('rendering', () => {
    it('renders strategy name and type', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      expect(screen.getByText('BTC Momentum')).toBeInTheDocument();
      expect(screen.getByText('momentum')).toBeInTheDocument();
    });

    it('renders strategy state badge', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      expect(screen.getByText('Running')).toBeInTheDocument();
    });

    it('renders P&L with correct formatting', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      // Positive P&L should show + prefix
      expect(screen.getByText(/\+\$2,500\.00/)).toBeInTheDocument();
    });

    it('renders negative P&L correctly', () => {
      render(<StrategyCard strategy={mockStoppedStrategy} />);

      // Negative P&L
      expect(screen.getByText(/-\$300\.00/)).toBeInTheDocument();
    });

    it('renders trade count', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      expect(screen.getByText('45')).toBeInTheDocument();
    });
  });

  describe('controls', () => {
    it('shows Stop button when strategy is running', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      expect(screen.getByRole('button', { name: /stop/i })).toBeInTheDocument();
    });

    it('shows Start button when strategy is stopped', () => {
      render(<StrategyCard strategy={mockStoppedStrategy} />);

      expect(screen.getByRole('button', { name: /start/i })).toBeInTheDocument();
    });

    it('shows Configure button', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      expect(screen.getByRole('button', { name: /configure/i })).toBeInTheDocument();
    });
  });

  describe('interactions', () => {
    it('calls onSelect when card is clicked', async () => {
      const user = userEvent.setup();
      const onSelect = vi.fn();

      render(<StrategyCard strategy={mockStrategy} onSelect={onSelect} />);

      // Click on the card (not a button)
      await user.click(screen.getByText('BTC Momentum'));

      expect(onSelect).toHaveBeenCalledWith(mockStrategy);
    });

    it('calls onConfigure when Configure button is clicked', async () => {
      const user = userEvent.setup();
      const onConfigure = vi.fn();

      render(<StrategyCard strategy={mockStrategy} onConfigure={onConfigure} />);

      await user.click(screen.getByRole('button', { name: /configure/i }));

      expect(onConfigure).toHaveBeenCalledWith(mockStrategy);
    });

    it('does not call onSelect when Configure button is clicked', async () => {
      const user = userEvent.setup();
      const onSelect = vi.fn();
      const onConfigure = vi.fn();

      render(
        <StrategyCard
          strategy={mockStrategy}
          onSelect={onSelect}
          onConfigure={onConfigure}
        />
      );

      await user.click(screen.getByRole('button', { name: /configure/i }));

      expect(onConfigure).toHaveBeenCalled();
      expect(onSelect).not.toHaveBeenCalled();
    });

    it('triggers start mutation when Start button is clicked', async () => {
      const user = userEvent.setup();

      render(<StrategyCard strategy={mockStoppedStrategy} />);

      await user.click(screen.getByRole('button', { name: /start/i }));

      // Button should show loading state
      await waitFor(() => {
        expect(screen.getByRole('button', { name: /loading/i })).toBeInTheDocument();
      });
    });

    it('triggers stop mutation when Stop button is clicked', async () => {
      const user = userEvent.setup();

      render(<StrategyCard strategy={mockStrategy} />);

      await user.click(screen.getByRole('button', { name: /stop/i }));

      // Button should show loading state
      await waitFor(() => {
        expect(screen.getByRole('button', { name: /loading/i })).toBeInTheDocument();
      });
    });
  });

  describe('styling', () => {
    it('applies success color for positive P&L', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      const pnlElement = screen.getByText(/\+\$2,500\.00/);
      expect(pnlElement).toHaveClass('text-success');
    });

    it('applies danger color for negative P&L', () => {
      render(<StrategyCard strategy={mockStoppedStrategy} />);

      const pnlElement = screen.getByText(/-\$300\.00/);
      expect(pnlElement).toHaveClass('text-danger');
    });

    it('has hover effect on card', () => {
      render(<StrategyCard strategy={mockStrategy} />);

      // The card should have cursor-pointer and hover classes
      const card = screen.getByText('BTC Momentum').closest('.cursor-pointer');
      expect(card).toBeInTheDocument();
    });
  });
});
