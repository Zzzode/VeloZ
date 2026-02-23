/**
 * OrderHistory Component Tests
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { OrderHistory } from '../components/OrderHistory';
import { useTradingStore } from '../store';
import type { OrderState } from '@/shared/api/types';

const mockOrders: OrderState[] = [
  {
    client_order_id: 'order-001',
    symbol: 'BTCUSDT',
    side: 'BUY',
    order_qty: 0.1,
    limit_price: 42000,
    venue_order_id: 'venue-001',
    status: 'FILLED',
    executed_qty: 0.1,
    avg_price: 41950,
    last_ts_ns: Date.now() * 1_000_000,
  },
  {
    client_order_id: 'order-002',
    symbol: 'ETHUSDT',
    side: 'SELL',
    order_qty: 1.5,
    limit_price: 2200,
    venue_order_id: 'venue-002',
    status: 'ACCEPTED',
    executed_qty: 0,
    avg_price: 0,
    last_ts_ns: Date.now() * 1_000_000 - 60_000_000_000,
  },
];

const mockRefetch = vi.fn().mockResolvedValue(undefined);
const mockCancelOrder = vi.fn().mockResolvedValue({ ok: true });

// Mock the hooks
vi.mock('../hooks', () => ({
  useOrders: vi.fn(() => ({
    orders: mockOrders,
    isLoading: false,
    refetch: mockRefetch,
  })),
  useCancelOrder: vi.fn(() => ({
    cancelOrder: mockCancelOrder,
  })),
  useOrderUpdates: vi.fn(() => ({
    connectionState: 'connected',
  })),
}));

describe('OrderHistory', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    useTradingStore.setState({
      orders: mockOrders,
      isLoadingOrders: false,
      sseConnectionState: 'connected',
    });
  });

  it('should render order history title', () => {
    render(<OrderHistory />);
    expect(screen.getByText('Order History')).toBeInTheDocument();
  });

  it('should render filter tabs', () => {
    render(<OrderHistory />);
    expect(screen.getByRole('button', { name: /^all$/i })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^open$/i })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^filled$/i })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^cancelled$/i })).toBeInTheDocument();
  });

  it('should render orders table', () => {
    render(<OrderHistory />);
    expect(screen.getByRole('table')).toBeInTheDocument();
  });

  it('should render order symbols', () => {
    render(<OrderHistory />);
    expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
    expect(screen.getByText('ETHUSDT')).toBeInTheDocument();
  });

  it('should render order sides', () => {
    render(<OrderHistory />);
    expect(screen.getByText('BUY')).toBeInTheDocument();
    expect(screen.getByText('SELL')).toBeInTheDocument();
  });

  it('should render order statuses', () => {
    render(<OrderHistory />);
    expect(screen.getByText('FILLED')).toBeInTheDocument();
    expect(screen.getByText('ACCEPTED')).toBeInTheDocument();
  });

  it('should show refresh button', () => {
    render(<OrderHistory />);
    expect(screen.getByRole('button', { name: /refresh/i })).toBeInTheDocument();
  });

  it('should call refetch when refresh clicked', async () => {
    const user = userEvent.setup();
    render(<OrderHistory />);

    const refreshButton = screen.getByRole('button', { name: /refresh/i });
    await user.click(refreshButton);

    expect(mockRefetch).toHaveBeenCalled();
  });

  it('should show connection status', () => {
    render(<OrderHistory />);
    expect(screen.getByText(/connected/i)).toBeInTheDocument();
  });

  it('should have clickable filter tabs', async () => {
    const user = userEvent.setup();
    render(<OrderHistory />);

    const openTab = screen.getByRole('button', { name: /^open$/i });
    await user.click(openTab);
    expect(openTab).toBeInTheDocument();
  });
});
