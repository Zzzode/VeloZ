/**
 * Tests for MarketInfoPanel component
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import { MarketInfoPanel } from '../components/MarketInfoPanel';
import { useMarketStore } from '../store/marketStore';

describe('MarketInfoPanel', () => {
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
  });

  describe('rendering', () => {
    it('renders selected symbol', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
    });

    it('renders Spot Trading label', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Spot Trading')).toBeInTheDocument();
    });

    it('renders Last Price label', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Last Price')).toBeInTheDocument();
    });

    it('shows placeholder when no price available', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('--')).toBeInTheDocument();
    });
  });

  describe('with price data', () => {
    beforeEach(() => {
      useMarketStore.setState({
        prices: {
          BTCUSDT: { price: 45000.50, timestamp: Date.now() * 1_000_000 },
        },
      });
    });

    it('displays current price', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('45,000.50')).toBeInTheDocument();
    });
  });

  describe('with book top data', () => {
    beforeEach(() => {
      useMarketStore.setState({
        prices: {
          BTCUSDT: { price: 45000, timestamp: Date.now() * 1_000_000 },
        },
        bookTops: {
          BTCUSDT: {
            bidPrice: 44990,
            bidQty: 1.5,
            askPrice: 45010,
            askQty: 2.0,
            timestamp: Date.now() * 1_000_000,
          },
        },
      });
    });

    it('displays bid price', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('44,990.00')).toBeInTheDocument();
    });

    it('displays ask price', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('45,010.00')).toBeInTheDocument();
    });

    it('displays bid label', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Bid')).toBeInTheDocument();
    });

    it('displays ask label', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Ask')).toBeInTheDocument();
    });

    it('displays spread', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Spread')).toBeInTheDocument();
      expect(screen.getByText('20.00')).toBeInTheDocument();
    });

    it('displays spread percentage', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('Spread %')).toBeInTheDocument();
    });
  });

  describe('connection state', () => {
    it('shows disconnected state', () => {
      useMarketStore.setState({ wsConnectionState: 'disconnected' });
      render(<MarketInfoPanel />);

      expect(screen.getByText('disconnected')).toBeInTheDocument();
    });

    it('shows connected state', () => {
      useMarketStore.setState({ wsConnectionState: 'connected' });
      render(<MarketInfoPanel />);

      expect(screen.getByText('connected')).toBeInTheDocument();
    });

    it('shows connecting state', () => {
      useMarketStore.setState({ wsConnectionState: 'connecting' });
      render(<MarketInfoPanel />);

      expect(screen.getByText('connecting')).toBeInTheDocument();
    });

    it('shows reconnecting state', () => {
      useMarketStore.setState({ wsConnectionState: 'reconnecting' });
      render(<MarketInfoPanel />);

      expect(screen.getByText('reconnecting')).toBeInTheDocument();
    });
  });

  describe('symbol switching', () => {
    beforeEach(() => {
      useMarketStore.setState({
        selectedSymbol: 'ETHUSDT',
        prices: {
          BTCUSDT: { price: 45000, timestamp: Date.now() * 1_000_000 },
          ETHUSDT: { price: 2500, timestamp: Date.now() * 1_000_000 },
        },
      });
    });

    it('displays correct symbol after switch', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('ETHUSDT')).toBeInTheDocument();
    });

    it('displays price for selected symbol', () => {
      render(<MarketInfoPanel />);

      expect(screen.getByText('2,500.00')).toBeInTheDocument();
    });
  });
});
