import { describe, it, expect } from 'vitest';
import { render, screen } from '@/test/test-utils';
import { EquityCurveChart } from './EquityCurveChart';
import type { EquityCurvePoint } from '@/shared/api/types';

describe('EquityCurveChart', () => {
  const mockData: EquityCurvePoint[] = [
    { timestamp: '2024-01-01T00:00:00Z', equity: 10000 },
    { timestamp: '2024-02-01T00:00:00Z', equity: 10500 },
    { timestamp: '2024-03-01T00:00:00Z', equity: 11200 },
    { timestamp: '2024-04-01T00:00:00Z', equity: 10800 },
    { timestamp: '2024-05-01T00:00:00Z', equity: 12000 },
    { timestamp: '2024-06-01T00:00:00Z', equity: 12500 },
  ];

  describe('rendering', () => {
    it('renders chart container', () => {
      const { container } = render(
        <EquityCurveChart data={mockData} initialCapital={10000} />
      );
      expect(container.querySelector('.h-64')).toBeInTheDocument();
    });

    it('renders without crashing with valid data', () => {
      expect(() => render(
        <EquityCurveChart data={mockData} initialCapital={10000} />
      )).not.toThrow();
    });
  });

  describe('empty state', () => {
    it('shows message when data is empty', () => {
      render(<EquityCurveChart data={[]} initialCapital={10000} />);
      expect(screen.getByText(/no equity curve data available/i)).toBeInTheDocument();
    });

    it('shows message when data is undefined', () => {
      render(<EquityCurveChart data={undefined as unknown as EquityCurvePoint[]} initialCapital={10000} />);
      expect(screen.getByText(/no equity curve data available/i)).toBeInTheDocument();
    });
  });

  describe('styling', () => {
    it('has correct height class', () => {
      const { container } = render(
        <EquityCurveChart data={mockData} initialCapital={10000} />
      );
      expect(container.firstChild).toHaveClass('h-64');
    });
  });

  describe('with different data sets', () => {
    it('handles single data point', () => {
      const singlePoint: EquityCurvePoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', equity: 10000 },
      ];
      expect(() => render(
        <EquityCurveChart data={singlePoint} initialCapital={10000} />
      )).not.toThrow();
    });

    it('handles data with negative equity', () => {
      const negativeData: EquityCurvePoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', equity: 10000 },
        { timestamp: '2024-02-01T00:00:00Z', equity: -500 },
      ];
      expect(() => render(
        <EquityCurveChart data={negativeData} initialCapital={10000} />
      )).not.toThrow();
    });

    it('handles large equity values', () => {
      const largeData: EquityCurvePoint[] = [
        { timestamp: '2024-01-01T00:00:00Z', equity: 1000000 },
        { timestamp: '2024-02-01T00:00:00Z', equity: 1500000 },
      ];
      expect(() => render(
        <EquityCurveChart data={largeData} initialCapital={1000000} />
      )).not.toThrow();
    });
  });
});
