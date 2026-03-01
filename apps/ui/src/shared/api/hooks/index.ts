/**
 * Shared API Hooks
 * Re-export all TanStack Query hooks for API integration
 */

// Client utilities
export { getApiClient, getSSEClient, getMarketWsClient, getOrderWsClient, resetClients, API_BASE } from './client';

// Query keys
export { queryKeys } from './queryKeys';

// Market data hooks
export { useMarketData } from './useMarketData';

// Account hooks
export { useAccount } from './useAccount';

// Position hooks
export { usePositions, usePosition } from './usePositions';

// Order hooks
export {
  useOrders,
  useOrderStates,
  useOrderState,
  usePlaceOrder,
  useCancelOrder,
} from './useOrders';

// Strategy hooks
export {
  useStrategies,
  useStrategy,
  useStartStrategy,
  useStopStrategy,
} from './useStrategies';

// Backtest hooks
export {
  useBacktests,
  useBacktest,
  useRunBacktest,
  useCancelBacktest,
  useDeleteBacktest,
} from './useBacktest';

// Engine hooks
export {
  useEngineStatus,
  useStartEngine,
  useStopEngine,
} from './useEngine';

// SSE hooks
export {
  useSSE,
  useSSEConnectionState,
  type UseSSEOptions,
  type UseSSEReturn,
} from './useSSE';
