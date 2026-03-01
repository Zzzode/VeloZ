# VeloZ UI Architecture Design

## Executive Summary

This document outlines a comprehensive UI architecture for the VeloZ trading platform, designed to support all implemented backend features while providing a modern, production-grade user experience.

---

## 1. Current State Analysis

### 1.1 Existing Implementation

The current UI (`apps/ui/`) is a static HTML/CSS/JS application with:

**Strengths:**
- Modular JavaScript architecture (orderbook.js, positions.js, strategies.js, backtest.js)
- Modular CSS with component-specific stylesheets
- Real-time SSE integration for live updates
- Clean, minimal design with CSS custom properties
- Responsive layout foundations

**Limitations:**
- No authentication UI (JWT login, token refresh)
- No RBAC-aware component rendering
- No audit log viewer (admin feature)
- Limited state management (no centralized store)
- No TypeScript type safety
- No component library (custom CSS only)
- No dark mode toggle (only CSS media query)
- No comprehensive error handling UI
- No loading states for async operations

### 1.2 Backend Features Requiring UI Support

| Feature | Backend Status | Current UI Status |
|---------|---------------|-------------------|
| JWT Authentication | Implemented | Not implemented |
| Token Refresh | Implemented | Not implemented |
| RBAC (viewer/trader/admin) | Implemented | Not implemented |
| Audit Log Query | Implemented | Not implemented |
| Order Book Display | Implemented | Implemented |
| Position Tracking | Implemented | Implemented |
| Strategy Management | Implemented | Implemented |
| Backtest Visualization | Implemented | Implemented |
| Order Placement | Implemented | Implemented |
| Account/Balance View | Implemented | Implemented |
| Rate Limiting Display | Implemented | Not implemented |
| WebSocket Market Data | Implemented | Partial (SSE only) |

---

## 2. Proposed Architecture

### 2.1 Technology Stack

| Layer | Technology | Justification |
|-------|------------|---------------|
| **Language** | TypeScript 5.x | Type safety, better DX, catch errors at compile time |
| **Framework** | React 18.x | Industry standard, large ecosystem, concurrent features |
| **Build Tool** | Vite 5.x | Fast HMR, native ESM, excellent TypeScript support |
| **Component Library** | Ant Design 5.x | See Section 2.2 for detailed comparison |
| **State Management** | Zustand + React Query | Lightweight, TypeScript-first, server state handling |
| **Styling** | CSS Modules + Ant Design tokens | Scoped styles, design system integration |
| **Charts** | Recharts or Lightweight Charts | Trading-specific, performant |
| **Testing** | Vitest + React Testing Library | Fast, compatible with existing setup |

### 2.2 Component Library Comparison

| Criteria | Ant Design | Material UI | Chakra UI |
|----------|------------|-------------|-----------|
| **Trading UI Fit** | Excellent (data tables, forms) | Good | Good |
| **Data Table Component** | Pro Table (advanced) | DataGrid (paid) | Basic |
| **Form Handling** | Built-in Form component | Manual | Basic |
| **Bundle Size** | Tree-shakeable | Large | Medium |
| **TypeScript Support** | Excellent | Excellent | Good |
| **Theme Customization** | Design tokens | Theme provider | Theme tokens |
| **Enterprise Features** | Yes (Pro components) | Limited | No |
| **Documentation** | Comprehensive | Comprehensive | Good |

**Recommendation: Ant Design 5.x**

Rationale:
1. **Pro Table**: Advanced data table with sorting, filtering, pagination - essential for order lists, audit logs, trade history
2. **Form Component**: Built-in validation, layout, error handling - critical for order forms, strategy configuration
3. **Design Tokens**: Easy theming for dark/light mode
4. **Enterprise-Ready**: Proven in financial/trading applications
5. **Tree-Shakeable**: Import only what you use

### 2.3 Application Structure

