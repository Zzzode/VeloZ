/**
 * VeloZ Strategy API Integration
 *
 * This module provides API integration for the Strategy Configuration UI.
 * It can be loaded alongside the existing index.html to enable real API calls
 * when the backend is available, with fallback to mock data.
 *
 * Usage: Include this script after the main index.html script section
 * <script type="module" src="js/strategy-api.js"></script>
 */

// Configuration
const CONFIG = {
  // Set to true to use real API, false for mock data
  useApi: false,
  // API endpoints
  endpoints: {
    strategies: '/api/strategies',
    strategy: (id) => `/api/strategies/${id}`,
    params: (id) => `/api/strategies/${id}/params`,
    start: (id) => `/api/strategies/${id}/start`,
    pause: (id) => `/api/strategies/${id}/pause`,
    stop: (id) => `/api/strategies/${id}/stop`,
    stream: '/api/stream?types=strategy_update',
  },
  // SSE reconnection settings
  sse: {
    reconnectDelayMs: 1000,
    maxReconnectDelayMs: 30000,
    reconnectBackoffMultiplier: 1.5,
  },
};

/**
 * SSE Connection states
 */
const ConnectionState = {
  DISCONNECTED: 'disconnected',
  CONNECTING: 'connecting',
  CONNECTED: 'connected',
  RECONNECTING: 'reconnecting',
};

/**
 * Strategy API Client
 * Handles REST API calls and SSE subscriptions for strategy management
 */
class StrategyApiClient {
  constructor(apiBase) {
    this.apiBase = apiBase || '';
    this.sseConnection = null;
    this.onStrategyUpdate = null;
    this.onConnectionStateChange = null;
    this.connectionState = ConnectionState.DISCONNECTED;
    this.reconnectAttempts = 0;
    this.reconnectTimeout = null;
  }

  /**
   * Get current connection state
   */
  getConnectionState() {
    return this.connectionState;
  }

  /**
   * Set connection state and notify listeners
   */
  _setConnectionState(state) {
    if (this.connectionState !== state) {
      this.connectionState = state;
      if (this.onConnectionStateChange) {
        try {
          this.onConnectionStateChange(state);
        } catch (e) {
          console.error('Connection state change callback error:', e);
        }
      }
    }
  }

