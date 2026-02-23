/**
 * MSW Request Handlers
 * Mock handlers for all VeloZ API endpoints
 */

import { http, HttpResponse, delay } from 'msw';
import type {
  HealthResponse,
  ConfigResponse,
  MarketDataResponse,
  ListOrdersResponse,
  ListOrderStatesResponse,
  PlaceOrderResponse,
  CancelOrderResponse,
  AccountResponse,
  ListPositionsResponse,
  EngineStatusResponse,
  EngineControlResponse,
  ListStrategiesResponse,
  StrategyControlResponse,
  ListBacktestsResponse,
  RunBacktestResponse,
  LoginResponse,
  RefreshTokenResponse,
  LogoutResponse,
  ListApiKeysResponse,
  CreateApiKeyResponse,
  RevokeApiKeyResponse,
  OrderEvent,
  OrderState,
  Position,
  StrategySummary,
  BacktestResult,
  ApiKeyInfo,
} from '../../shared/api/types';

// =============================================================================
// Mock Data
// =============================================================================

export const mockOrders: OrderEvent[] = [
  {
    type: 'order_update',
    ts_ns: Date.now() * 1_000_000,
    client_order_id: 'order-001',
    symbol: 'BTCUSDT',
    side: 'BUY',
    qty: 0.1,
    price: 42000,
    status: 'FILLED',
  },
  {
    type: 'order_update',
    ts_ns: Date.now() * 1_000_000 - 60_000_000_000,
    client_order_id: 'order-002',
    symbol: 'ETHUSDT',
    side: 'SELL',
    qty: 1.5,
    price: 2200,
    status: 'ACCEPTED',
  },
];

export const mockOrderStates: OrderState[] = [
  {
    client_order_id: 'order-001',
    symbol: 'BTCUSDT',
    side: 'BUY',
    order_qty: 0.1,
    limit_price: 42000,
    venue_order_id: 'venue-001',
    status: 'FILLED',
    executed_qty: 0.1,
    avg_price: 41950,
    last_ts_ns: Date.now() * 1_000_000,
  },
  {
    client_order_id: 'order-002',
    symbol: 'ETHUSDT',
    side: 'SELL',
    order_qty: 1.5,
    limit_price: 2200,
    venue_order_id: 'venue-002',
    status: 'ACCEPTED',
    executed_qty: 0,
    avg_price: 0,
    last_ts_ns: Date.now() * 1_000_000,
  },
];

export const mockPositions: Position[] = [
  {
    symbol: 'BTCUSDT',
    venue: 'binance',
    side: 'LONG',
    qty: 0.5,
    avg_entry_price: 41000,
    current_price: 42500,
    unrealized_pnl: 750,
    realized_pnl: 1200,
    last_update_ts_ns: Date.now() * 1_000_000,
  },
  {
    symbol: 'ETHUSDT',
    venue: 'binance',
    side: 'SHORT',
    qty: 2.0,
    avg_entry_price: 2300,
    current_price: 2200,
    unrealized_pnl: 200,
    realized_pnl: 150,
    last_update_ts_ns: Date.now() * 1_000_000,
  },
];

export const mockStrategies: StrategySummary[] = [
  {
    id: 'strat-001',
    name: 'BTC Momentum',
    type: 'momentum',
    state: 'Running',
    pnl: 2500,
    trade_count: 45,
  },
  {
    id: 'strat-002',
    name: 'ETH Grid',
    type: 'grid',
    state: 'Paused',
    pnl: 1200,
    trade_count: 120,
  },
  {
    id: 'strat-003',
    name: 'Mean Reversion',
    type: 'mean_reversion',
    state: 'Stopped',
    pnl: -300,
    trade_count: 15,
  },
];

