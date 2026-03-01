/**
 * Tests for SymbolSelector component
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { SymbolSelector } from '../components/SymbolSelector';
import { useMarketStore } from '../store/marketStore';

describe('SymbolSelector', () => {
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
    it('renders a select element', () => {
      render(<SymbolSelector />);

      expect(screen.getByRole('combobox')).toBeInTheDocument();
    });

    it('renders label', () => {
      render(<SymbolSelector />);

      expect(screen.getByText('Symbol')).toBeInTheDocument();
    });

    it('shows current selected symbol', () => {
      render(<SymbolSelector />);

      const select = screen.getByRole('combobox');
      expect(select).toHaveValue('BTCUSDT');
    });

    it('renders all available symbols as options', () => {
      render(<SymbolSelector />);

      const options = screen.getAllByRole('option');
      expect(options).toHaveLength(5);
      expect(options.map((o) => o.textContent)).toEqual([
        'BTCUSDT',
        'ETHUSDT',
        'BNBUSDT',
        'SOLUSDT',
        'XRPUSDT',
      ]);
    });
  });

  describe('interactions', () => {
    it('updates store when symbol is changed', async () => {
      const user = userEvent.setup();
      render(<SymbolSelector />);

      const select = screen.getByRole('combobox');
      await user.selectOptions(select, 'ETHUSDT');

      expect(useMarketStore.getState().selectedSymbol).toBe('ETHUSDT');
    });

    it('reflects store changes in UI', async () => {
      const user = userEvent.setup();
      render(<SymbolSelector />);

      const select = screen.getByRole('combobox');
      await user.selectOptions(select, 'BNBUSDT');

      expect(select).toHaveValue('BNBUSDT');
    });
  });
});
