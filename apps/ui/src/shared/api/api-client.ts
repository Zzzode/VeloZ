/**
 * VeloZ API Client
 * TypeScript API client with authentication, error handling, and request interceptors
 */

import type {
  ApiClientConfig,
  ApiError,
  RateLimitInfo,
  // Auth
  LoginRequest,
  LoginResponse,
  RefreshTokenRequest,
  RefreshTokenResponse,
  LogoutRequest,
  LogoutResponse,
  ListApiKeysResponse,
  CreateApiKeyRequest,
  CreateApiKeyResponse,
  RevokeApiKeyResponse,
  // Health & Config
  HealthResponse,
  ConfigResponse,
  // Market
  MarketDataResponse,
  // Orders
  ListOrdersResponse,
  ListOrderStatesResponse,
  OrderState,
  PlaceOrderRequest,
  PlaceOrderResponse,
  CancelOrderRequest,
  CancelOrderResponse,
  // Account
  AccountResponse,
  // Positions
  ListPositionsResponse,
  Position,
  // Engine
  EngineStatusResponse,
  EngineControlResponse,
  ListStrategiesResponse,
  StrategyDetail,
  StrategyControlResponse,
  // Execution
  ExecutionPingResponse,
  // Audit
  QueryAuditLogsParams,
  QueryAuditLogsResponse,
  AuditStatsResponse,
  AuditArchiveResponse,
  // Backtest
  RunBacktestRequest,
  RunBacktestResponse,
  ListBacktestsResponse,
  BacktestResult,
} from './types';

// =============================================================================
// Error Classes
// =============================================================================

/** Base API error */
export class VelozApiError extends Error {
  readonly code: string;
  readonly status: number;
  readonly response?: ApiError;

  constructor(
    message: string,
    code: string,
    status: number,
    response?: ApiError
  ) {
    super(message);
    this.name = 'VelozApiError';
    this.code = code;
    this.status = status;
    this.response = response;
  }
}

/** Authentication error */
export class AuthenticationError extends VelozApiError {
  constructor(message: string, code: string, status: number, response?: ApiError) {
    super(message, code, status, response);
    this.name = 'AuthenticationError';
  }
}

/** Rate limit error */
export class RateLimitError extends VelozApiError {
  readonly rateLimitInfo: RateLimitInfo;

  constructor(
    message: string,
    rateLimitInfo: RateLimitInfo
  ) {
    super(message, 'rate_limit_exceeded', 429);
    this.name = 'RateLimitError';
    this.rateLimitInfo = rateLimitInfo;
  }
}

/** Network error */
export class NetworkError extends Error {
  readonly cause?: Error;

  constructor(message: string, cause?: Error) {
    super(message);
    this.name = 'NetworkError';
    this.cause = cause;
  }
}

// =============================================================================
// Request/Response Interceptors
// =============================================================================

export type RequestInterceptor = (
  url: string,
  options: RequestInit
) => Promise<{ url: string; options: RequestInit }> | { url: string; options: RequestInit };

export type ResponseInterceptor = (
  response: Response,
  request: { url: string; options: RequestInit }
) => Promise<Response> | Response;

export type ErrorInterceptor = (
  error: Error,
  request: { url: string; options: RequestInit }
) => Promise<Error> | Error;

// =============================================================================
// Token Manager
// =============================================================================

export interface TokenStorage {
  getAccessToken(): string | null;
  setAccessToken(token: string | null): void;
  getRefreshToken(): string | null;
  setRefreshToken(token: string | null): void;
  clear(): void;
}

/** In-memory token storage */
export class MemoryTokenStorage implements TokenStorage {
  private accessToken: string | null = null;
  private refreshToken: string | null = null;

  getAccessToken(): string | null {
    return this.accessToken;
  }

  setAccessToken(token: string | null): void {
    this.accessToken = token;
  }

  getRefreshToken(): string | null {
    return this.refreshToken;
  }

  setRefreshToken(token: string | null): void {
    this.refreshToken = token;
  }

  clear(): void {
    this.accessToken = null;
    this.refreshToken = null;
  }
}

/** LocalStorage-based token storage */
export class LocalStorageTokenStorage implements TokenStorage {
  private readonly accessTokenKey = 'veloz_access_token';
  private readonly refreshTokenKey = 'veloz_refresh_token';

