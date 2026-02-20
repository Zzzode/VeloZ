/**
 * VeloZ Order Book Display Module
 *
 * Real-time order book visualization with:
 * - Bid/ask price level display
 * - Depth visualization (volume bars)
 * - Price level grouping
 * - Update throttling for performance
 * - SSE integration for real-time updates
 */

import { escapeHtml, formatPrice, formatQuantity, calculateSpread } from './utils.js';

/**
 * Order book configuration
 */
export const OrderBookConfig = {
  // Number of price levels to display per side
  maxLevels: 15,
  // Update throttle interval (ms)
  throttleMs: 100,
  // Price grouping options (tick sizes)
  groupingOptions: [0.01, 0.1, 1, 10, 100],
  // Default grouping (0 = no grouping)
  defaultGrouping: 0,
  // Show cumulative depth
  showCumulative: true,
  // Animation duration for updates (ms)
  animationMs: 150,
};

/**
 * Price level with additional display data
 */
export class PriceLevel {
  constructor(price, qty, side) {
    this.price = price;
    this.qty = qty;
    this.side = side; // 'bid' or 'ask'
    this.total = qty; // Cumulative total (set by OrderBook)
    this.depthPercent = 0; // Percentage of max depth (for bar width)
    this.isNew = false; // Flash animation for new levels
    this.isUpdated = false; // Flash animation for updated levels
  }
}

/**
 * Order book state manager
 */
export class OrderBook {
  constructor(config = {}) {
    this.config = { ...OrderBookConfig, ...config };
    this.bids = []; // Array of PriceLevel, sorted descending by price
    this.asks = []; // Array of PriceLevel, sorted ascending by price
    this.lastUpdateTime = 0;
    this.sequence = 0;
    this.symbol = '';
    this.grouping = this.config.defaultGrouping;
    this.listeners = [];
    this._throttleTimeout = null;
    this._pendingUpdate = null;
  }

