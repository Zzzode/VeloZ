/**
 * VeloZ UI Utility Functions
 * Extracted for testability and reuse
 */

/**
 * Escape HTML special characters to prevent XSS
 * @param {string|null|undefined} str - Input string
 * @returns {string} Escaped string
 */
export function escapeHtml(str) {
  if (str == null) return "";
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

/**
 * Format parameter name from snake_case to Title Case
 * @param {string} key - Parameter key in snake_case
 * @returns {string} Formatted name
 */
export function formatParamName(key) {
  return key.replace(/_/g, " ").replace(/\b\w/g, c => c.toUpperCase());
}

/**
 * Format timestamp to locale time string
 * @param {number} ts - Timestamp in milliseconds
 * @returns {string} Formatted time
 */
export function formatTime(ts) {
  const d = new Date(ts);
  return d.toLocaleTimeString();
}

/**
 * Format uptime duration from milliseconds
 * @param {number} ms - Duration in milliseconds
 * @returns {string} Formatted duration (e.g., "3d 5h" or "2h 30m")
 */
export function formatUptime(ms) {
  const hours = Math.floor(ms / 3600000);
  const minutes = Math.floor((ms % 3600000) / 60000);
  if (hours > 24) {
    const days = Math.floor(hours / 24);
    return `${days}d ${hours % 24}h`;
  }
  return `${hours}h ${minutes}m`;
}

/**
 * Format price with appropriate decimal places
 * @param {number} price - Price value
 * @param {number} [decimals=2] - Number of decimal places
 * @returns {string} Formatted price
 */
export function formatPrice(price, decimals = 2) {
  if (price == null || isNaN(price)) return "-";
  return price.toFixed(decimals);
}

/**
 * Format quantity with appropriate decimal places
 * @param {number} qty - Quantity value
 * @param {number} [decimals=4] - Number of decimal places
 * @returns {string} Formatted quantity
 */
export function formatQuantity(qty, decimals = 4) {
  if (qty == null || isNaN(qty)) return "-";
  return qty.toFixed(decimals);
}

/**
 * Format PnL with sign and color class
 * @param {number} pnl - PnL value
 * @param {number} [decimals=2] - Number of decimal places
 * @returns {{value: string, className: string}} Formatted PnL with CSS class
 */
export function formatPnL(pnl, decimals = 2) {
  if (pnl == null || isNaN(pnl)) {
    return { value: "-", className: "" };
  }
  const sign = pnl >= 0 ? "+" : "";
  return {
    value: `${sign}${pnl.toFixed(decimals)}`,
    className: pnl >= 0 ? "positive" : "negative"
  };
}

/**
 * Format percentage with sign
 * @param {number} pct - Percentage value (0.05 = 5%)
 * @param {number} [decimals=2] - Number of decimal places
 * @returns {string} Formatted percentage
 */
export function formatPercent(pct, decimals = 2) {
  if (pct == null || isNaN(pct)) return "-";
  const sign = pct >= 0 ? "+" : "";
  return `${sign}${(pct * 100).toFixed(decimals)}%`;
}

/**
 * Calculate spread from best bid and ask
 * @param {number} bid - Best bid price
 * @param {number} ask - Best ask price
 * @returns {{absolute: number, percent: number}|null} Spread info or null if invalid
 */
export function calculateSpread(bid, ask) {
  if (bid == null || ask == null || bid <= 0 || ask <= 0) {
    return null;
  }
  const absolute = ask - bid;
  const midPrice = (bid + ask) / 2;
  const percent = (absolute / midPrice) * 100;
  return { absolute, percent };
}

/**
 * Validate order parameters
 * @param {Object} params - Order parameters
 * @param {string} params.side - BUY or SELL
 * @param {string} params.symbol - Trading symbol
 * @param {number} params.qty - Quantity
 * @param {number} params.price - Price
 * @returns {{valid: boolean, errors: string[]}} Validation result
 */
export function validateOrderParams({ side, symbol, qty, price }) {
  const errors = [];

  if (!side || !["BUY", "SELL"].includes(side.toUpperCase())) {
    errors.push("Side must be BUY or SELL");
  }

  if (!symbol || typeof symbol !== "string" || symbol.trim() === "") {
    errors.push("Symbol is required");
  }

  if (qty == null || isNaN(qty) || qty <= 0) {
    errors.push("Quantity must be a positive number");
  }

  if (price == null || isNaN(price) || price <= 0) {
    errors.push("Price must be a positive number");
  }

  return { valid: errors.length === 0, errors };
}

/**
 * Parse API base URL, removing trailing slash
 * @param {string} url - Input URL
 * @returns {string} Normalized URL
 */
export function normalizeApiBase(url) {
  if (!url || typeof url !== "string") return "";
  return url.trim().replace(/\/+$/, "");
}

/**
 * Generate a client order ID
 * @param {string} [prefix="web"] - ID prefix
 * @returns {string} Generated order ID
 */
export function generateClientOrderId(prefix = "web") {
  return `${prefix}-${Date.now()}`;
}