  /**
   * Fetch all strategies
   */
  async fetchStrategies() {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.strategies}`, {
      cache: 'no-store',
    });
    if (!res.ok) {
      throw new Error(`Failed to fetch strategies: HTTP ${res.status}`);
    }
    const data = await res.json();
    return data.strategies || [];
  }

  /**
   * Fetch single strategy
   */
  async fetchStrategy(id) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.strategy(id)}`, {
      cache: 'no-store',
    });
    if (!res.ok) {
      throw new Error(`Failed to fetch strategy: HTTP ${res.status}`);
    }
    return res.json();
  }

  /**
   * Create new strategy
   */
  async createStrategy(config) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.strategies}`, {
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
   */
  async updateParams(id, params) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.params(id)}`, {
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
   * Start strategy
   */
  async startStrategy(id) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.start(id)}`, {
      method: 'POST',
    });
    if (!res.ok) {
      const error = await res.json().catch(() => ({}));
      throw new Error(error.error || `Failed to start strategy: HTTP ${res.status}`);
    }
    return res.json();
  }

  /**
   * Pause strategy
   */
  async pauseStrategy(id) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.pause(id)}`, {
      method: 'POST',
    });
    if (!res.ok) {
      const error = await res.json().catch(() => ({}));
      throw new Error(error.error || `Failed to pause strategy: HTTP ${res.status}`);
    }
    return res.json();
  }

  /**
   * Stop strategy
   */
  async stopStrategy(id) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.stop(id)}`, {
      method: 'POST',
    });
    if (!res.ok) {
      const error = await res.json().catch(() => ({}));
      throw new Error(error.error || `Failed to stop strategy: HTTP ${res.status}`);
    }
    return res.json();
  }

  /**
   * Delete strategy
   */
  async deleteStrategy(id) {
    const res = await fetch(`${this.apiBase}${CONFIG.endpoints.strategy(id)}`, {
      method: 'DELETE',
    });
    if (!res.ok) {
      const error = await res.json().catch(() => ({}));
      throw new Error(error.error || `Failed to delete strategy: HTTP ${res.status}`);
    }
    return res.json();
  }

  /**
   * Subscribe to strategy updates via SSE
   * Supports both explicit event types and fallback to parsing type from JSON
   * @param {Function} callback - Callback for strategy updates
   * @param {Function} onStateChange - Optional callback for connection state changes
   */
  subscribeUpdates(callback, onStateChange) {
    if (this.sseConnection) {
      this.sseConnection.close();
      this.sseConnection = null;
    }

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    this.onStrategyUpdate = callback;
    if (onStateChange) {
      this.onConnectionStateChange = onStateChange;
    }

    this._connectSSE();
    return this.sseConnection;
  }

  /**
   * Internal method to establish SSE connection
   */
  _connectSSE() {
    const url = `${this.apiBase}${CONFIG.endpoints.stream}`;
    this._setConnectionState(
      this.reconnectAttempts > 0 ? ConnectionState.RECONNECTING : ConnectionState.CONNECTING
    );

    try {
      this.sseConnection = new EventSource(url);

      this.sseConnection.onopen = () => {
        this._setConnectionState(ConnectionState.CONNECTED);
        this.reconnectAttempts = 0;
        console.log('SSE connection established');
      };

      // Handle explicit strategy_update events (preferred)
      this.sseConnection.addEventListener('strategy_update', (event) => {
        this._handleStrategyEvent(event.data);
      });

      // Fallback: Handle generic message events for servers that don't set event type
      // Parse the type from JSON payload
      this.sseConnection.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          // Only handle if it looks like a strategy update
          if (data.type === 'strategy_update' || data.strategy_id) {
            this._handleStrategyEvent(event.data);
          }
        } catch (e) {
          // Not JSON or not a strategy event, ignore
        }
      };

      this.sseConnection.onerror = (event) => {
        console.error('SSE connection error:', event);
        this._setConnectionState(ConnectionState.DISCONNECTED);

        // Only reconnect if connection was previously established or connecting
        if (this.sseConnection && this.sseConnection.readyState === EventSource.CLOSED) {
          this._scheduleReconnect();
        }
      };

    } catch (e) {
      console.error('Failed to create SSE connection:', e);
      this._setConnectionState(ConnectionState.DISCONNECTED);
      this._scheduleReconnect();
    }
  }

  /**
   * Handle strategy event data
   */
  _handleStrategyEvent(dataStr) {
    try {
      const data = JSON.parse(dataStr);
      if (this.onStrategyUpdate) {
        this.onStrategyUpdate(data);
      }
    } catch (e) {
      console.error('Failed to parse strategy update:', e);
    }
  }

  /**
   * Schedule reconnection with exponential backoff
   */
  _scheduleReconnect() {
    if (this.reconnectTimeout) {
      return; // Already scheduled
    }

    const delay = Math.min(
      CONFIG.sse.reconnectDelayMs * Math.pow(CONFIG.sse.reconnectBackoffMultiplier, this.reconnectAttempts),
      CONFIG.sse.maxReconnectDelayMs
    );

    console.log(`Scheduling SSE reconnect in ${delay}ms (attempt ${this.reconnectAttempts + 1})`);

    this.reconnectTimeout = setTimeout(() => {
      this.reconnectTimeout = null;
      this.reconnectAttempts++;
      this._connectSSE();
    }, delay);
  }

  /**
   * Close SSE connection and cleanup
   */
  unsubscribe() {
    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    if (this.sseConnection) {
      this.sseConnection.close();
      this.sseConnection = null;
    }

    this.reconnectAttempts = 0;
    this._setConnectionState(ConnectionState.DISCONNECTED);
  }
}

/**
 * Parameter validation utilities
 */
const ParamValidator = {
  /**
   * Validate a single parameter value
   */
  validate(value, schema) {
    const { type, min, max, options } = schema;

    if (value === null || value === undefined || value === '') {
      return { valid: false, error: 'Value is required' };
    }

    switch (type) {
      case 'int': {
        const num = parseInt(value, 10);
        if (isNaN(num)) return { valid: false, error: 'Must be an integer' };
        if (min !== undefined && num < min) return { valid: false, error: `Min: ${min}` };
        if (max !== undefined && num > max) return { valid: false, error: `Max: ${max}` };
        return { valid: true, error: null };
      }

      case 'float': {
        const num = parseFloat(value);
        if (isNaN(num)) return { valid: false, error: 'Must be a number' };
        if (min !== undefined && num < min) return { valid: false, error: `Min: ${min}` };
        if (max !== undefined && num > max) return { valid: false, error: `Max: ${max}` };
        return { valid: true, error: null };
      }

      case 'bool':
        if (typeof value !== 'boolean' && value !== 'true' && value !== 'false') {
          return { valid: false, error: 'Must be true/false' };
        }
        return { valid: true, error: null };

      case 'enum':
        if (options && !options.includes(value)) {
          return { valid: false, error: `Must be: ${options.join(', ')}` };
        }
        return { valid: true, error: null };

      default:
        return { valid: true, error: null };
    }
  },

  /**
   * Validate all parameters
   */
  validateAll(params, schema) {
    const errors = {};
    let valid = true;

    for (const [key, paramSchema] of Object.entries(schema)) {
      const result = this.validate(params[key], paramSchema);
      if (!result.valid) {
        valid = false;
        errors[key] = result.error;
      }
    }

    return { valid, errors };
  },

  /**
   * Convert value to appropriate type
   */
  convert(value, type) {
    switch (type) {
      case 'int': return parseInt(value, 10);
      case 'float': return parseFloat(value);
      case 'bool': return value === true || value === 'true';
      default: return value;
    }
  },
};

/**
 * Strategy Manager - Integrates API with UI
 */
class StrategyManager {
  constructor(apiBase) {
    this.client = new StrategyApiClient(apiBase);
    this.strategies = [];
    this.useApi = CONFIG.useApi;
    this.listeners = [];
    this.connectionListeners = [];
  }

  /**
   * Get current connection state
   */
  getConnectionState() {
    return this.client.getConnectionState();
  }

  /**
   * Add listener for connection state changes
   */
  addConnectionListener(callback) {
    this.connectionListeners.push(callback);
  }

  /**
   * Notify connection listeners
   */
  _notifyConnectionListeners(state) {
    for (const listener of this.connectionListeners) {
      try {
        listener(state);
      } catch (e) {
        console.error('Connection listener error:', e);
      }
    }
  }

  /**
   * Initialize - load strategies and setup SSE
   */
  async init() {
    if (this.useApi) {
      try {
        await this.loadStrategies();
        this.client.subscribeUpdates(
          (data) => this.handleUpdate(data),
          (state) => this._notifyConnectionListeners(state)
        );
        console.log('Strategy API connected');
      } catch (e) {
        console.warn('API unavailable, using mock data:', e.message);
        this.useApi = false;
      }
    }
    return this;
  }

  /**
   * Load strategies from API
   */
  async loadStrategies() {
    if (!this.useApi) return;
    this.strategies = await this.client.fetchStrategies();
    this.notifyListeners();
  }

  /**
   * Handle SSE update
   */
  handleUpdate(data) {
    const idx = this.strategies.findIndex(s => s.id === data.id);
    if (idx >= 0) {
      this.strategies[idx] = { ...this.strategies[idx], ...data };
    } else {
      this.strategies.push(data);
    }
    this.notifyListeners();
  }

  /**
   * Add listener for strategy changes
   */
  addListener(callback) {
    this.listeners.push(callback);
  }

  /**
   * Notify all listeners
   */
  notifyListeners() {
    for (const listener of this.listeners) {
      try {
        listener(this.strategies);
      } catch (e) {
        console.error('Listener error:', e);
      }
    }
  }

  /**
   * Start a strategy
   */
  async start(id) {
    if (this.useApi) {
      await this.client.startStrategy(id);
    }
  }

  /**
   * Pause a strategy
   */
  async pause(id) {
    if (this.useApi) {
      await this.client.pauseStrategy(id);
    }
  }

  /**
   * Stop a strategy
   */
  async stop(id) {
    if (this.useApi) {
      await this.client.stopStrategy(id);
    }
  }

  /**
   * Update strategy parameters
   */
  async updateParams(id, params) {
    if (this.useApi) {
      await this.client.updateParams(id, params);
    }
  }

  /**
   * Create a new strategy
   */
  async create(config) {
    if (this.useApi) {
      return await this.client.createStrategy(config);
    }
    return null;
  }

  /**
   * Delete a strategy
   */
  async delete(id) {
    if (this.useApi) {
      await this.client.deleteStrategy(id);
    }
  }

  /**
   * Cleanup
   */
  destroy() {
    this.client.unsubscribe();
    this.listeners = [];
    this.connectionListeners = [];
  }
}

// Export for use
window.VeloZStrategy = {
  CONFIG,
  ConnectionState,
  StrategyApiClient,
  ParamValidator,
  StrategyManager,
};

// Auto-initialize if apiBase is available
document.addEventListener('DOMContentLoaded', () => {
  // Check if we should enable API mode
  const urlParams = new URLSearchParams(window.location.search);
  if (urlParams.get('api_mode') === 'true') {
    CONFIG.useApi = true;
    console.log('Strategy API mode enabled via URL parameter');
  }
});

console.log('VeloZ Strategy API module loaded');
