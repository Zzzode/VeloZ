/**
 * VeloZ Backtest Result Visualization Module
 *
 * Displays backtest results with:
 * - Results list with key metrics
 * - Equity curve chart
 * - Trade details table
 * - Performance metrics display
 * - Parameter comparison for optimization
 */

import { escapeHtml, formatPrice, formatPnL, formatPercent } from './utils.js';

/**
 * Backtest result configuration
 */
export const BacktestConfig = {
  // Chart dimensions
  chartHeight: 300,
  chartWidth: 800,
  // Table page size
  tradesPerPage: 20,
  // Decimal places
  priceDecimals: 2,
  pctDecimals: 2,
  ratioDecimals: 3,
};

/**
 * Trade record from backtest
 */
export class TradeRecord {
  constructor(data = {}) {
    this.timestamp = data.timestamp || 0;
    this.symbol = data.symbol || '';
    this.side = data.side || '';
    this.price = parseFloat(data.price) || 0;
    this.quantity = parseFloat(data.quantity) || 0;
    this.fee = parseFloat(data.fee) || 0;
    this.pnl = parseFloat(data.pnl) || 0;
    this.strategyId = data.strategy_id || data.strategyId || '';
  }

  get isWin() {
    return this.pnl > 0;
  }

  get isBuy() {
    return this.side.toLowerCase() === 'buy';
  }
}

/**
 * Equity curve point
 */
export class EquityPoint {
  constructor(data = {}) {
    this.timestamp = data.timestamp || 0;
    this.equity = parseFloat(data.equity) || 0;
    this.cumulativeReturn = parseFloat(data.cumulative_return || data.cumulativeReturn) || 0;
  }
}

/**
 * Drawdown point
 */
export class DrawdownPoint {
  constructor(data = {}) {
    this.timestamp = data.timestamp || 0;
    this.drawdown = parseFloat(data.drawdown) || 0;
  }
}

/**
 * Backtest result data
 */
export class BacktestResult {
  constructor(data = {}) {
    this.id = data.id || `bt-${Date.now()}`;
    this.strategyName = data.strategy_name || data.strategyName || '';
    this.symbol = data.symbol || '';
    this.startTime = data.start_time || data.startTime || 0;
    this.endTime = data.end_time || data.endTime || 0;
    this.initialBalance = parseFloat(data.initial_balance || data.initialBalance) || 0;
    this.finalBalance = parseFloat(data.final_balance || data.finalBalance) || 0;
    this.totalReturn = parseFloat(data.total_return || data.totalReturn) || 0;
    this.maxDrawdown = parseFloat(data.max_drawdown || data.maxDrawdown) || 0;
    this.sharpeRatio = parseFloat(data.sharpe_ratio || data.sharpeRatio) || 0;
    this.winRate = parseFloat(data.win_rate || data.winRate) || 0;
    this.profitFactor = parseFloat(data.profit_factor || data.profitFactor) || 0;
    this.tradeCount = parseInt(data.trade_count || data.tradeCount) || 0;
    this.winCount = parseInt(data.win_count || data.winCount) || 0;
    this.loseCount = parseInt(data.lose_count || data.loseCount) || 0;
    this.avgWin = parseFloat(data.avg_win || data.avgWin) || 0;
    this.avgLose = parseFloat(data.avg_lose || data.avgLose) || 0;

    // Parse trades
    this.trades = (data.trades || []).map(t => new TradeRecord(t));

    // Parse equity curve
    this.equityCurve = (data.equity_curve || data.equityCurve || []).map(p => new EquityPoint(p));

    // Parse drawdown curve
    this.drawdownCurve = (data.drawdown_curve || data.drawdownCurve || []).map(p => new DrawdownPoint(p));

    // Parameters used
    this.parameters = data.parameters || data.strategy_parameters || {};

    // Extended metrics (optional)
    this.sortinoRatio = parseFloat(data.sortino_ratio || data.sortinoRatio) || 0;
    this.calmarRatio = parseFloat(data.calmar_ratio || data.calmarRatio) || 0;
    this.var95 = parseFloat(data.value_at_risk_95 || data.var95) || 0;
  }