```
apps/ui/
├── public/
│   └── favicon.ico
├── src/
│   ├── main.tsx                    # Application entry point
│   ├── App.tsx                     # Root component with routing
│   ├── vite-env.d.ts
│   │
│   ├── api/                        # API client layer
│   │   ├── client.ts               # Axios/fetch wrapper with auth
│   │   ├── auth.ts                 # Authentication endpoints
│   │   ├── market.ts               # Market data endpoints
│   │   ├── orders.ts               # Order management endpoints
│   │   ├── positions.ts            # Position endpoints
│   │   ├── strategies.ts           # Strategy endpoints
│   │   ├── backtest.ts             # Backtest endpoints
│   │   ├── audit.ts                # Audit log endpoints (admin)
│   │   └── types.ts                # API response types
│   │
│   ├── components/                 # Reusable UI components
│   │   ├── common/                 # Generic components
│   │   │   ├── LoadingSpinner.tsx
│   │   │   ├── ErrorBoundary.tsx
│   │   │   ├── ProtectedRoute.tsx
│   │   │   └── RoleGate.tsx        # RBAC-aware rendering
│   │   │
│   │   ├── layout/                 # Layout components
│   │   │   ├── AppLayout.tsx       # Main app shell
│   │   │   ├── Sidebar.tsx
│   │   │   ├── Header.tsx
│   │   │   └── Footer.tsx
│   │   │
│   │   ├── auth/                   # Authentication components
│   │   │   ├── LoginForm.tsx
│   │   │   ├── LogoutButton.tsx
│   │   │   └── SessionExpiredModal.tsx
│   │   │
│   │   ├── trading/                # Trading components
│   │   │   ├── OrderBook/
│   │   │   │   ├── OrderBook.tsx
│   │   │   │   ├── PriceLevel.tsx
│   │   │   │   ├── SpreadDisplay.tsx
│   │   │   │   └── DepthChart.tsx
│   │   │   ├── OrderForm/
│   │   │   │   ├── OrderForm.tsx
│   │   │   │   ├── OrderTypeSelector.tsx
│   │   │   │   └── OrderConfirmModal.tsx
│   │   │   ├── OrderList/
│   │   │   │   ├── OrderList.tsx
│   │   │   │   ├── OrderRow.tsx
│   │   │   │   └── OrderActions.tsx
│   │   │   └── TradeHistory/
│   │   │       └── TradeHistory.tsx
│   │   │
│   │   ├── positions/              # Position components
│   │   │   ├── PositionTable.tsx
│   │   │   ├── PositionCard.tsx
│   │   │   ├── PnLSummary.tsx
│   │   │   └── PnLChart.tsx
│   │   │
│   │   ├── strategies/             # Strategy components
│   │   │   ├── StrategyGrid.tsx
│   │   │   ├── StrategyCard.tsx
│   │   │   ├── StrategyForm.tsx
│   │   │   ├── StrategyMetrics.tsx
│   │   │   └── ParameterEditor.tsx
│   │   │
│   │   ├── backtest/               # Backtest components
│   │   │   ├── BacktestForm.tsx
│   │   │   ├── BacktestResults.tsx
│   │   │   ├── EquityCurve.tsx
│   │   │   ├── DrawdownChart.tsx
│   │   │   └── TradeAnalysis.tsx
│   │   │
│   │   ├── account/                # Account components
│   │   │   ├── BalanceTable.tsx
│   │   │   ├── AccountSummary.tsx
│   │   │   └── APIKeyManager.tsx
│   │   │
│   │   └── admin/                  # Admin-only components
│   │       ├── AuditLogViewer.tsx
│   │       ├── AuditLogFilters.tsx
│   │       ├── UserManagement.tsx
│   │       └── SystemMonitor.tsx
│   │
│   ├── hooks/                      # Custom React hooks
│   │   ├── useAuth.ts              # Authentication state
│   │   ├── usePermissions.ts       # RBAC permission checks
│   │   ├── useSSE.ts               # Server-Sent Events
│   │   ├── useWebSocket.ts         # WebSocket connection
│   │   ├── useOrderBook.ts         # Order book state
│   │   ├── usePositions.ts         # Position state
│   │   ├── useStrategies.ts        # Strategy state
│   │   └── useTheme.ts             # Theme toggle
│   │
│   ├── stores/                     # Zustand stores
│   │   ├── authStore.ts            # Auth state (tokens, user)
│   │   ├── marketStore.ts          # Market data state
│   │   ├── orderStore.ts           # Order state
│   │   ├── positionStore.ts        # Position state
│   │   ├── strategyStore.ts        # Strategy state
│   │   └── uiStore.ts              # UI state (theme, sidebar)
│   │
│   ├── pages/                      # Page components (routes)
│   │   ├── LoginPage.tsx
│   │   ├── DashboardPage.tsx
│   │   ├── TradingPage.tsx
│   │   ├── PositionsPage.tsx
│   │   ├── StrategiesPage.tsx
│   │   ├── BacktestPage.tsx
│   │   ├── AccountPage.tsx
│   │   ├── AuditPage.tsx           # Admin only
│   │   └── NotFoundPage.tsx
│   │
│   ├── styles/                     # Global styles
│   │   ├── variables.css           # CSS custom properties
│   │   ├── global.css              # Global styles
│   │   └── theme.ts                # Ant Design theme config
│   │
│   ├── utils/                      # Utility functions
│   │   ├── format.ts               # Number/date formatting
│   │   ├── validation.ts           # Form validation
│   │   ├── storage.ts              # LocalStorage helpers
│   │   └── constants.ts            # App constants
│   │
│   └── types/                      # TypeScript types
│       ├── auth.ts
│       ├── market.ts
│       ├── order.ts
│       ├── position.ts
│       ├── strategy.ts
│       ├── backtest.ts
│       └── audit.ts
│
├── index.html
├── package.json
├── tsconfig.json
├── vite.config.ts
└── vitest.config.ts
```

