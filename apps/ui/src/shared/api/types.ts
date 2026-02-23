/**
 * VeloZ API Type Definitions
 * Generated from docs/api/http_api.md and docs/api/sse_api.md
 */

// =============================================================================
// Common Types
// =============================================================================

/** Order side */
export type OrderSide = 'BUY' | 'SELL';

/** Position side */
export type PositionSide = 'LONG' | 'SHORT';

/** Order status */
export type OrderStatus =
  | 'ACCEPTED'
  | 'PARTIALLY_FILLED'
  | 'FILLED'
  | 'CANCELLED'
  | 'REJECTED'
  | 'EXPIRED';

/** Engine lifecycle state */
export type EngineState = 'Starting' | 'Running' | 'Stopping' | 'Stopped';

/** Strategy state */
export type StrategyState = 'Running' | 'Paused' | 'Stopped';

/** Permission level */
export type Permission = 'read' | 'write' | 'admin';

/** SSE connection state */
export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

/** Audit log type */
export type AuditLogType = 'auth' | 'order' | 'api_key' | 'error' | 'access';

// =============================================================================
// API Error Types
// =============================================================================

/** API error codes */
export type ApiErrorCode =
  | 'bad_params'
  | 'bad_json'
  | 'binance_not_configured'
  | 'authentication_required'
  | 'invalid_token'
  | 'invalid_api_key'
  | 'invalid_credentials'
  | 'invalid_refresh_token'
  | 'forbidden'
  | 'not_found'
  | 'rate_limit_exceeded'
  | 'execution_unavailable'
  | 'auth_not_configured'
  | 'not_initialized';

/** Standard API error response */
export interface ApiError {
  error: ApiErrorCode;
  message?: string;
}

/** Rate limit headers */
export interface RateLimitInfo {
  limit: number;
  remaining: number;
  reset: number;
  retryAfter?: number;
}

// =============================================================================
// Authentication Types
// =============================================================================

/** Login request */
export interface LoginRequest {
  user_id: string;
  password: string;
}

/** Login response */
export interface LoginResponse {
  access_token: string;
  refresh_token: string;
  token_type: 'Bearer';
  expires_in: number;
}

/** Token refresh request */
export interface RefreshTokenRequest {
  refresh_token: string;
}

/** Token refresh response */
export interface RefreshTokenResponse {
  access_token: string;
  token_type: 'Bearer';
  expires_in: number;
}

/** Logout request */
export interface LogoutRequest {
  refresh_token: string;
}

/** Logout response */
export interface LogoutResponse {
  ok: true;
  message: string;
}

/** API key info */
export interface ApiKeyInfo {
  key_id: string;
  user_id: string;
  name: string;
  created_at: number;
  last_used_at: number | null;
  revoked: boolean;
  permissions: Permission[];
}

/** List API keys response */
export interface ListApiKeysResponse {
  keys: ApiKeyInfo[];
}

/** Create API key request */
export interface CreateApiKeyRequest {
  name: string;
  permissions?: Permission[];
}

/** Create API key response */
export interface CreateApiKeyResponse {
  key_id: string;
  api_key: string;
  message: string;
}

/** Revoke API key response */
export interface RevokeApiKeyResponse {
  ok: true;
  key_id: string;
}

// =============================================================================
// Health & Config Types
// =============================================================================

/** Health check response */
export interface HealthResponse {
  ok: true;
}

/** Gateway configuration */
export interface ConfigResponse {
  market_source: 'sim' | 'binance_rest';
  market_symbol: string;
  binance_base_url: string;
  execution_mode: 'sim_engine' | 'binance_testnet_spot';
  binance_trade_enabled: boolean;
  binance_trade_base_url: string;
  binance_user_stream_connected: boolean;
}

// =============================================================================
// Market Data Types
// =============================================================================

/** Market data response */
export interface MarketDataResponse {
  symbol: string;
  price: number;
  ts_ns: number;
}