  getAccessToken(): string | null {
    return localStorage.getItem(this.accessTokenKey);
  }

  setAccessToken(token: string | null): void {
    if (token) {
      localStorage.setItem(this.accessTokenKey, token);
    } else {
      localStorage.removeItem(this.accessTokenKey);
    }
  }

  getRefreshToken(): string | null {
    return localStorage.getItem(this.refreshTokenKey);
  }

  setRefreshToken(token: string | null): void {
    if (token) {
      localStorage.setItem(this.refreshTokenKey, token);
    } else {
      localStorage.removeItem(this.refreshTokenKey);
    }
  }

  clear(): void {
    localStorage.removeItem(this.accessTokenKey);
    localStorage.removeItem(this.refreshTokenKey);
  }
}

// =============================================================================
// API Client
// =============================================================================

const DEFAULT_CONFIG: Partial<ApiClientConfig> = {
  timeout: 30000,
  retry: {
    maxRetries: 3,
    retryDelay: 1000,
    retryOn: [502, 503, 504],
  },
};

export class VelozApiClient {
  private readonly config: Required<ApiClientConfig>;
  private readonly requestInterceptors: RequestInterceptor[] = [];
  private readonly responseInterceptors: ResponseInterceptor[] = [];
  private readonly errorInterceptors: ErrorInterceptor[] = [];
  private tokenStorage: TokenStorage;
  private refreshPromise: Promise<string> | null = null;

  constructor(config: ApiClientConfig, tokenStorage?: TokenStorage) {
    this.config = { ...DEFAULT_CONFIG, ...config } as Required<ApiClientConfig>;
    this.tokenStorage = tokenStorage ?? new MemoryTokenStorage();

    // Initialize with provided tokens
    if (config.accessToken) {
      this.tokenStorage.setAccessToken(config.accessToken);
    }
  }

  // ===========================================================================
  // Interceptor Management
  // ===========================================================================

  addRequestInterceptor(interceptor: RequestInterceptor): () => void {
    this.requestInterceptors.push(interceptor);
    return () => {
      const index = this.requestInterceptors.indexOf(interceptor);
      if (index >= 0) {
        this.requestInterceptors.splice(index, 1);
      }
    };
  }

  addResponseInterceptor(interceptor: ResponseInterceptor): () => void {
    this.responseInterceptors.push(interceptor);
    return () => {
      const index = this.responseInterceptors.indexOf(interceptor);
      if (index >= 0) {
        this.responseInterceptors.splice(index, 1);
      }
    };
  }

  addErrorInterceptor(interceptor: ErrorInterceptor): () => void {
    this.errorInterceptors.push(interceptor);
    return () => {
      const index = this.errorInterceptors.indexOf(interceptor);
      if (index >= 0) {
        this.errorInterceptors.splice(index, 1);
      }
    };
  }

  // ===========================================================================
  // Token Management
  // ===========================================================================

  setTokenStorage(storage: TokenStorage): void {
    this.tokenStorage = storage;
  }

  getAccessToken(): string | null {
    return this.tokenStorage.getAccessToken();
  }

  setAccessToken(token: string | null): void {
    this.tokenStorage.setAccessToken(token);
  }

  getRefreshToken(): string | null {
    return this.tokenStorage.getRefreshToken();
  }

  setRefreshToken(token: string | null): void {
    this.tokenStorage.setRefreshToken(token);
  }

  clearTokens(): void {
    this.tokenStorage.clear();
  }

  // ===========================================================================
  // Core Request Method
  // ===========================================================================

