/**
 * Trading Page
 * Main trading interface with order book, order form, and order history
 */

import { OrderForm, OrderBook, OrderHistory, PositionsList } from './components';
import { useTradingStore } from './store';

export default function Trading() {
  const { error, setError } = useTradingStore();

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold text-text">Trading</h1>
        <p className="text-text-muted mt-1">Place orders and manage positions</p>
      </div>

      {/* Error Banner */}
      {error && (
        <div className="p-4 bg-danger-50 border border-danger-200 rounded-lg flex items-center justify-between">
          <span className="text-danger">{error}</span>
          <button
            onClick={() => setError(null)}
            className="text-danger hover:text-danger-700"
          >
            Dismiss
          </button>
        </div>
      )}

      {/* Main Trading Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Order Form - Left Column */}
        <div className="lg:col-span-1">
          <OrderForm />
        </div>

        {/* Order Book - Right Columns */}
        <div className="lg:col-span-2">
          <OrderBook />
        </div>
      </div>

      {/* Order History */}
      <OrderHistory />

      {/* Positions */}
      <PositionsList />
    </div>
  );
}

// Re-export store and hooks for external use
export { useTradingStore } from './store';
export type { TradingState, OrderBookState, OrderBookLevel } from './store';
export {
  useOrders,
  usePositions,
  usePlaceOrder,
  useCancelOrder,
  useOrderBookStream,
  useOrderUpdates,
} from './hooks';
