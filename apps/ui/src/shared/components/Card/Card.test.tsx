import { describe, it, expect } from 'vitest';
import { render, screen } from '@/test/test-utils';
import { Card } from './Card';

describe('Card', () => {
  describe('rendering', () => {
    it('renders children', () => {
      render(<Card>Card content</Card>);
      expect(screen.getByText('Card content')).toBeInTheDocument();
    });

    it('applies base styling', () => {
      render(<Card data-testid="card">Content</Card>);
      const card = screen.getByTestId('card');
      expect(card).toHaveClass('bg-background', 'border', 'rounded-lg');
    });
  });

  describe('header', () => {
    it('renders title', () => {
      render(<Card title="Card Title">Content</Card>);
      expect(screen.getByRole('heading', { name: 'Card Title' })).toBeInTheDocument();
    });

    it('renders subtitle', () => {
      render(<Card title="Title" subtitle="Subtitle text">Content</Card>);
      expect(screen.getByText('Subtitle text')).toBeInTheDocument();
    });

    it('renders header action', () => {
      render(
        <Card title="Title" headerAction={<button>Action</button>}>
          Content
        </Card>
      );
      expect(screen.getByRole('button', { name: 'Action' })).toBeInTheDocument();
    });

    it('does not render header when no title, subtitle, or headerAction', () => {
      render(<Card data-testid="card">Content</Card>);
      const card = screen.getByTestId('card');
      expect(card.querySelector('.border-b')).not.toBeInTheDocument();
    });

    it('renders header with only headerAction', () => {
      render(
        <Card headerAction={<button>Action</button>} data-testid="card">
          Content
        </Card>
      );
      expect(screen.getByRole('button', { name: 'Action' })).toBeInTheDocument();
    });
  });

  describe('footer', () => {
    it('renders footer', () => {
      render(<Card footer={<div>Footer content</div>}>Content</Card>);
      expect(screen.getByText('Footer content')).toBeInTheDocument();
    });

    it('applies footer styling', () => {
      render(<Card footer={<div data-testid="footer-content">Footer</div>}>Content</Card>);
      const footerContent = screen.getByTestId('footer-content');
      expect(footerContent.parentElement).toHaveClass('border-t', 'bg-background-secondary');
    });
  });

  describe('padding', () => {
    it('applies padding by default', () => {
      render(<Card data-testid="card">Content</Card>);
      const card = screen.getByTestId('card');
      const contentDiv = card.querySelector('div:not(.flex)');
      expect(contentDiv).toHaveClass('p-4');
    });

    it('removes padding when noPadding is true', () => {
      render(<Card noPadding data-testid="card">Content</Card>);
      const card = screen.getByTestId('card');
      // Find the content wrapper div (not the header)
      const divs = card.querySelectorAll(':scope > div');
      const contentDiv = divs[divs.length - 1];
      expect(contentDiv).not.toHaveClass('p-4');
    });
  });

  describe('custom className', () => {
    it('applies custom className', () => {
      render(<Card className="custom-class" data-testid="card">Content</Card>);
      expect(screen.getByTestId('card')).toHaveClass('custom-class');
    });

    it('preserves base classes with custom className', () => {
      render(<Card className="custom-class" data-testid="card">Content</Card>);
      const card = screen.getByTestId('card');
      expect(card).toHaveClass('bg-background', 'custom-class');
    });
  });

  describe('props forwarding', () => {
    it('forwards HTML attributes', () => {
      render(<Card data-testid="card" role="region" aria-label="Test card">Content</Card>);
      const card = screen.getByTestId('card');
      expect(card).toHaveAttribute('role', 'region');
      expect(card).toHaveAttribute('aria-label', 'Test card');
    });
  });
});
