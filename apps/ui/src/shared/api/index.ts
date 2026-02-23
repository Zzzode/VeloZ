/**
 * VeloZ API Client Library
 * Main entry point for TypeScript API integration
 */

// Re-export all types
export * from './types';

// Re-export API client
export {
  VelozApiClient,
  VelozApiError,
  AuthenticationError,
  RateLimitError,
  NetworkError,
  MemoryTokenStorage,
  LocalStorageTokenStorage,
  createApiClient,
  type RequestInterceptor,
  type ResponseInterceptor,
  type ErrorInterceptor,
  type TokenStorage,
} from './api-client';

// Re-export SSE client
export {
  VelozSSEClient,
  createSSEClient,
  useSSE,
  type SSEEventType,
  type SSEClientEvents,
  type UseSSEOptions,
} from './sse-client';

// Re-export WebSocket clients
export {
  VelozMarketWsClient,
  VelozOrderWsClient,
  createMarketWsClient,
  createOrderWsClient,
  type MarketWsHandlers,
  type MarketWsEvents,
  type OrderWsHandlers,
  type OrderWsEvents,
} from './ws-client';

// Re-export TanStack Query hooks
export * from './hooks';

// =============================================================================
// Unified Client Factory
// =============================================================================

import { VelozApiClient, createApiClient, LocalStorageTokenStorage } from './api-client';
import type { TokenStorage } from './api-client';
import { VelozSSEClient, createSSEClient } from './sse-client';
import { VelozMarketWsClient, VelozOrderWsClient, createMarketWsClient, createOrderWsClient } from './ws-client';
import type { ApiClientConfig, SSEEventHandlers, ConnectionState } from './types';

export interface VelozClientOptions {
  /** Base URL for all connections */
  baseUrl: string;
  /** API client configuration */
  api?: Partial<Omit<ApiClientConfig, 'baseUrl'>>;
  /** Token storage implementation */
  tokenStorage?: TokenStorage;
  /** Auto-connect SSE on creation */
  autoConnectSSE?: boolean;
  /** Auto-connect WebSocket on creation */
  autoConnectWS?: boolean;
  /** SSE event handlers */
  sseHandlers?: SSEEventHandlers;
  /** Connection state change callback */
  onConnectionStateChange?: (type: 'sse' | 'market_ws' | 'order_ws', state: ConnectionState) => void;
}

/**
 * Unified VeloZ client that manages API, SSE, and WebSocket connections
 */
export class VelozClient {
  public readonly api: VelozApiClient;
  public readonly sse: VelozSSEClient;
  public readonly marketWs: VelozMarketWsClient;
  public readonly orderWs: VelozOrderWsClient;

  private readonly baseUrl: string;

  constructor(options: VelozClientOptions) {
    this.baseUrl = options.baseUrl.replace(/\/+$/, '');

    // Create API client
    const tokenStorage = options.tokenStorage ?? new LocalStorageTokenStorage();
    this.api = createApiClient(this.baseUrl, {
      ...options.api,
    });
    this.api.setTokenStorage(tokenStorage);

    // Create SSE client
    this.sse = createSSEClient(this.baseUrl);
    if (options.sseHandlers) {
      this.sse.setHandlers(options.sseHandlers);
    }
    if (options.onConnectionStateChange) {
      this.sse.setClientEvents({
        onStateChange: (state) => options.onConnectionStateChange!('sse', state),
      });
    }

    // Create WebSocket clients
    this.marketWs = createMarketWsClient(this.baseUrl);
    this.orderWs = createOrderWsClient(this.baseUrl);

    if (options.onConnectionStateChange) {
      this.marketWs.setClientEvents({
        onStateChange: (state) => options.onConnectionStateChange!('market_ws', state),
      });
      this.orderWs.setClientEvents({
        onStateChange: (state) => options.onConnectionStateChange!('order_ws', state),
      });
    }

    // Auto-connect if requested
    if (options.autoConnectSSE) {
      this.sse.connect();
    }
    if (options.autoConnectWS) {
      this.marketWs.connect();
      this.orderWs.connect();
    }
  }

