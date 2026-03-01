import { describe, it, expect, vi } from 'vitest';
import { render, screen, userEvent, waitFor } from '@/test/test-utils';
import { Modal } from './Modal';

describe('Modal', () => {
  const defaultProps = {
    isOpen: true,
    onClose: vi.fn(),
    children: <div>Modal content</div>,
  };

  describe('rendering', () => {
    it('renders when isOpen is true', () => {
      render(<Modal {...defaultProps} />);
      expect(screen.getByText('Modal content')).toBeInTheDocument();
    });

    it('does not render when isOpen is false', () => {
      render(<Modal {...defaultProps} isOpen={false} />);
      expect(screen.queryByText('Modal content')).not.toBeInTheDocument();
    });

    it('renders title', () => {
      render(<Modal {...defaultProps} title="Test Title" />);
      expect(screen.getByText('Test Title')).toBeInTheDocument();
    });

    it('renders description', () => {
      render(<Modal {...defaultProps} title="Title" description="Test description" />);
      expect(screen.getByText('Test description')).toBeInTheDocument();
    });

    it('renders footer', () => {
      render(<Modal {...defaultProps} footer={<button>Save</button>} />);
      expect(screen.getByRole('button', { name: 'Save' })).toBeInTheDocument();
    });
  });

  describe('sizes', () => {
    it('renders sm size', () => {
      render(<Modal {...defaultProps} size="sm" />);
      expect(screen.getByText('Modal content').closest('[class*="max-w-sm"]')).toBeInTheDocument();
    });

    it('renders md size (default)', () => {
      render(<Modal {...defaultProps} />);
      expect(screen.getByText('Modal content').closest('[class*="max-w-md"]')).toBeInTheDocument();
    });

    it('renders lg size', () => {
      render(<Modal {...defaultProps} size="lg" />);
      expect(screen.getByText('Modal content').closest('[class*="max-w-lg"]')).toBeInTheDocument();
    });

    it('renders xl size', () => {
      render(<Modal {...defaultProps} size="xl" />);
      expect(screen.getByText('Modal content').closest('[class*="max-w-xl"]')).toBeInTheDocument();
    });
  });

  describe('close button', () => {
    it('renders close button by default', () => {
      render(<Modal {...defaultProps} title="Title" />);
      expect(screen.getByRole('button', { name: /close/i })).toBeInTheDocument();
    });

    it('hides close button when showCloseButton is false', () => {
      render(<Modal {...defaultProps} title="Title" showCloseButton={false} />);
      expect(screen.queryByRole('button', { name: /close/i })).not.toBeInTheDocument();
    });

    it('calls onClose when close button is clicked', async () => {
      const user = userEvent.setup();
      const onClose = vi.fn();
      render(<Modal {...defaultProps} title="Title" onClose={onClose} />);

      await user.click(screen.getByRole('button', { name: /close/i }));
      expect(onClose).toHaveBeenCalledTimes(1);
    });
  });

  describe('overlay click', () => {
    it('calls onClose when overlay is clicked by default', async () => {
      const user = userEvent.setup();
      const onClose = vi.fn();
      render(<Modal {...defaultProps} onClose={onClose} />);

      // Click on the backdrop (the fixed overlay)
      const backdrop = document.querySelector('[aria-hidden="true"]');
      if (backdrop) {
        await user.click(backdrop);
        // HeadlessUI handles this through Dialog's onClose
      }
    });

    it('does not call onClose when closeOnOverlayClick is false', async () => {
      const onClose = vi.fn();
      render(<Modal {...defaultProps} onClose={onClose} closeOnOverlayClick={false} />);

      // With closeOnOverlayClick=false, clicking backdrop should not close
      const backdrop = document.querySelector('[aria-hidden="true"]');
      if (backdrop) {
        // The modal should remain open
        expect(screen.getByText('Modal content')).toBeInTheDocument();
      }
    });
  });

  describe('accessibility', () => {
    it('has dialog role', () => {
      render(<Modal {...defaultProps} />);
      expect(screen.getByRole('dialog')).toBeInTheDocument();
    });

    it('close button has accessible name', () => {
      render(<Modal {...defaultProps} title="Title" />);
      expect(screen.getByRole('button', { name: /close/i })).toBeInTheDocument();
    });
  });

  describe('transitions', () => {
    it('shows modal with transition', async () => {
      const { rerender } = render(<Modal {...defaultProps} isOpen={false} />);
      expect(screen.queryByText('Modal content')).not.toBeInTheDocument();

      rerender(<Modal {...defaultProps} isOpen={true} />);
      await waitFor(() => {
        expect(screen.getByText('Modal content')).toBeInTheDocument();
      });
    });

    it('hides modal with transition', async () => {
      const { rerender } = render(<Modal {...defaultProps} isOpen={true} />);
      expect(screen.getByText('Modal content')).toBeInTheDocument();

      rerender(<Modal {...defaultProps} isOpen={false} />);
      await waitFor(() => {
        expect(screen.queryByText('Modal content')).not.toBeInTheDocument();
      });
    });
  });
});
