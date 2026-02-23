/**
 * Active Positions Card - displays open positions
 */
import { Link } from 'react-router-dom';
import { ArrowRight, TrendingUp, TrendingDown } from 'lucide-react';
import { Card, Spinner, Button } from '@/shared/components';
import { usePositions } from '../hooks/useDashboardData';
import { formatPrice, formatQuantity, formatCurrency } from '@/shared/utils';

export function ActivePositionsCard() {
  const { data: positions, isLoading, error } = usePositions();

  if (isLoading) {
    return (
      <Card title="Active Positions">
        <div className="flex items-center justify-center h-32">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (error) {
    return (
      <Card title="Active Positions">
        <div className="text-danger text-sm">Failed to load positions</div>
      </Card>
    );
  }

  const hasPositions = positions && positions.length > 0;

  return (
    <Card
      title="Active Positions"
      headerAction={
        hasPositions && (
          <Link to="/trading">
            <Button variant="secondary" size="sm">
              View All
              <ArrowRight className="w-4 h-4 ml-1" />
            </Button>
          </Link>
        )
      }
    >
      {!hasPositions ? (
        <div className="text-center py-8">
          <p className="text-text-muted mb-4">No open positions</p>
          <Link to="/trading">
            <Button variant="primary" size="sm">
              Start Trading
            </Button>
          </Link>
        </div>
      ) : (
        <div className="space-y-3">
          {positions.slice(0, 5).map((position) => (
            <div
              key={`${position.symbol}-${position.venue}`}
              className="flex items-center justify-between p-3 bg-background-secondary rounded-lg"
            >
              <div className="flex items-center gap-3">
                <div
                  className={`w-2 h-8 rounded-full ${
                    position.side === 'LONG' ? 'bg-success' : 'bg-danger'
                  }`}
                />
                <div>
                  <div className="font-medium">{position.symbol}</div>
                  <div className="text-xs text-text-muted">
                    {position.side} {formatQuantity(position.qty)}
                  </div>
                </div>
              </div>

              <div className="text-right">
                <div className="font-tabular">
                  ${formatPrice(position.current_price, 2)}
                </div>
                <div
                  className={`flex items-center justify-end gap-1 text-sm ${
                    position.unrealized_pnl >= 0 ? 'text-success' : 'text-danger'
                  }`}
                >
                  {position.unrealized_pnl >= 0 ? (
                    <TrendingUp className="w-3 h-3" />
                  ) : (
                    <TrendingDown className="w-3 h-3" />
                  )}
                  <span className="font-tabular">
                    {position.unrealized_pnl >= 0 ? '+' : ''}
                    {formatCurrency(position.unrealized_pnl)}
                  </span>
                </div>
              </div>
            </div>
          ))}

          {positions.length > 5 && (
            <div className="text-center text-sm text-text-muted">
              +{positions.length - 5} more positions
            </div>
          )}
        </div>
      )}
    </Card>
  );
}