/** Trade data (WebSocket) */
export interface TradeData {
  type: 'trade';
  symbol: string;
  price: number;
  qty: number;
  is_buyer_maker: boolean;
  ts_ns: number;
}

/** Book top data (WebSocket) */
export interface BookTopData {
  type: 'book_top';
  symbol: string;
  bid_price: number;
  bid_qty: number;
  ask_price: number;
  ask_qty: number;
  ts_ns: number;
}

/** WebSocket subscribe request */
export interface WsSubscribeRequest {
  action: 'subscribe';
  symbols: string[];
  channels: ('trade' | 'book_top' | 'kline')[];
}

/** WebSocket unsubscribe request */
export interface WsUnsubscribeRequest {
  action: 'unsubscribe';
  symbols: string[];
  channels: ('trade' | 'book_top' | 'kline')[];
}

// =============================================================================
// Order Types
// =============================================================================

/** Order event (from engine) */
export interface OrderEvent {
  type: 'order_update';
  ts_ns: number;
  client_order_id: string;
  symbol: string;
  side: OrderSide;
  qty: number;
  price: number;
  status: OrderStatus;
}

/** Order state (aggregated) */
export interface OrderState {
  client_order_id: string;
  symbol: string;
  side: OrderSide;
  order_qty: number;
  limit_price: number;
  venue_order_id: string;
  status: OrderStatus;
  executed_qty: number;
  avg_price: number;
  last_ts_ns: number;
  reason?: string;
}

/** List orders response */
export interface ListOrdersResponse {
  items: OrderEvent[];
}

/** List order states response */
export interface ListOrderStatesResponse {
  items: OrderState[];
}

/** Place order request */
export interface PlaceOrderRequest {
  side: OrderSide;
  symbol: string;
  qty: number;
  price: number;
  client_order_id?: string;
}

/** Place order response */
export interface PlaceOrderResponse {
  ok: true;
  client_order_id: string;
  venue_order_id: string;
}

/** Cancel order request */
export interface CancelOrderRequest {
  client_order_id: string;
  symbol?: string;
}

/** Cancel order response */
export interface CancelOrderResponse {
  ok: true;
  client_order_id: string;
}

// =============================================================================
// Account Types
// =============================================================================

/** Asset balance */
export interface AssetBalance {
  asset: string;
  free: number;
  locked: number;
}

/** Account state response */
export interface AccountResponse {
  ts_ns: number;
  balances: AssetBalance[];
}

// =============================================================================
// Position Types
// =============================================================================

/** Position info */
export interface Position {
  symbol: string;
  venue: string;
  side: PositionSide;
  qty: number;
  avg_entry_price: number;
  current_price: number;
  unrealized_pnl: number;
  realized_pnl: number;
  last_update_ts_ns: number;
}

/** List positions response */
export interface ListPositionsResponse {
  positions: Position[];
}

// =============================================================================
// Engine Service Mode Types
// =============================================================================

/** Engine status response */
export interface EngineStatusResponse {
  state: EngineState;
  uptime_ms: number;
  strategies_loaded: number;
  strategies_running: number;
}

/** Engine start/stop response */
export interface EngineControlResponse {
  ok: true;
  message: string;
}

/** Strategy summary */
export interface StrategySummary {
  id: string;
  name: string;
  type: string;
  state: StrategyState;
  pnl: number;
  trade_count: number;
}

/** Strategy detail */
export interface StrategyDetail extends StrategySummary {
  is_running: boolean;
  max_drawdown: number;
  win_count: number;
  lose_count: number;
  win_rate: number;
  profit_factor: number;
}

/** List strategies response */
export interface ListStrategiesResponse {
  strategies: StrategySummary[];
}

/** Strategy control response */
export interface StrategyControlResponse {
  ok: true;
  message: string;
}

// =============================================================================
// Execution Types
// =============================================================================

