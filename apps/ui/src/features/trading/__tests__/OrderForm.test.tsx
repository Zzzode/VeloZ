/**
 * OrderForm Component Tests
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen, userEvent, waitFor } from '@/test/test-utils';
import { OrderForm } from '../components/OrderForm';
import { useTradingStore } from '../store';

// Mock the hooks
vi.mock('../hooks', () => ({
  usePlaceOrder: vi.fn(() => ({
    placeOrder: vi.fn().mockResolvedValue({ ok: true, client_order_id: 'test-order' }),
    isLoading: false,
  })),
}));

describe('OrderForm', () => {
  beforeEach(() => {
    // Reset store state
    useTradingStore.setState({
      selectedSymbol: 'BTCUSDT',
      orderFormSide: 'BUY',
      orderFormPrice: '',
      orderFormQty: '',
      orderBook: {
        bids: [{ price: 42000, qty: 1.0, total: 1.0 }],
        asks: [{ price: 42010, qty: 1.0, total: 1.0 }],
        spread: 10,
        spreadPercent: 0.024,
        lastUpdate: Date.now() * 1_000_000,
      },
      isPlacingOrder: false,
      error: null,
    });
  });

  it('should render order form with title', () => {
    render(<OrderForm />);
    expect(screen.getByText('Place Order')).toBeInTheDocument();
  });

  it('should render symbol in subtitle', () => {
    render(<OrderForm />);
    expect(screen.getByText('BTCUSDT')).toBeInTheDocument();
  });

  it('should render BUY and SELL toggle buttons', () => {
    render(<OrderForm />);
    expect(screen.getByRole('button', { name: /^buy$/i })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^sell$/i })).toBeInTheDocument();
  });

  it('should render price input', () => {
    render(<OrderForm />);
    expect(screen.getByLabelText(/price/i)).toBeInTheDocument();
  });

  it('should render quantity input', () => {
    render(<OrderForm />);
    expect(screen.getByLabelText(/quantity/i)).toBeInTheDocument();
  });

  it('should update store when side toggle clicked', async () => {
    const user = userEvent.setup();
    render(<OrderForm />);

    const sellButton = screen.getByRole('button', { name: /^sell$/i });
    await user.click(sellButton);

    expect(useTradingStore.getState().orderFormSide).toBe('SELL');
  });

  it('should update store when price entered', async () => {
    const user = userEvent.setup();
    render(<OrderForm />);

    const priceInput = screen.getByLabelText(/price/i);
    await user.type(priceInput, '42500');

    expect(useTradingStore.getState().orderFormPrice).toBe('42500');
  });

  it('should update store when quantity entered', async () => {
    const user = userEvent.setup();
    render(<OrderForm />);

    const qtyInput = screen.getByLabelText(/quantity/i);
    await user.type(qtyInput, '0.5');

    expect(useTradingStore.getState().orderFormQty).toBe('0.5');
  });

  it('should have submit button', () => {
    render(<OrderForm />);
    // Submit button contains the symbol
    const submitButton = screen.getByRole('button', { name: /btcusdt/i });
    expect(submitButton).toBeInTheDocument();
  });

  it('should disable submit when form is empty', () => {
    render(<OrderForm />);
    const submitButton = screen.getByRole('button', { name: /btcusdt/i });
    expect(submitButton).toBeDisabled();
  });
});
