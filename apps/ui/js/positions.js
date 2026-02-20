/**
 * VeloZ Position and PnL Display Module
 *
 * Real-time position tracking with:
 * - Position list with size, avg price, PnL
 * - Unrealized/realized PnL breakdown
 * - Multi-symbol support
 * - Historical PnL chart data
 * - SSE integration for real-time updates
 */

import { escapeHtml, formatPrice, formatQuantity, formatPnL } from './utils.js';

/**
 * Position side enumeration (matches C++ PositionSide)
 */
export const PositionSide = {
  NONE: 'none',
  LONG: 'long',
  SHORT: 'short',
};

/**
 * Position configuration
 */
export const PositionConfig = {
  // Update throttle interval (ms)
  throttleMs: 100,
  // Price decimal places
  priceDecimals: 2,
  // Quantity decimal places
  qtyDecimals: 4,
  // PnL decimal places
  pnlDecimals: 2,
  // Max history points for chart
  maxHistoryPoints: 100,
};

/**
 * Single position data
 */
export class PositionData {
  constructor(data = {}) {
    this.symbol = data.symbol || '';
    this.size = parseFloat(data.size) || 0;
    this.avgPrice = parseFloat(data.avg_price || data.avgPrice) || 0;
    this.realizedPnl = parseFloat(data.realized_pnl || data.realizedPnl) || 0;
    this.unrealizedPnl = parseFloat(data.unrealized_pnl || data.unrealizedPnl) || 0;
    this.side = this._determineSide(this.size);
    this.currentPrice = parseFloat(data.current_price || data.currentPrice) || 0;
    this.timestamp = data.timestamp_ns || data.timestamp || Date.now();
  }

  _determineSide(size) {
    if (size > 0) return PositionSide.LONG;
    if (size < 0) return PositionSide.SHORT;
    return PositionSide.NONE;
  }

  /**
   * Total PnL (realized + unrealized)
   */
  get totalPnl() {
    return this.realizedPnl + this.unrealizedPnl;
  }

  /**
   * Notional value at current price
   */
  get notionalValue() {
    return Math.abs(this.size) * this.currentPrice;
  }

  /**
   * PnL percentage based on notional
   */
  get pnlPercent() {
    const cost = Math.abs(this.size) * this.avgPrice;
    if (cost === 0) return 0;
    return (this.totalPnl / cost) * 100;
  }

  /**
   * Check if position is flat (zero size)
   */
  get isFlat() {
    return Math.abs(this.size) < 1e-8;
  }
}

/**
 * Position manager for tracking multiple positions
 */
export class PositionManager {
  constructor(config = {}) {
    this.config = { ...PositionConfig, ...config };
    this.positions = new Map(); // symbol -> PositionData
    this.listeners = [];
    this.pnlHistory = []; // Array of { timestamp, totalPnl, unrealizedPnl, realizedPnl }
    this._throttleTimeout = null;
    this._pendingUpdate = null;
  }

  /**
   * Apply position update (snapshot or delta)
   * @param {Object|Array} data - Position data or array of positions
   */
  applyUpdate(data) {
    const now = Date.now();

    // Throttle updates
    if (now - this._lastUpdateTime < this.config.throttleMs) {
      this._pendingUpdate = data;
      if (!this._throttleTimeout) {
        this._throttleTimeout = setTimeout(() => {
          this._throttleTimeout = null;
          if (this._pendingUpdate) {
            this._applyUpdateInternal(this._pendingUpdate);
            this._pendingUpdate = null;
          }
        }, this.config.throttleMs);
      }
      return;
    }

    this._applyUpdateInternal(data);
  }

  _applyUpdateInternal(data) {
    this._lastUpdateTime = Date.now();

    // Handle array of positions (snapshot)
    if (Array.isArray(data)) {
      for (const pos of data) {
        this._updatePosition(pos);
      }
    } else if (data.positions && Array.isArray(data.positions)) {
      // Handle { positions: [...] } format
      for (const pos of data.positions) {
        this._updatePosition(pos);
      }
    } else if (data.symbol) {
      // Single position update
      this._updatePosition(data);
    }

    // Record PnL history
    this._recordPnlHistory();

    // Notify listeners
    this._notifyListeners();
  }

  _updatePosition(data) {
    const position = new PositionData(data);
    if (position.symbol) {
      if (position.isFlat) {
        // Remove flat positions
        this.positions.delete(position.symbol);
      } else {
        this.positions.set(position.symbol, position);
      }
    }
  }