---

## 3. Component Hierarchy

### 3.1 Application Shell

```
<App>
  <ConfigProvider theme={theme}>
    <AuthProvider>
      <Router>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route element={<ProtectedRoute />}>
            <Route element={<AppLayout />}>
              <Route path="/" element={<DashboardPage />} />
              <Route path="/trading" element={<TradingPage />} />
              <Route path="/positions" element={<PositionsPage />} />
              <Route path="/strategies" element={<StrategiesPage />} />
              <Route path="/backtest" element={<BacktestPage />} />
              <Route path="/account" element={<AccountPage />} />
              <Route element={<RoleGate roles={['admin']} />}>
                <Route path="/audit" element={<AuditPage />} />
              </Route>
            </Route>
          </Route>
        </Routes>
      </Router>
    </AuthProvider>
  </ConfigProvider>
</App>
```

### 3.2 AppLayout Structure

```
<AppLayout>
  <Layout>
    <Sider collapsible>
      <Logo />
      <Navigation />
      <UserMenu />
    </Sider>
    <Layout>
      <Header>
        <ConnectionStatus />
        <ThemeToggle />
        <NotificationBell />
        <UserAvatar />
      </Header>
      <Content>
        <Outlet />  {/* Page content */}
      </Content>
      <Footer>
        <SystemStatus />
        <Version />
      </Footer>
    </Layout>
  </Layout>
</AppLayout>
```

### 3.3 Page Component Hierarchy

#### TradingPage
```
<TradingPage>
  <Row gutter={16}>
    <Col span={8}>
      <OrderBook symbol={symbol} />
    </Col>
    <Col span={8}>
      <OrderForm onSubmit={placeOrder} />
      <RecentTrades />
    </Col>
    <Col span={8}>
      <MarketStats />
      <PositionSummary />
    </Col>
  </Row>
  <Row>
    <Col span={24}>
      <Tabs>
        <TabPane tab="Open Orders">
          <OrderList filter="open" />
        </TabPane>
        <TabPane tab="Order History">
          <OrderList filter="all" />
        </TabPane>
        <TabPane tab="Trade History">
          <TradeHistory />
        </TabPane>
      </Tabs>
    </Col>
  </Row>
</TradingPage>
```

#### StrategiesPage
```
<StrategiesPage>
  <PageHeader
    title="Strategy Management"
    extra={<Button onClick={openCreateModal}>+ New Strategy</Button>}
  />
  <Row gutter={16}>
    <Col span={24}>
      <PerformanceSummary />
    </Col>
  </Row>
  <Row gutter={16}>
    <Col span={24}>
      <StrategyGrid>
        {strategies.map(s => (
          <StrategyCard
            key={s.id}
            strategy={s}
            onStart={startStrategy}
            onStop={stopStrategy}
            onEdit={openEditModal}
          />
        ))}
      </StrategyGrid>
    </Col>
  </Row>
  <StrategyFormModal />
</StrategiesPage>
```