  /**
   * Get profit/loss amount
   */
  get profitLoss() {
    return this.finalBalance - this.initialBalance;
  }

  /**
   * Get return percentage
   */
  get returnPct() {
    if (this.initialBalance === 0) return 0;
    return (this.profitLoss / this.initialBalance) * 100;
  }

  /**
   * Get average trade PnL
   */
  get avgTradePnl() {
    if (this.tradeCount === 0) return 0;
    return this.profitLoss / this.tradeCount;
  }

  /**
   * Get duration in days
   */
  get durationDays() {
    if (this.startTime === 0 || this.endTime === 0) return 0;
    return (this.endTime - this.startTime) / (1000 * 60 * 60 * 24);
  }
}

/**
 * Ranked optimization result
 */
export class RankedResult {
  constructor(data = {}) {
    this.rank = parseInt(data.rank) || 0;
    this.fitness = parseFloat(data.fitness) || 0;
    this.parameters = data.parameters || {};
    this.result = new BacktestResult(data.result || {});
  }
}

/**
 * Backtest results manager
 */
export class BacktestManager {
  constructor(config = {}) {
    this.config = { ...BacktestConfig, ...config };
    this.results = new Map(); // id -> BacktestResult
    this.selectedId = null;
    this.listeners = [];
    this.comparisonIds = new Set(); // IDs selected for comparison
  }

  /**
   * Add a backtest result
   * @param {Object} data - Result data
   * @returns {BacktestResult}
   */
  addResult(data) {
    const result = new BacktestResult(data);
    this.results.set(result.id, result);
    this._notifyListeners();
    return result;
  }

  /**
   * Get result by ID
   * @param {string} id
   * @returns {BacktestResult|null}
   */
  getResult(id) {
    return this.results.get(id) || null;
  }

  /**
   * Get all results as array
   * @returns {BacktestResult[]}
   */
  getAllResults() {
    return Array.from(this.results.values());
  }

  /**
   * Get results sorted by metric
   * @param {string} metric - Metric to sort by
   * @param {boolean} descending - Sort descending
   * @returns {BacktestResult[]}
   */
  getSortedResults(metric = 'totalReturn', descending = true) {
    const results = this.getAllResults();
    results.sort((a, b) => {
      const aVal = a[metric] || 0;
      const bVal = b[metric] || 0;
      return descending ? bVal - aVal : aVal - bVal;
    });
    return results;
  }

  /**
   * Select a result for detailed view
   * @param {string} id
   */
  selectResult(id) {
    this.selectedId = id;
    this._notifyListeners();
  }

  /**
   * Get selected result
   * @returns {BacktestResult|null}
   */
  getSelectedResult() {
    return this.selectedId ? this.getResult(this.selectedId) : null;
  }

  /**
   * Toggle result for comparison
   * @param {string} id
   */
  toggleComparison(id) {
    if (this.comparisonIds.has(id)) {
      this.comparisonIds.delete(id);
    } else {
      this.comparisonIds.add(id);
    }
    this._notifyListeners();
  }

  /**
   * Get results selected for comparison
   * @returns {BacktestResult[]}
   */
  getComparisonResults() {
    return Array.from(this.comparisonIds)
      .map(id => this.getResult(id))
      .filter(r => r !== null);
  }

  /**
   * Clear all results
   */
  clear() {
    this.results.clear();
    this.selectedId = null;
    this.comparisonIds.clear();
    this._notifyListeners();
  }

  /**
   * Delete a result
   * @param {string} id
   */
  deleteResult(id) {
    this.results.delete(id);
    if (this.selectedId === id) {
      this.selectedId = null;
    }
    this.comparisonIds.delete(id);
    this._notifyListeners();
  }

