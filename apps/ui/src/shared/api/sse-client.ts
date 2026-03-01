/**
 * VeloZ SSE Client
 * Server-Sent Events client with automatic reconnection and event handling
 */

import type {
  SSEClientConfig,
  SSEEvent,
  SSEEventHandlers,
  ConnectionState,
  MarketSSEEvent,
  OrderUpdateSSEEvent,
  FillSSEEvent,
  OrderStateSSEEvent,
  AccountSSEEvent,
  ErrorSSEEvent,
} from './types';

// =============================================================================
// SSE Event Types
// =============================================================================

export type SSEEventType = 'market' | 'order_update' | 'fill' | 'order_state' | 'account' | 'error';

// =============================================================================
// SSE Client Configuration
// =============================================================================

const DEFAULT_CONFIG: Required<Omit<SSEClientConfig, 'baseUrl'>> = {
  reconnectDelay: 1000,
  maxReconnectDelay: 30000,
  backoffMultiplier: 1.5,
  lastEventId: '',
};

// =============================================================================
// SSE Client
// =============================================================================

export interface SSEClientEvents {
  onConnect?: () => void;
  onDisconnect?: () => void;
  onReconnecting?: (attempt: number, delay: number) => void;
  onError?: (error: Event) => void;
  onStateChange?: (state: ConnectionState) => void;
}

export class VelozSSEClient {
  private readonly config: Required<SSEClientConfig>;
  private eventSource: EventSource | null = null;
  private reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
  private reconnectAttempts = 0;
  private lastEventId: string = '';
  private state: ConnectionState = 'disconnected';
  private handlers: SSEEventHandlers = {};
  private clientEvents: SSEClientEvents = {};
  private isManualClose = false;

  constructor(config: SSEClientConfig) {
    this.config = { ...DEFAULT_CONFIG, ...config } as Required<SSEClientConfig>;
    if (config.lastEventId) {
      this.lastEventId = config.lastEventId;
    }
  }

  // ===========================================================================
  // State Management
  // ===========================================================================

  getState(): ConnectionState {
    return this.state;
  }