#### AuditPage (Admin Only)
```
<AuditPage>
  <RoleGate roles={['admin']} fallback={<AccessDenied />}>
    <PageHeader title="Audit Logs" />
    <Card>
      <AuditLogFilters
        onFilter={setFilters}
        logTypes={['auth', 'order', 'api_key', 'error', 'access']}
      />
    </Card>
    <Card>
      <AuditLogTable
        data={logs}
        loading={loading}
        pagination={pagination}
        onPageChange={fetchLogs}
      />
    </Card>
    <Card>
      <AuditStats stats={stats} />
    </Card>
  </RoleGate>
</AuditPage>
```

---

## 4. State Management

### 4.1 Store Architecture

```typescript
// authStore.ts
interface AuthState {
  user: User | null;
  accessToken: string | null;
  refreshToken: string | null;
  permissions: Permission[];
  isAuthenticated: boolean;
  isLoading: boolean;

  // Actions
  login: (credentials: LoginCredentials) => Promise<void>;
  logout: () => Promise<void>;
  refreshAccessToken: () => Promise<void>;
  hasPermission: (permission: Permission) => boolean;
}

// marketStore.ts
interface MarketState {
  orderBook: OrderBook;
  ticker: Ticker | null;
  trades: Trade[];
  connectionStatus: 'connected' | 'connecting' | 'disconnected';

  // Actions
  subscribeSymbol: (symbol: string) => void;
  unsubscribeSymbol: (symbol: string) => void;
  updateOrderBook: (data: OrderBookUpdate) => void;
}

// orderStore.ts
interface OrderState {
  openOrders: Order[];
  orderHistory: Order[];
  isSubmitting: boolean;

  // Actions
  placeOrder: (order: NewOrder) => Promise<OrderResult>;
  cancelOrder: (clientOrderId: string) => Promise<void>;
  fetchOrders: () => Promise<void>;
}
```

### 4.2 React Query Integration

```typescript
// queries/useOrders.ts
export function useOpenOrders() {
  return useQuery({
    queryKey: ['orders', 'open'],
    queryFn: () => api.orders.getOpenOrders(),
    refetchInterval: 5000,
  });
}

export function usePlaceOrder() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (order: NewOrder) => api.orders.place(order),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['orders'] });
    },
  });
}
```

---

## 5. Real-Time Data Flow

### 5.1 SSE Integration

```typescript
// hooks/useSSE.ts
export function useSSE(eventTypes: string[]) {
  const { accessToken } = useAuthStore();
  const updateOrderBook = useMarketStore(s => s.updateOrderBook);
  const updatePositions = usePositionStore(s => s.updatePositions);

  useEffect(() => {
    const url = `${API_BASE}/api/stream?types=${eventTypes.join(',')}`;
    const es = new EventSource(url);

    es.addEventListener('depth', (e) => {
      updateOrderBook(JSON.parse(e.data));
    });

    es.addEventListener('position', (e) => {
      updatePositions(JSON.parse(e.data));
    });

    es.addEventListener('order_update', (e) => {
      // Handle order updates
    });

    return () => es.close();
  }, [eventTypes, accessToken]);
}
```

### 5.2 WebSocket Integration (Optional)

```typescript
// hooks/useWebSocket.ts
export function useMarketWebSocket(symbols: string[]) {
  const [ws, setWs] = useState<WebSocket | null>(null);

  useEffect(() => {
    const socket = new WebSocket(`${WS_BASE}/ws/market`);

    socket.onopen = () => {
      socket.send(JSON.stringify({
        action: 'subscribe',
        symbols,
        channels: ['trade', 'book_top'],
      }));
    };

    socket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      // Handle different message types
    };

    setWs(socket);
    return () => socket.close();
  }, [symbols]);

  return ws;
}
```

---

## 6. Authentication Flow

### 6.1 Login Flow

```
User enters credentials
       │
       ▼
  POST /api/auth/login
       │
       ├─── Success ───► Store tokens in memory
       │                        │
       │                        ▼
       │                 Redirect to Dashboard
       │
       └─── Failure ───► Show error message
```

### 6.2 Token Refresh Flow

```
API request returns 401
       │
       ▼
  Check if refresh token exists
       │
       ├─── Yes ───► POST /api/auth/refresh
       │                    │
       │                    ├─── Success ───► Update access token
       │                    │                        │
       │                    │                        ▼
       │                    │                 Retry original request
       │                    │
       │                    └─── Failure ───► Redirect to login
       │
       └─── No ───► Redirect to login
```

### 6.3 RBAC Component

