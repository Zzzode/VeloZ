import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import { ToastProvider, useToast } from './Toast';

// Test component to trigger toasts
function TestComponent() {
  const { showToast, hideToast, toasts } = useToast();

  return (
    <div>
      <button onClick={() => showToast({ type: 'success', message: 'Success message' })}>
        Show Success
      </button>
      <button onClick={() => showToast({ type: 'error', message: 'Error message' })}>
        Show Error
      </button>
      <button onClick={() => showToast({ type: 'warning', message: 'Warning message' })}>
        Show Warning
      </button>
      <button onClick={() => showToast({ type: 'info', message: 'Info message' })}>
        Show Info
      </button>
      <button onClick={() => showToast({ type: 'success', message: 'No auto dismiss', duration: 0 })}>
        Show Persistent
      </button>
      {toasts.length > 0 && (
        <button onClick={() => hideToast(toasts[0].id)}>Hide First</button>
      )}
      <span data-testid="toast-count">{toasts.length}</span>
    </div>
  );
}

describe('Toast', () => {
  describe('ToastProvider', () => {
    it('renders children', () => {
      render(
        <ToastProvider>
          <div>Child content</div>
        </ToastProvider>
      );
      expect(screen.getByText('Child content')).toBeInTheDocument();
    });
  });

  describe('useToast', () => {
    it('throws error when used outside provider', () => {
      const consoleError = vi.spyOn(console, 'error').mockImplementation(() => {});

      expect(() => {
        render(<TestComponent />);
      }).toThrow('useToast must be used within a ToastProvider');

      consoleError.mockRestore();
    });
  });

  describe('showToast', () => {
    it('shows success toast', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Success'));

      await waitFor(() => {
        expect(screen.getByText('Success message')).toBeInTheDocument();
      });
      expect(screen.getByRole('alert')).toHaveClass('bg-success');
    });

    it('shows error toast', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Error'));

      await waitFor(() => {
        expect(screen.getByText('Error message')).toBeInTheDocument();
      });
      expect(screen.getByRole('alert')).toHaveClass('bg-danger');
    });

    it('shows warning toast', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Warning'));

      await waitFor(() => {
        expect(screen.getByText('Warning message')).toBeInTheDocument();
      });
      expect(screen.getByRole('alert')).toHaveClass('bg-warning');
    });

    it('shows info toast', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Info'));

      await waitFor(() => {
        expect(screen.getByText('Info message')).toBeInTheDocument();
      });
      expect(screen.getByRole('alert')).toHaveClass('bg-primary');
    });

    it('shows multiple toasts', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Success'));
      fireEvent.click(screen.getByText('Show Error'));

      await waitFor(() => {
        expect(screen.getByText('Success message')).toBeInTheDocument();
        expect(screen.getByText('Error message')).toBeInTheDocument();
      });
      expect(screen.getAllByRole('alert')).toHaveLength(2);
    });
  });

  describe('hideToast', () => {
    it('manually hides toast', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Persistent'));

      await waitFor(() => {
        expect(screen.getByText('No auto dismiss')).toBeInTheDocument();
      });

      fireEvent.click(screen.getByText('Hide First'));

      await waitFor(() => {
        expect(screen.queryByText('No auto dismiss')).not.toBeInTheDocument();
      });
    });

    it('hides toast via close button', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Persistent'));

      await waitFor(() => {
        expect(screen.getByText('No auto dismiss')).toBeInTheDocument();
      });

      fireEvent.click(screen.getByRole('button', { name: /close/i }));

      await waitFor(() => {
        expect(screen.queryByText('No auto dismiss')).not.toBeInTheDocument();
      });
    });
  });

  describe('accessibility', () => {
    it('toast has alert role', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Success'));

      await waitFor(() => {
        expect(screen.getByRole('alert')).toBeInTheDocument();
      });
    });

    it('close button has accessible name', async () => {
      render(
        <ToastProvider>
          <TestComponent />
        </ToastProvider>
      );

      fireEvent.click(screen.getByText('Show Success'));

      await waitFor(() => {
        expect(screen.getByRole('button', { name: /close/i })).toBeInTheDocument();
      });
    });
  });
});