  public async request<T>(
    method: string,
    path: string,
    options: {
      body?: unknown;
      query?: Record<string, string | number | undefined>;
      headers?: Record<string, string>;
      skipAuth?: boolean;
      retryCount?: number;
    } = {}
  ): Promise<T> {
    const { body, query, headers = {}, skipAuth = false, retryCount = 0 } = options;

    // Build URL with query params
    let url = `${this.config.baseUrl}${path}`;
    if (query) {
      const params = new URLSearchParams();
      for (const [key, value] of Object.entries(query)) {
        if (value !== undefined) {
          params.append(key, String(value));
        }
      }
      const queryString = params.toString();
      if (queryString) {
        url += `?${queryString}`;
      }
    }

    // Build request options
    let requestOptions: RequestInit = {
      method,
      headers: {
        ...headers,
      },
      cache: 'no-store',
    };

    // Add body if present
    if (body !== undefined) {
      requestOptions.headers = {
        ...requestOptions.headers,
        'Content-Type': 'application/json',
      };
      requestOptions.body = JSON.stringify(body);
    }

    // Add authentication
    if (!skipAuth) {
      const accessToken = this.tokenStorage.getAccessToken();
      const apiKey = this.config.apiKey;

      if (accessToken) {
        requestOptions.headers = {
          ...requestOptions.headers,
          Authorization: `Bearer ${accessToken}`,
        };
      } else if (apiKey) {
        requestOptions.headers = {
          ...requestOptions.headers,
          'X-API-Key': apiKey,
        };
      }
    }

    // Run request interceptors
    for (const interceptor of this.requestInterceptors) {
      const result = await interceptor(url, requestOptions);
      url = result.url;
      requestOptions = result.options;
    }

    try {
      // Create abort controller for timeout
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), this.config.timeout);
      requestOptions.signal = controller.signal;

      let response: Response;
      try {
        response = await fetch(url, requestOptions);
      } finally {
        clearTimeout(timeoutId);
      }

      // Run response interceptors
      for (const interceptor of this.responseInterceptors) {
        response = await interceptor(response, { url, options: requestOptions });
      }

      // Extract rate limit info
      const rateLimitInfo = this.extractRateLimitInfo(response);

      // Handle rate limiting
      if (response.status === 429) {
        if (this.config.onRateLimited && rateLimitInfo) {
          this.config.onRateLimited(rateLimitInfo);
        }
        throw new RateLimitError('Rate limit exceeded', rateLimitInfo!);
      }

      // Handle authentication errors
      if (response.status === 401) {
        const errorBody = await response.json().catch(() => ({ error: 'authentication_required' }));

        // Try to refresh token if we have a refresh token
        if (
          !skipAuth &&
          this.tokenStorage.getRefreshToken() &&
          errorBody.error !== 'invalid_refresh_token'
        ) {
          try {
            await this.refreshAccessToken();
            // Retry the request with new token
            return this.request<T>(method, path, { ...options, retryCount: retryCount + 1 });
          } catch {
            // Refresh failed, notify and throw
            if (this.config.onTokenExpired) {
              this.config.onTokenExpired();
            }
          }
        }

        throw new AuthenticationError(
          errorBody.message || 'Authentication required',
          errorBody.error || 'authentication_required',
          401,
          errorBody
        );
      }

      // Handle forbidden
      if (response.status === 403) {
        const errorBody = await response.json().catch(() => ({ error: 'forbidden' }));
        throw new VelozApiError(
          errorBody.message || 'Permission denied',
          errorBody.error || 'forbidden',
          403,
          errorBody
        );
      }

      // Handle other errors
      if (!response.ok) {
        const errorBody = await response.json().catch(() => ({ error: 'unknown_error' }));

        // Retry on configured status codes
        if (
          this.config.retry.retryOn.includes(response.status) &&
          retryCount < this.config.retry.maxRetries
        ) {
          await this.delay(this.config.retry.retryDelay * (retryCount + 1));
          return this.request<T>(method, path, { ...options, retryCount: retryCount + 1 });
        }

        throw new VelozApiError(
          errorBody.message || `Request failed with status ${response.status}`,
          errorBody.error || 'request_failed',
          response.status,
          errorBody
        );
      }

      // Parse response
      const contentType = response.headers.get('content-type');
      if (contentType?.includes('application/json')) {
        return (await response.json()) as T;
      }