export const mockBacktests: BacktestResult[] = [
  {
    id: 'bt-001',
    config: {
      symbol: 'BTCUSDT',
      strategy: 'momentum',
      start_date: '2024-01-01',
      end_date: '2024-06-30',
      initial_capital: 10000,
      commission: 0.001,
      slippage: 0.0005,
    },
    total_return: 0.25,
    sharpe_ratio: 1.8,
    max_drawdown: 0.12,
    win_rate: 0.58,
    profit_factor: 1.6,
    total_trades: 150,
    winning_trades: 87,
    losing_trades: 63,
    avg_win: 45,
    avg_loss: 28,
    equity_curve: [
      { timestamp: '2024-01-01', equity: 10000 },
      { timestamp: '2024-03-01', equity: 11200 },
      { timestamp: '2024-06-30', equity: 12500 },
    ],
    drawdown_curve: [
      { timestamp: '2024-01-01', drawdown: 0 },
      { timestamp: '2024-02-15', drawdown: 0.08 },
      { timestamp: '2024-06-30', drawdown: 0.02 },
    ],
    trades: [
      {
        entry_time: '2024-01-15T10:00:00Z',
        exit_time: '2024-01-15T14:00:00Z',
        side: 'BUY',
        entry_price: 42000,
        exit_price: 42500,
        quantity: 0.1,
        pnl: 50,
        pnl_percent: 0.012,
        commission: 8.45,
      },
    ],
    created_at: '2024-07-01T12:00:00Z',
  },
];

export const mockApiKeys: ApiKeyInfo[] = [
  {
    key_id: 'key-001',
    user_id: 'user-001',
    name: 'Trading Bot',
    created_at: Date.now() - 86400000,
    last_used_at: Date.now() - 3600000,
    revoked: false,
    permissions: ['read', 'write'],
  },
  {
    key_id: 'key-002',
    user_id: 'user-001',
    name: 'Read Only',
    created_at: Date.now() - 172800000,
    last_used_at: null,
    revoked: false,
    permissions: ['read'],
  },
];

// =============================================================================
// Base URL
// =============================================================================

const API_BASE = 'http://127.0.0.1:8080';

// =============================================================================
// Health & Config Handlers
// =============================================================================

const healthHandlers = [
  http.get(`${API_BASE}/api/health`, async () => {
    await delay(50);
    return HttpResponse.json<HealthResponse>({ ok: true });
  }),

  http.get(`${API_BASE}/api/config`, async () => {
    await delay(50);
    return HttpResponse.json<ConfigResponse>({
      market_source: 'binance_rest',
      market_symbol: 'BTCUSDT',
      binance_base_url: 'https://api.binance.com',
      execution_mode: 'sim_engine',
      binance_trade_enabled: true,
      binance_trade_base_url: 'https://testnet.binance.vision',
      binance_user_stream_connected: true,
    });
  }),
];

// =============================================================================
// Authentication Handlers
// =============================================================================

const authHandlers = [
  http.post(`${API_BASE}/api/auth/login`, async ({ request }) => {
    await delay(100);
    const body = await request.json() as { user_id: string; password: string };

    if (body.user_id === 'test' && body.password === 'password') {
      return HttpResponse.json<LoginResponse>({
        access_token: 'mock-access-token-12345',
        refresh_token: 'mock-refresh-token-67890',
        token_type: 'Bearer',
        expires_in: 3600,
      });
    }

    return HttpResponse.json(
      { error: 'invalid_credentials', message: 'Invalid username or password' },
      { status: 401 }
    );
  }),

  http.post(`${API_BASE}/api/auth/refresh`, async () => {
    await delay(50);
    return HttpResponse.json<RefreshTokenResponse>({
      access_token: 'mock-access-token-refreshed',
      token_type: 'Bearer',
      expires_in: 3600,
    });
  }),

  http.post(`${API_BASE}/api/auth/logout`, async () => {
    await delay(50);
    return HttpResponse.json<LogoutResponse>({
      ok: true,
      message: 'Logged out successfully',
    });
  }),

  http.get(`${API_BASE}/api/auth/keys`, async () => {
    await delay(50);
    return HttpResponse.json<ListApiKeysResponse>({
      keys: mockApiKeys,
    });
  }),

  http.post(`${API_BASE}/api/auth/keys`, async ({ request }) => {
    await delay(100);
    const body = await request.json() as { name: string };
    return HttpResponse.json<CreateApiKeyResponse>({
      key_id: `key-${Date.now()}`,
      api_key: `vz_${Math.random().toString(36).substring(2)}`,
      message: `API key "${body.name}" created successfully`,
    });
  }),

  http.delete(`${API_BASE}/api/auth/keys/:keyId`, async ({ params }) => {
    await delay(50);
    return HttpResponse.json<RevokeApiKeyResponse>({
      ok: true,
      key_id: params.keyId as string,
    });
  }),
];

