/**
 * OrderBook Component Tests
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen } from '@/test/test-utils';
import { OrderBook } from '../components/OrderBook';
import { useTradingStore } from '../store';

// Mock the hooks
vi.mock('../hooks', () => ({
  useOrderBookStream: vi.fn(() => ({
    orderBook: useTradingStore.getState().orderBook,
    connectionState: useTradingStore.getState().wsConnectionState,
  })),
}));

describe('OrderBook', () => {
  beforeEach(() => {
    useTradingStore.setState({
      selectedSymbol: 'BTCUSDT',
      orderBook: {
        bids: [{ price: 42000, qty: 1.5, total: 1.5 }],
        asks: [{ price: 42010, qty: 2.0, total: 2.0 }],
        spread: 10,
        spreadPercent: 0.024,
        lastUpdate: Date.now() * 1_000_000,
      },
      wsConnectionState: 'connected',
    });
  });

  it('should render order book title', () => {
    render(<OrderBook />);
    expect(screen.getByText('Order Book')).toBeInTheDocument();
  });

  it('should render symbol', () => {
    render(<OrderBook />);
    expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
  });

  it('should render spread information', () => {
    render(<OrderBook />);
    expect(screen.getByText(/spread/i)).toBeInTheDocument();
  });

  it('should show connection status', () => {
    render(<OrderBook />);
    expect(screen.getByText(/connected/i)).toBeInTheDocument();
  });

  it('should show disconnected status when not connected', () => {
    useTradingStore.setState({ wsConnectionState: 'disconnected' });
    render(<OrderBook />);
    expect(screen.getByText(/disconnected/i)).toBeInTheDocument();
  });

  it('should render price column header', () => {
    render(<OrderBook />);
    expect(screen.getByText(/price/i)).toBeInTheDocument();
  });

  it('should render amount column header', () => {
    render(<OrderBook />);
    expect(screen.getByText(/amount/i)).toBeInTheDocument();
  });
});
