/**
 * Order History Component
 * Table displaying order history with status and actions
 */

import { useEffect, useState, useCallback } from 'react';
import { Card, Button, Spinner } from '@/shared/components';
// useTradingStore available if needed for local state management
import { useOrders, useCancelOrder, useOrderUpdates } from '../hooks';
import { formatPrice, formatQuantity, formatTimestamp, truncate } from '@/shared/utils/format';
import { STATUS_COLORS, SIDE_COLORS } from '@/shared/utils/constants';
import type { OrderState } from '@/shared/api/types';

interface OrderRowProps {
  order: OrderState;
  onCancel: (clientOrderId: string) => void;
  isCancelling: boolean;
}

function OrderRow({ order, onCancel, isCancelling }: OrderRowProps) {
  const canCancel = order.status === 'ACCEPTED' || order.status === 'PARTIALLY_FILLED';
  const fillPercent =
    order.order_qty > 0 ? (order.executed_qty / order.order_qty) * 100 : 0;

  return (
    <tr className="border-b border-border hover:bg-background-secondary/50">
      <td className="px-3 py-2 text-sm">
        <span className="font-mono text-xs" title={order.client_order_id}>
          {truncate(order.client_order_id, 12)}
        </span>
      </td>
      <td className="px-3 py-2 text-sm font-medium">{order.symbol}</td>
      <td className="px-3 py-2 text-sm">
        <span className={SIDE_COLORS[order.side]}>{order.side}</span>
      </td>
      <td className="px-3 py-2 text-sm font-mono">{formatPrice(order.limit_price, 2)}</td>
      <td className="px-3 py-2 text-sm font-mono">
        {formatQuantity(order.executed_qty, 4)} / {formatQuantity(order.order_qty, 4)}
        {order.order_qty > 0 && (
          <div className="mt-1 h-1 bg-gray-200 rounded-full overflow-hidden">
            <div
              className="h-full bg-primary"
              style={{ width: `${fillPercent}%` }}
            />
          </div>
        )}
      </td>
      <td className="px-3 py-2 text-sm font-mono">
        {order.avg_price > 0 ? formatPrice(order.avg_price, 2) : '-'}
      </td>
      <td className="px-3 py-2 text-sm">
        <span
          className={`inline-flex items-center px-2 py-0.5 rounded text-xs font-medium ${
            STATUS_COLORS[order.status] || 'bg-gray-100 text-gray-800'
          }`}
        >
          {order.status}
        </span>
      </td>
      <td className="px-3 py-2 text-sm text-text-muted">
        {formatTimestamp(order.last_ts_ns)}
      </td>
      <td className="px-3 py-2 text-sm">
        {canCancel && (
          <Button
            variant="danger"
            size="sm"
            onClick={() => onCancel(order.client_order_id)}
            isLoading={isCancelling}
            disabled={isCancelling}
          >
            Cancel
          </Button>
        )}
      </td>
    </tr>
  );
}

type FilterStatus = 'all' | 'open' | 'filled' | 'cancelled';

export function OrderHistory() {
  const { orders, isLoading, refetch } = useOrders();
  const { cancelOrder } = useCancelOrder();
  const { connectionState } = useOrderUpdates();

  const [filter, setFilter] = useState<FilterStatus>('all');
  const [cancellingId, setCancellingId] = useState<string | null>(null);

  // Fetch orders on mount
  useEffect(() => {
    refetch();
  }, [refetch]);

  const handleCancel = useCallback(
    async (clientOrderId: string) => {
      setCancellingId(clientOrderId);
      try {
        await cancelOrder({ client_order_id: clientOrderId });
        // Refetch to get updated status
        await refetch();
      } catch {
        // Error handled in hook
      } finally {
        setCancellingId(null);
      }
    },
    [cancelOrder, refetch]
  );

  const filteredOrders = orders.filter((order) => {
    switch (filter) {
      case 'open':
        return order.status === 'ACCEPTED' || order.status === 'PARTIALLY_FILLED';
      case 'filled':
        return order.status === 'FILLED';
      case 'cancelled':
        return order.status === 'CANCELLED' || order.status === 'REJECTED';
      default:
        return true;
    }
  });

  const connectionIndicator = (
    <div className="flex items-center gap-2">
      <div
        className={`w-2 h-2 rounded-full ${
          connectionState === 'connected'
            ? 'bg-success'
            : connectionState === 'connecting' || connectionState === 'reconnecting'
              ? 'bg-warning animate-pulse'
              : 'bg-danger'
        }`}
      />
      <span className="text-xs text-text-muted capitalize">{connectionState}</span>
    </div>
  );

  return (
    <Card
      title="Order History"
      subtitle={`${filteredOrders.length} orders`}
      headerAction={
        <div className="flex items-center gap-4">
          {connectionIndicator}
          <Button variant="secondary" size="sm" onClick={refetch} disabled={isLoading}>
            Refresh
          </Button>
        </div>
      }
    >
      {/* Filter Tabs */}
      <div className="flex gap-2 mb-4 border-b border-border pb-2">
        {(['all', 'open', 'filled', 'cancelled'] as FilterStatus[]).map((status) => (
          <button
            key={status}
            onClick={() => setFilter(status)}
            className={`px-3 py-1.5 text-sm font-medium rounded-md transition-colors ${
              filter === status
                ? 'bg-primary text-white'
                : 'text-text-muted hover:bg-background-secondary'
            }`}
          >
            {status.charAt(0).toUpperCase() + status.slice(1)}
          </button>
        ))}
      </div>

      {/* Table */}
      {isLoading && orders.length === 0 ? (
        <div className="flex items-center justify-center py-8">
          <Spinner size="lg" />
        </div>
      ) : filteredOrders.length === 0 ? (
        <div className="text-center py-8 text-text-muted">
          No {filter === 'all' ? '' : filter} orders found.
        </div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="border-b border-border">
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Order ID
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Symbol
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Side
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Price
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Filled / Qty
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Avg Price
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Status
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Time
                </th>
                <th className="px-3 py-2 text-left text-xs font-medium text-text-muted uppercase">
                  Action
                </th>
              </tr>
            </thead>
            <tbody>
              {filteredOrders.map((order) => (
                <OrderRow
                  key={order.client_order_id}
                  order={order}
                  onCancel={handleCancel}
                  isCancelling={cancellingId === order.client_order_id}
                />
              ))}
            </tbody>
          </table>
        </div>
      )}
    </Card>
  );
}