```typescript
// components/common/RoleGate.tsx
interface RoleGateProps {
  roles: Role[];
  permissions?: Permission[];
  fallback?: React.ReactNode;
  children: React.ReactNode;
}

export function RoleGate({ roles, permissions, fallback, children }: RoleGateProps) {
  const { user, hasPermission } = useAuthStore();

  const hasRole = roles.some(role => user?.role === role);
  const hasRequiredPermissions = !permissions ||
    permissions.every(p => hasPermission(p));

  if (!hasRole || !hasRequiredPermissions) {
    return fallback ?? null;
  }

  return <>{children}</>;
}
```

---

## 7. Error Handling

### 7.1 Error Boundary

```typescript
// components/common/ErrorBoundary.tsx
export class ErrorBoundary extends React.Component<Props, State> {
  static getDerivedStateFromError(error: Error) {
    return { hasError: true, error };
  }

  render() {
    if (this.state.hasError) {
      return (
        <Result
          status="error"
          title="Something went wrong"
          subTitle={this.state.error?.message}
          extra={
            <Button onClick={() => window.location.reload()}>
              Reload Page
            </Button>
          }
        />
      );
    }
    return this.props.children;
  }
}
```

### 7.2 API Error Handling

```typescript
// api/client.ts
const apiClient = axios.create({
  baseURL: API_BASE,
});

apiClient.interceptors.response.use(
  (response) => response,
  async (error) => {
    if (error.response?.status === 401) {
      // Try token refresh
      const refreshed = await tryRefreshToken();
      if (refreshed) {
        return apiClient.request(error.config);
      }
      // Redirect to login
      window.location.href = '/login';
    }

    if (error.response?.status === 403) {
      notification.error({
        message: 'Access Denied',
        description: 'You do not have permission to perform this action.',
      });
    }

    if (error.response?.status === 429) {
      notification.warning({
        message: 'Rate Limited',
        description: 'Too many requests. Please wait before trying again.',
      });
    }

    throw error;
  }
);
```

---

## 8. Performance Considerations

### 8.1 Code Splitting

```typescript
// Lazy load pages
const TradingPage = lazy(() => import('./pages/TradingPage'));
const StrategiesPage = lazy(() => import('./pages/StrategiesPage'));
const BacktestPage = lazy(() => import('./pages/BacktestPage'));
const AuditPage = lazy(() => import('./pages/AuditPage'));
```

### 8.2 Memoization

```typescript
// Memoize expensive computations
const sortedOrders = useMemo(
  () => orders.sort((a, b) => b.timestamp - a.timestamp),
  [orders]
);

// Memoize callbacks
const handleOrderSubmit = useCallback(
  (order: NewOrder) => placeOrder(order),
  [placeOrder]
);
```

### 8.3 Virtual Lists

```typescript
// Use virtual list for large datasets
import { List } from 'react-virtualized';

<List
  width={width}
  height={height}
  rowCount={trades.length}
  rowHeight={40}
  rowRenderer={({ index, key, style }) => (
    <TradeRow key={key} trade={trades[index]} style={style} />
  )}
/>
```

---

## 9. Migration Strategy

### Phase 1: Setup (Week 1)
- Initialize Vite + React + TypeScript project
- Configure Ant Design with custom theme
- Set up folder structure
- Configure build and test tools

### Phase 2: Core Infrastructure (Week 2)
- Implement API client with auth interceptors
- Create Zustand stores
- Implement authentication flow
- Create layout components

### Phase 3: Trading Features (Weeks 3-4)
- Port order book component
- Port order form and list
- Port position display
- Implement real-time updates (SSE)

### Phase 4: Strategy & Backtest (Weeks 5-6)
- Port strategy management
- Port backtest visualization
- Add parameter editor
- Add equity curve charts

### Phase 5: Admin Features (Week 7)
- Implement audit log viewer
- Add API key management
- Add system monitoring

### Phase 6: Polish & Testing (Week 8)
- Comprehensive testing
- Performance optimization
- Accessibility audit
- Documentation

---

## 10. Next Steps

1. Review and approve this architecture document
2. Create detailed component specifications (see `02-component-specs.md`)
3. Define design system tokens (see `03-design-system.md`)
4. Create wireframes for all views (see `04-wireframes.md`)
5. Document user flows (see `05-user-flows.md`)
6. Define accessibility requirements (see `06-accessibility.md`)