/** Execution ping response */
export interface ExecutionPingResponse {
  ok: boolean;
  result?: Record<string, unknown>;
  error?: string;
}

// =============================================================================
// Backtest Types
// =============================================================================

/** Backtest status */
export type BacktestStatus = 'queued' | 'running' | 'completed' | 'failed';

/** Backtest configuration */
export interface BacktestConfig {
  symbol: string;
  strategy: string;
  start_date: string;
  end_date: string;
  initial_capital: number;
  commission: number;
  slippage: number;
  parameters?: Record<string, unknown>;
}

/** Trade record from backtest */
export interface TradeRecord {
  entry_time: string;
  exit_time: string;
  side: 'BUY' | 'SELL';
  entry_price: number;
  exit_price: number;
  quantity: number;
  pnl: number;
  pnl_percent: number;
  commission: number;
}

/** Equity curve data point */
export interface EquityCurvePoint {
  timestamp: string;
  equity: number;
}

/** Drawdown curve data point */
export interface DrawdownPoint {
  timestamp: string;
  drawdown: number;
}

/** Backtest result */
export interface BacktestResult {
  id: string;
  config: BacktestConfig;
  total_return: number;
  sharpe_ratio: number;
  max_drawdown: number;
  win_rate: number;
  profit_factor: number;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  avg_win: number;
  avg_loss: number;
  equity_curve: EquityCurvePoint[];
  drawdown_curve: DrawdownPoint[];
  trades: TradeRecord[];
  created_at: string;
}

/** Run backtest request */
export interface RunBacktestRequest {
  config: BacktestConfig;
}

/** Run backtest response */
export interface RunBacktestResponse {
  backtest_id: string;
  status: BacktestStatus;
}

/** List backtests response */
export interface ListBacktestsResponse {
  backtests: BacktestResult[];
  total: number;
}

// =============================================================================
// Audit Log Types
// =============================================================================

/** Audit log entry */
export interface AuditLogEntry {
  timestamp: number;
  datetime: string;
  log_type: AuditLogType;
  action: string;
  user_id: string;
  ip_address: string;
  details: Record<string, unknown>;
  request_id?: string;
}

/** Query audit logs request params */
export interface QueryAuditLogsParams {
  type?: AuditLogType;
  user_id?: string;
  action?: string;
  start_time?: number;
  end_time?: number;
  limit?: number;
  offset?: number;
}

/** Query audit logs response */
export interface QueryAuditLogsResponse {
  logs: AuditLogEntry[];
  total: number;
  limit: number;
  offset: number;
  has_more: boolean;
}

/** Audit retention policy */
export interface AuditRetentionPolicy {
  retention_days: number;
  archive_before_delete: boolean;
  max_file_size_mb: number;
}

/** Audit log file stats */
export interface AuditLogFileStats {
  count: number;
  size_bytes: number;
}

/** Audit stats response */
export interface AuditStatsResponse {
  status: string;
  policies: Record<AuditLogType | 'default', AuditRetentionPolicy>;
  log_files: Record<AuditLogType, AuditLogFileStats>;
  archive_files: Record<AuditLogType, AuditLogFileStats>;
  total_size_bytes: number;
  archive_size_bytes: number;
}

/** Audit archive response */
export interface AuditArchiveResponse {
  ok: true;
  stats: {
    archived_files: number;
    deleted_files: number;
    bytes_freed: number;
    errors: string[];
  };
}

// =============================================================================
// SSE Event Types
// =============================================================================

/** Base SSE event */
interface BaseSSEEvent {
  type: string;
}

/** Market SSE event */
export interface MarketSSEEvent extends BaseSSEEvent {
  type: 'market';
  symbol: string;
  ts_ns: number;
  price: number;
}

/** Order update SSE event */
export interface OrderUpdateSSEEvent extends BaseSSEEvent {
  type: 'order_update';
  ts_ns: number;
  client_order_id: string;
  venue_order_id?: string;
  status: OrderStatus;
  symbol?: string;
  side?: OrderSide;
  qty?: number;
  price?: number;
  reason?: string;
}

