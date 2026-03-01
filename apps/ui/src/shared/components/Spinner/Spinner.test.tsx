import { describe, it, expect } from 'vitest';
import { render, screen } from '@/test/test-utils';
import { Spinner, LoadingSpinner, FullPageSpinner } from './Spinner';

describe('Spinner', () => {
  describe('rendering', () => {
    it('renders with status role', () => {
      render(<Spinner />);
      expect(screen.getByRole('status')).toBeInTheDocument();
    });

    it('renders svg spinner', () => {
      render(<Spinner />);
      const status = screen.getByRole('status');
      expect(status.querySelector('svg')).toBeInTheDocument();
    });

    it('has screen reader text', () => {
      render(<Spinner />);
      expect(screen.getByText('Loading...')).toBeInTheDocument();
    });
  });

  describe('sizes', () => {
    it('renders sm size', () => {
      render(<Spinner size="sm" />);
      const svg = screen.getByRole('status').querySelector('svg');
      expect(svg).toHaveClass('h-4', 'w-4');
    });

    it('renders md size (default)', () => {
      render(<Spinner />);
      const svg = screen.getByRole('status').querySelector('svg');
      expect(svg).toHaveClass('h-6', 'w-6');
    });

    it('renders lg size', () => {
      render(<Spinner size="lg" />);
      const svg = screen.getByRole('status').querySelector('svg');
      expect(svg).toHaveClass('h-8', 'w-8');
    });

    it('renders xl size', () => {
      render(<Spinner size="xl" />);
      const svg = screen.getByRole('status').querySelector('svg');
      expect(svg).toHaveClass('h-12', 'w-12');
    });
  });

  describe('label', () => {
    it('renders visible label when provided', () => {
      render(<Spinner label="Processing..." />);
      const labels = screen.getAllByText('Processing...');
      // One visible label and one sr-only
      expect(labels.length).toBeGreaterThanOrEqual(1);
    });

    it('uses custom label for screen reader', () => {
      render(<Spinner label="Saving data" />);
      const labels = screen.getAllByText('Saving data');
      expect(labels.length).toBeGreaterThanOrEqual(1);
    });
  });

  describe('custom className', () => {
    it('applies custom className', () => {
      render(<Spinner className="custom-class" />);
      expect(screen.getByRole('status')).toHaveClass('custom-class');
    });
  });
});

describe('LoadingSpinner', () => {
  it('renders centered spinner', () => {
    render(<LoadingSpinner />);
    expect(screen.getByRole('status')).toBeInTheDocument();
  });

  it('renders with default label', () => {
    render(<LoadingSpinner />);
    const labels = screen.getAllByText('Loading...');
    expect(labels.length).toBeGreaterThanOrEqual(1);
  });

  it('renders with custom label', () => {
    render(<LoadingSpinner label="Fetching data..." />);
    const labels = screen.getAllByText('Fetching data...');
    expect(labels.length).toBeGreaterThanOrEqual(1);
  });

  it('uses lg size', () => {
    render(<LoadingSpinner />);
    const svg = screen.getByRole('status').querySelector('svg');
    expect(svg).toHaveClass('h-8', 'w-8');
  });
});

describe('FullPageSpinner', () => {
  it('renders full page overlay', () => {
    render(<FullPageSpinner />);
    expect(screen.getByRole('status')).toBeInTheDocument();
  });

  it('renders with default label', () => {
    render(<FullPageSpinner />);
    const labels = screen.getAllByText('Loading...');
    expect(labels.length).toBeGreaterThanOrEqual(1);
  });

  it('renders with custom label', () => {
    render(<FullPageSpinner label="Initializing..." />);
    const labels = screen.getAllByText('Initializing...');
    expect(labels.length).toBeGreaterThanOrEqual(1);
  });

  it('uses xl size', () => {
    render(<FullPageSpinner />);
    const svg = screen.getByRole('status').querySelector('svg');
    expect(svg).toHaveClass('h-12', 'w-12');
  });

  it('has fixed positioning', () => {
    render(<FullPageSpinner />);
    const container = screen.getByRole('status').parentElement;
    expect(container).toHaveClass('fixed', 'inset-0');
  });
});
