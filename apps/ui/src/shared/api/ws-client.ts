/**
 * VeloZ WebSocket Client
 * WebSocket client for real-time market data and order updates
 */

import type {
  WebSocketClientConfig,
  ConnectionState,
  WsSubscribeRequest,
  WsUnsubscribeRequest,
  WsMarketMessage,
  WsOrderMessage,
  TradeData,
  BookTopData,
  WsOrderUpdateMessage,
  WsFillMessage,
} from './types';

// =============================================================================
// WebSocket Configuration
// =============================================================================

const DEFAULT_CONFIG: Required<Omit<WebSocketClientConfig, 'baseUrl'>> = {
  reconnectDelay: 1000,
  maxReconnectDelay: 30000,
  pingInterval: 30000,
};

// =============================================================================
// Market WebSocket Client
// =============================================================================

export interface MarketWsHandlers {
  onTrade?: (data: TradeData) => void;
  onBookTop?: (data: BookTopData) => void;
}

export interface MarketWsEvents {
  onConnect?: () => void;
  onDisconnect?: () => void;
  onReconnecting?: (attempt: number, delay: number) => void;
  onError?: (error: Event) => void;
  onStateChange?: (state: ConnectionState) => void;
}

export class VelozMarketWsClient {
  private readonly config: Required<WebSocketClientConfig>;
  private ws: WebSocket | null = null;
  private reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
  private pingInterval: ReturnType<typeof setInterval> | null = null;
  private reconnectAttempts = 0;
  private state: ConnectionState = 'disconnected';
  private handlers: MarketWsHandlers = {};
  private clientEvents: MarketWsEvents = {};
  private isManualClose = false;
  private subscriptions: Map<string, Set<string>> = new Map(); // symbol -> channels

  constructor(config: WebSocketClientConfig) {
    this.config = { ...DEFAULT_CONFIG, ...config } as Required<WebSocketClientConfig>;
  }

  // ===========================================================================
  // State Management
  // ===========================================================================

  getState(): ConnectionState {
    return this.state;
  }

  private setState(state: ConnectionState): void {
    if (this.state !== state) {
      this.state = state;
      this.clientEvents.onStateChange?.(state);
    }
  }

  // ===========================================================================
  // Event Registration
  // ===========================================================================

  setHandlers(handlers: MarketWsHandlers): void {
    this.handlers = handlers;
  }

  setClientEvents(events: MarketWsEvents): void {
    this.clientEvents = events;
  }

  // ===========================================================================
  // Connection Management
  // ===========================================================================

