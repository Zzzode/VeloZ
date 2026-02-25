import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { TradingModeBadge } from '../components/TradingModeBadge';

describe('TradingModeBadge', () => {
  it('should render simulated mode badge', () => {
    render(<TradingModeBadge mode="simulated" />);

    expect(screen.getByText('SIMULATED')).toBeInTheDocument();
  });

  it('should render paper mode badge', () => {
    render(<TradingModeBadge mode="paper" />);

    expect(screen.getByText('PAPER')).toBeInTheDocument();
  });

  it('should render live mode badge', () => {
    render(<TradingModeBadge mode="live" />);

    expect(screen.getByText('LIVE')).toBeInTheDocument();
  });

  it('should render in different sizes', () => {
    const { rerender } = render(<TradingModeBadge mode="simulated" size="sm" />);
    expect(screen.getByText('SIMULATED')).toHaveClass('text-xs');

    rerender(<TradingModeBadge mode="simulated" size="md" />);
    expect(screen.getByText('SIMULATED')).toHaveClass('text-sm');

    rerender(<TradingModeBadge mode="simulated" size="lg" />);
    expect(screen.getByText('SIMULATED')).toHaveClass('text-base');
  });

  it('should hide icon when showIcon is false', () => {
    render(<TradingModeBadge mode="simulated" showIcon={false} />);

    // The icon should not be rendered
    expect(screen.queryByRole('img')).not.toBeInTheDocument();
  });

  it('should be clickable when onClick is provided', () => {
    const handleClick = vi.fn();
    render(<TradingModeBadge mode="simulated" onClick={handleClick} />);

    fireEvent.click(screen.getByText('SIMULATED'));

    expect(handleClick).toHaveBeenCalledTimes(1);
  });

  it('should not be clickable when onClick is not provided', () => {
    render(<TradingModeBadge mode="simulated" />);

    const badge = screen.getByText('SIMULATED').parentElement;
    expect(badge?.tagName).toBe('DIV');
  });
});
