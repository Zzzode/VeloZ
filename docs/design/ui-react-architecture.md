# VeloZ React + TypeScript UI Architecture

Technical architecture document for the VeloZ UI redesign project.

**Version**: 1.0.0
**Date**: 2026-02-23
**Author**: Technical Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Technology Stack](#technology-stack)
3. [Project Structure](#project-structure)
4. [State Management](#state-management)
5. [API Client Architecture](#api-client-architecture)
6. [SSE Event Handling](#sse-event-handling)
7. [Routing Strategy](#routing-strategy)
8. [Form Handling](#form-handling)
9. [Chart Library](#chart-library)
10. [Authentication Flow](#authentication-flow)
11. [Error Handling](#error-handling)
12. [Performance Optimization](#performance-optimization)
13. [Build and Deployment](#build-and-deployment)
14. [Coding Standards](#coding-standards)
15. [Testing Strategy](#testing-strategy)

---

## Executive Summary

This document defines the technical architecture for migrating VeloZ's static HTML UI to a modern React + TypeScript single-page application. The new architecture prioritizes:

- **Type Safety**: Full TypeScript coverage with strict mode
- **Real-time Updates**: Robust SSE/WebSocket handling with automatic reconnection
- **Performance**: Code splitting, lazy loading, and optimized re-renders
- **Developer Experience**: Fast builds, hot module replacement, comprehensive testing
- **Maintainability**: Feature-based organization, clear separation of concerns

---

## Technology Stack

### Core Framework

| Technology | Version | Justification |
|------------|---------|---------------|
| **React** | 18.3+ | Concurrent features, Suspense, automatic batching |
| **TypeScript** | 5.4+ | Strict type checking, improved inference |
| **Vite** | 5.x | Fast HMR, native ESM, excellent build performance |

### State Management

| Technology | Version | Justification |
|------------|---------|---------------|
| **Zustand** | 4.x | Lightweight, TypeScript-first, no boilerplate |
| **TanStack Query** | 5.x | Server state management, caching, background refetch |

**Decision Rationale**: Zustand over Redux Toolkit for client state due to:
- Simpler API with less boilerplate
- Better TypeScript inference out of the box
- Smaller bundle size (~1KB vs ~12KB)
- No need for complex middleware for this application scale

TanStack Query handles server state (API responses), separating concerns cleanly.

### UI Components

| Technology | Version | Justification |
|------------|---------|---------------|
| **Tailwind CSS** | 3.4+ | Utility-first, design system consistency |
| **Headless UI** | 2.x | Accessible, unstyled components |
| **Lucide React** | 0.x | Consistent icon set, tree-shakeable |

### Data Visualization

| Technology | Version | Justification |
|------------|---------|---------------|
| **Recharts** | 2.x | React-native, declarative, good TypeScript support |
| **Lightweight Charts** | 4.x | TradingView-quality candlestick charts |

**Decision Rationale**: Recharts for general charts (equity curves, drawdowns) due to React integration. Lightweight Charts for financial charts (candlesticks, order book depth) due to performance and trading-specific features.

### Forms and Validation

| Technology | Version | Justification |
|------------|---------|---------------|
| **React Hook Form** | 7.x | Performance, minimal re-renders |
| **Zod** | 3.x | TypeScript-first schema validation |

### Testing

| Technology | Version | Justification |
|------------|---------|---------------|
| **Vitest** | 1.x | Vite-native, Jest-compatible API |
| **React Testing Library** | 14.x | User-centric testing |
| **Playwright** | 1.x | E2E testing, cross-browser |
| **MSW** | 2.x | API mocking for tests |

### Code Quality

| Technology | Version | Justification |
|------------|---------|---------------|
| **ESLint** | 8.x | Linting with React/TypeScript rules |
| **Prettier** | 3.x | Code formatting |
| **Husky** | 9.x | Git hooks for pre-commit checks |

---

## Project Structure

Feature-based organization for scalability and maintainability:

```
apps/ui/
├── public/
│   ├── favicon.ico
│   └── robots.txt
├── src/
│   ├── app/                      # Application shell
│   │   ├── App.tsx               # Root component
│   │   ├── Router.tsx            # Route definitions
│   │   └── providers/            # Context providers
│   │       ├── QueryProvider.tsx
│   │       ├── AuthProvider.tsx
│   │       └── ThemeProvider.tsx
│   │
│   ├── features/                 # Feature modules
│   │   ├── auth/                 # Authentication
│   │   │   ├── api/              # Auth API calls
│   │   │   ├── components/       # Login, Logout, etc.
│   │   │   ├── hooks/            # useAuth, usePermissions
│   │   │   ├── store/            # Auth state (Zustand)
│   │   │   ├── types/            # Auth types
│   │   │   └── index.ts          # Public exports
│   │   │
│   │   ├── dashboard/            # Main dashboard
│   │   │   ├── components/
│   │   │   ├── hooks/
│   │   │   └── index.ts
│   │   │
│   │   ├── trading/              # Order management
│   │   │   ├── api/
│   │   │   ├── components/
│   │   │   │   ├── OrderForm.tsx
│   │   │   │   ├── OrderBook.tsx
│   │   │   │   ├── OrderList.tsx
│   │   │   │   └── PositionTable.tsx
│   │   │   ├── hooks/
│   │   │   │   ├── useOrders.ts
│   │   │   │   ├── usePositions.ts
│   │   │   │   └── useOrderBook.ts
│   │   │   ├── store/
│   │   │   ├── types/
│   │   │   └── index.ts
│   │   │
│   │   ├── market/               # Market data
│   │   │   ├── api/
│   │   │   ├── components/
│   │   │   │   ├── PriceDisplay.tsx
│   │   │   │   ├── MarketChart.tsx
│   │   │   │   └── SymbolSelector.tsx
│   │   │   ├── hooks/
│   │   │   │   ├── useMarketData.ts
│   │   │   │   └── useSSEStream.ts
│   │   │   ├── store/
│   │   │   ├── types/
│   │   │   └── index.ts
│   │   │
│   │   ├── strategies/           # Strategy management
│   │   │   ├── api/
│   │   │   ├── components/
│   │   │   │   ├── StrategyList.tsx
│   │   │   │   ├── StrategyCard.tsx
│   │   │   │   └── StrategyControls.tsx
│   │   │   ├── hooks/
│   │   │   ├── types/
│   │   │   └── index.ts
│   │   │
│   │   ├── backtest/             # Backtesting
│   │   │   ├── api/
│   │   │   ├── components/
│   │   │   │   ├── BacktestForm.tsx
│   │   │   │   ├── BacktestProgress.tsx
│   │   │   │   ├── BacktestResults.tsx
│   │   │   │   ├── EquityCurve.tsx
│   │   │   │   └── TradeList.tsx
│   │   │   ├── hooks/
│   │   │   ├── types/
│   │   │   └── index.ts
│   │   │
│   │   └── settings/             # Configuration
│   │       ├── components/
│   │       ├── hooks/
│   │       └── index.ts
│   │
│   ├── shared/                   # Shared utilities
│   │   ├── api/                  # API client setup
│   │   │   ├── client.ts         # Axios/fetch instance
│   │   │   ├── sse.ts            # SSE client
│   │   │   ├── websocket.ts      # WebSocket client
│   │   │   └── types.ts          # API response types
│   │   │
│   │   ├── components/           # Reusable UI components
│   │   │   ├── Button/
│   │   │   ├── Card/
│   │   │   ├── Modal/
│   │   │   ├── Table/
│   │   │   ├── Input/
│   │   │   ├── Select/
│   │   │   ├── Spinner/
│   │   │   ├── Toast/
│   │   │   └── ErrorBoundary/
│   │   │
│   │   ├── hooks/                # Shared hooks
│   │   │   ├── useLocalStorage.ts
│   │   │   ├── useDebounce.ts
│   │   │   └── useMediaQuery.ts
│   │   │
│   │   ├── utils/                # Utility functions
│   │   │   ├── format.ts         # formatPrice, formatTime, etc.
│   │   │   ├── validation.ts     # Zod schemas
│   │   │   └── constants.ts      # App constants
│   │   │
│   │   └── types/                # Shared types
│   │       ├── api.ts            # API response types
│   │       ├── market.ts         # Market data types
│   │       ├── order.ts          # Order types
│   │       └── common.ts         # Common types
│   │
│   ├── styles/                   # Global styles
│   │   ├── globals.css           # Tailwind imports
│   │   └── theme.css             # CSS variables
│   │
│   ├── main.tsx                  # Entry point
│   └── vite-env.d.ts             # Vite types
│
├── tests/                        # Test files
│   ├── setup.ts                  # Test setup
│   ├── mocks/                    # MSW handlers
│   │   ├── handlers.ts
│   │   └── server.ts
│   └── e2e/                      # Playwright tests
│       ├── auth.spec.ts
│       ├── trading.spec.ts
│       └── backtest.spec.ts
│
├── .eslintrc.cjs                 # ESLint config
├── .prettierrc                   # Prettier config
├── index.html                    # HTML template
├── package.json
├── tailwind.config.js            # Tailwind config
├── tsconfig.json                 # TypeScript config
├── tsconfig.node.json            # Node TypeScript config
├── vite.config.ts                # Vite config
└── vitest.config.ts              # Vitest config
```

---

## State Management

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     React Components                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────┐    ┌─────────────────────────────────┐ │
│  │  Zustand Store  │    │       TanStack Query            │ │
│  │  (Client State) │    │       (Server State)            │ │
│  │                 │    │                                 │ │
│  │  - UI state     │    │  - API responses                │ │
│  │  - Auth tokens  │    │  - Orders, positions            │ │
│  │  - Preferences  │    │  - Market data (REST)           │ │
│  │  - SSE state    │    │  - Strategies                   │ │
│  └─────────────────┘    └─────────────────────────────────┘ │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              SSE/WebSocket Event Store                   │ │
│  │              (Real-time streaming state)                 │ │
│  │                                                          │ │
│  │  - Live market prices                                    │ │
│  │  - Order updates                                         │ │
│  │  - Fill notifications                                    │ │
│  │  - Account balance changes                               │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Zustand Store Structure

```typescript
// src/features/auth/store/authStore.ts
import { create } from 'zustand';
import { persist } from 'zustand/middleware';

interface AuthState {
  accessToken: string | null;
  refreshToken: string | null;
  user: User | null;
  permissions: Permission[];
  isAuthenticated: boolean;

  // Actions
  setTokens: (access: string, refresh: string) => void;
  setUser: (user: User) => void;
  logout: () => void;
  hasPermission: (permission: Permission) => boolean;
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set, get) => ({
      accessToken: null,
      refreshToken: null,
      user: null,
      permissions: [],
      isAuthenticated: false,

      setTokens: (access, refresh) => set({
        accessToken: access,
        refreshToken: refresh,
        isAuthenticated: true,
      }),

      setUser: (user) => set({
        user,
        permissions: user.permissions,
      }),

      logout: () => set({
        accessToken: null,
        refreshToken: null,
        user: null,
        permissions: [],
        isAuthenticated: false,
      }),

      hasPermission: (permission) =>
        get().permissions.includes(permission),
    }),
    {
      name: 'veloz-auth',
      partialize: (state) => ({
        accessToken: state.accessToken,
        refreshToken: state.refreshToken,
      }),
    }
  )
);
```

```typescript
// src/features/market/store/marketStore.ts
import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';

interface MarketState {
  prices: Record<string, MarketPrice>;
  selectedSymbol: string;
  connectionStatus: 'connected' | 'connecting' | 'disconnected';
  lastEventId: number | null;

  // Actions
  updatePrice: (symbol: string, price: MarketPrice) => void;
  setSelectedSymbol: (symbol: string) => void;
  setConnectionStatus: (status: MarketState['connectionStatus']) => void;
  setLastEventId: (id: number) => void;
}

export const useMarketStore = create<MarketState>()(
  subscribeWithSelector((set) => ({
    prices: {},
    selectedSymbol: 'BTCUSDT',
    connectionStatus: 'disconnected',
    lastEventId: null,

    updatePrice: (symbol, price) => set((state) => ({
      prices: { ...state.prices, [symbol]: price },
    })),

    setSelectedSymbol: (symbol) => set({ selectedSymbol: symbol }),

    setConnectionStatus: (status) => set({ connectionStatus: status }),

    setLastEventId: (id) => set({ lastEventId: id }),
  }))
);
```

### TanStack Query Configuration

```typescript
// src/app/providers/QueryProvider.tsx
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactQueryDevtools } from '@tanstack/react-query-devtools';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      staleTime: 5 * 1000,           // 5 seconds
      gcTime: 10 * 60 * 1000,        // 10 minutes (formerly cacheTime)
      retry: 3,
      retryDelay: (attemptIndex) => Math.min(1000 * 2 ** attemptIndex, 30000),
      refetchOnWindowFocus: false,   // Disable for trading app
    },
    mutations: {
      retry: 1,
    },
  },
});

export function QueryProvider({ children }: { children: React.ReactNode }) {
  return (
    <QueryClientProvider client={queryClient}>
      {children}
      <ReactQueryDevtools initialIsOpen={false} />
    </QueryClientProvider>
  );
}
```

---

## API Client Architecture

### HTTP Client Setup

```typescript
// src/shared/api/client.ts
import { useAuthStore } from '@/features/auth/store/authStore';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';

interface RequestConfig extends RequestInit {
  skipAuth?: boolean;
}

class ApiClient {
  private baseUrl: string;

  constructor(baseUrl: string) {
    this.baseUrl = baseUrl;
  }

  private async request<T>(
    endpoint: string,
    config: RequestConfig = {}
  ): Promise<T> {
    const { skipAuth = false, ...fetchConfig } = config;

    const headers = new Headers(fetchConfig.headers);
    headers.set('Content-Type', 'application/json');

    if (!skipAuth) {
      const token = useAuthStore.getState().accessToken;
      if (token) {
        headers.set('Authorization', `Bearer ${token}`);
      }
    }

    const response = await fetch(`${this.baseUrl}${endpoint}`, {
      ...fetchConfig,
      headers,
    });

    if (response.status === 401) {
      // Attempt token refresh
      const refreshed = await this.refreshToken();
      if (refreshed) {
        return this.request(endpoint, config);
      }
      useAuthStore.getState().logout();
      throw new ApiError('Authentication required', 401);
    }

    if (!response.ok) {
      const error = await response.json().catch(() => ({}));
      throw new ApiError(error.message || 'Request failed', response.status, error);
    }

    return response.json();
  }

  private async refreshToken(): Promise<boolean> {
    const refreshToken = useAuthStore.getState().refreshToken;
    if (!refreshToken) return false;

    try {
      const response = await fetch(`${this.baseUrl}/api/auth/refresh`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ refresh_token: refreshToken }),
      });

      if (!response.ok) return false;

      const data = await response.json();
      useAuthStore.getState().setTokens(data.access_token, refreshToken);
      return true;
    } catch {
      return false;
    }
  }

  get<T>(endpoint: string, config?: RequestConfig): Promise<T> {
    return this.request<T>(endpoint, { ...config, method: 'GET' });
  }

  post<T>(endpoint: string, body?: unknown, config?: RequestConfig): Promise<T> {
    return this.request<T>(endpoint, {
      ...config,
      method: 'POST',
      body: body ? JSON.stringify(body) : undefined,
    });
  }

  delete<T>(endpoint: string, config?: RequestConfig): Promise<T> {
    return this.request<T>(endpoint, { ...config, method: 'DELETE' });
  }
}

export const apiClient = new ApiClient(API_BASE);

export class ApiError extends Error {
  constructor(
    message: string,
    public status: number,
    public data?: unknown
  ) {
    super(message);
    this.name = 'ApiError';
  }
}
```

### TypeScript API Types

```typescript
// src/shared/types/api.ts

// Market Data
export interface MarketData {
  symbol: string;
  price: number;
  ts_ns: number;
}

// Order Types
export type OrderSide = 'BUY' | 'SELL';
export type OrderStatus =
  | 'ACCEPTED'
  | 'PARTIALLY_FILLED'
  | 'FILLED'
  | 'CANCELLED'
  | 'REJECTED'
  | 'EXPIRED';

export interface OrderState {
  client_order_id: string;
  symbol: string;
  side: OrderSide;
  order_qty: number;
  limit_price: number;
  venue_order_id?: string;
  status: OrderStatus;
  executed_qty: number;
  avg_price: number;
  reason?: string;
  last_ts_ns: number;
}

export interface PlaceOrderRequest {
  side: OrderSide;
  symbol: string;
  qty: number;
  price: number;
  client_order_id?: string;
}

export interface PlaceOrderResponse {
  ok: boolean;
  client_order_id: string;
  venue_order_id?: string;
}

export interface CancelOrderRequest {
  client_order_id: string;
  symbol?: string;
}

// Position Types
export type PositionSide = 'LONG' | 'SHORT';

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

// Account Types
export interface AccountBalance {
  asset: string;
  free: number;
  locked: number;
}

export interface AccountState {
  ts_ns: number;
  balances: AccountBalance[];
}

// Strategy Types
export type StrategyState = 'Running' | 'Stopped' | 'Error';

export interface Strategy {
  id: string;
  name: string;
  type: string;
  state: StrategyState;
  pnl: number;
  trade_count: number;
}

export interface StrategyDetail extends Strategy {
  is_running: boolean;
  max_drawdown: number;
  win_count: number;
  lose_count: number;
  win_rate: number;
  profit_factor: number;
}

// Backtest Types
export interface BacktestConfig {
  strategy_name: string;
  symbol: string;
  start_time: number;
  end_time: number;
  initial_balance: number;
  risk_per_trade: number;
  max_position_size: number;
  strategy_parameters: Record<string, number>;
  data_source: 'csv' | 'binance';
  data_type: 'kline' | 'trade' | 'book';
  time_frame: '1m' | '5m' | '15m' | '1h' | '4h' | '1d';
}

export interface BacktestResult {
  strategy_name: string;
  symbol: string;
  start_time: number;
  end_time: number;
  initial_balance: number;
  final_balance: number;
  total_return: number;
  max_drawdown: number;
  sharpe_ratio: number;
  win_rate: number;
  profit_factor: number;
  trade_count: number;
  win_count: number;
  lose_count: number;
  avg_win: number;
  avg_lose: number;
  trades: TradeRecord[];
  equity_curve: EquityCurvePoint[];
  drawdown_curve: DrawdownPoint[];
}

export interface TradeRecord {
  timestamp: number;
  symbol: string;
  side: 'buy' | 'sell';
  price: number;
  quantity: number;
  fee: number;
  pnl: number;
  strategy_id: string;
}

export interface EquityCurvePoint {
  timestamp: number;
  equity: number;
  cumulative_return: number;
}

export interface DrawdownPoint {
  timestamp: number;
  drawdown: number;
}

// Auth Types
export type Permission = 'read' | 'write' | 'admin';

export interface User {
  user_id: string;
  permissions: Permission[];
}

export interface LoginRequest {
  user_id: string;
  password: string;
}

export interface LoginResponse {
  access_token: string;
  refresh_token: string;
  token_type: 'Bearer';
  expires_in: number;
}

export interface ApiKey {
  key_id: string;
  user_id: string;
  name: string;
  created_at: number;
  last_used_at?: number;
  revoked: boolean;
  permissions: Permission[];
}

// Config Types
export interface GatewayConfig {
  market_source: 'sim' | 'binance_rest';
  market_symbol: string;
  binance_base_url: string;
  execution_mode: 'sim_engine' | 'binance_testnet_spot';
  binance_trade_enabled: boolean;
  binance_trade_base_url: string;
  binance_user_stream_connected: boolean;
}

// SSE Event Types
export interface SSEMarketEvent {
  type: 'market';
  symbol: string;
  ts_ns: number;
  price: number;
}

export interface SSEOrderUpdateEvent {
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

export interface SSEFillEvent {
  type: 'fill';
  ts_ns: number;
  client_order_id: string;
  symbol: string;
  qty: number;
  price: number;
}

export interface SSEAccountEvent {
  type: 'account';
  ts_ns: number;
  balances: AccountBalance[];
}

export interface SSEErrorEvent {
  type: 'error';
  ts_ns: number;
  message: string;
}

export type SSEEvent =
  | SSEMarketEvent
  | SSEOrderUpdateEvent
  | SSEFillEvent
  | SSEAccountEvent
  | SSEErrorEvent;
```

### Query Hooks Example

```typescript
// src/features/trading/api/orderQueries.ts
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/shared/api/client';
import type {
  OrderState,
  PlaceOrderRequest,
  PlaceOrderResponse,
  CancelOrderRequest
} from '@/shared/types/api';

export const orderKeys = {
  all: ['orders'] as const,
  lists: () => [...orderKeys.all, 'list'] as const,
  list: (filters: string) => [...orderKeys.lists(), { filters }] as const,
  details: () => [...orderKeys.all, 'detail'] as const,
  detail: (id: string) => [...orderKeys.details(), id] as const,
};

export function useOrders() {
  return useQuery({
    queryKey: orderKeys.lists(),
    queryFn: () => apiClient.get<{ items: OrderState[] }>('/api/orders_state'),
    select: (data) => data.items,
  });
}

export function useOrderDetail(clientOrderId: string) {
  return useQuery({
    queryKey: orderKeys.detail(clientOrderId),
    queryFn: () =>
      apiClient.get<OrderState>(`/api/order_state?client_order_id=${clientOrderId}`),
    enabled: !!clientOrderId,
  });
}

export function usePlaceOrder() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (order: PlaceOrderRequest) =>
      apiClient.post<PlaceOrderResponse>('/api/order', order),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: orderKeys.lists() });
    },
  });
}

export function useCancelOrder() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (request: CancelOrderRequest) =>
      apiClient.post<{ ok: boolean }>('/api/cancel', request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: orderKeys.lists() });
    },
  });
}
```

---

## SSE Event Handling

### SSE Client with Reconnection

```typescript
// src/shared/api/sse.ts
import { useMarketStore } from '@/features/market/store/marketStore';
import type { SSEEvent } from '@/shared/types/api';

type SSEEventHandler = (event: SSEEvent) => void;

interface SSEClientOptions {
  baseUrl: string;
  onEvent: SSEEventHandler;
  onConnect?: () => void;
  onDisconnect?: () => void;
  onError?: (error: Error) => void;
  reconnectInterval?: number;
  maxReconnectAttempts?: number;
}

export class SSEClient {
  private eventSource: EventSource | null = null;
  private options: Required<SSEClientOptions>;
  private reconnectAttempts = 0;
  private reconnectTimer: number | null = null;
  private lastEventId: number | null = null;

  constructor(options: SSEClientOptions) {
    this.options = {
      reconnectInterval: 3000,
      maxReconnectAttempts: 10,
      onConnect: () => {},
      onDisconnect: () => {},
      onError: () => {},
      ...options,
    };
  }

  connect(): void {
    if (this.eventSource) {
      this.disconnect();
    }

    const url = new URL('/api/stream', this.options.baseUrl);
    if (this.lastEventId !== null) {
      url.searchParams.set('last_id', String(this.lastEventId));
    }

    useMarketStore.getState().setConnectionStatus('connecting');

    this.eventSource = new EventSource(url.toString());

    this.eventSource.onopen = () => {
      this.reconnectAttempts = 0;
      useMarketStore.getState().setConnectionStatus('connected');
      this.options.onConnect();
    };

    this.eventSource.onerror = (error) => {
      useMarketStore.getState().setConnectionStatus('disconnected');
      this.options.onError(new Error('SSE connection error'));
      this.scheduleReconnect();
    };

    // Handle specific event types
    const eventTypes = ['market', 'order_update', 'fill', 'account', 'error'];

    eventTypes.forEach((eventType) => {
      this.eventSource!.addEventListener(eventType, (event: MessageEvent) => {
        try {
          const data = JSON.parse(event.data) as SSEEvent;

          // Track last event ID for reconnection
          if (event.lastEventId) {
            this.lastEventId = parseInt(event.lastEventId, 10);
            useMarketStore.getState().setLastEventId(this.lastEventId);
          }

          this.options.onEvent(data);
        } catch (e) {
          console.error('Failed to parse SSE event:', e);
        }
      });
    });
  }

  disconnect(): void {
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }

    if (this.eventSource) {
      this.eventSource.close();
      this.eventSource = null;
    }

    useMarketStore.getState().setConnectionStatus('disconnected');
    this.options.onDisconnect();
  }

  private scheduleReconnect(): void {
    if (this.reconnectAttempts >= this.options.maxReconnectAttempts) {
      this.options.onError(new Error('Max reconnection attempts reached'));
      return;
    }

    const delay = this.options.reconnectInterval * Math.pow(2, this.reconnectAttempts);
    this.reconnectAttempts++;

    this.reconnectTimer = window.setTimeout(() => {
      this.connect();
    }, Math.min(delay, 30000)); // Cap at 30 seconds
  }

  getLastEventId(): number | null {
    return this.lastEventId;
  }
}
```

### SSE Hook

```typescript
// src/features/market/hooks/useSSEStream.ts
import { useEffect, useRef, useCallback } from 'react';
import { useQueryClient } from '@tanstack/react-query';
import { SSEClient } from '@/shared/api/sse';
import { useMarketStore } from '../store/marketStore';
import { orderKeys } from '@/features/trading/api/orderQueries';
import type { SSEEvent } from '@/shared/types/api';

const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';

export function useSSEStream() {
  const clientRef = useRef<SSEClient | null>(null);
  const queryClient = useQueryClient();
  const updatePrice = useMarketStore((state) => state.updatePrice);

  const handleEvent = useCallback((event: SSEEvent) => {
    switch (event.type) {
      case 'market':
        updatePrice(event.symbol, {
          price: event.price,
          timestamp: event.ts_ns,
        });
        break;

      case 'order_update':
      case 'fill':
        // Invalidate order queries to refetch
        queryClient.invalidateQueries({ queryKey: orderKeys.lists() });
        break;

      case 'account':
        // Invalidate account query
        queryClient.invalidateQueries({ queryKey: ['account'] });
        break;

      case 'error':
        console.error('SSE error event:', event.message);
        break;
    }
  }, [updatePrice, queryClient]);

  useEffect(() => {
    clientRef.current = new SSEClient({
      baseUrl: API_BASE,
      onEvent: handleEvent,
      onConnect: () => console.log('SSE connected'),
      onDisconnect: () => console.log('SSE disconnected'),
      onError: (error) => console.error('SSE error:', error),
    });

    clientRef.current.connect();

    return () => {
      clientRef.current?.disconnect();
    };
  }, [handleEvent]);

  return {
    disconnect: () => clientRef.current?.disconnect(),
    reconnect: () => clientRef.current?.connect(),
  };
}
```

---

## Routing Strategy

### React Router v6 Configuration

```typescript
// src/app/Router.tsx
import { createBrowserRouter, RouterProvider, Navigate } from 'react-router-dom';
import { lazy, Suspense } from 'react';
import { MainLayout } from '@/shared/components/Layout';
import { LoadingSpinner } from '@/shared/components/Spinner';
import { ProtectedRoute } from '@/features/auth/components/ProtectedRoute';

// Lazy load feature modules
const Dashboard = lazy(() => import('@/features/dashboard'));
const Trading = lazy(() => import('@/features/trading'));
const Strategies = lazy(() => import('@/features/strategies'));
const Backtest = lazy(() => import('@/features/backtest'));
const Settings = lazy(() => import('@/features/settings'));
const Login = lazy(() => import('@/features/auth/components/Login'));

const router = createBrowserRouter([
  {
    path: '/login',
    element: (
      <Suspense fallback={<LoadingSpinner />}>
        <Login />
      </Suspense>
    ),
  },
  {
    path: '/',
    element: (
      <ProtectedRoute>
        <MainLayout />
      </ProtectedRoute>
    ),
    children: [
      {
        index: true,
        element: <Navigate to="/dashboard" replace />,
      },
      {
        path: 'dashboard',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Dashboard />
          </Suspense>
        ),
      },
      {
        path: 'trading',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Trading />
          </Suspense>
        ),
      },
      {
        path: 'strategies',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Strategies />
          </Suspense>
        ),
      },
      {
        path: 'strategies/:id',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Strategies />
          </Suspense>
        ),
      },
      {
        path: 'backtest',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Backtest />
          </Suspense>
        ),
      },
      {
        path: 'settings',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Settings />
          </Suspense>
        ),
      },
    ],
  },
  {
    path: '*',
    element: <Navigate to="/dashboard" replace />,
  },
]);

export function AppRouter() {
  return <RouterProvider router={router} />;
}
```

### Protected Route Component

```typescript
// src/features/auth/components/ProtectedRoute.tsx
import { Navigate, useLocation } from 'react-router-dom';
import { useAuthStore } from '../store/authStore';

interface ProtectedRouteProps {
  children: React.ReactNode;
  requiredPermission?: Permission;
}

export function ProtectedRoute({
  children,
  requiredPermission
}: ProtectedRouteProps) {
  const location = useLocation();
  const { isAuthenticated, hasPermission } = useAuthStore();

  // Check if auth is enabled (from config)
  const authEnabled = import.meta.env.VITE_AUTH_ENABLED === 'true';

  if (!authEnabled) {
    return <>{children}</>;
  }

  if (!isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  if (requiredPermission && !hasPermission(requiredPermission)) {
    return <Navigate to="/dashboard" replace />;
  }

  return <>{children}</>;
}
```

---

## Form Handling

### Zod Schemas

```typescript
// src/shared/utils/validation.ts
import { z } from 'zod';

export const orderSchema = z.object({
  side: z.enum(['BUY', 'SELL']),
  symbol: z.string().min(1, 'Symbol is required'),
  qty: z.number().positive('Quantity must be positive'),
  price: z.number().positive('Price must be positive'),
  client_order_id: z.string().optional(),
});

export type OrderFormData = z.infer<typeof orderSchema>;

export const loginSchema = z.object({
  user_id: z.string().min(1, 'User ID is required'),
  password: z.string().min(1, 'Password is required'),
});

export type LoginFormData = z.infer<typeof loginSchema>;

export const backtestConfigSchema = z.object({
  strategy_name: z.string().min(1, 'Strategy is required'),
  symbol: z.string().min(1, 'Symbol is required'),
  start_time: z.number().positive(),
  end_time: z.number().positive(),
  initial_balance: z.number().positive('Initial balance must be positive'),
  risk_per_trade: z.number().min(0).max(1, 'Risk must be between 0 and 1'),
  max_position_size: z.number().positive(),
  data_source: z.enum(['csv', 'binance']),
  data_type: z.enum(['kline', 'trade', 'book']),
  time_frame: z.enum(['1m', '5m', '15m', '1h', '4h', '1d']),
  strategy_parameters: z.record(z.number()),
}).refine(
  (data) => data.end_time > data.start_time,
  { message: 'End time must be after start time', path: ['end_time'] }
);

export type BacktestConfigFormData = z.infer<typeof backtestConfigSchema>;
```

### Order Form Component

```typescript
// src/features/trading/components/OrderForm.tsx
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { orderSchema, type OrderFormData } from '@/shared/utils/validation';
import { usePlaceOrder } from '../api/orderQueries';
import { useMarketStore } from '@/features/market/store/marketStore';
import { Button } from '@/shared/components/Button';
import { Input } from '@/shared/components/Input';
import { Select } from '@/shared/components/Select';

export function OrderForm() {
  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const currentPrice = useMarketStore(
    (state) => state.prices[state.selectedSymbol]?.price
  );

  const { mutate: placeOrder, isPending } = usePlaceOrder();

  const {
    register,
    handleSubmit,
    formState: { errors },
    setValue,
    reset,
  } = useForm<OrderFormData>({
    resolver: zodResolver(orderSchema),
    defaultValues: {
      symbol: selectedSymbol,
      side: 'BUY',
      qty: 0,
      price: currentPrice ?? 0,
    },
  });

  const onSubmit = (data: OrderFormData) => {
    placeOrder(data, {
      onSuccess: () => {
        reset();
      },
    });
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <div>
          <label className="block text-sm font-medium text-gray-700">
            Side
          </label>
          <Select {...register('side')}>
            <option value="BUY">Buy</option>
            <option value="SELL">Sell</option>
          </Select>
          {errors.side && (
            <p className="mt-1 text-sm text-red-600">{errors.side.message}</p>
          )}
        </div>

        <div>
          <label className="block text-sm font-medium text-gray-700">
            Symbol
          </label>
          <Input {...register('symbol')} readOnly />
          {errors.symbol && (
            <p className="mt-1 text-sm text-red-600">{errors.symbol.message}</p>
          )}
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4">
        <div>
          <label className="block text-sm font-medium text-gray-700">
            Quantity
          </label>
          <Input
            type="number"
            step="0.0001"
            {...register('qty', { valueAsNumber: true })}
          />
          {errors.qty && (
            <p className="mt-1 text-sm text-red-600">{errors.qty.message}</p>
          )}
        </div>

        <div>
          <label className="block text-sm font-medium text-gray-700">
            Price
          </label>
          <Input
            type="number"
            step="0.01"
            {...register('price', { valueAsNumber: true })}
          />
          {errors.price && (
            <p className="mt-1 text-sm text-red-600">{errors.price.message}</p>
          )}
        </div>
      </div>

      <Button type="submit" disabled={isPending} className="w-full">
        {isPending ? 'Placing Order...' : 'Place Order'}
      </Button>
    </form>
  );
}
```

---

## Chart Library

### Recharts for Equity Curves

```typescript
// src/features/backtest/components/EquityCurve.tsx
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
} from 'recharts';
import type { EquityCurvePoint } from '@/shared/types/api';

interface EquityCurveProps {
  data: EquityCurvePoint[];
  initialBalance: number;
}

export function EquityCurve({ data, initialBalance }: EquityCurveProps) {
  const formattedData = data.map((point) => ({
    ...point,
    date: new Date(point.timestamp).toLocaleDateString(),
    returnPct: (point.cumulative_return * 100).toFixed(2),
  }));

  return (
    <div className="h-80">
      <ResponsiveContainer width="100%" height="100%">
        <LineChart data={formattedData}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis
            dataKey="date"
            tick={{ fontSize: 12 }}
            interval="preserveStartEnd"
          />
          <YAxis
            tickFormatter={(value) => `$${value.toLocaleString()}`}
            domain={['dataMin - 1000', 'dataMax + 1000']}
          />
          <Tooltip
            formatter={(value: number) => [`$${value.toLocaleString()}`, 'Equity']}
            labelFormatter={(label) => `Date: ${label}`}
          />
          <ReferenceLine
            y={initialBalance}
            stroke="#888"
            strokeDasharray="5 5"
            label="Initial"
          />
          <Line
            type="monotone"
            dataKey="equity"
            stroke="#2563eb"
            strokeWidth={2}
            dot={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
```

### Lightweight Charts for Candlesticks

```typescript
// src/features/market/components/CandlestickChart.tsx
import { useEffect, useRef } from 'react';
import { createChart, IChartApi, ISeriesApi } from 'lightweight-charts';

interface CandleData {
  time: number;
  open: number;
  high: number;
  low: number;
  close: number;
}

interface CandlestickChartProps {
  data: CandleData[];
  width?: number;
  height?: number;
}

export function CandlestickChart({
  data,
  width = 800,
  height = 400
}: CandlestickChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    chartRef.current = createChart(containerRef.current, {
      width,
      height,
      layout: {
        background: { color: '#ffffff' },
        textColor: '#333',
      },
      grid: {
        vertLines: { color: '#e0e0e0' },
        horzLines: { color: '#e0e0e0' },
      },
      crosshair: {
        mode: 1, // Normal
      },
      rightPriceScale: {
        borderColor: '#cccccc',
      },
      timeScale: {
        borderColor: '#cccccc',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    seriesRef.current = chartRef.current.addCandlestickSeries({
      upColor: '#26a69a',
      downColor: '#ef5350',
      borderVisible: false,
      wickUpColor: '#26a69a',
      wickDownColor: '#ef5350',
    });

    return () => {
      chartRef.current?.remove();
    };
  }, [width, height]);

  useEffect(() => {
    if (seriesRef.current && data.length > 0) {
      seriesRef.current.setData(
        data.map((d) => ({
          time: d.time as any, // lightweight-charts expects UTCTimestamp
          open: d.open,
          high: d.high,
          low: d.low,
          close: d.close,
        }))
      );
    }
  }, [data]);

  return <div ref={containerRef} />;
}
```

---

## Authentication Flow

### Auth Flow Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                        Authentication Flow                        │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. User Login                                                    │
│  ┌─────────┐    POST /api/auth/login    ┌─────────┐              │
│  │  Login  │ ─────────────────────────> │ Gateway │              │
│  │  Form   │ <───────────────────────── │         │              │
│  └─────────┘    { access_token,         └─────────┘              │
│                   refresh_token }                                 │
│                                                                   │
│  2. Store Tokens (Zustand + localStorage)                        │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │  authStore.setTokens(access_token, refresh_token)           │ │
│  │  → Persisted to localStorage via zustand/persist            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                   │
│  3. API Requests                                                  │
│  ┌─────────┐    Authorization: Bearer <token>    ┌─────────┐    │
│  │   API   │ ──────────────────────────────────> │ Gateway │    │
│  │  Client │ <────────────────────────────────── │         │    │
│  └─────────┘                                     └─────────┘    │
│                                                                   │
│  4. Token Refresh (on 401)                                       │
│  ┌─────────┐    POST /api/auth/refresh    ┌─────────┐           │
│  │   API   │ ───────────────────────────> │ Gateway │           │
│  │  Client │ <─────────────────────────── │         │           │
│  └─────────┘    { access_token }          └─────────┘           │
│                                                                   │
│  5. Logout                                                        │
│  ┌─────────┐    POST /api/auth/logout     ┌─────────┐           │
│  │  Logout │ ───────────────────────────> │ Gateway │           │
│  │  Button │    { refresh_token }         │         │           │
│  └─────────┘                              └─────────┘           │
│       │                                                          │
│       └─> authStore.logout() → Clear tokens & redirect           │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Auth Provider

```typescript
// src/app/providers/AuthProvider.tsx
import { useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuthStore } from '@/features/auth/store/authStore';
import { apiClient } from '@/shared/api/client';

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const { accessToken, setUser, logout } = useAuthStore();
  const navigate = useNavigate();

  useEffect(() => {
    // Validate token on mount
    if (accessToken) {
      validateToken();
    }
  }, []);

  const validateToken = async () => {
    try {
      // Attempt to fetch user info or config to validate token
      const config = await apiClient.get('/api/config');
      // Token is valid
    } catch (error) {
      // Token invalid, logout
      logout();
      navigate('/login');
    }
  };

  return <>{children}</>;
}
```

---

## Error Handling

### Error Boundary

```typescript
// src/shared/components/ErrorBoundary/ErrorBoundary.tsx
import { Component, ErrorInfo, ReactNode } from 'react';

interface Props {
  children: ReactNode;
  fallback?: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
}

export class ErrorBoundary extends Component<Props, State> {
  public state: State = {
    hasError: false,
    error: null,
  };

  public static getDerivedStateFromError(error: Error): State {
    return { hasError: true, error };
  }

  public componentDidCatch(error: Error, errorInfo: ErrorInfo) {
    console.error('Uncaught error:', error, errorInfo);
    // Send to error tracking service
  }

  public render() {
    if (this.state.hasError) {
      return this.props.fallback || (
        <div className="flex flex-col items-center justify-center min-h-screen">
          <h1 className="text-2xl font-bold text-red-600">Something went wrong</h1>
          <p className="mt-2 text-gray-600">{this.state.error?.message}</p>
          <button
            onClick={() => window.location.reload()}
            className="mt-4 px-4 py-2 bg-blue-600 text-white rounded"
          >
            Reload Page
          </button>
        </div>
      );
    }

    return this.props.children;
  }
}
```

### Global Error Handler

```typescript
// src/shared/hooks/useGlobalError.ts
import { useEffect } from 'react';
import { useToast } from '@/shared/components/Toast';

export function useGlobalError() {
  const { showToast } = useToast();

  useEffect(() => {
    const handleError = (event: ErrorEvent) => {
      showToast({
        type: 'error',
        message: event.message || 'An unexpected error occurred',
      });
    };

    const handleUnhandledRejection = (event: PromiseRejectionEvent) => {
      showToast({
        type: 'error',
        message: event.reason?.message || 'An unexpected error occurred',
      });
    };

    window.addEventListener('error', handleError);
    window.addEventListener('unhandledrejection', handleUnhandledRejection);

    return () => {
      window.removeEventListener('error', handleError);
      window.removeEventListener('unhandledrejection', handleUnhandledRejection);
    };
  }, [showToast]);
}
```

---

## Performance Optimization

### Code Splitting Strategy

```typescript
// Lazy load feature modules
const Dashboard = lazy(() => import('@/features/dashboard'));
const Trading = lazy(() => import('@/features/trading'));
const Backtest = lazy(() => import('@/features/backtest'));

// Preload on hover for faster navigation
const preloadDashboard = () => import('@/features/dashboard');
const preloadTrading = () => import('@/features/trading');
```

### React.memo and useMemo Patterns

```typescript
// Memoize expensive components
export const OrderList = memo(function OrderList({ orders }: Props) {
  return (
    <table>
      {orders.map((order) => (
        <OrderRow key={order.client_order_id} order={order} />
      ))}
    </table>
  );
});

// Memoize expensive calculations
function useFilteredOrders(orders: OrderState[], filter: string) {
  return useMemo(() => {
    if (!filter) return orders;
    return orders.filter((o) =>
      o.symbol.includes(filter) || o.status.includes(filter)
    );
  }, [orders, filter]);
}
```

### Virtual Scrolling for Large Lists

```typescript
// Use @tanstack/react-virtual for large order lists
import { useVirtualizer } from '@tanstack/react-virtual';

function VirtualOrderList({ orders }: { orders: OrderState[] }) {
  const parentRef = useRef<HTMLDivElement>(null);

  const virtualizer = useVirtualizer({
    count: orders.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 48,
    overscan: 5,
  });

  return (
    <div ref={parentRef} className="h-96 overflow-auto">
      <div
        style={{
          height: `${virtualizer.getTotalSize()}px`,
          position: 'relative',
        }}
      >
        {virtualizer.getVirtualItems().map((virtualItem) => (
          <div
            key={virtualItem.key}
            style={{
              position: 'absolute',
              top: 0,
              left: 0,
              width: '100%',
              height: `${virtualItem.size}px`,
              transform: `translateY(${virtualItem.start}px)`,
            }}
          >
            <OrderRow order={orders[virtualItem.index]} />
          </div>
        ))}
      </div>
    </div>
  );
}
```

---

## Build and Deployment

### Vite Configuration

```typescript
// vite.config.ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  build: {
    target: 'es2020',
    outDir: 'dist',
    sourcemap: true,
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: ['react', 'react-dom', 'react-router-dom'],
          charts: ['recharts', 'lightweight-charts'],
          query: ['@tanstack/react-query'],
        },
      },
    },
  },
  server: {
    port: 3000,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
    },
  },
});
```

### TypeScript Configuration

```json
// tsconfig.json
{
  "compilerOptions": {
    "target": "ES2020",
    "useDefineForClassFields": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,

    /* Bundler mode */
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",

    /* Linting */
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,

    /* Paths */
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"]
    }
  },
  "include": ["src"],
  "references": [{ "path": "./tsconfig.node.json" }]
}
```

### ESLint Configuration

```javascript
// .eslintrc.cjs
module.exports = {
  root: true,
  env: { browser: true, es2020: true },
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
    'plugin:react-hooks/recommended',
    'plugin:react/recommended',
    'plugin:react/jsx-runtime',
    'prettier',
  ],
  ignorePatterns: ['dist', '.eslintrc.cjs'],
  parser: '@typescript-eslint/parser',
  plugins: ['react-refresh'],
  rules: {
    'react-refresh/only-export-components': [
      'warn',
      { allowConstantExport: true },
    ],
    '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_' }],
    '@typescript-eslint/explicit-function-return-type': 'off',
    '@typescript-eslint/no-explicit-any': 'warn',
  },
  settings: {
    react: {
      version: 'detect',
    },
  },
};
```

### Package.json Scripts

```json
{
  "name": "veloz-ui",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc && vite build",
    "preview": "vite preview",
    "lint": "eslint . --ext ts,tsx --report-unused-disable-directives --max-warnings 0",
    "lint:fix": "eslint . --ext ts,tsx --fix",
    "format": "prettier --write \"src/**/*.{ts,tsx,css}\"",
    "format:check": "prettier --check \"src/**/*.{ts,tsx,css}\"",
    "test": "vitest run",
    "test:watch": "vitest",
    "test:coverage": "vitest run --coverage",
    "test:e2e": "playwright test",
    "typecheck": "tsc --noEmit",
    "prepare": "husky install"
  }
}
```

---

## Coding Standards

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Components | PascalCase | `OrderForm.tsx` |
| Hooks | camelCase with `use` prefix | `useOrders.ts` |
| Utilities | camelCase | `formatPrice.ts` |
| Types/Interfaces | PascalCase | `OrderState` |
| Constants | SCREAMING_SNAKE_CASE | `API_BASE_URL` |
| CSS Classes | kebab-case | `order-form-container` |
| Files | kebab-case or PascalCase for components | `order-queries.ts` |

### Component Structure

```typescript
// Standard component structure
import { useState, useEffect } from 'react';
import { useQuery } from '@tanstack/react-query';

// 1. Type definitions
interface Props {
  orderId: string;
  onCancel?: () => void;
}

// 2. Component
export function OrderDetail({ orderId, onCancel }: Props) {
  // 3. Hooks (in order: state, queries, effects)
  const [isExpanded, setIsExpanded] = useState(false);
  const { data, isLoading } = useOrderDetail(orderId);

  useEffect(() => {
    // Side effects
  }, [orderId]);

  // 4. Event handlers
  const handleCancel = () => {
    onCancel?.();
  };

  // 5. Early returns
  if (isLoading) return <Spinner />;
  if (!data) return null;

  // 6. Render
  return (
    <div className="order-detail">
      {/* JSX */}
    </div>
  );
}
```

### Import Order

```typescript
// 1. React/external libraries
import { useState, useEffect } from 'react';
import { useQuery } from '@tanstack/react-query';

// 2. Internal shared modules
import { Button } from '@/shared/components/Button';
import { formatPrice } from '@/shared/utils/format';

// 3. Feature-specific imports
import { useOrders } from '../api/orderQueries';
import { OrderRow } from './OrderRow';

// 4. Types
import type { OrderState } from '@/shared/types/api';

// 5. Styles (if any)
import './OrderList.css';
```

---

## Testing Strategy

### Unit Tests (Vitest)

```typescript
// src/features/trading/components/OrderForm.test.tsx
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { OrderForm } from './OrderForm';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: { retry: false },
  },
});

const wrapper = ({ children }: { children: React.ReactNode }) => (
  <QueryClientProvider client={queryClient}>
    {children}
  </QueryClientProvider>
);

describe('OrderForm', () => {
  it('renders all form fields', () => {
    render(<OrderForm />, { wrapper });

    expect(screen.getByLabelText(/side/i)).toBeInTheDocument();
    expect(screen.getByLabelText(/symbol/i)).toBeInTheDocument();
    expect(screen.getByLabelText(/quantity/i)).toBeInTheDocument();
    expect(screen.getByLabelText(/price/i)).toBeInTheDocument();
  });

  it('validates required fields', async () => {
    render(<OrderForm />, { wrapper });

    await userEvent.click(screen.getByRole('button', { name: /place order/i }));

    await waitFor(() => {
      expect(screen.getByText(/quantity must be positive/i)).toBeInTheDocument();
    });
  });

  it('submits valid order', async () => {
    render(<OrderForm />, { wrapper });

    await userEvent.type(screen.getByLabelText(/quantity/i), '0.001');
    await userEvent.type(screen.getByLabelText(/price/i), '50000');
    await userEvent.click(screen.getByRole('button', { name: /place order/i }));

    // Assert mutation was called
  });
});
```

### MSW Handlers

```typescript
// tests/mocks/handlers.ts
import { http, HttpResponse } from 'msw';

export const handlers = [
  http.get('/api/orders_state', () => {
    return HttpResponse.json({
      items: [
        {
          client_order_id: 'test-001',
          symbol: 'BTCUSDT',
          side: 'BUY',
          order_qty: 0.001,
          limit_price: 50000,
          status: 'FILLED',
          executed_qty: 0.001,
          avg_price: 50000,
          last_ts_ns: Date.now() * 1000000,
        },
      ],
    });
  }),

  http.post('/api/order', async ({ request }) => {
    const body = await request.json();
    return HttpResponse.json({
      ok: true,
      client_order_id: `web-${Date.now()}`,
    });
  }),

  http.get('/api/market', () => {
    return HttpResponse.json({
      symbol: 'BTCUSDT',
      price: 50000,
      ts_ns: Date.now() * 1000000,
    });
  }),
];
```

### E2E Tests (Playwright)

```typescript
// tests/e2e/trading.spec.ts
import { test, expect } from '@playwright/test';

test.describe('Trading', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/trading');
  });

  test('displays order form', async ({ page }) => {
    await expect(page.getByRole('heading', { name: /place order/i })).toBeVisible();
    await expect(page.getByLabel(/side/i)).toBeVisible();
    await expect(page.getByLabel(/quantity/i)).toBeVisible();
  });

  test('places a buy order', async ({ page }) => {
    await page.getByLabel(/quantity/i).fill('0.001');
    await page.getByLabel(/price/i).fill('50000');
    await page.getByRole('button', { name: /place order/i }).click();

    await expect(page.getByText(/order placed/i)).toBeVisible();
  });

  test('displays order list', async ({ page }) => {
    await expect(page.getByRole('table')).toBeVisible();
    await expect(page.getByText('BTCUSDT')).toBeVisible();
  });
});
```

---

## Summary

This architecture provides:

1. **Type Safety**: Full TypeScript with strict mode and Zod validation
2. **Scalability**: Feature-based organization for easy team collaboration
3. **Performance**: Code splitting, lazy loading, virtual scrolling
4. **Real-time**: Robust SSE handling with automatic reconnection
5. **Testing**: Comprehensive unit, integration, and E2E testing
6. **Developer Experience**: Fast builds, HMR, excellent tooling

The architecture is designed to evolve with the VeloZ platform while maintaining clean separation of concerns and high code quality standards.
