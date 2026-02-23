/**
 * Tests for TimeframeSelector component
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { TimeframeSelector } from '../components/TimeframeSelector';
import { useMarketStore } from '../store/marketStore';

describe('TimeframeSelector', () => {
  beforeEach(() => {
    // Reset store to initial state
    useMarketStore.setState({
      selectedSymbol: 'BTCUSDT',
      selectedTimeframe: '1h',
      availableSymbols: ['BTCUSDT', 'ETHUSDT', 'BNBUSDT', 'SOLUSDT', 'XRPUSDT'],
      prices: {},
      bookTops: {},
      recentTrades: {},
      wsConnectionState: 'disconnected',
    });
  });

  describe('rendering', () => {
    it('renders all timeframe buttons', () => {
      render(<TimeframeSelector />);

      expect(screen.getByRole('button', { name: '1m' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: '5m' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: '15m' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: '1H' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: '4H' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: '1D' })).toBeInTheDocument();
    });

    it('highlights the currently selected timeframe', () => {
      render(<TimeframeSelector />);

      // 1h is the default, shown as "1H"
      const selectedButton = screen.getByRole('button', { name: '1H' });
      expect(selectedButton).toHaveClass('bg-primary');
    });

    it('shows non-selected timeframes with secondary style', () => {
      render(<TimeframeSelector />);

      const nonSelectedButton = screen.getByRole('button', { name: '5m' });
      expect(nonSelectedButton).toHaveClass('bg-background-secondary');
    });
  });

  describe('interactions', () => {
    it('updates store when timeframe is clicked', async () => {
      const user = userEvent.setup();
      render(<TimeframeSelector />);

      await user.click(screen.getByRole('button', { name: '15m' }));

      expect(useMarketStore.getState().selectedTimeframe).toBe('15m');
    });

    it('updates visual state when timeframe changes', async () => {
      const user = userEvent.setup();
      render(<TimeframeSelector />);

      await user.click(screen.getByRole('button', { name: '4H' }));

      // 4H should now be primary
      expect(screen.getByRole('button', { name: '4H' })).toHaveClass('bg-primary');

      // 1H should now be secondary
      expect(screen.getByRole('button', { name: '1H' })).toHaveClass('bg-background-secondary');
    });

    it('can select each timeframe', async () => {
      const user = userEvent.setup();
      render(<TimeframeSelector />);

      const timeframes = [
        { label: '1m', value: '1m' },
        { label: '5m', value: '5m' },
        { label: '15m', value: '15m' },
        { label: '1H', value: '1h' },
        { label: '4H', value: '4h' },
        { label: '1D', value: '1d' },
      ];

      for (const tf of timeframes) {
        await user.click(screen.getByRole('button', { name: tf.label }));
        expect(useMarketStore.getState().selectedTimeframe).toBe(tf.value);
      }
    });
  });
});
