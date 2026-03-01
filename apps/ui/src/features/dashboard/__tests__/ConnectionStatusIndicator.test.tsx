/**
 * ConnectionStatusIndicator Component Tests
 * Tests for SSE connection status display and reconnect functionality
 */
import { describe, it, expect, beforeEach, vi } from 'vitest';
import { render, screen } from '@/test/test-utils';
import userEvent from '@testing-library/user-event';
import { ConnectionStatusIndicator } from '../components/ConnectionStatusIndicator';
import { useDashboardStore } from '../store/dashboardStore';

describe('ConnectionStatusIndicator', () => {
  beforeEach(() => {
    // Reset store before each test
    useDashboardStore.setState({
      sseConnectionState: 'disconnected',
      lastEventId: null,
      prices: {},
    });
  });

  describe('connection states', () => {
    it('should show "Connected" when connected', () => {
      useDashboardStore.setState({ sseConnectionState: 'connected' });

      render(<ConnectionStatusIndicator />);

      expect(screen.getByText('Connected')).toBeInTheDocument();
    });

    it('should show "Connecting..." when connecting', () => {
      useDashboardStore.setState({ sseConnectionState: 'connecting' });

      render(<ConnectionStatusIndicator />);

      expect(screen.getByText('Connecting...')).toBeInTheDocument();
    });

    it('should show "Reconnecting..." when reconnecting', () => {
      useDashboardStore.setState({ sseConnectionState: 'reconnecting' });

      render(<ConnectionStatusIndicator />);

      expect(screen.getByText('Reconnecting...')).toBeInTheDocument();
    });

    it('should show "Disconnected" when disconnected', () => {
      useDashboardStore.setState({ sseConnectionState: 'disconnected' });

      render(<ConnectionStatusIndicator />);

      expect(screen.getByText('Disconnected')).toBeInTheDocument();
    });
  });

  describe('reconnect button', () => {
    it('should show reconnect button when disconnected and onReconnect provided', () => {
      useDashboardStore.setState({ sseConnectionState: 'disconnected' });
      const onReconnect = vi.fn();

      render(<ConnectionStatusIndicator onReconnect={onReconnect} />);

      expect(screen.getByText('Reconnect')).toBeInTheDocument();
    });

    it('should not show reconnect button when connected', () => {
      useDashboardStore.setState({ sseConnectionState: 'connected' });
      const onReconnect = vi.fn();

      render(<ConnectionStatusIndicator onReconnect={onReconnect} />);

      expect(screen.queryByText('Reconnect')).not.toBeInTheDocument();
    });

    it('should not show reconnect button when no onReconnect provided', () => {
      useDashboardStore.setState({ sseConnectionState: 'disconnected' });

      render(<ConnectionStatusIndicator />);

      expect(screen.queryByText('Reconnect')).not.toBeInTheDocument();
    });

    it('should call onReconnect when reconnect button clicked', async () => {
      useDashboardStore.setState({ sseConnectionState: 'disconnected' });
      const onReconnect = vi.fn();
      const user = userEvent.setup();

      render(<ConnectionStatusIndicator onReconnect={onReconnect} />);

      await user.click(screen.getByText('Reconnect'));

      expect(onReconnect).toHaveBeenCalledTimes(1);
    });
  });

  describe('styling', () => {
    it('should have success color when connected', () => {
      useDashboardStore.setState({ sseConnectionState: 'connected' });

      render(<ConnectionStatusIndicator />);

      const statusText = screen.getByText('Connected');
      expect(statusText).toHaveClass('text-success');
    });

    it('should have warning color when connecting', () => {
      useDashboardStore.setState({ sseConnectionState: 'connecting' });

      render(<ConnectionStatusIndicator />);

      const statusText = screen.getByText('Connecting...');
      expect(statusText).toHaveClass('text-warning');
    });

    it('should have danger color when disconnected', () => {
      useDashboardStore.setState({ sseConnectionState: 'disconnected' });

      render(<ConnectionStatusIndicator />);

      const statusText = screen.getByText('Disconnected');
      expect(statusText).toHaveClass('text-danger');
    });
  });
});
