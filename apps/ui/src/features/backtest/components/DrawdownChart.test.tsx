import { describe, it, expect } from 'vitest';
import { render, screen } from '@/test/test-utils';
import { DrawdownChart } from './DrawdownChart';
import type { DrawdownPoint } from '@/shared/api/types';

describe('DrawdownChart', () => {
  const mockData: DrawdownPoint[] = [
    { timestamp: '2024-01-01T00:00:00Z', drawdown: 0 },
    { timestamp: '2024-02-01T00:00:00Z', drawdown: -0.05 },
    { timestamp: '2024-03-01T00:00:00Z', drawdown: -0.12 },
    { timestamp: '2024-04-01T00:00:00Z', drawdown: -0.08 },
    { timestamp: '2024-05-01T00:00:00Z', drawdown: -0.03 },
    { timestamp: '2024-06-01T00:00:00Z', drawdown: -0.02 },
  ];

  describe('rendering', () => {
    it('renders chart container', () => {
      const { container } = render(<DrawdownChart data={mockData} />);
      expect(container.querySelector('.h-48')).toBeInTheDocument();
    });

    it('renders without crashing with valid data', () => {
      expect(() => render(<DrawdownChart data={mockData} />)).not.toThrow();
    });
  });

  describe('empty state', () => {
    it('shows message when data is empty', () => {
      render(<DrawdownChart data={[]} />);
      expect(screen.getByText(/no drawdown data available/i)).toBeInTheDocument();
    });

    it('shows message when data is undefined', () => {
      render(<DrawdownChart data={undefined as unknown as DrawdownPoint[]} />);
      expect(screen.getByText(/no drawdown data available/i)).toBeInTheDocument();
    });
  });

  describe('styling', () => {
    it('has correct height class', () => {
      const { container } = render(<DrawdownChart data={mockData} />);
      expect(container.firstChild).toHaveClass('h-48');
    });
  });

  describe('with different data sets', () => {
    it('handles single data point', () => {
      const singlePoint: DrawdownPoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', drawdown: 0 },
      ];
      expect(() => render(<DrawdownChart data={singlePoint} />)).not.toThrow();
    });

    it('handles zero drawdown', () => {
      const zeroDrawdown: DrawdownPoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', drawdown: 0 },
        { timestamp: '2024-02-01T00:00:00Z', drawdown: 0 },
      ];
      expect(() => render(<DrawdownChart data={zeroDrawdown} />)).not.toThrow();
    });

    it('handles large drawdown values', () => {
      const largeDrawdown: DrawdownPoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', drawdown: 0 },
        { timestamp: '2024-02-01T00:00:00Z', drawdown: -0.5 },
        { timestamp: '2024-03-01T00:00:00Z', drawdown: -0.8 },
      ];
      expect(() => render(<DrawdownChart data={largeDrawdown} />)).not.toThrow();
    });
  });
});