/** Fill SSE event */
export interface FillSSEEvent extends BaseSSEEvent {
  type: 'fill';
  ts_ns: number;
  client_order_id: string;
  symbol: string;
  qty: number;
  price: number;
}

/** Order state SSE event */
export interface OrderStateSSEEvent extends BaseSSEEvent {
  type: 'order_state';
  client_order_id: string;
  status?: OrderStatus;
  symbol?: string;
  side?: OrderSide;
  order_qty?: number;
  limit_price?: number;
  venue_order_id?: string;
  executed_qty: number;
  avg_price: number;
  reason?: string;
  last_ts_ns: number;
}

/** Account SSE event */
export interface AccountSSEEvent extends BaseSSEEvent {
  type: 'account';
  ts_ns: number;
  balances?: AssetBalance[];
}

/** Error SSE event */
export interface ErrorSSEEvent extends BaseSSEEvent {
  type: 'error';
  ts_ns: number;
  message: string;
}

/** Union of all SSE event types */
export type SSEEvent =
  | MarketSSEEvent
  | OrderUpdateSSEEvent
  | FillSSEEvent
  | OrderStateSSEEvent
  | AccountSSEEvent
  | ErrorSSEEvent;

/** SSE event handler map */
export interface SSEEventHandlers {
  market?: (event: MarketSSEEvent) => void;
  order_update?: (event: OrderUpdateSSEEvent) => void;
  fill?: (event: FillSSEEvent) => void;
  order_state?: (event: OrderStateSSEEvent) => void;
  account?: (event: AccountSSEEvent) => void;
  error?: (event: ErrorSSEEvent) => void;
}

// =============================================================================
// WebSocket Types
// =============================================================================

/** WebSocket message types */
export type WsMarketMessage = TradeData | BookTopData;

/** WebSocket order message */
export interface WsOrderUpdateMessage {
  type: 'order_update';
  client_order_id: string;
  status: OrderStatus;
  symbol: string;
  side: OrderSide;
  qty: number;
  price: number;
  ts_ns: number;
}

/** WebSocket fill message */
export interface WsFillMessage {
  type: 'fill';
  client_order_id: string;
  symbol: string;
  qty: number;
  price: number;
  ts_ns: number;
}

/** WebSocket order channel message */
export type WsOrderMessage = WsOrderUpdateMessage | WsFillMessage;

// =============================================================================
// API Client Options
// =============================================================================

/** API client configuration */
export interface ApiClientConfig {
  /** Base URL for API requests */
  baseUrl: string;
  /** Request timeout in milliseconds */
  timeout?: number;
  /** Retry configuration */
  retry?: {
    maxRetries: number;
    retryDelay: number;
    retryOn: number[];
  };
  /** Authentication token */
  accessToken?: string;
  /** API key for authentication */
  apiKey?: string;
  /** Callback when token expires */
  onTokenExpired?: () => void;
  /** Callback when rate limited */
  onRateLimited?: (info: RateLimitInfo) => void;
}

/** SSE client configuration */
export interface SSEClientConfig {
  /** Base URL for SSE endpoint */
  baseUrl: string;
  /** Reconnection delay in milliseconds */
  reconnectDelay?: number;
  /** Maximum reconnection delay */
  maxReconnectDelay?: number;
  /** Backoff multiplier for reconnection */
  backoffMultiplier?: number;
  /** Last event ID for resumption */
  lastEventId?: string;
}

/** WebSocket client configuration */
export interface WebSocketClientConfig {
  /** Base URL for WebSocket endpoint */
  baseUrl: string;
  /** Reconnection delay in milliseconds */
  reconnectDelay?: number;
  /** Maximum reconnection delay */
  maxReconnectDelay?: number;
  /** Ping interval in milliseconds */
  pingInterval?: number;
}