  connect(): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return;
    }

    this.isManualClose = false;
    this.doConnect();
  }

  disconnect(): void {
    this.isManualClose = true;
    this.cleanup();
    this.setState('disconnected');
  }

  reconnect(): void {
    this.cleanup();
    this.reconnectAttempts = 0;
    this.isManualClose = false;
    this.doConnect();
  }

  private doConnect(): void {
    this.setState(this.reconnectAttempts > 0 ? 'reconnecting' : 'connecting');

    const url = `${this.config.baseUrl}/ws/market`;

    try {
      this.ws = new WebSocket(url);
      this.setupEventHandlers();
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
      this.scheduleReconnect();
    }
  }

  private setupEventHandlers(): void {
    if (!this.ws) return;

    this.ws.onopen = () => {
      this.setState('connected');
      this.reconnectAttempts = 0;
      this.startPing();
      this.resubscribe();
      this.clientEvents.onConnect?.();
    };

    this.ws.onclose = () => {
      this.stopPing();
      this.setState('disconnected');
      this.clientEvents.onDisconnect?.();

      if (!this.isManualClose) {
        this.scheduleReconnect();
      }
    };

    this.ws.onerror = (event) => {
      this.clientEvents.onError?.(event);
    };

    this.ws.onmessage = (event) => {
      this.handleMessage(event.data);
    };
  }

  private handleMessage(data: string): void {
    try {
      const message = JSON.parse(data) as WsMarketMessage;

      switch (message.type) {
        case 'trade':
          this.handlers.onTrade?.(message);
          break;
        case 'book_top':
          this.handlers.onBookTop?.(message);
          break;
      }
    } catch (error) {
      console.error('Failed to parse WebSocket message:', error);
    }
  }

  // ===========================================================================
  // Subscription Management
  // ===========================================================================

  subscribe(symbols: string[], channels: ('trade' | 'book_top' | 'kline')[]): void {
    // Track subscriptions
    for (const symbol of symbols) {
      if (!this.subscriptions.has(symbol)) {
        this.subscriptions.set(symbol, new Set());
      }
      for (const channel of channels) {
        this.subscriptions.get(symbol)!.add(channel);
      }
    }

    // Send subscription if connected
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      const request: WsSubscribeRequest = {
        action: 'subscribe',
        symbols,
        channels,
      };
      this.ws.send(JSON.stringify(request));
    }
  }

  unsubscribe(symbols: string[], channels: ('trade' | 'book_top' | 'kline')[]): void {
    // Update tracked subscriptions
    for (const symbol of symbols) {
      const symbolChannels = this.subscriptions.get(symbol);
      if (symbolChannels) {
        for (const channel of channels) {
          symbolChannels.delete(channel);
        }
        if (symbolChannels.size === 0) {
          this.subscriptions.delete(symbol);
        }
      }
    }

    // Send unsubscription if connected
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      const request: WsUnsubscribeRequest = {
        action: 'unsubscribe',
        symbols,
        channels,
      };
      this.ws.send(JSON.stringify(request));
    }
  }

  private resubscribe(): void {
    // Resubscribe to all tracked subscriptions after reconnect
    const symbolChannels = new Map<string, string[]>();

    for (const [symbol, channels] of this.subscriptions) {
      symbolChannels.set(symbol, Array.from(channels));
    }

    if (symbolChannels.size > 0 && this.ws && this.ws.readyState === WebSocket.OPEN) {
      // Group by channels for efficient subscription
      const channelSymbols = new Map<string, string[]>();

      for (const [symbol, channels] of symbolChannels) {
        for (const channel of channels) {
          if (!channelSymbols.has(channel)) {
            channelSymbols.set(channel, []);
          }
          channelSymbols.get(channel)!.push(symbol);
        }
      }

      // Send subscription requests
      for (const [channel, symbols] of channelSymbols) {
        const request: WsSubscribeRequest = {
          action: 'subscribe',
          symbols,
          channels: [channel as 'trade' | 'book_top' | 'kline'],
        };
        this.ws.send(JSON.stringify(request));
      }
    }
  }

  // ===========================================================================
  // Ping/Pong
  // ===========================================================================

  private startPing(): void {
    this.stopPing();
    this.pingInterval = setInterval(() => {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        // Send ping frame (browser WebSocket API doesn't expose ping/pong,
        // but the server may handle text "ping" messages)
        try {
          this.ws.send(JSON.stringify({ type: 'ping' }));
        } catch {
          // Ignore send errors
        }
      }
    }, this.config.pingInterval);
  }

  private stopPing(): void {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
  }

  // ===========================================================================
  // Reconnection Logic
  // ===========================================================================

  private scheduleReconnect(): void {
    if (this.reconnectTimeout || this.isManualClose) {
      return;
    }

    const delay = Math.min(
      this.config.reconnectDelay * Math.pow(1.5, this.reconnectAttempts),
      this.config.maxReconnectDelay
    );

    this.clientEvents.onReconnecting?.(this.reconnectAttempts + 1, delay);

    this.reconnectTimeout = setTimeout(() => {
      this.reconnectTimeout = null;
      this.reconnectAttempts++;
      this.doConnect();
    }, delay);
  }

  private cleanup(): void {
    this.stopPing();

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}

// =============================================================================
// Order WebSocket Client
// =============================================================================

export interface OrderWsHandlers {
  onOrderUpdate?: (data: WsOrderUpdateMessage) => void;
  onFill?: (data: WsFillMessage) => void;
}

export interface OrderWsEvents {
  onConnect?: () => void;
  onDisconnect?: () => void;
  onReconnecting?: (attempt: number, delay: number) => void;
  onError?: (error: Event) => void;
  onStateChange?: (state: ConnectionState) => void;
}

export class VelozOrderWsClient {
  private readonly config: Required<WebSocketClientConfig>;
  private ws: WebSocket | null = null;
  private reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
  private pingInterval: ReturnType<typeof setInterval> | null = null;
  private reconnectAttempts = 0;
  private state: ConnectionState = 'disconnected';
  private handlers: OrderWsHandlers = {};
  private clientEvents: OrderWsEvents = {};
  private isManualClose = false;

