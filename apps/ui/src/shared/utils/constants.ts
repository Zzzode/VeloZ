/**
 * Application constants
 */

export const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';

export const AUTH_ENABLED = import.meta.env.VITE_AUTH_ENABLED === 'true';

export const ORDER_SIDES = ['BUY', 'SELL'] as const;

export const ORDER_STATUSES = [
  'ACCEPTED',
  'PARTIALLY_FILLED',
  'FILLED',
  'CANCELLED',
  'REJECTED',
  'EXPIRED',
] as const;

export const STRATEGY_STATES = ['Running', 'Paused', 'Stopped'] as const;

export const TIME_FRAMES = ['1m', '5m', '15m', '1h', '4h', '1d'] as const;

export const DATA_SOURCES = ['csv', 'binance'] as const;

export const DATA_TYPES = ['kline', 'trade', 'book'] as const;

/**
 * Status color mapping
 */
export const STATUS_COLORS: Record<string, string> = {
  ACCEPTED: 'bg-info-100 text-info-800',
  PARTIALLY_FILLED: 'bg-warning-100 text-warning-800',
  FILLED: 'bg-success-100 text-success-800',
  CANCELLED: 'bg-gray-100 text-gray-800',
  REJECTED: 'bg-danger-100 text-danger-800',
  EXPIRED: 'bg-gray-100 text-gray-800',
  Running: 'bg-success-100 text-success-800',
  Paused: 'bg-warning-100 text-warning-800',
  Stopped: 'bg-gray-100 text-gray-800',
};

/**
 * Side color mapping
 */
export const SIDE_COLORS: Record<string, string> = {
  BUY: 'text-success',
  SELL: 'text-danger',
  LONG: 'text-success',
  SHORT: 'text-danger',
};