  /**
   * Connect all real-time channels
   */
  connectAll(): void {
    this.sse.connect();
    this.marketWs.connect();
    this.orderWs.connect();
  }

  /**
   * Disconnect all real-time channels
   */
  disconnectAll(): void {
    this.sse.disconnect();
    this.marketWs.disconnect();
    this.orderWs.disconnect();
  }

  /**
   * Get connection states for all channels
   */
  getConnectionStates(): {
    sse: ConnectionState;
    marketWs: ConnectionState;
    orderWs: ConnectionState;
  } {
    return {
      sse: this.sse.getState(),
      marketWs: this.marketWs.getState(),
      orderWs: this.orderWs.getState(),
    };
  }
}

/**
 * Create a unified VeloZ client
 */
export function createVelozClient(options: VelozClientOptions): VelozClient {
  return new VelozClient(options);
}

// =============================================================================
// Response Caching
// =============================================================================

export interface CacheEntry<T> {
  data: T;
  timestamp: number;
  expiresAt: number;
}

export interface CacheOptions {
  /** Time-to-live in milliseconds */
  ttl: number;
  /** Maximum number of entries */
  maxEntries?: number;
}

/**
 * Simple in-memory cache for API responses
 */
export class ResponseCache {
  private cache = new Map<string, CacheEntry<unknown>>();
  private readonly defaultTtl: number;
  private readonly maxEntries: number;

  constructor(options: CacheOptions) {
    this.defaultTtl = options.ttl;
    this.maxEntries = options.maxEntries ?? 100;
  }

  /**
   * Get cached value if not expired
   */
  get<T>(key: string): T | null {
    const entry = this.cache.get(key);
    if (!entry) {
      return null;
    }

    if (Date.now() > entry.expiresAt) {
      this.cache.delete(key);
      return null;
    }

    return entry.data as T;
  }

  /**
   * Set cached value
   */
  set<T>(key: string, data: T, ttl?: number): void {
    // Evict oldest entries if at capacity
    if (this.cache.size >= this.maxEntries) {
      const oldestKey = this.cache.keys().next().value;
      if (oldestKey) {
        this.cache.delete(oldestKey);
      }
    }

    const now = Date.now();
    this.cache.set(key, {
      data,
      timestamp: now,
      expiresAt: now + (ttl ?? this.defaultTtl),
    });
  }

  /**
   * Invalidate cached value
   */
  invalidate(key: string): void {
    this.cache.delete(key);
  }

  /**
   * Invalidate all entries matching a pattern
   */
  invalidatePattern(pattern: RegExp): void {
    for (const key of this.cache.keys()) {
      if (pattern.test(key)) {
        this.cache.delete(key);
      }
    }
  }

  /**
   * Clear all cached values
   */
  clear(): void {
    this.cache.clear();
  }

  /**
   * Get cache statistics
   */
  getStats(): { size: number; maxEntries: number } {
    return {
      size: this.cache.size,
      maxEntries: this.maxEntries,
    };
  }
}

/**
 * Create a cached API client wrapper
 */
export function withCache(
  client: VelozApiClient,
  cache: ResponseCache
): VelozApiClient & { cache: ResponseCache } {
  // Create a proxy that intercepts GET requests and caches responses
  const proxy = new Proxy(client, {
    get(target, prop, receiver) {
      const value = Reflect.get(target, prop, receiver);

      // Only cache specific GET methods
      const cacheMethods = [
        'getConfig',
        'getMarketData',
        'listOrders',
        'listOrderStates',
        'getAccount',
        'listPositions',
        'getEngineStatus',
        'listStrategies',
      ];

      if (typeof value === 'function' && cacheMethods.includes(prop as string)) {
        return async function (...args: unknown[]) {
          const cacheKey = `${String(prop)}:${JSON.stringify(args)}`;
          const cached = cache.get(cacheKey);

          if (cached !== null) {
            return cached;
          }

          const result = await value.apply(target, args);
          cache.set(cacheKey, result);
          return result;
        };
      }

      return value;
    },
  }) as VelozApiClient & { cache: ResponseCache };

  // Attach cache for direct access
  (proxy as unknown as { cache: ResponseCache }).cache = cache;

  return proxy;
}
