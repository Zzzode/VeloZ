/**
 * VeloZ Strategy Management API Client
 * Handles all strategy-related API calls and data management
 */

import { escapeHtml, formatParamName, formatUptime, formatPnL, formatPercent } from './utils.js';

/**
 * Strategy status enum
 */
export const StrategyStatus = {
  STOPPED: 'stopped',
  RUNNING: 'running',
  PAUSED: 'paused',
  ERROR: 'error',
};

/**
 * Parameter types supported by strategies
 */
export const ParamType = {
  INT: 'int',
  FLOAT: 'float',
  STRING: 'string',
  BOOL: 'bool',
  ENUM: 'enum',
};

/**
 * Fetch all strategies from the API
 * @param {string} apiBase - API base URL
 * @returns {Promise<Array>} List of strategies
 */
export async function fetchStrategies(apiBase) {
  const res = await fetch(`${apiBase}/api/strategies`, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch strategies: HTTP ${res.status}`);
  }
  const data = await res.json();
  return data.strategies || [];
}

/**
 * Fetch a single strategy by ID
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @returns {Promise<Object>} Strategy details
 */
export async function fetchStrategy(apiBase, strategyId) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}`, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`Failed to fetch strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Create a new strategy
 * @param {string} apiBase - API base URL
 * @param {Object} config - Strategy configuration
 * @returns {Promise<Object>} Created strategy
 */
export async function createStrategy(apiBase, config) {
  const res = await fetch(`${apiBase}/api/strategies`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to create strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Update strategy parameters
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @param {Object} params - New parameter values
 * @returns {Promise<Object>} Updated strategy
 */
export async function updateStrategyParams(apiBase, strategyId, params) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}/params`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ params }),
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to update params: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Start a strategy
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @returns {Promise<Object>} Result
 */
