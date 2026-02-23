/**
 * PositionsList Component Tests
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { PositionsList } from '../components/PositionsList';
import { useTradingStore } from '../store';
import type { Position } from '@/shared/api/types';

const mockPositions: Position[] = [
  {
    symbol: 'BTCUSDT',
    venue: 'binance',
    side: 'LONG',
    qty: 0.5,
    avg_entry_price: 41000,
    current_price: 42500,
    unrealized_pnl: 750,
    realized_pnl: 1200,
    last_update_ts_ns: Date.now() * 1_000_000,
  },
  {
    symbol: 'ETHUSDT',
    venue: 'binance',
    side: 'SHORT',
    qty: 2.0,
    avg_entry_price: 2300,
    current_price: 2200,
    unrealized_pnl: 200,
    realized_pnl: 150,
    last_update_ts_ns: Date.now() * 1_000_000,
  },
];

const mockRefetch = vi.fn().mockResolvedValue(undefined);

// Mock the hooks
vi.mock('../hooks', () => ({
  usePositions: vi.fn(() => ({
    positions: mockPositions,
    isLoading: false,
    refetch: mockRefetch,
  })),
}));

describe('PositionsList', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    useTradingStore.setState({
      positions: mockPositions,
      isLoadingPositions: false,
    });
  });

  it('should render positions title', () => {
    render(<PositionsList />);
    expect(screen.getByText('Positions')).toBeInTheDocument();
  });

  it('should show position count', () => {
    render(<PositionsList />);
    expect(screen.getByText(/2 open positions/i)).toBeInTheDocument();
  });

  it('should render positions table', () => {
    render(<PositionsList />);
    expect(screen.getByRole('table')).toBeInTheDocument();
  });

  it('should render position symbols', () => {
    render(<PositionsList />);
    expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
    expect(screen.getByText('ETHUSDT')).toBeInTheDocument();
  });

  it('should render position sides', () => {
    render(<PositionsList />);
    expect(screen.getByText('LONG')).toBeInTheDocument();
    expect(screen.getByText('SHORT')).toBeInTheDocument();
  });

  it('should render venue information', () => {
    render(<PositionsList />);
    const venueElements = screen.getAllByText('binance');
    expect(venueElements).toHaveLength(2);
  });

  it('should show refresh button', () => {
    render(<PositionsList />);
    expect(screen.getByRole('button', { name: /refresh/i })).toBeInTheDocument();
  });

  it('should call refetch when refresh clicked', async () => {
    const user = userEvent.setup();
    render(<PositionsList />);

    const refreshButton = screen.getByRole('button', { name: /refresh/i });
    await user.click(refreshButton);

    expect(mockRefetch).toHaveBeenCalled();
  });

  it('should show P&L summary section', () => {
    render(<PositionsList />);
    // Summary section should exist
    const unrealizedLabels = screen.getAllByText(/unrealized/i);
    expect(unrealizedLabels.length).toBeGreaterThan(0);
  });
});