      return {} as T;
    } catch (error) {
      // Run error interceptors
      let processedError = error as Error;
      for (const interceptor of this.errorInterceptors) {
        processedError = await interceptor(processedError, { url, options: requestOptions });
      }

      // Handle abort/timeout
      if (processedError instanceof DOMException && processedError.name === 'AbortError') {
        throw new NetworkError('Request timeout');
      }

      // Handle network errors
      if (processedError instanceof TypeError) {
        throw new NetworkError('Network error', processedError);
      }

      throw processedError;
    }
  }

  private extractRateLimitInfo(response: Response): RateLimitInfo | null {
    const limit = response.headers.get('X-RateLimit-Limit');
    const remaining = response.headers.get('X-RateLimit-Remaining');
    const reset = response.headers.get('X-RateLimit-Reset');
    const retryAfter = response.headers.get('Retry-After');

    if (limit && remaining && reset) {
      return {
        limit: parseInt(limit, 10),
        remaining: parseInt(remaining, 10),
        reset: parseInt(reset, 10),
        retryAfter: retryAfter ? parseInt(retryAfter, 10) : undefined,
      };
    }

    return null;
  }

  private delay(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  private async refreshAccessToken(): Promise<string> {
    // Deduplicate concurrent refresh requests
    if (this.refreshPromise) {
      return this.refreshPromise;
    }

    const refreshToken = this.tokenStorage.getRefreshToken();
    if (!refreshToken) {
      throw new AuthenticationError('No refresh token available', 'no_refresh_token', 401);
    }

    this.refreshPromise = (async () => {
      try {
        const response = await this.request<RefreshTokenResponse>(
          'POST',
          '/api/auth/refresh',
          {
            body: { refresh_token: refreshToken },
            skipAuth: true,
          }
        );
        this.tokenStorage.setAccessToken(response.access_token);
        return response.access_token;
      } finally {
        this.refreshPromise = null;
      }
    })();

    return this.refreshPromise;
  }

  // ===========================================================================
  // Authentication Endpoints
  // ===========================================================================

  async login(request: LoginRequest): Promise<LoginResponse> {
    const response = await this.request<LoginResponse>('POST', '/api/auth/login', {
      body: request,
      skipAuth: true,
    });
    this.tokenStorage.setAccessToken(response.access_token);
    this.tokenStorage.setRefreshToken(response.refresh_token);
    return response;
  }

  async refreshToken(request: RefreshTokenRequest): Promise<RefreshTokenResponse> {
    const response = await this.request<RefreshTokenResponse>('POST', '/api/auth/refresh', {
      body: request,
      skipAuth: true,
    });
    this.tokenStorage.setAccessToken(response.access_token);
    return response;
  }

  async logout(request: LogoutRequest): Promise<LogoutResponse> {
    const response = await this.request<LogoutResponse>('POST', '/api/auth/logout', {
      body: request,
    });
    this.tokenStorage.clear();
    return response;
  }

  async listApiKeys(): Promise<ListApiKeysResponse> {
    return this.request<ListApiKeysResponse>('GET', '/api/auth/keys');
  }

  async createApiKey(request: CreateApiKeyRequest): Promise<CreateApiKeyResponse> {
    return this.request<CreateApiKeyResponse>('POST', '/api/auth/keys', {
      body: request,
    });
  }

  async revokeApiKey(keyId: string): Promise<RevokeApiKeyResponse> {
    return this.request<RevokeApiKeyResponse>('DELETE', '/api/auth/keys', {
      query: { key_id: keyId },
    });
  }

  // ===========================================================================
  // Health & Config Endpoints
  // ===========================================================================

  async health(): Promise<HealthResponse> {
    return this.request<HealthResponse>('GET', '/api/health', { skipAuth: true });
  }

  async getConfig(): Promise<ConfigResponse> {
    return this.request<ConfigResponse>('GET', '/api/config');
  }

  // ===========================================================================
  // Market Data Endpoints
  // ===========================================================================

  async getMarketData(): Promise<MarketDataResponse> {
    return this.request<MarketDataResponse>('GET', '/api/market');
  }

  // ===========================================================================
  // Order Endpoints
  // ===========================================================================

  async listOrders(): Promise<ListOrdersResponse> {
    return this.request<ListOrdersResponse>('GET', '/api/orders');
  }

  async listOrderStates(): Promise<ListOrderStatesResponse> {
    return this.request<ListOrderStatesResponse>('GET', '/api/orders_state');
  }

  async getOrderState(clientOrderId: string): Promise<OrderState> {
    return this.request<OrderState>('GET', '/api/order_state', {
      query: { client_order_id: clientOrderId },
    });
  }

  async placeOrder(request: PlaceOrderRequest): Promise<PlaceOrderResponse> {
    return this.request<PlaceOrderResponse>('POST', '/api/order', {
      body: request,
    });
  }

  async cancelOrder(request: CancelOrderRequest): Promise<CancelOrderResponse> {
    return this.request<CancelOrderResponse>('POST', '/api/cancel', {
      body: request,
    });
  }

  // ===========================================================================
  // Account Endpoints
  // ===========================================================================

  async getAccount(): Promise<AccountResponse> {
    return this.request<AccountResponse>('GET', '/api/account');
  }

  // ===========================================================================
  // Position Endpoints
  // ===========================================================================

  async listPositions(): Promise<ListPositionsResponse> {
    return this.request<ListPositionsResponse>('GET', '/api/positions');
  }

  async getPosition(symbol: string): Promise<Position> {
    return this.request<Position>('GET', `/api/positions/${encodeURIComponent(symbol)}`);
  }

  // ===========================================================================
  // Engine Service Mode Endpoints
  // ===========================================================================

  async getEngineStatus(): Promise<EngineStatusResponse> {
    return this.request<EngineStatusResponse>('GET', '/api/status');
  }

  async startEngine(): Promise<EngineControlResponse> {
    return this.request<EngineControlResponse>('POST', '/api/start');
  }

  async stopEngine(): Promise<EngineControlResponse> {
    return this.request<EngineControlResponse>('POST', '/api/stop');
  }

  async listStrategies(): Promise<ListStrategiesResponse> {
    return this.request<ListStrategiesResponse>('GET', '/api/strategies');
  }

  async getStrategy(id: string): Promise<StrategyDetail> {
    return this.request<StrategyDetail>('GET', `/api/strategies/${encodeURIComponent(id)}`);
  }

  async startStrategy(id: string): Promise<StrategyControlResponse> {
    return this.request<StrategyControlResponse>(
      'POST',
      `/api/strategies/${encodeURIComponent(id)}/start`
    );
  }

  async stopStrategy(id: string): Promise<StrategyControlResponse> {
    return this.request<StrategyControlResponse>(
      'POST',
      `/api/strategies/${encodeURIComponent(id)}/stop`
    );
  }

  // ===========================================================================
  // Execution Endpoints
  // ===========================================================================

  async pingExecution(): Promise<ExecutionPingResponse> {
    return this.request<ExecutionPingResponse>('GET', '/api/execution/ping');
  }

  // ===========================================================================
  // Audit Log Endpoints
  // ===========================================================================

  async queryAuditLogs(params: QueryAuditLogsParams = {}): Promise<QueryAuditLogsResponse> {
    return this.request<QueryAuditLogsResponse>('GET', '/api/audit/logs', {
      query: params as Record<string, string | number | undefined>,
    });
  }

  async getAuditStats(): Promise<AuditStatsResponse> {
    return this.request<AuditStatsResponse>('GET', '/api/audit/stats');
  }

  async triggerAuditArchive(): Promise<AuditArchiveResponse> {
    return this.request<AuditArchiveResponse>('POST', '/api/audit/archive');
  }

  // ===========================================================================
  // Backtest Endpoints
  // ===========================================================================

  async runBacktest(request: RunBacktestRequest): Promise<RunBacktestResponse> {
    return this.request<RunBacktestResponse>('POST', '/api/backtest', {
      body: request,
    });
  }

  async getBacktest(backtestId: string): Promise<BacktestResult> {
    return this.request<BacktestResult>(
      'GET',
      `/api/backtest/${encodeURIComponent(backtestId)}`
    );
  }

  async listBacktests(): Promise<ListBacktestsResponse> {
    return this.request<ListBacktestsResponse>('GET', '/api/backtests');
  }

  async cancelBacktest(backtestId: string): Promise<{ ok: true }> {
    return this.request<{ ok: true }>(
      'POST',
      `/api/backtest/${encodeURIComponent(backtestId)}/cancel`
    );
  }

  async deleteBacktest(backtestId: string): Promise<{ ok: true }> {
    return this.request<{ ok: true }>(
      'DELETE',
      `/api/backtest/${encodeURIComponent(backtestId)}`
    );
  }
}

// =============================================================================
// Factory Function
// =============================================================================

export function createApiClient(
  baseUrl: string,
  options: Partial<Omit<ApiClientConfig, 'baseUrl'>> = {}
): VelozApiClient {
  return new VelozApiClient({
    baseUrl: baseUrl.replace(/\/+$/, ''),
    ...options,
  });
}