  constructor(config: WebSocketClientConfig) {
    this.config = { ...DEFAULT_CONFIG, ...config } as Required<WebSocketClientConfig>;
  }

  // ===========================================================================
  // State Management
  // ===========================================================================

  getState(): ConnectionState {
    return this.state;
  }

  private setState(state: ConnectionState): void {
    if (this.state !== state) {
      this.state = state;
      this.clientEvents.onStateChange?.(state);
    }
  }

  // ===========================================================================
  // Event Registration
  // ===========================================================================

  setHandlers(handlers: OrderWsHandlers): void {
    this.handlers = handlers;
  }

  setClientEvents(events: OrderWsEvents): void {
    this.clientEvents = events;
  }

  // ===========================================================================
  // Connection Management
  // ===========================================================================

  connect(): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return;
    }

    this.isManualClose = false;
    this.doConnect();
  }

  disconnect(): void {
    this.isManualClose = true;
    this.cleanup();
    this.setState('disconnected');
  }

  reconnect(): void {
    this.cleanup();
    this.reconnectAttempts = 0;
    this.isManualClose = false;
    this.doConnect();
  }

  private doConnect(): void {
    this.setState(this.reconnectAttempts > 0 ? 'reconnecting' : 'connecting');

    const url = `${this.config.baseUrl}/ws/orders`;

    try {
      this.ws = new WebSocket(url);
      this.setupEventHandlers();
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
      this.scheduleReconnect();
    }
  }

  private setupEventHandlers(): void {
    if (!this.ws) return;

    this.ws.onopen = () => {
      this.setState('connected');
      this.reconnectAttempts = 0;
      this.startPing();
      this.clientEvents.onConnect?.();
    };

    this.ws.onclose = () => {
      this.stopPing();
      this.setState('disconnected');
      this.clientEvents.onDisconnect?.();

      if (!this.isManualClose) {
        this.scheduleReconnect();
      }
    };

    this.ws.onerror = (event) => {
      this.clientEvents.onError?.(event);
    };

    this.ws.onmessage = (event) => {
      this.handleMessage(event.data);
    };
  }

  private handleMessage(data: string): void {
    try {
      const message = JSON.parse(data) as WsOrderMessage;

      switch (message.type) {
        case 'order_update':
          this.handlers.onOrderUpdate?.(message);
          break;
        case 'fill':
          this.handlers.onFill?.(message);
          break;
      }
    } catch (error) {
      console.error('Failed to parse WebSocket message:', error);
    }
  }

  // ===========================================================================
  // Ping/Pong
  // ===========================================================================

  private startPing(): void {
    this.stopPing();
    this.pingInterval = setInterval(() => {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        try {
          this.ws.send(JSON.stringify({ type: 'ping' }));
        } catch {
          // Ignore send errors
        }
      }
    }, this.config.pingInterval);
  }

  private stopPing(): void {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
  }

  // ===========================================================================
  // Reconnection Logic
  // ===========================================================================

  private scheduleReconnect(): void {
    if (this.reconnectTimeout || this.isManualClose) {
      return;
    }

    const delay = Math.min(
      this.config.reconnectDelay * Math.pow(1.5, this.reconnectAttempts),
      this.config.maxReconnectDelay
    );

    this.clientEvents.onReconnecting?.(this.reconnectAttempts + 1, delay);

    this.reconnectTimeout = setTimeout(() => {
      this.reconnectTimeout = null;
      this.reconnectAttempts++;
      this.doConnect();
    }, delay);
  }

  private cleanup(): void {
    this.stopPing();

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}

// =============================================================================
// Factory Functions
// =============================================================================

export function createMarketWsClient(
  baseUrl: string,
  options: Partial<Omit<WebSocketClientConfig, 'baseUrl'>> = {}
): VelozMarketWsClient {
  // Convert http(s) to ws(s)
  const wsUrl = baseUrl.replace(/^http/, 'ws').replace(/\/+$/, '');
  return new VelozMarketWsClient({
    baseUrl: wsUrl,
    ...options,
  });
}

export function createOrderWsClient(
  baseUrl: string,
  options: Partial<Omit<WebSocketClientConfig, 'baseUrl'>> = {}
): VelozOrderWsClient {
  // Convert http(s) to ws(s)
  const wsUrl = baseUrl.replace(/^http/, 'ws').replace(/\/+$/, '');
  return new VelozOrderWsClient({
    baseUrl: wsUrl,
    ...options,
  });
}