  getLastEventId(): string {
    return this.lastEventId;
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

  /**
   * Set event handlers for SSE events
   */
  setHandlers(handlers: SSEEventHandlers): void {
    this.handlers = handlers;
  }

  /**
   * Set handler for a specific event type
   */
  on<T extends SSEEventType>(
    type: T,
    handler: T extends 'market'
      ? (event: MarketSSEEvent) => void
      : T extends 'order_update'
        ? (event: OrderUpdateSSEEvent) => void
        : T extends 'fill'
          ? (event: FillSSEEvent) => void
          : T extends 'order_state'
            ? (event: OrderStateSSEEvent) => void
            : T extends 'account'
              ? (event: AccountSSEEvent) => void
              : T extends 'error'
                ? (event: ErrorSSEEvent) => void
                : never
  ): void {
    (this.handlers as Record<string, unknown>)[type] = handler;
  }

  /**
   * Remove handler for a specific event type
   */
  off(type: SSEEventType): void {
    delete (this.handlers as Record<string, unknown>)[type];
  }

  /**
   * Set client lifecycle event handlers
   */
  setClientEvents(events: SSEClientEvents): void {
    this.clientEvents = events;
  }

  // ===========================================================================
  // Connection Management
  // ===========================================================================

  /**
   * Connect to SSE stream
   */
  connect(): void {
    if (this.eventSource) {
      return; // Already connected
    }

    this.isManualClose = false;
    this.doConnect();
  }

  /**
   * Disconnect from SSE stream
   */
  disconnect(): void {
    this.isManualClose = true;
    this.cleanup();
    this.setState('disconnected');
  }

  /**
   * Reconnect to SSE stream (manual reconnection)
   */
  reconnect(): void {
    this.cleanup();
    this.reconnectAttempts = 0;
    this.isManualClose = false;
    this.doConnect();
  }

  private doConnect(): void {
    this.setState(this.reconnectAttempts > 0 ? 'reconnecting' : 'connecting');

    // Build URL with last_id for resumption
    let url = `${this.config.baseUrl}/api/stream`;
    if (this.lastEventId) {
      url += `?last_id=${encodeURIComponent(this.lastEventId)}`;
    }

    try {
      this.eventSource = new EventSource(url);
      this.setupEventHandlers();
    } catch (error) {
      console.error('Failed to create EventSource:', error);
      this.scheduleReconnect();
    }
  }

  private setupEventHandlers(): void {
    if (!this.eventSource) return;

    this.eventSource.onopen = () => {
      this.setState('connected');
      this.reconnectAttempts = 0;
      this.clientEvents.onConnect?.();
    };

    this.eventSource.onerror = (event) => {
      this.clientEvents.onError?.(event);

      if (this.eventSource?.readyState === EventSource.CLOSED) {
        this.setState('disconnected');
        this.clientEvents.onDisconnect?.();

        if (!this.isManualClose) {
          this.scheduleReconnect();
        }
      }
    };

    // Register typed event handlers
    this.registerEventHandler('market');
    this.registerEventHandler('order_update');
    this.registerEventHandler('fill');
    this.registerEventHandler('order_state');
    this.registerEventHandler('account');
    this.registerEventHandler('error');

    // Fallback: Handle generic message events for servers that don't set event type
    this.eventSource.onmessage = (event) => {
      this.handleGenericMessage(event);
    };
  }

  private registerEventHandler(type: SSEEventType): void {
    if (!this.eventSource) return;

    this.eventSource.addEventListener(type, (event: MessageEvent) => {
      this.handleEvent(type, event);
    });
  }

  private handleEvent(type: SSEEventType, event: MessageEvent): void {
    // Update last event ID
    if (event.lastEventId) {
      this.lastEventId = event.lastEventId;
    }

    try {
      const data = JSON.parse(event.data) as SSEEvent;
      this.dispatchEvent(type, data);
    } catch (error) {
      console.error(`Failed to parse ${type} event:`, error);
    }
  }

  private handleGenericMessage(event: MessageEvent): void {
    // Update last event ID
    if (event.lastEventId) {
      this.lastEventId = event.lastEventId;
    }

    try {
      const data = JSON.parse(event.data) as SSEEvent;
      if (data.type && this.handlers[data.type as SSEEventType]) {
        this.dispatchEvent(data.type as SSEEventType, data);
      }
    } catch {
      // Not JSON or not a recognized event, ignore
    }
  }

  private dispatchEvent(type: SSEEventType, data: SSEEvent): void {
    const handler = this.handlers[type];
    if (handler) {
      try {
        // Type-safe dispatch
        switch (type) {
          case 'market':
            (handler as (e: MarketSSEEvent) => void)(data as MarketSSEEvent);
            break;
          case 'order_update':
            (handler as (e: OrderUpdateSSEEvent) => void)(data as OrderUpdateSSEEvent);
            break;
          case 'fill':
            (handler as (e: FillSSEEvent) => void)(data as FillSSEEvent);
            break;
          case 'order_state':
            (handler as (e: OrderStateSSEEvent) => void)(data as OrderStateSSEEvent);
            break;
          case 'account':
            (handler as (e: AccountSSEEvent) => void)(data as AccountSSEEvent);
            break;
          case 'error':
            (handler as (e: ErrorSSEEvent) => void)(data as ErrorSSEEvent);
            break;
        }
      } catch (error) {
        console.error(`Error in ${type} handler:`, error);
      }
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
      this.config.reconnectDelay * Math.pow(this.config.backoffMultiplier, this.reconnectAttempts),
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
    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
      this.reconnectTimeout = null;
    }

    if (this.eventSource) {
      this.eventSource.close();
      this.eventSource = null;
    }
  }
}

// =============================================================================
// Factory Function
// =============================================================================

export function createSSEClient(
  baseUrl: string,
  options: Partial<Omit<SSEClientConfig, 'baseUrl'>> = {}
): VelozSSEClient {
  return new VelozSSEClient({
    baseUrl: baseUrl.replace(/\/+$/, ''),
    ...options,
  });
}

// =============================================================================
// Convenience Hook-style API
// =============================================================================

export interface UseSSEOptions {
  baseUrl: string;
  handlers?: SSEEventHandlers;
  autoConnect?: boolean;
  lastEventId?: string;
  onStateChange?: (state: ConnectionState) => void;
}

/**
 * Create and manage an SSE connection with a simpler API
 */
export function useSSE(options: UseSSEOptions): {
  client: VelozSSEClient;
  connect: () => void;
  disconnect: () => void;
  getState: () => ConnectionState;
  getLastEventId: () => string;
} {
  const client = createSSEClient(options.baseUrl, {
    lastEventId: options.lastEventId,
  });

  if (options.handlers) {
    client.setHandlers(options.handlers);
  }

  if (options.onStateChange) {
    client.setClientEvents({
      onStateChange: options.onStateChange,
    });
  }

  if (options.autoConnect !== false) {
    client.connect();
  }

  return {
    client,
    connect: () => client.connect(),
    disconnect: () => client.disconnect(),
    getState: () => client.getState(),
    getLastEventId: () => client.getLastEventId(),
  };
}