// =============================================================================
// Market Data Handlers
// =============================================================================

const marketHandlers = [
  http.get(`${API_BASE}/api/market`, async ({ request }) => {
    await delay(50);
    const url = new URL(request.url);
    const symbol = url.searchParams.get('symbol') || 'BTCUSDT';

    return HttpResponse.json<MarketDataResponse>({
      symbol,
      price: symbol === 'BTCUSDT' ? 42500.50 : 2250.75,
      ts_ns: Date.now() * 1_000_000,
    });
  }),
];

// =============================================================================
// Order Handlers
// =============================================================================

const orderHandlers = [
  http.get(`${API_BASE}/api/orders`, async () => {
    await delay(50);
    return HttpResponse.json<ListOrdersResponse>({
      items: mockOrders,
    });
  }),

  http.get(`${API_BASE}/api/orders_state`, async () => {
    await delay(50);
    return HttpResponse.json<ListOrderStatesResponse>({
      items: mockOrderStates,
    });
  }),

  http.get(`${API_BASE}/api/order_state`, async ({ request }) => {
    await delay(50);
    const url = new URL(request.url);
    const clientOrderId = url.searchParams.get('client_order_id');
    const orderState = mockOrderStates.find(o => o.client_order_id === clientOrderId);

    if (!orderState) {
      return HttpResponse.json(
        { error: 'not_found', message: 'Order not found' },
        { status: 404 }
      );
    }

    return HttpResponse.json(orderState);
  }),

  http.post(`${API_BASE}/api/order`, async ({ request }) => {
    await delay(100);
    const body = await request.json() as { client_order_id?: string };
    const clientOrderId = body.client_order_id || `order-${Date.now()}`;

    return HttpResponse.json<PlaceOrderResponse>({
      ok: true,
      client_order_id: clientOrderId,
      venue_order_id: `venue-${Date.now()}`,
    });
  }),

  http.post(`${API_BASE}/api/cancel`, async ({ request }) => {
    await delay(50);
    const body = await request.json() as { client_order_id: string };
    return HttpResponse.json<CancelOrderResponse>({
      ok: true,
      client_order_id: body.client_order_id,
    });
  }),
];

// =============================================================================
// Account Handlers
// =============================================================================

const accountHandlers = [
  http.get(`${API_BASE}/api/account`, async () => {
    await delay(50);
    return HttpResponse.json<AccountResponse>({
      ts_ns: Date.now() * 1_000_000,
      balances: [
        { asset: 'USDT', free: 10000, locked: 500 },
        { asset: 'BTC', free: 0.5, locked: 0.1 },
        { asset: 'ETH', free: 5.0, locked: 0 },
      ],
    });
  }),
];

// =============================================================================
// Position Handlers
// =============================================================================

const positionHandlers = [
  http.get(`${API_BASE}/api/positions`, async () => {
    await delay(50);
    return HttpResponse.json<ListPositionsResponse>({
      positions: mockPositions,
    });
  }),

  http.get(`${API_BASE}/api/positions/:symbol`, async ({ params }) => {
    await delay(50);
    const position = mockPositions.find(p => p.symbol === params.symbol);

    if (!position) {
      return HttpResponse.json(
        { error: 'not_found', message: 'Position not found' },
        { status: 404 }
      );
    }

    return HttpResponse.json(position);
  }),
];