  _recordPnlHistory() {
    const totals = this.getTotals();
    this.pnlHistory.push({
      timestamp: Date.now(),
      totalPnl: totals.totalPnl,
      unrealizedPnl: totals.unrealizedPnl,
      realizedPnl: totals.realizedPnl,
    });

    // Trim history to max points
    if (this.pnlHistory.length > this.config.maxHistoryPoints) {
      this.pnlHistory = this.pnlHistory.slice(-this.config.maxHistoryPoints);
    }
  }

  /**
   * Get position by symbol
   * @param {string} symbol
   * @returns {PositionData|null}
   */
  getPosition(symbol) {
    return this.positions.get(symbol) || null;
  }

  /**
   * Get all positions as array
   * @returns {PositionData[]}
   */
  getAllPositions() {
    return Array.from(this.positions.values());
  }

  /**
   * Get total PnL across all positions
   * @returns {Object} { totalPnl, unrealizedPnl, realizedPnl, notionalValue }
   */
  getTotals() {
    let totalPnl = 0;
    let unrealizedPnl = 0;
    let realizedPnl = 0;
    let notionalValue = 0;

    for (const pos of this.positions.values()) {
      totalPnl += pos.totalPnl;
      unrealizedPnl += pos.unrealizedPnl;
      realizedPnl += pos.realizedPnl;
      notionalValue += pos.notionalValue;
    }

    return { totalPnl, unrealizedPnl, realizedPnl, notionalValue };
  }

  /**
   * Get PnL history for charting
   * @returns {Array}
   */
  getPnlHistory() {
    return this.pnlHistory.slice();
  }

  /**
   * Clear all positions
   */
  clear() {
    this.positions.clear();
    this.pnlHistory = [];
    this._notifyListeners();
  }

  /**
   * Add listener for position updates
   * @param {Function} callback
   */
  addListener(callback) {
    this.listeners.push(callback);
  }

  /**
   * Remove listener
   * @param {Function} callback
   */
  removeListener(callback) {
    const idx = this.listeners.indexOf(callback);
    if (idx >= 0) {
      this.listeners.splice(idx, 1);
    }
  }

  _notifyListeners() {
    for (const listener of this.listeners) {
      try {
        listener(this);
      } catch (e) {
        console.error('Position listener error:', e);
      }
    }
  }
}

/**
 * Render a single position row
 * @param {PositionData} position
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderPositionRow(position, options = {}) {
  const { priceDecimals = 2, qtyDecimals = 4, pnlDecimals = 2 } = options;

  const sideClass = position.side === PositionSide.LONG ? 'side-long' :
                    position.side === PositionSide.SHORT ? 'side-short' : 'side-flat';
  const sideLabel = position.side === PositionSide.LONG ? 'LONG' :
                    position.side === PositionSide.SHORT ? 'SHORT' : 'FLAT';

  const pnlClass = position.totalPnl >= 0 ? 'pnl-positive' : 'pnl-negative';
  const unrealizedClass = position.unrealizedPnl >= 0 ? 'pnl-positive' : 'pnl-negative';

  return `
    <tr class="position-row" data-symbol="${escapeHtml(position.symbol)}">
      <td class="symbol-cell">${escapeHtml(position.symbol)}</td>
      <td class="side-cell ${sideClass}">${sideLabel}</td>
      <td class="size-cell mono">${formatQuantity(Math.abs(position.size), qtyDecimals)}</td>
      <td class="price-cell mono">${formatPrice(position.avgPrice, priceDecimals)}</td>
      <td class="price-cell mono">${formatPrice(position.currentPrice, priceDecimals)}</td>
      <td class="pnl-cell mono ${unrealizedClass}">${formatPnL(position.unrealizedPnl, pnlDecimals).value}</td>
      <td class="pnl-cell mono ${pnlClass}">${formatPnL(position.realizedPnl, pnlDecimals).value}</td>
      <td class="pnl-cell mono ${pnlClass}">${formatPnL(position.totalPnl, pnlDecimals).value}</td>
      <td class="pnl-pct-cell mono ${pnlClass}">${position.pnlPercent.toFixed(2)}%</td>
    </tr>
  `;
}

/**
 * Render positions table
 * @param {PositionManager} manager
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderPositionsTable(manager, options = {}) {
  const positions = manager.getAllPositions();

  if (positions.length === 0) {
    return `
      <div class="positions-empty">
        <p>No open positions</p>
      </div>
    `;
  }

  const rows = positions.map(pos => renderPositionRow(pos, options)).join('');

  return `
    <table class="positions-table">
      <thead>
        <tr>
          <th>Symbol</th>
          <th>Side</th>
          <th>Size</th>
          <th>Avg Price</th>
          <th>Current</th>
          <th>Unrealized</th>
          <th>Realized</th>
          <th>Total PnL</th>
          <th>PnL %</th>
        </tr>
      </thead>
      <tbody>
        ${rows}
      </tbody>
    </table>
  `;
}

/**
 * Render PnL summary card
 * @param {PositionManager} manager
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderPnlSummary(manager, options = {}) {
  const { pnlDecimals = 2 } = options;
  const totals = manager.getTotals();

  const totalClass = totals.totalPnl >= 0 ? 'pnl-positive' : 'pnl-negative';
  const unrealizedClass = totals.unrealizedPnl >= 0 ? 'pnl-positive' : 'pnl-negative';
  const realizedClass = totals.realizedPnl >= 0 ? 'pnl-positive' : 'pnl-negative';

  return `
    <div class="pnl-summary">
      <div class="pnl-item">
        <span class="pnl-label">Total PnL</span>
        <span class="pnl-value mono ${totalClass}">${formatPnL(totals.totalPnl, pnlDecimals).value}</span>
      </div>
      <div class="pnl-item">
        <span class="pnl-label">Unrealized</span>
        <span class="pnl-value mono ${unrealizedClass}">${formatPnL(totals.unrealizedPnl, pnlDecimals).value}</span>
      </div>
      <div class="pnl-item">
        <span class="pnl-label">Realized</span>
        <span class="pnl-value mono ${realizedClass}">${formatPnL(totals.realizedPnl, pnlDecimals).value}</span>
      </div>
      <div class="pnl-item">
        <span class="pnl-label">Notional</span>
        <span class="pnl-value mono">${formatPrice(totals.notionalValue, pnlDecimals)}</span>
      </div>
    </div>
  `;
}

/**
 * Subscribe to position updates via SSE
 * @param {string} apiBase - API base URL
 * @param {PositionManager} manager
 * @param {Object} options
 * @returns {EventSource}
 */
