/**
 * Query Key Factory
 * Centralized query keys for TanStack Query cache management
 */

export const queryKeys = {
  // Market data
  market: {
    all: ['market'] as const,
    data: (symbol?: string) => [...queryKeys.market.all, 'data', symbol] as const,
  },

  // Account
  account: {
    all: ['account'] as const,
    balance: () => [...queryKeys.account.all, 'balance'] as const,
  },

  // Positions
  positions: {
    all: ['positions'] as const,
    list: () => [...queryKeys.positions.all, 'list'] as const,
    detail: (symbol: string) => [...queryKeys.positions.all, 'detail', symbol] as const,
  },

  // Orders
  orders: {
    all: ['orders'] as const,
    list: () => [...queryKeys.orders.all, 'list'] as const,
    states: () => [...queryKeys.orders.all, 'states'] as const,
    state: (clientOrderId: string) => [...queryKeys.orders.all, 'state', clientOrderId] as const,
  },

  // Strategies
  strategies: {
    all: ['strategies'] as const,
    list: () => [...queryKeys.strategies.all, 'list'] as const,
    detail: (id: string) => [...queryKeys.strategies.all, 'detail', id] as const,
  },

  // Backtests
  backtests: {
    all: ['backtests'] as const,
    list: () => [...queryKeys.backtests.all, 'list'] as const,
    detail: (id: string) => [...queryKeys.backtests.all, 'detail', id] as const,
  },

  // Engine
  engine: {
    all: ['engine'] as const,
    status: () => [...queryKeys.engine.all, 'status'] as const,
  },

  // Config
  config: {
    all: ['config'] as const,
  },

  // Audit
  audit: {
    all: ['audit'] as const,
    logs: (params?: Record<string, unknown>) => [...queryKeys.audit.all, 'logs', params] as const,
    stats: () => [...queryKeys.audit.all, 'stats'] as const,
  },
} as const;
