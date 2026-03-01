/**
 * Tests for PriceChart component
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, act } from '@testing-library/react';
import { PriceChart } from '../components/PriceChart';
import { useMarketStore } from '../store/marketStore';

// Mock lightweight-charts since it requires canvas
vi.mock('lightweight-charts', () => ({
  createChart: vi.fn(() => ({
    addSeries: vi.fn(() => ({
      setData: vi.fn(),
      update: vi.fn(),
    })),
    applyOptions: vi.fn(),
    timeScale: vi.fn(() => ({
      fitContent: vi.fn(),
    })),
    remove: vi.fn(),
  })),
  CandlestickSeries: {},
}));

describe('PriceChart', () => {
  beforeEach(() => {
    // Reset store to initial state
    useMarketStore.setState({
      selectedSymbol: 'BTCUSDT',
      selectedTimeframe: '1h',
      availableSymbols: ['BTCUSDT', 'ETHUSDT'],
      prices: {},
      bookTops: {},
      recentTrades: {},
      wsConnectionState: 'disconnected',
    });

    // Mock timers for loading simulation
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  describe('rendering', () => {
    it('renders chart container', () => {
      render(<PriceChart />);

      // Should have a Card with title
      expect(screen.getByText('Price Chart')).toBeInTheDocument();
    });

    it('shows symbol and timeframe in subtitle', () => {
      render(<PriceChart />);

      expect(screen.getByText('BTCUSDT - 1H')).toBeInTheDocument();
    });

    it('shows loading spinner initially', () => {
      render(<PriceChart />);

      expect(screen.getByRole('status')).toBeInTheDocument();
    });

    it('hides loading spinner after data loads', async () => {
      render(<PriceChart />);

      // Fast-forward past the loading delay (500ms in component)
      // Use act to properly handle state updates
      await act(async () => {
        vi.advanceTimersByTime(600);
      });

      expect(screen.queryByRole('status')).not.toBeInTheDocument();
    });
  });

  describe('height prop', () => {
    it('uses default height of 400', () => {
      const { container } = render(<PriceChart />);

      const chartContainer = container.querySelector('[style*="height"]');
      expect(chartContainer).toHaveStyle({ height: '400px' });
    });

    it('accepts custom height', () => {
      const { container } = render(<PriceChart height={500} />);

      const chartContainer = container.querySelector('[style*="height"]');
      expect(chartContainer).toHaveStyle({ height: '500px' });
    });
  });

  describe('symbol changes', () => {
    it('updates subtitle when symbol changes', () => {
      const { rerender } = render(<PriceChart />);

      expect(screen.getByText('BTCUSDT - 1H')).toBeInTheDocument();

      // Change symbol in store
      act(() => {
        useMarketStore.setState({ selectedSymbol: 'ETHUSDT' });
      });

      // Rerender to pick up store changes
      rerender(<PriceChart />);

      expect(screen.getByText('ETHUSDT - 1H')).toBeInTheDocument();
    });

    it('shows loading when symbol changes', async () => {
      render(<PriceChart />);

      // Initial load complete
      await act(async () => {
        vi.advanceTimersByTime(600);
      });

      expect(screen.queryByRole('status')).not.toBeInTheDocument();

      // Change symbol - this triggers loading state
      act(() => {
        useMarketStore.setState({ selectedSymbol: 'ETHUSDT' });
      });

      // Should show loading again
      expect(screen.getByRole('status')).toBeInTheDocument();
    });
  });

  describe('timeframe changes', () => {
    it('updates subtitle when timeframe changes', () => {
      const { rerender } = render(<PriceChart />);

      expect(screen.getByText('BTCUSDT - 1H')).toBeInTheDocument();

      // Change timeframe in store
      act(() => {
        useMarketStore.setState({ selectedTimeframe: '15m' });
      });

      rerender(<PriceChart />);

      expect(screen.getByText('BTCUSDT - 15M')).toBeInTheDocument();
    });
  });

  describe('real-time updates', () => {
    it('handles price updates without crashing', async () => {
      render(<PriceChart />);

      // Complete initial load
      await act(async () => {
        vi.advanceTimersByTime(600);
      });

      // Simulate price update
      act(() => {
        useMarketStore.setState({
          prices: {
            BTCUSDT: { price: 45000, timestamp: Date.now() * 1_000_000 },
          },
        });
      });

      // Should not crash
      expect(screen.getByText('Price Chart')).toBeInTheDocument();
    });
  });
});