// =============================================================================
// Engine Handlers
// =============================================================================

const engineHandlers = [
  http.get(`${API_BASE}/api/status`, async () => {
    await delay(50);
    return HttpResponse.json<EngineStatusResponse>({
      state: 'Running',
      uptime_ms: 3600000,
      strategies_loaded: 3,
      strategies_running: 1,
    });
  }),

  http.post(`${API_BASE}/api/start`, async () => {
    await delay(200);
    return HttpResponse.json<EngineControlResponse>({
      ok: true,
      message: 'Engine started successfully',
    });
  }),

  http.post(`${API_BASE}/api/stop`, async () => {
    await delay(200);
    return HttpResponse.json<EngineControlResponse>({
      ok: true,
      message: 'Engine stopped successfully',
    });
  }),
];

// =============================================================================
// Strategy Handlers
// =============================================================================

const strategyHandlers = [
  http.get(`${API_BASE}/api/strategies`, async () => {
    await delay(50);
    return HttpResponse.json<ListStrategiesResponse>({
      strategies: mockStrategies,
    });
  }),

  http.get(`${API_BASE}/api/strategies/:strategyId`, async ({ params }) => {
    await delay(50);
    const strategy = mockStrategies.find(s => s.id === params.strategyId);

    if (!strategy) {
      return HttpResponse.json(
        { error: 'not_found', message: 'Strategy not found' },
        { status: 404 }
      );
    }

    return HttpResponse.json({
      ...strategy,
      is_running: strategy.state === 'Running',
      max_drawdown: 0.08,
      win_count: 25,
      lose_count: 20,
      win_rate: 0.556,
      profit_factor: 1.4,
    });
  }),

  http.post(`${API_BASE}/api/strategies/:strategyId/start`, async () => {
    await delay(100);
    return HttpResponse.json<StrategyControlResponse>({
      ok: true,
      message: 'Strategy started successfully',
    });
  }),

  http.post(`${API_BASE}/api/strategies/:strategyId/stop`, async () => {
    await delay(100);
    return HttpResponse.json<StrategyControlResponse>({
      ok: true,
      message: 'Strategy stopped successfully',
    });
  }),
];

// =============================================================================
// Backtest Handlers
// =============================================================================

const backtestHandlers = [
  http.get(`${API_BASE}/api/backtests`, async () => {
    await delay(50);
    return HttpResponse.json<ListBacktestsResponse>({
      backtests: mockBacktests,
      total: mockBacktests.length,
    });
  }),

  http.get(`${API_BASE}/api/backtest/:backtestId`, async ({ params }) => {
    await delay(50);
    const backtest = mockBacktests.find(b => b.id === params.backtestId);

    if (!backtest) {
      return HttpResponse.json(
        { error: 'not_found', message: 'Backtest not found' },
        { status: 404 }
      );
    }

    return HttpResponse.json(backtest);
  }),

  http.post(`${API_BASE}/api/backtest`, async () => {
    await delay(200);
    return HttpResponse.json<RunBacktestResponse>({
      backtest_id: `bt-${Date.now()}`,
      status: 'queued',
    });
  }),

  http.delete(`${API_BASE}/api/backtest/:backtestId`, async () => {
    await delay(50);
    return HttpResponse.json({ ok: true });
  }),

  http.post(`${API_BASE}/api/backtest/:backtestId/cancel`, async () => {
    await delay(50);
    return HttpResponse.json({ ok: true });
  }),
];

// =============================================================================
// Export All Handlers
// =============================================================================

export const handlers = [
  ...healthHandlers,
  ...authHandlers,
  ...marketHandlers,
  ...orderHandlers,
  ...accountHandlers,
  ...positionHandlers,
  ...engineHandlers,
  ...strategyHandlers,
  ...backtestHandlers,
];
