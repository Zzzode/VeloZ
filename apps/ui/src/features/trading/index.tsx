/**
 * Trading Page
 * Main trading interface with order book, order form, and order history
 */

export { default } from './TradingPage';

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