  /**
   * Add listener for changes
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
        console.error('Backtest listener error:', e);
      }
    }
  }
}

/**
 * Render backtest result summary card
 * @param {BacktestResult} result
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderResultCard(result, options = {}) {
  const { selected = false, comparing = false } = options;

  const returnClass = result.totalReturn >= 0 ? 'positive' : 'negative';
  const selectedClass = selected ? 'selected' : '';
  const comparingClass = comparing ? 'comparing' : '';

  const startDate = result.startTime ? new Date(result.startTime).toLocaleDateString() : '-';
  const endDate = result.endTime ? new Date(result.endTime).toLocaleDateString() : '-';

  return `
    <div class="backtest-card ${selectedClass} ${comparingClass}" data-id="${escapeHtml(result.id)}">
      <div class="backtest-card-header">
        <div class="backtest-card-title">
          <span class="strategy-name">${escapeHtml(result.strategyName)}</span>
          <span class="symbol-badge">${escapeHtml(result.symbol)}</span>
        </div>
        <div class="backtest-card-actions">
          <button class="btn-compare" title="Compare">
            <span class="compare-icon">${comparing ? '✓' : '+'}</span>
          </button>
        </div>
      </div>
      <div class="backtest-card-metrics">
        <div class="metric">
          <span class="metric-label">Return</span>
          <span class="metric-value ${returnClass}">${result.totalReturn >= 0 ? '+' : ''}${result.totalReturn.toFixed(2)}%</span>
        </div>
        <div class="metric">
          <span class="metric-label">Sharpe</span>
          <span class="metric-value">${result.sharpeRatio.toFixed(2)}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Max DD</span>
          <span class="metric-value negative">${result.maxDrawdown.toFixed(2)}%</span>
        </div>
        <div class="metric">
          <span class="metric-label">Win Rate</span>
          <span class="metric-value">${result.winRate.toFixed(1)}%</span>
        </div>
      </div>
      <div class="backtest-card-footer">
        <span class="date-range">${startDate} - ${endDate}</span>
        <span class="trade-count">${result.tradeCount} trades</span>
      </div>
    </div>
  `;
}

/**
 * Render results list
 * @param {BacktestManager} manager
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderResultsList(manager, options = {}) {
  const { sortBy = 'totalReturn', descending = true } = options;
  const results = manager.getSortedResults(sortBy, descending);

  if (results.length === 0) {
    return `
      <div class="backtest-empty">
        <p>No backtest results</p>
        <p class="muted">Run a backtest to see results here</p>
      </div>
    `;
  }

  const cards = results.map(result => renderResultCard(result, {
    selected: result.id === manager.selectedId,
    comparing: manager.comparisonIds.has(result.id),
  })).join('');

  return `<div class="backtest-results-list">${cards}</div>`;
}

/**
 * Render detailed metrics panel
 * @param {BacktestResult} result
 * @returns {string} HTML string
 */
export function renderMetricsPanel(result) {
  if (!result) {
    return '<div class="metrics-panel-empty">Select a backtest to view details</div>';
  }

  const returnClass = result.totalReturn >= 0 ? 'positive' : 'negative';
  const pnlFormatted = formatPnL(result.profitLoss, 2);

  return `
    <div class="metrics-panel">
      <div class="metrics-section">
        <h3>Performance</h3>
        <div class="metrics-grid">
          <div class="metric-item">
            <span class="metric-label">Total Return</span>
            <span class="metric-value ${returnClass}">${result.totalReturn >= 0 ? '+' : ''}${result.totalReturn.toFixed(2)}%</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Profit/Loss</span>
            <span class="metric-value ${pnlFormatted.className}">${pnlFormatted.value}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Initial Balance</span>
            <span class="metric-value">${formatPrice(result.initialBalance, 2)}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Final Balance</span>
            <span class="metric-value">${formatPrice(result.finalBalance, 2)}</span>
          </div>
        </div>
      </div>

      <div class="metrics-section">
        <h3>Risk Metrics</h3>
        <div class="metrics-grid">
          <div class="metric-item">
            <span class="metric-label">Sharpe Ratio</span>
            <span class="metric-value">${result.sharpeRatio.toFixed(3)}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Max Drawdown</span>
            <span class="metric-value negative">${result.maxDrawdown.toFixed(2)}%</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Profit Factor</span>
            <span class="metric-value">${result.profitFactor.toFixed(2)}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Sortino Ratio</span>
            <span class="metric-value">${result.sortinoRatio.toFixed(3)}</span>
          </div>
        </div>
      </div>

      <div class="metrics-section">
        <h3>Trade Statistics</h3>
        <div class="metrics-grid">
          <div class="metric-item">
            <span class="metric-label">Total Trades</span>
            <span class="metric-value">${result.tradeCount}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Win Rate</span>
            <span class="metric-value">${result.winRate.toFixed(1)}%</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Wins / Losses</span>
            <span class="metric-value">${result.winCount} / ${result.loseCount}</span>
          </div>
          <div class="metric-item">
            <span class="metric-label">Avg Win / Loss</span>
            <span class="metric-value">${formatPrice(result.avgWin, 2)} / ${formatPrice(result.avgLose, 2)}</span>
          </div>
        </div>
      </div>
    </div>
  `;
}

/**
 * Render trades table
 * @param {BacktestResult} result
 * @param {Object} options
 * @returns {string} HTML string
 */
export function renderTradesTable(result, options = {}) {
  const { page = 0, pageSize = 20 } = options;

  if (!result || result.trades.length === 0) {
    return '<div class="trades-empty">No trades to display</div>';
  }

  const start = page * pageSize;
  const end = Math.min(start + pageSize, result.trades.length);
  const trades = result.trades.slice(start, end);
  const totalPages = Math.ceil(result.trades.length / pageSize);

  const rows = trades.map((trade, idx) => {
    const pnlClass = trade.pnl >= 0 ? 'positive' : 'negative';
    const sideClass = trade.isBuy ? 'side-buy' : 'side-sell';
    const date = new Date(trade.timestamp).toLocaleString();

    return `
      <tr class="trade-row">
        <td>${start + idx + 1}</td>
        <td>${date}</td>
        <td class="${sideClass}">${trade.side.toUpperCase()}</td>
        <td>${escapeHtml(trade.symbol)}</td>
        <td class="mono">${trade.price.toFixed(2)}</td>
        <td class="mono">${trade.quantity.toFixed(4)}</td>
        <td class="mono">${trade.fee.toFixed(4)}</td>
        <td class="mono ${pnlClass}">${trade.pnl >= 0 ? '+' : ''}${trade.pnl.toFixed(2)}</td>
      </tr>
    `;
  }).join('');

  return `
    <div class="trades-table-container">
      <table class="trades-table">
        <thead>
          <tr>
            <th>#</th>
            <th>Time</th>
            <th>Side</th>
            <th>Symbol</th>
            <th>Price</th>
            <th>Qty</th>
            <th>Fee</th>
            <th>PnL</th>
          </tr>
        </thead>
        <tbody>${rows}</tbody>
      </table>
      <div class="trades-pagination">
        <span>Page ${page + 1} of ${totalPages}</span>
        <span>${result.trades.length} total trades</span>
      </div>
    </div>
  `;
}

/**
 * Render equity curve data for charting
 * @param {BacktestResult} result
 * @returns {Object} Chart data
 */
export function getEquityCurveData(result) {
  if (!result || result.equityCurve.length === 0) {
    return { labels: [], values: [], returns: [] };
  }

  const labels = result.equityCurve.map(p => new Date(p.timestamp).toLocaleDateString());
  const values = result.equityCurve.map(p => p.equity);
  const returns = result.equityCurve.map(p => p.cumulativeReturn);

  return { labels, values, returns };
}

/**
 * Render drawdown curve data for charting
 * @param {BacktestResult} result
 * @returns {Object} Chart data
 */
export function getDrawdownData(result) {
  if (!result || result.drawdownCurve.length === 0) {
    return { labels: [], values: [] };
  }

  const labels = result.drawdownCurve.map(p => new Date(p.timestamp).toLocaleDateString());
  const values = result.drawdownCurve.map(p => p.drawdown);

  return { labels, values };
}

/**
 * Render parameter comparison table
 * @param {BacktestResult[]} results
 * @returns {string} HTML string
 */
export function renderParameterComparison(results) {
  if (!results || results.length === 0) {
    return '<div class="comparison-empty">Select results to compare</div>';
  }

  // Collect all parameter keys
  const allParams = new Set();
  for (const result of results) {
    for (const key of Object.keys(result.parameters)) {
      allParams.add(key);
    }
  }

  const paramKeys = Array.from(allParams).sort();

  // Build comparison table
  const headerCells = results.map(r =>
    `<th>${escapeHtml(r.strategyName)}<br><small>${escapeHtml(r.symbol)}</small></th>`
  ).join('');

  const metricRows = [
    { label: 'Return', key: 'totalReturn', format: v => `${v >= 0 ? '+' : ''}${v.toFixed(2)}%` },
    { label: 'Sharpe', key: 'sharpeRatio', format: v => v.toFixed(3) },
    { label: 'Max DD', key: 'maxDrawdown', format: v => `${v.toFixed(2)}%` },
    { label: 'Win Rate', key: 'winRate', format: v => `${v.toFixed(1)}%` },
    { label: 'Trades', key: 'tradeCount', format: v => v.toString() },
  ].map(metric => {
    const cells = results.map(r => {
      const val = r[metric.key] || 0;
      const cls = metric.key === 'totalReturn' ? (val >= 0 ? 'positive' : 'negative') : '';
      return `<td class="${cls}">${metric.format(val)}</td>`;
    }).join('');
    return `<tr><td class="row-label">${metric.label}</td>${cells}</tr>`;
  }).join('');

  const paramRows = paramKeys.map(key => {
    const cells = results.map(r => {
      const val = r.parameters[key];
      return `<td>${val !== undefined ? val : '-'}</td>`;
    }).join('');
    return `<tr><td class="row-label param-label">${escapeHtml(key)}</td>${cells}</tr>`;
  }).join('');

  return `
    <div class="comparison-table-container">
      <table class="comparison-table">
        <thead>
          <tr>
            <th>Metric</th>
            ${headerCells}
          </tr>
        </thead>
        <tbody>
          ${metricRows}
          <tr class="section-divider"><td colspan="${results.length + 1}">Parameters</td></tr>
          ${paramRows}
        </tbody>
      </table>
    </div>
  `;
}

/**
 * Fetch backtest results from API
 * @param {string} apiBase - API base URL
 * @returns {Promise<Array>}
 */
export async function fetchBacktestResults(apiBase) {
  const url = `${apiBase}/api/backtest/results`;
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch backtest results: HTTP ${res.status}`);
  }
  const data = await res.json();
  return data.results || data || [];
}

/**
 * Fetch single backtest result
 * @param {string} apiBase - API base URL
 * @param {string} id - Result ID
 * @returns {Promise<Object>}
 */
export async function fetchBacktestResult(apiBase, id) {
  const url = `${apiBase}/api/backtest/results/${encodeURIComponent(id)}`;
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch backtest result: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Run a new backtest
 * @param {string} apiBase - API base URL
 * @param {Object} config - Backtest configuration
 * @returns {Promise<Object>}
 */
export async function runBacktest(apiBase, config) {
  const url = `${apiBase}/api/backtest/run`;
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });
  if (!res.ok) {
    throw new Error(`Failed to run backtest: HTTP ${res.status}`);
  }
  return res.json();
}

// Export to window for non-module script access
if (typeof window !== 'undefined') {
  window.VeloZBacktest = {
    BacktestConfig,
    TradeRecord,
    EquityPoint,
    DrawdownPoint,
    BacktestResult,
    RankedResult,
    BacktestManager,
    renderResultCard,
    renderResultsList,
    renderMetricsPanel,
    renderTradesTable,
    getEquityCurveData,
    getDrawdownData,
    renderParameterComparison,
    fetchBacktestResults,
    fetchBacktestResult,
    runBacktest,
  };
}
