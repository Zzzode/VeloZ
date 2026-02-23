/**
 * Format a price with appropriate decimal places
 */
export function formatPrice(price: number, decimals = 2): string {
  return price.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

/**
 * Format a price with currency symbol
 */
export function formatCurrency(amount: number, currency = 'USD'): string {
  return new Intl.NumberFormat('en-US', {
    style: 'currency',
    currency,
  }).format(amount);
}

/**
 * Format a quantity with appropriate decimal places
 */
export function formatQuantity(qty: number, decimals = 4): string {
  return qty.toLocaleString('en-US', {
    minimumFractionDigits: 0,
    maximumFractionDigits: decimals,
  });
}

/**
 * Format a percentage
 */
export function formatPercent(value: number, decimals = 2): string {
  return `${(value * 100).toFixed(decimals)}%`;
}

/**
 * Format a timestamp (nanoseconds) to date string
 */
export function formatTimestamp(tsNs: number): string {
  const date = new Date(tsNs / 1_000_000);
  return date.toLocaleString();
}

/**
 * Format a timestamp (nanoseconds) to time string
 */
export function formatTime(tsNs: number): string {
  const date = new Date(tsNs / 1_000_000);
  return date.toLocaleTimeString();
}

/**
 * Format a timestamp (nanoseconds) to date only
 */
export function formatDate(tsNs: number): string {
  const date = new Date(tsNs / 1_000_000);
  return date.toLocaleDateString();
}

/**
 * Format a number with K/M/B suffixes
 */
export function formatCompact(value: number): string {
  return new Intl.NumberFormat('en-US', {
    notation: 'compact',
    compactDisplay: 'short',
  }).format(value);
}

/**
 * Format PnL with color class
 */
export function formatPnL(pnl: number): { value: string; className: string } {
  const value = formatCurrency(pnl);
  const className = pnl >= 0 ? 'text-success' : 'text-danger';
  return { value: pnl >= 0 ? `+${value}` : value, className };
}

/**
 * Truncate a string with ellipsis
 */
export function truncate(str: string, maxLength: number): string {
  if (str.length <= maxLength) return str;
  return `${str.slice(0, maxLength - 3)}...`;
}