export async function startStrategy(apiBase, strategyId) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}/start`, {
    method: 'POST',
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to start strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Pause a strategy
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @returns {Promise<Object>} Result
 */
export async function pauseStrategy(apiBase, strategyId) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}/pause`, {
    method: 'POST',
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to pause strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Stop a strategy
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @returns {Promise<Object>} Result
 */
export async function stopStrategy(apiBase, strategyId) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}/stop`, {
    method: 'POST',
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to stop strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Delete a strategy
 * @param {string} apiBase - API base URL
 * @param {string} strategyId - Strategy ID
 * @returns {Promise<Object>} Result
 */
export async function deleteStrategy(apiBase, strategyId) {
  const res = await fetch(`${apiBase}/api/strategies/${strategyId}`, {
    method: 'DELETE',
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({}));
    throw new Error(error.error || `Failed to delete strategy: HTTP ${res.status}`);
  }
  return res.json();
}

/**
 * Validate a parameter value against its schema
 * @param {*} value - Parameter value
 * @param {Object} schema - Parameter schema
 * @returns {{valid: boolean, error: string|null}} Validation result
 */
export function validateParam(value, schema) {
  const { type, min, max, options } = schema;

  if (value === null || value === undefined || value === '') {
    return { valid: false, error: 'Value is required' };
  }

  switch (type) {
    case ParamType.INT: {
      const num = parseInt(value, 10);
      if (isNaN(num)) {
        return { valid: false, error: 'Must be an integer' };
      }
      if (min !== undefined && num < min) {
        return { valid: false, error: `Must be at least ${min}` };
      }
      if (max !== undefined && num > max) {
        return { valid: false, error: `Must be at most ${max}` };
      }
      return { valid: true, error: null };
    }

    case ParamType.FLOAT: {
      const num = parseFloat(value);
      if (isNaN(num)) {
        return { valid: false, error: 'Must be a number' };
      }
      if (min !== undefined && num < min) {
        return { valid: false, error: `Must be at least ${min}` };
      }
      if (max !== undefined && num > max) {
        return { valid: false, error: `Must be at most ${max}` };
      }
      return { valid: true, error: null };
    }

    case ParamType.STRING: {
      if (typeof value !== 'string') {
        return { valid: false, error: 'Must be a string' };
      }
      if (min !== undefined && value.length < min) {
        return { valid: false, error: `Must be at least ${min} characters` };
      }
      if (max !== undefined && value.length > max) {
        return { valid: false, error: `Must be at most ${max} characters` };
      }
      return { valid: true, error: null };
    }

    case ParamType.BOOL: {
      if (typeof value !== 'boolean' && value !== 'true' && value !== 'false') {
        return { valid: false, error: 'Must be true or false' };
      }
      return { valid: true, error: null };
    }

    case ParamType.ENUM: {
      if (!options || !Array.isArray(options)) {
        return { valid: true, error: null }; // No options defined
      }
      if (!options.includes(value)) {
        return { valid: false, error: `Must be one of: ${options.join(', ')}` };
      }
      return { valid: true, error: null };
    }

    default:
      return { valid: true, error: null };
  }
}

/**
 * Validate all parameters for a strategy
 * @param {Object} params - Parameter values { key: value }
 * @param {Object} schema - Parameter schemas { key: schema }
 * @returns {{valid: boolean, errors: Object}} Validation result
 */
export function validateAllParams(params, schema) {
  const errors = {};
  let valid = true;

  for (const [key, paramSchema] of Object.entries(schema)) {
    const result = validateParam(params[key], paramSchema);
    if (!result.valid) {
      valid = false;
      errors[key] = result.error;
    }
  }

  return { valid, errors };
}

/**
 * Convert parameter value to appropriate type
 * @param {*} value - Raw value (usually string from input)
 * @param {string} type - Parameter type
 * @returns {*} Converted value
 */
export function convertParamValue(value, type) {
  switch (type) {
    case ParamType.INT:
      return parseInt(value, 10);
    case ParamType.FLOAT:
      return parseFloat(value);
    case ParamType.BOOL:
      return value === true || value === 'true';
    default:
      return value;
  }
}

/**
 * Generate HTML for a parameter input field
 * @param {string} key - Parameter key
 * @param {Object} schema - Parameter schema
 * @param {*} currentValue - Current value
 * @param {boolean} disabled - Whether input is disabled
 * @returns {string} HTML string
 */
export function renderParamInput(key, schema, currentValue, disabled = false) {
  const { type, min, max, step, options, description, hot } = schema;
  const inputId = `param-${key}`;
  const disabledAttr = disabled ? 'disabled' : '';
  const hotBadge = hot ? '<span class="hot-badge">HOT</span>' : '';

  let inputHtml = '';

  switch (type) {
    case ParamType.BOOL:
      inputHtml = `
        <select id="${inputId}" class="param-input" ${disabledAttr}>
          <option value="true" ${currentValue === true ? 'selected' : ''}>True</option>
          <option value="false" ${currentValue === false ? 'selected' : ''}>False</option>
        </select>
      `;
      break;

    case ParamType.ENUM:
      inputHtml = `
        <select id="${inputId}" class="param-input" ${disabledAttr}>
          ${(options || []).map(opt =>
            `<option value="${escapeHtml(opt)}" ${currentValue === opt ? 'selected' : ''}>${escapeHtml(opt)}</option>`
          ).join('')}
        </select>
      `;
      break;

    case ParamType.INT:
    case ParamType.FLOAT:
      inputHtml = `
        <input
          type="number"
          id="${inputId}"
          class="param-input"
          value="${currentValue}"
          ${min !== undefined ? `min="${min}"` : ''}
          ${max !== undefined ? `max="${max}"` : ''}
          ${step !== undefined ? `step="${step}"` : (type === ParamType.FLOAT ? 'step="any"' : '')}
          ${disabledAttr}
        />
      `;
      break;

    default: // STRING
      inputHtml = `
        <input
          type="text"
          id="${inputId}"
          class="param-input"
          value="${escapeHtml(currentValue)}"
          ${min !== undefined ? `minlength="${min}"` : ''}
          ${max !== undefined ? `maxlength="${max}"` : ''}
          ${disabledAttr}
        />
      `;
  }

  return `
    <div class="param-group">
      <label class="param-label">
        ${escapeHtml(formatParamName(key))}
        ${hotBadge}
      </label>
      ${inputHtml}
      ${description ? `<div class="param-hint">${escapeHtml(description)}</div>` : ''}
      <div class="param-error" id="${inputId}-error"></div>
    </div>
  `;
}

/**
 * Render a strategy card for the grid view
 * @param {Object} strategy - Strategy object
 * @returns {string} HTML string
 */
export function renderStrategyCard(strategy) {
  const { id, name, type, symbol, status, capital, pnl, pnl_percent, trades, win_rate, max_drawdown, start_time } = strategy;

  const pnlFormatted = formatPnL(pnl || 0);
  const pnlPctFormatted = formatPercent((pnl_percent || 0) / 100);
  const uptime = start_time ? formatUptime(Date.now() - start_time) : '-';

  const statusBadgeClass = `badge-${status || 'stopped'}`;

  let actionButtons = '';
  switch (status) {
    case StrategyStatus.RUNNING:
      actionButtons = `
        <button class="btn-warning btn-sm" data-action="pause" data-id="${id}">Pause</button>
        <button class="btn-danger btn-sm" data-action="stop" data-id="${id}">Stop</button>
      `;
      break;
    case StrategyStatus.PAUSED:
      actionButtons = `
        <button class="btn-success btn-sm" data-action="start" data-id="${id}">Resume</button>
        <button class="btn-danger btn-sm" data-action="stop" data-id="${id}">Stop</button>
      `;
      break;
    default: // STOPPED, ERROR
      actionButtons = `
        <button class="btn-success btn-sm" data-action="start" data-id="${id}">Start</button>
      `;
  }

  return `
    <div class="strategy-card" data-strategy-id="${id}">
      <div class="strategy-card-header">
        <div>
          <h3 class="strategy-name">${escapeHtml(name)}</h3>
          <div class="strategy-type">${escapeHtml(type || 'custom')} | ${escapeHtml(symbol || '-')}</div>
        </div>
        <span class="badge ${statusBadgeClass}">${escapeHtml(status || 'stopped')}</span>
      </div>
      <div class="strategy-metrics">
        <div class="metric-item">
          <span class="metric-label">PnL</span>
          <span class="metric-value ${pnlFormatted.className}">
            ${pnlFormatted.value} (${pnlPctFormatted})
          </span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Trades</span>
          <span class="metric-value">${trades || 0}</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Win Rate</span>
          <span class="metric-value">${(win_rate || 0).toFixed(1)}%</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Max DD</span>
          <span class="metric-value negative">-${(max_drawdown || 0).toFixed(1)}%</span>
        </div>
      </div>
      <div class="strategy-actions">
        ${actionButtons}
        <button class="btn-secondary btn-sm" data-action="edit" data-id="${id}">Edit</button>
        <button class="btn-secondary btn-sm" data-action="delete" data-id="${id}">Delete</button>
      </div>
    </div>
  `;
}

/**
 * Subscribe to strategy updates via SSE
 * @param {string} apiBase - API base URL
 * @param {Function} onUpdate - Callback for strategy updates
 * @param {Function} onError - Callback for errors
 * @returns {EventSource} SSE connection
 */
export function subscribeStrategyUpdates(apiBase, onUpdate, onError) {
  const url = `${apiBase}/api/stream?types=strategy_update`;
  const es = new EventSource(url);

  es.addEventListener('strategy_update', (event) => {
    try {
      const data = JSON.parse(event.data);
      onUpdate(data);
    } catch (e) {
      console.error('Failed to parse strategy update:', e);
    }
  });

  es.onerror = (event) => {
    if (onError) {
      onError(event);
    }
  };

  return es;
}