  /**
   * Apply a depth snapshot or delta update
   * @param {Object} data - Depth data with bids and asks arrays
   */
  applyUpdate(data) {
    const now = Date.now();

    // Throttle updates
    if (now - this.lastUpdateTime < this.config.throttleMs) {
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

  /**
   * Internal update application
   */
  _applyUpdateInternal(data) {
    this.lastUpdateTime = Date.now();

    if (data.symbol) {
      this.symbol = data.symbol;
    }
    if (data.sequence !== undefined) {
      this.sequence = data.sequence;
    }

    // Check if this is a snapshot (replace all) or delta (update)
    const isSnapshot = data.is_snapshot || data.isSnapshot || false;

    if (isSnapshot) {
      this._applySnapshot(data);
    } else {
      this._applyDelta(data);
    }

    // Recalculate cumulative totals and depth percentages
    this._recalculateTotals();

    // Notify listeners
    this._notifyListeners();
  }

  /**
   * Apply full snapshot (replaces existing book)
   */
  _applySnapshot(data) {
    // Process bids
    this.bids = (data.bids || [])
      .map(level => new PriceLevel(
        parseFloat(level.price !== undefined ? level.price : level[0]),
        parseFloat(level.qty !== undefined ? level.qty : level[1]),
        'bid'
      ))
      .filter(level => level.qty > 0)
      .sort((a, b) => b.price - a.price)
      .slice(0, this.config.maxLevels);

    // Process asks
    this.asks = (data.asks || [])
      .map(level => new PriceLevel(
        parseFloat(level.price !== undefined ? level.price : level[0]),
        parseFloat(level.qty !== undefined ? level.qty : level[1]),
        'ask'
      ))
      .filter(level => level.qty > 0)
      .sort((a, b) => a.price - b.price)
      .slice(0, this.config.maxLevels);

    // Mark all as new for animation
    this.bids.forEach(level => level.isNew = true);
    this.asks.forEach(level => level.isNew = true);
  }

  /**
   * Apply delta update (update/remove individual levels)
   */
  _applyDelta(data) {
    // Update bids
    if (data.bids) {
      for (const level of data.bids) {
        const price = parseFloat(level.price !== undefined ? level.price : level[0]);
        const qty = parseFloat(level.qty !== undefined ? level.qty : level[1]);
        this._updateLevel(this.bids, price, qty, 'bid', true);
      }
      // Re-sort and trim
      this.bids.sort((a, b) => b.price - a.price);
      this.bids = this.bids.slice(0, this.config.maxLevels);
    }

    // Update asks
    if (data.asks) {
      for (const level of data.asks) {
        const price = parseFloat(level.price !== undefined ? level.price : level[0]);
        const qty = parseFloat(level.qty !== undefined ? level.qty : level[1]);
        this._updateLevel(this.asks, price, qty, 'ask', false);
      }
      // Re-sort and trim
      this.asks.sort((a, b) => a.price - b.price);
      this.asks = this.asks.slice(0, this.config.maxLevels);
    }
  }

  /**
   * Update a single price level
   */
  _updateLevel(levels, price, qty, side, isBid) {
    const existingIndex = levels.findIndex(l => l.price === price);

    // Use <= 0 to handle floating point issues and negative values
    if (qty <= 0) {
      // Remove level
      if (existingIndex >= 0) {
        levels.splice(existingIndex, 1);
      }
    } else if (existingIndex >= 0) {
      // Update existing level
      levels[existingIndex].qty = qty;
      levels[existingIndex].isUpdated = true;
    } else {
      // Add new level
      const newLevel = new PriceLevel(price, qty, side);
      newLevel.isNew = true;
      levels.push(newLevel);
    }
  }

  /**
   * Recalculate cumulative totals and depth percentages
   */
  _recalculateTotals() {
    let maxDepth = 0;

    // Calculate cumulative totals for bids
    let cumulative = 0;
    for (const level of this.bids) {
      cumulative += level.qty;
      level.total = cumulative;
      maxDepth = Math.max(maxDepth, cumulative);
    }

    // Calculate cumulative totals for asks
    cumulative = 0;
    for (const level of this.asks) {
      cumulative += level.qty;
      level.total = cumulative;
      maxDepth = Math.max(maxDepth, cumulative);
    }

    // Calculate depth percentages
    if (maxDepth > 0) {
      for (const level of this.bids) {
        level.depthPercent = (level.total / maxDepth) * 100;
      }
      for (const level of this.asks) {
        level.depthPercent = (level.total / maxDepth) * 100;
      }
    }
  }

  /**
   * Get best bid price
   */
  bestBid() {
    return this.bids.length > 0 ? this.bids[0].price : null;
  }

  /**
   * Get best ask price
   */
  bestAsk() {
    return this.asks.length > 0 ? this.asks[0].price : null;
  }

  /**
   * Get spread (returns object with absolute and percent)
   * @returns {{absolute: number, percent: number}|null}
   */
  spread() {
    const bid = this.bestBid();
    const ask = this.bestAsk();
    if (bid !== null && ask !== null) {
      return calculateSpread(bid, ask);
    }
    return null;
  }

  /**
   * Get absolute spread value
   * @returns {number|null}
   */
  spreadAbsolute() {
    const spread = this.spread();
    return spread ? spread.absolute : null;
  }

  /**
   * Get mid price
   */
  midPrice() {
    const bid = this.bestBid();
    const ask = this.bestAsk();
    if (bid !== null && ask !== null) {
      return (bid + ask) / 2;
    }
    return null;
  }

  /**
   * Set price grouping
   */
  setGrouping(tickSize) {
    this.grouping = tickSize;
    // Re-apply grouping would require re-fetching data
    // For now, just notify listeners
    this._notifyListeners();
  }

  /**
   * Add listener for order book changes
   */
  addListener(callback) {
    this.listeners.push(callback);
  }

  /**
   * Remove listener
   */
  removeListener(callback) {
    const index = this.listeners.indexOf(callback);
    if (index >= 0) {
      this.listeners.splice(index, 1);
    }
  }

  /**
   * Notify all listeners
   */
  _notifyListeners() {
    for (const listener of this.listeners) {
      try {
        listener(this);
      } catch (e) {
        console.error('Order book listener error:', e);
      }
    }
  }

  /**
   * Clear animation flags after render
   */
  clearAnimationFlags() {
    for (const level of this.bids) {
      level.isNew = false;
      level.isUpdated = false;
    }
    for (const level of this.asks) {
      level.isNew = false;
      level.isUpdated = false;
    }
  }

  /**
   * Clear order book
   */
  clear() {
    this.bids = [];
    this.asks = [];
    this.sequence = 0;
    this._notifyListeners();
  }
}

/**
 * Render a single price level row
 * @param {PriceLevel} level - Price level to render
 * @param {number} priceDecimals - Number of decimal places for price
 * @param {number} qtyDecimals - Number of decimal places for quantity
 * @returns {string} HTML string
 */
export function renderPriceLevel(level, priceDecimals = 2, qtyDecimals = 4) {
  const priceClass = level.side === 'bid' ? 'bid-price' : 'ask-price';
  const rowClass = level.isNew ? 'level-new' : (level.isUpdated ? 'level-updated' : '');
  const depthStyle = `width: ${level.depthPercent.toFixed(1)}%`;

  return `
    <div class="orderbook-row ${rowClass}" data-price="${level.price}">
      <div class="depth-bar ${level.side}-depth" style="${depthStyle}"></div>
      <span class="level-price ${priceClass}">${formatPrice(level.price, priceDecimals)}</span>
      <span class="level-qty">${formatQuantity(level.qty, qtyDecimals)}</span>
      <span class="level-total">${formatQuantity(level.total, qtyDecimals)}</span>
    </div>
  `;
}

/**
 * Render the order book header
 * @param {OrderBook} orderBook - Order book instance
 * @returns {string} HTML string
 */
export function renderOrderBookHeader(orderBook) {
  const spread = orderBook.spread();
  const midPrice = orderBook.midPrice();
  const spreadValue = spread !== null ? spread.absolute.toFixed(4) : '-';

  return `
    <div class="orderbook-header">
      <div class="orderbook-symbol">${escapeHtml(orderBook.symbol || '-')}</div>
      <div class="orderbook-stats">
        <span class="stat-item">
          <span class="stat-label">Spread</span>
          <span class="stat-value">${spreadValue}</span>
        </span>
        <span class="stat-item">
          <span class="stat-label">Mid</span>
          <span class="stat-value">${midPrice !== null ? formatPrice(midPrice, 2) : '-'}</span>
        </span>
      </div>
    </div>
  `;
}

/**
 * Render the full order book
 * @param {OrderBook} orderBook - Order book instance
 * @param {Object} options - Render options
 * @returns {string} HTML string
 */
export function renderOrderBook(orderBook, options = {}) {
  const { priceDecimals = 2, qtyDecimals = 4, showHeader = true } = options;

  const headerHtml = showHeader ? renderOrderBookHeader(orderBook) : '';

  // Render asks in reverse order (lowest ask at bottom, closest to spread)
  const asksHtml = orderBook.asks
    .slice()
    .reverse()
    .map(level => renderPriceLevel(level, priceDecimals, qtyDecimals))
    .join('');

  // Render bids (highest bid at top, closest to spread)
  const bidsHtml = orderBook.bids
    .map(level => renderPriceLevel(level, priceDecimals, qtyDecimals))
    .join('');

  // Spread row
  const spread = orderBook.spread();
  const spreadValue = spread !== null ? spread.absolute.toFixed(4) : '-';
  const spreadHtml = `
    <div class="orderbook-spread">
      <span class="spread-label">Spread</span>
      <span class="spread-value">${spreadValue}</span>
    </div>
  `;

  return `
    ${headerHtml}
    <div class="orderbook-container">
      <div class="orderbook-column-headers">
        <span>Price</span>
        <span>Size</span>
        <span>Total</span>
      </div>
      <div class="orderbook-asks">
        ${asksHtml || '<div class="orderbook-empty">No asks</div>'}
      </div>
      ${spreadHtml}
      <div class="orderbook-bids">
        ${bidsHtml || '<div class="orderbook-empty">No bids</div>'}
      </div>
    </div>
  `;
}

/**
 * Subscribe to order book updates via SSE
 * @param {string} apiBase - API base URL
 * @param {OrderBook} orderBook - Order book instance to update
 * @param {Object} options - Subscription options
 * @returns {EventSource} SSE connection
 */
export function subscribeOrderBookUpdates(apiBase, orderBook, options = {}) {
  const { symbol = '', onError = null, onConnect = null } = options;

  // Build URL with optional symbol filter
  let url = `${apiBase}/api/stream?types=depth`;
  if (symbol) {
    url += `&symbol=${encodeURIComponent(symbol)}`;
  }

  const es = new EventSource(url);

  es.onopen = () => {
    if (onConnect) {
      onConnect();
    }
  };

  // Handle explicit depth events
  es.addEventListener('depth', (event) => {
    try {
      const data = JSON.parse(event.data);
      orderBook.applyUpdate(data);
    } catch (e) {
      console.error('Failed to parse depth update:', e);
    }
  });

  // Handle book_delta events (alternative event name)
  es.addEventListener('book_delta', (event) => {
    try {
      const data = JSON.parse(event.data);
      orderBook.applyUpdate(data);
    } catch (e) {
      console.error('Failed to parse book_delta update:', e);
    }
  });

  // Handle book_snapshot events
  es.addEventListener('book_snapshot', (event) => {
    try {
      const data = JSON.parse(event.data);
      data.is_snapshot = true;
      orderBook.applyUpdate(data);
    } catch (e) {
      console.error('Failed to parse book_snapshot:', e);
    }
  });

  // Fallback: Handle generic message events
  es.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (data.type === 'depth' || data.type === 'book_delta' || data.type === 'book_snapshot') {
        if (data.type === 'book_snapshot') {
          data.is_snapshot = true;
        }
        orderBook.applyUpdate(data);
      }
    } catch (e) {
      // Not JSON or not a depth event, ignore
    }
  };

  es.onerror = (event) => {
    console.error('Order book SSE error:', event);
    if (onError) {
      onError(event);
    }
  };

  return es;
}

/**
 * Fetch order book snapshot via REST API
 * @param {string} apiBase - API base URL
 * @param {string} symbol - Trading symbol
 * @param {number} limit - Number of levels to fetch
 * @returns {Promise<Object>} Depth data
 */
export async function fetchOrderBookSnapshot(apiBase, symbol, limit = 20) {
  const url = `${apiBase}/api/depth?symbol=${encodeURIComponent(symbol)}&limit=${limit}`;
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch order book: HTTP ${res.status}`);
  }
  const data = await res.json();
  data.is_snapshot = true;
  return data;
}

// Export to window for non-module script access
if (typeof window !== 'undefined') {
  window.VeloZOrderBook = {
    OrderBookConfig,
    PriceLevel,
    OrderBook,
    renderPriceLevel,
    renderOrderBookHeader,
    renderOrderBook,
    subscribeOrderBookUpdates,
    fetchOrderBookSnapshot,
  };
}
