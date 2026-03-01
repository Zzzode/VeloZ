/**
 * Recent Orders Card - displays recent order activity
 */
import { Link } from 'react-router-dom';
import { ArrowRight, Clock } from 'lucide-react';
import { Card, Spinner, Button } from '@/shared/components';
import { useOrderStates } from '../hooks/useDashboardData';
import { formatPrice, formatQuantity, formatTime } from '@/shared/utils';
import { STATUS_COLORS, SIDE_COLORS } from '@/shared/utils/constants';

export function RecentOrdersCard() {
  const { data: orders, isLoading, error } = useOrderStates();

  if (isLoading) {
    return (
      <Card title="Recent Orders">
        <div className="flex items-center justify-center h-32">
          <Spinner size="md" />
        </div>
      </Card>
    );
  }

  if (error) {
    return (
      <Card title="Recent Orders">
        <div className="text-danger text-sm">Failed to load orders</div>
      </Card>
    );
  }

  const hasOrders = orders && orders.length > 0;

  // Sort by timestamp descending and take recent 5
  const recentOrders = hasOrders
    ? [...orders].sort((a, b) => b.last_ts_ns - a.last_ts_ns).slice(0, 5)
    : [];

  return (
    <Card
      title="Recent Orders"
      headerAction={
        hasOrders && (
          <Link to="/trading">
            <Button variant="secondary" size="sm">
              View All
              <ArrowRight className="w-4 h-4 ml-1" />
            </Button>
          </Link>
        )
      }
    >
      {!hasOrders ? (
        <div className="text-center py-8">
          <p className="text-text-muted mb-4">No orders yet</p>
          <Link to="/trading">
            <Button variant="primary" size="sm">
              Place Order
            </Button>
          </Link>
        </div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="text-xs text-text-muted uppercase">
                <th className="text-left pb-2">Symbol</th>
                <th className="text-left pb-2">Side</th>
                <th className="text-right pb-2">Qty</th>
                <th className="text-right pb-2">Price</th>
                <th className="text-right pb-2">Status</th>
              </tr>
            </thead>
            <tbody className="text-sm">
              {recentOrders.map((order) => (
                <tr key={order.client_order_id} className="border-t border-border-light">
                  <td className="py-2">
                    <div className="font-medium">{order.symbol}</div>
                    <div className="text-xs text-text-muted flex items-center gap-1">
                      <Clock className="w-3 h-3" />
                      {formatTime(order.last_ts_ns)}
                    </div>
                  </td>
                  <td className={`py-2 font-medium ${SIDE_COLORS[order.side] || ''}`}>
                    {order.side}
                  </td>
                  <td className="py-2 text-right font-tabular">
                    {formatQuantity(order.order_qty)}
                  </td>
                  <td className="py-2 text-right font-tabular">
                    ${formatPrice(order.limit_price, 2)}
                  </td>
                  <td className="py-2 text-right">
                    <span
                      className={`inline-flex items-center px-2 py-0.5 rounded text-xs font-medium ${
                        STATUS_COLORS[order.status] || 'bg-gray-100 text-gray-800'
                      }`}
                    >
                      {order.status}
                    </span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </Card>
  );
}