export function subscribePositionUpdates(apiBase, manager, options = {}) {
  const { onError = null, onConnect = null } = options;

  const url = `${apiBase}/api/stream?types=position,account`;
  const es = new EventSource(url);

  es.onopen = () => {
    if (onConnect) {
      onConnect();
    }
  };

  // Handle explicit position events
  es.addEventListener('position', (event) => {
    try {
      const data = JSON.parse(event.data);
      manager.applyUpdate(data);
    } catch (e) {
      console.error('Failed to parse position update:', e);
    }
  });

  // Handle account events (may contain positions)
  es.addEventListener('account', (event) => {
    try {
      const data = JSON.parse(event.data);
      if (data.positions) {
        manager.applyUpdate(data.positions);
      }
    } catch (e) {
      console.error('Failed to parse account update:', e);
    }
  });

  // Handle position_update events
  es.addEventListener('position_update', (event) => {
    try {
      const data = JSON.parse(event.data);
      manager.applyUpdate(data);
    } catch (e) {
      console.error('Failed to parse position_update:', e);
    }
  });

  // Fallback: Handle generic message events
  es.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (data.type === 'position' || data.type === 'position_update') {
        manager.applyUpdate(data);
      } else if (data.type === 'account' && data.positions) {
        manager.applyUpdate(data.positions);
      }
    } catch (e) {
      // Not JSON or not a position event, ignore
    }
  };

  es.onerror = (event) => {
    console.error('Position SSE error:', event);
    if (onError) {
      onError(event);
    }
  };

  return es;
}

/**
 * Fetch positions snapshot via REST API
 * @param {string} apiBase - API base URL
 * @returns {Promise<Array>} Position data array
 */
export async function fetchPositions(apiBase) {
  const url = `${apiBase}/api/positions`;
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch positions: HTTP ${res.status}`);
  }
  const data = await res.json();
  return data.positions || data || [];
}

/**
 * Fetch account summary via REST API
 * @param {string} apiBase - API base URL
 * @returns {Promise<Object>} Account data
 */
export async function fetchAccount(apiBase) {
  const url = `${apiBase}/api/account`;
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch account: HTTP ${res.status}`);
  }
  return res.json();
}

// Export to window for non-module script access
if (typeof window !== 'undefined') {
  window.VeloZPositions = {
    PositionSide,
    PositionConfig,
    PositionData,
    PositionManager,
    renderPositionRow,
    renderPositionsTable,
    renderPnlSummary,
    subscribePositionUpdates,
    fetchPositions,
    fetchAccount,
  };
}
