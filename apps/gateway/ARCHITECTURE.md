# C++ Gateway Architecture

**Version**: 1.0
**Last Updated**: 2026-02-27
**Author**: Gateway Migration Team

## Table of Contents

1. [Overview](#overview)
2. [Design Goals](#design-goals)
3. [Architecture Diagram](#architecture-diagram)
4. [Module Layout](#module-layout)
5. [Core Components](#core-components)
6. [Data Flow](#data-flow)
7. [Key Design Decisions](#key-design-decisions)
8. [Configuration](#configuration)
9. [Performance Targets](#performance-targets)
10. [Migration Strategy](#migration-strategy)

---

## Overview

The C++ Gateway replaces the Python Gateway (`apps/gateway/gateway.py`) for improved performance and type safety. It maintains API compatibility with the Python implementation while leveraging KJ library for asynchronous I/O and memory management.

### Target Metrics

- **Latency**: <100μs P50, <1ms P99 for request processing
- **Throughput**: >10,000 requests/second
- **Memory**: Stable memory footprint with arena allocation
- **Startup**: <100ms cold start time

---

## Design Goals

1. **Performance**: 10x improvement over Python Gateway
2. **Safety**: Type-safe request handling with KJ types
3. **Simplicity**: Single-threaded event loop architecture
4. **Compatibility**: Maintain identical API surface
5. **Maintainability**: Clear module boundaries and interfaces
6. **Testability**: Dependency injection for all components

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         HTTP Client                              │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                     kj::HttpService (KJ HTTP)                    │
│                   (gateway_server.cpp)                           │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                          Router                                  │
│                      (router.h/cpp)                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Middleware Chain:                                         │  │
│  │  1. MetricsMiddleware    → Record request timing          │  │
│  │  2. CorsMiddleware       → Add CORS headers               │  │
│  │  3. AuthMiddleware       → Validate JWT/API key           │  │
│  │  4. RateLimitMiddleware  → Token bucket rate limiting     │  │
│  │  5. AuditMiddleware      → Log request details            │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Handlers:                                                 │  │
│  │  • HealthHandler         → /health, /api/health           │  │
│  │  • MarketHandler         → /api/market                    │  │
│  │  • OrderHandler          → /api/order, /api/cancel        │  │
│  │  • OrderStateHandler     → /api/orders_state, /api/order_state │
│  │  • AccountHandler        → /api/account                   │  │
│  │  • ConfigHandler         → /api/config                    │  │
│  │  • AuthHandler           → /api/auth/*                    │  │
│  │  • SSEHandler            → /api/stream (Server-Sent Events)│  │
│  │  • MetricsHandler        → /metrics (Prometheus)          │  │
│  │  • AuditHandler          → /api/audit/*                   │  │
│  │  • StaticHandler         → Static file serving            │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Business Logic Layer                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │  EngineBridge    │  │  EventBroadcaster│  │  OrderStore    │ │
│  │  (bridge/)       │  │  (bridge/)       │  │  (state/)      │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │  AccountStore    │  │  ExecutionRouter │  │  MarketStore   │ │
│  │  (state/)        │  │  (exec/)         │  │  (state/)      │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Infrastructure Layer                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │  AuthManager     │  │  AuditLogger     │  │  Metrics       │ │
│  │  (auth/)         │  │  (audit/)        │  │  (metrics/)    │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │  ConfigManager   │  │  Keychain        │  │  Logger        │ │
│  │  (config/)       │  │  (keychain/)     │  │  (core/)       │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Engine Process (veloz_engine)                 │
│                    (Single process, same binary)                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Module Layout

```
apps/gateway_cpp/
├── src/
│   ├── main.cpp                      # Entry point, CLI flag parsing
│   ├── gateway_server.h/cpp          # kj::HttpService implementation
│   ├── router.h/cpp                  # HTTP routing and middleware chain
│   ├── request_context.h             # Per-request context (auth, metrics)
│   │
│   ├── middleware/
│   │   ├── middleware.h              # Base middleware interface
│   │   ├── auth_middleware.h/cpp     # JWT/API key validation
│   │   ├── rate_limit_middleware.h/cpp # Token bucket rate limiting
│   │   ├── cors_middleware.h/cpp     # CORS headers
│   │   ├── metrics_middleware.h/cpp  # Prometheus metrics collection
│   │   └── audit_middleware.h/cpp    # Audit logging
│   │
│   ├── handlers/
│   │   ├── handler.h                 # Base handler interface
│   │   ├── health_handler.h/cpp      # Health check endpoints
│   │   ├── market_handler.h/cpp      # Market data endpoints
│   │   ├── order_handler.h/cpp       # Order submission/cancellation
│   │   ├── order_state_handler.h/cpp # Order state queries
│   │   ├── account_handler.h/cpp     # Account state endpoints
│   │   ├── config_handler.h/cpp      # Configuration endpoints
│   │   ├── auth_handler.h/cpp        # Authentication endpoints
│   │   ├── sse_handler.h/cpp         # Server-Sent Events streaming
│   │   ├── metrics_handler.h/cpp     # Prometheus /metrics endpoint
│   │   ├── audit_handler.h/cpp       # Audit log query endpoints
│   │   └── static_handler.h/cpp      # Static file serving
│   │
│   ├── bridge/
│   │   ├── engine_bridge.h/cpp       # Engine communication (in-process)
│   │   ├── event_broadcaster.h/cpp   # Event distribution to SSE clients
│   │   └── execution_router.h/cpp    # Order routing (engine vs Binance)
│   │
│   ├── state/
│   │   ├── order_store.h/cpp         # Order state management
│   │   ├── account_store.h/cpp       # Account state management
│   │   └── market_store.h/cpp        # Market data cache
│   │
│   ├── auth/
│   │   ├── jwt_manager.h/cpp         # JWT token creation/validation
│   │   ├── api_key_manager.h/cpp     # API key management
│   │   ├── rbac.h/cpp                # Role-based access control
│   │   ├── permission_manager.h/cpp  # Permission checking
│   │   └── auth_manager.h/cpp        # Unified authentication
│   │
│   ├── audit/
│   │   ├── audit_logger.h/cpp        # Audit logging interface
│   │   ├── audit_store.h/cpp         # File-based log storage
│   │   └── retention_manager.h/cpp   # Log retention and cleanup
│   │
│   ├── metrics/
│   │   ├── metrics.h/cpp             # Prometheus metrics definitions
│   │   └── metrics_registry.h/cpp    # Metrics collection
│   │
│   ├── config/
│   │   ├── config_manager.h/cpp      # Configuration loading
│   │   └── gateway_config.h          # Gateway-specific config
│   │
│   ├── keychain/
│   │   └── keychain_manager.h/cpp    # Secure credential storage
│   │
│   └── util/
│       ├── json_utils.h/cpp          # JSON parsing utilities
│       ├── http_utils.h/cpp          # HTTP helpers
│       └── string_utils.h/cpp        # String utilities
│
├── tests/
│   ├── unit/                         # Unit tests
│   ├── integration/                  # Integration tests
│   └── benchmark/                    # Performance benchmarks
│
├── CMakeLists.txt
└── ARCHITECTURE.md                   # This document
```

---

## Core Components

### 1. GatewayServer (gateway_server.h/cpp)

**Purpose**: Main HTTP service implementation using KJ HTTP library.

**Responsibilities**:
- Implement `kj::HttpService` interface
- Create `kj::HttpHeaderTable` for efficient header handling
- Route requests to `Router`
- Manage server lifecycle

**Interface**:
```cpp
class GatewayServer final : public kj::HttpService {
public:
  explicit GatewayServer(const GatewayConfig& config);
  ~GatewayServer();

  kj::Promise<void> request(
    kj::HttpMethod method,
    kj::StringPtr url,
    const kj::HttpHeaders& headers,
    kj::AsyncInputStream& requestBody,
    Response& response
  ) override;

  // Lifecycle management
  kj::Promise<void> start(uint port);
  void shutdown();

private:
  kj::Own<Router> router_;
  kj::Own<AuthManager> auth_manager_;
  kj::Own<EngineBridge> engine_bridge_;
  kj::Own<EventBroadcaster> event_broadcaster_;
  kj::HttpHeaderTable headerTable_;
};
```

### 2. Router (router.h/cpp)

**Purpose**: Route HTTP requests to appropriate handlers with middleware chain.

**Responsibilities**:
- Parse URL and extract path parameters
- Match routes to handlers
- Execute middleware chain (metrics → CORS → auth → rate limit → audit)
- Call handler with request context

**Interface**:
```cpp
class Router {
public:
  explicit Router(kj::Own<AuthManager> authManager);

  using HandlerFn = kj::Function<kj::Promise<void>(RequestContext&)>;

  void addRoute(kj::HttpMethod method, kj::StringPtr path, HandlerFn handler);
  void addMiddleware(kj::Own<Middleware> middleware);

  kj::Promise<void> route(
    kj::HttpMethod method,
    kj::StringPtr url,
    const kj::HttpHeaders& headers,
    kj::AsyncInputStream& requestBody,
    kj::HttpService::Response& response
  );

private:
  struct Route {
    kj::HttpMethod method;
    kj::String path;
    HandlerFn handler;
  };

  kj::Vector<kj::Own<Middleware>> middlewares_;
  kj::Vector<Route> routes_;
};
```

### 3. RequestContext (request_context.h)

**Purpose**: Per-request state and utilities.

**Fields**:
```cpp
struct RequestContext {
  // Request data
  kj::HttpMethod method;
  kj::StringPtr path;
  kj::StringPtr queryString;
  kj::HttpHeaders headers;
  kj::Own<kj::AsyncInputStream> body;

  // Authentication
  kj::Maybe<AuthInfo> authInfo;  // Populated by AuthMiddleware

  // Response helpers
  kj::Promise<void> sendJson(uint status, const JsonValue& body);
  kj::Promise<void> sendError(uint status, kj::StringPtr error);
  kj::Promise<void> sendSSE(kj::Own<kj::AsyncOutputStream> stream);

  // Utilities
  kj::Promise<JsonValue> parseJsonBody();
  kj::String getHeader(kj::StringPtr name) const;
  kj::String getClientIP() const;

  // Metrics
  kj::Own<RequestTimer> timer;
};
```

### 4. EngineBridge (bridge/engine_bridge.h/cpp)

**Purpose**: In-process communication with engine (single-process architecture).

**Key Decision**: Single-process integration (see [Design Decisions](#key-design-decisions)).

**Responsibilities**:
- Initialize engine in same process (no subprocess)
- Call engine APIs directly (no stdio)
- Subscribe to engine events
- Forward events to EventBroadcaster

**Interface**:
```cpp
class EngineBridge {
public:
  explicit EngineBridge(const EngineConfig& config);
  ~EngineBridge();

  // Order management
  kj::Promise<void> placeOrder(const OrderParams& params);
  kj::Promise<void> cancelOrder(kj::StringPtr clientOrderId);

  // State queries
  MarketData getMarketData() const;
  OrderState getOrderState(kj::StringPtr clientOrderId) const;
  AccountState getAccountState() const;

  // Event subscription
  kj::Promise<void> subscribe(kj::Function<void(const EngineEvent&)> callback);

private:
  // Direct engine integration (no process boundary)
  kj::Own<Engine> engine_;
  kj::Own<EventBroadcaster> broadcaster_;
  kj::MutexGuarded<MarketData> marketData_;
  kj::MutexGuarded<OrderStore> orderStore_;
  kj::MutexGuarded<AccountStore> accountStore_;
};
```

### 5. EventBroadcaster (bridge/event_broadcaster.h/cpp)

**Purpose**: Distribute engine events to SSE clients.

**Responsibilities**:
- Manage SSE client connections
- Broadcast events to all clients
- Handle client disconnection

**Interface**:
```cpp
class EventBroadcaster {
public:
  EventBroadcaster();

  // Client management
  kj::Own<SSEClient> addClient();
  void removeClient(SSEClient& client);

  // Event broadcasting
  void broadcast(const EngineEvent& event);

  size_t getClientCount() const;

private:
  kj::MutexGuarded<kj::Vector<SSEClient*>> clients_;
};
```

### 6. AuthManager (auth/auth_manager.h/cpp)

**Purpose**: Unified authentication manager.

**Components**:
- `JWTManager`: JWT token creation and validation
- `APIKeyManager`: API key management
- `PermissionManager`: Permission checking
- `RBACManager`: Role-based access control

**Interface**:
```cpp
class AuthManager {
public:
  explicit AuthManager(const AuthConfig& config);

  // Authentication
  kj::Maybe<AuthInfo> authenticate(const kj::HttpHeaders& headers);

  // JWT management
  kj::Promise<TokenPair> createTokens(kj::StringPtr userId, kj::StringPtr password);
  kj::Maybe<TokenInfo> validateAccessToken(kj::StringPtr token);
  kj::Maybe<TokenInfo> validateRefreshToken(kj::StringPtr token);
  void revokeRefreshToken(kj::StringPtr token);

  // API key management
  kj::Promise<APIKey> createAPIKey(kj::StringPtr userId, kj::StringPtr name);
  bool validateAPIKey(kj::StringPtr key);
  void revokeAPIKey(kj::StringPtr keyId);

  // Permission checking
  bool checkPermission(const AuthInfo& auth, kj::ArrayPtr<kj::StringPtr> required);

private:
  kj::Own<JWTManager> jwtManager_;
  kj::Own<APIKeyManager> apiKeyManager_;
  kj::Own<PermissionManager> permissionManager_;
  kj::Own<RBACManager> rbacManager_;
  RateLimiter rateLimiter_;
};
```

### 7. AuditLogger (audit/audit_logger.h/cpp)

**Purpose**: Structured audit logging with file rotation.

**Features**:
- NDJSON log format
- Automatic log rotation by size/date
- Retention policies by log type
- Background cleanup thread

**Interface**:
```cpp
class AuditLogger {
public:
  explicit AuditLogger(const AuditConfig& config);

  void log(const AuditEntry& entry);
  kj::Promise<AuditQueryResult> query(const AuditQuery& query);
  kj::Promise<void> archive();

  AuditStats getStats() const;

private:
  kj::Own<AuditStore> store_;
  kj::Own<RetentionManager> retentionManager_;
  kj::MutexGuarded<kj::Vector<AuditEntry>> recentLogs_;
};
```

### 8. Metrics (metrics/metrics.h/cpp)

**Purpose**: Prometheus metrics collection.

**Metric Types**:
- **Counters**: Request counts, order counts, error counts
- **Gauges**: Active connections, active orders
- **Histograms**: Request latency, order latency

**Interface**:
```cpp
class Metrics {
public:
  // HTTP metrics
  void recordRequest(kj::StringPtr method, kj::StringPtr endpoint, uint status, double duration);
  void recordRequestSize(kj::StringPtr endpoint, size_t bytes);
  void recordResponseSize(kj::StringPtr endpoint, size_t bytes);

  // Order metrics
  void recordOrderSubmitted(kj::StringPtr side, kj::StringPtr type);
  void recordOrderFilled(kj::StringPtr side);
  void recordOrderCancelled();

  // Market metrics
  void recordMarketUpdate(kj::StringPtr type);

  // Engine metrics
  void setEngineRunning(bool running);
  void setSSEClients(size_t count);

  // Export
  kj::String exportPrometheus() const;
};
```

---

## Data Flow

### 1. Order Submission Flow

```
Client Request (POST /api/order)
  │
  ├─→ MetricsMiddleware: Start timer
  ├─→ CorsMiddleware: Add CORS headers
  ├─→ AuthMiddleware: Validate JWT/API key
  ├─→ RateLimitMiddleware: Check rate limit
  ├─→ AuditMiddleware: Log request start
  │
  └─→ OrderHandler::handle()
       │
       ├─→ Parse request body (JSON)
       ├─→ Validate parameters (side, symbol, qty, price)
       ├─→ ExecutionRouter::placeOrder()
       │    │
       │    ├─→ If binance_testnet_spot:
       │    │    └─→ BinanceRestClient::placeOrder()
       │    │         └─→ HTTP POST to Binance API
       │    │
       │    └─→ Else:
       │         └─→ EngineBridge::placeOrder()
       │              └─→ Engine::submitOrder() (direct call, in-process)
       │
       ├─→ OrderStore::applyEvent() (update state)
       ├─→ EventBroadcaster::broadcast() (notify SSE clients)
       ├─→ AuditLogger::log() (log order submission)
       └─→ Send JSON response
```

### 2. SSE Streaming Flow

```
Client Request (GET /api/stream)
  │
  ├─→ AuthMiddleware: Validate JWT/API key (optional)
  │
  └─→ SSEHandler::handle()
       │
       ├─→ Send SSE headers (Content-Type: text/event-stream)
       ├─→ EventBroadcaster::addClient()
       │
       └─→ Event Loop:
            ├─→ Wait for events (kj::Promise)
            ├─→ On event: format as SSE data
            ├─→ Write to client stream
            └─→ Repeat until client disconnects
```

### 3. Authentication Flow

```
Client Request (POST /api/auth/login)
  │
  └─→ AuthHandler::handle()
       │
       ├─→ Parse credentials (user_id, password)
       ├─→ AuthManager::createTokens()
       │    │
       │    ├─→ Validate credentials (admin password check)
       │    ├─→ JWTManager::createAccessToken()
       │    ├─→ JWTManager::createRefreshToken()
       │    └─→ Return TokenPair
       │
       ├─→ AuditLogger::log() (log login)
       └─→ Send JSON response (access_token, refresh_token)
```

---

## Key Design Decisions

### 1. Single-Process Architecture (RECOMMENDED)

**Decision**: Run engine and gateway in same process.

**Rationale**:
- **Zero IPC overhead**: Direct function calls instead of stdio serialization
- **Lower latency**: No process boundary crossing
- **Simpler deployment**: Single binary
- **Better error handling**: Shared memory, no subprocess management
- **Easier debugging**: Single process to debug

**Alternative Considered**: Subprocess architecture (Python Gateway approach)
- ✗ Higher latency (stdio serialization)
- ✗ Complex error handling (subprocess crashes)
- ✗ Deployment complexity (two binaries)

**Implementation**:
```cpp
// gateway_server.cpp
class GatewayServer {
  kj::Own<Engine> engine_;  // Direct ownership, same process
};

// No stdin/stdout bridging needed
engine_->submitOrder(...);  // Direct call
```

### 2. Single-Threaded Event Loop (RECOMMENDED)

**Decision**: Single-threaded KJ event loop with asynchronous I/O.

**Rationale**:
- **No lock contention**: All state in single thread
- **Deterministic ordering**: Events processed sequentially
- **Simpler code**: No race conditions
- **Better cache locality**: Single-threaded execution
- **KJ design**: KJ is designed for single-threaded async

**Alternative Considered**: Multi-threaded with thread pool
- ✗ Lock contention on shared state
- ✗ Non-deterministic event ordering
- ✗ Complex synchronization

**Implementation**:
```cpp
// main.cpp
int main() {
  kj::AsyncIoContext io = kj::setupAsyncIo();

  GatewayServer server(config);
  server.start(8080).wait(io.waitScope);

  io.waitScope.runForever();  // Single-threaded event loop
}
```

### 3. Memory Management: KJ Arena + kj::Own

**Decision**: Use `kj::Arena` for request-scoped allocation + `kj::Own` for ownership.

**Rationale**:
- **Zero fragmentation**: Arena allocates in large blocks
- **Fast deallocation**: Arena deallocation is O(1)
- **Ownership clarity**: `kj::Own` makes ownership explicit
- **No memory leaks**: RAII with deterministic cleanup

**Implementation**:
```cpp
class RequestContext {
  kj::Arena arena_;  // Request-scoped allocator

  template<typename T, typename... Args>
  T& alloc(Args&&... args) {
    return arena_.allocate<T>(kj::fwd<Args>(args)...);
  }

  // Automatic cleanup when RequestContext destroyed
};
```

### 4. SSE Implementation: Long-Polling with kj::Promise

**Decision**: Use KJ promises for SSE streaming.

**Implementation**:
```cpp
kj::Promise<void> SSEHandler::handle(RequestContext& ctx) {
  auto client = broadcaster_->addClient();

  // Send initial event
  co_await ctx.sendSSEHeader();

  // Event loop
  while (true) {
    auto event = co_await client->waitForEvent();
    co_await ctx.sendSSEEvent(event);
  }
}
```

### 5. Authentication: JWT HMAC-SHA256

**Decision**: Use HMAC-SHA256 for JWT (same as Python).

**Rationale**:
- **Compatibility**: Same algorithm as Python Gateway
- **Simplicity**: No external dependencies (OpenSSL HMAC)
- **Performance**: Fast verification

**Implementation**:
```cpp
class JWTManager {
  kj::Array<byte> secret_;  // HMAC secret

  kj::String createToken(kj::StringPtr userId);
  kj::Maybe<jwt::Payload> validateToken(kj::StringPtr token);
};
```

### 6. Logging: Structured Logging to Files

**Decision**: Structured logging with rotation (similar to Python audit.py).

**Implementation**:
- NDJSON format (one JSON object per line)
- Rotation by size (100MB) and time (daily)
- Retention policies by log type (auth: 90 days, order: 365 days)

---

## Configuration

### Configuration File (gateway_config.yaml)

```yaml
server:
  port: 8080
  bind: "0.0.0.0"
  static_dir: "./apps/ui"

auth:
  enabled: true
  jwt_secret: "${VELOZ_JWT_SECRET}"  # From environment
  token_expiry: 3600  # 1 hour
  admin_password: "${VELOZ_ADMIN_PASSWORD}"

rate_limit:
  enabled: true
  capacity: 100
  refill_rate: 10.0  # tokens per second

engine:
  mode: "simulation"  # simulation, binance_testnet_spot
  market_source: "binance_rest"
  market_symbol: "BTCUSDT"

binance:
  testnet_base_url: "https://testnet.binance.vision"
  api_key: "${BINANCE_API_KEY}"
  api_secret: "${BINANCE_API_SECRET}"

audit:
  log_dir: "/var/log/veloz/audit"
  cleanup_interval_hours: 24

logging:
  level: "info"  # debug, info, warn, error
  format: "json"  # json, text
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `VELOZ_JWT_SECRET` | JWT signing secret | Random 32 bytes |
| `VELOZ_ADMIN_PASSWORD` | Admin login password | None |
| `BINANCE_API_KEY` | Binance API key | None |
| `BINANCE_API_SECRET` | Binance API secret | None |
| `VELOZ_LOG_LEVEL` | Log level | `info` |

---

## Performance Targets

### Latency Targets

| Operation | P50 | P99 | P99.9 |
|-----------|-----|-----|-------|
| Health check | <10μs | <50μs | <100μs |
| Market data query | <50μs | <200μs | <500μs |
| Order submission | <100μs | <500μs | <1ms |
| Authentication | <100μs | <500μs | <1ms |

### Throughput Targets

| Operation | Target |
|-----------|--------|
| Total requests | >10,000 req/s |
| Order submissions | >1,000 req/s |
| SSE clients | >1,000 concurrent |

### Resource Targets

| Metric | Target |
|--------|--------|
| Memory (idle) | <50MB |
| Memory (active) | <200MB |
| CPU (idle) | <1% |
| CPU (load) | <50% at 10k req/s |

---

## Migration Strategy

### Phase 1: Core Infrastructure (Week 1-2)

**Deliverables**:
- GatewayServer with health check endpoint
- Router with middleware chain
- RequestContext
- Basic metrics

**Testing**:
- Unit tests for router
- Integration test: health check endpoint
- Benchmark: routing overhead

### Phase 2: Authentication & Authorization (Week 3-4)

**Deliverables**:
- JWTManager
- APIKeyManager
- PermissionManager
- RBACManager
- AuthMiddleware
- RateLimitMiddleware

**Testing**:
- Unit tests for auth components
- Integration tests for login flow
- Security tests for permission checks

### Phase 3: Engine Integration (Week 5-6)

**Deliverables**:
- EngineBridge (single-process)
- OrderStore
- AccountStore
- MarketStore

**Testing**:
- Integration tests with engine
- Order submission flow tests
- State consistency tests

### Phase 4: Order Management (Week 7-8)

**Deliverables**:
- OrderHandler
- OrderStateHandler
- AccountHandler
- ExecutionRouter

**Testing**:
- Order lifecycle tests
- Binance integration tests
- Failure scenario tests

### Phase 5: Streaming & Events (Week 9-10)

**Deliverables**:
- SSEHandler
- EventBroadcaster
- Real-time updates

**Testing**:
- SSE connection tests
- Event delivery tests
- Load tests (1000 concurrent SSE clients)

### Phase 6: Audit & Compliance (Week 11-12)

**Deliverables**:
- AuditLogger
- AuditStore
- RetentionManager
- AuditHandler

**Testing**:
- Log rotation tests
- Retention policy tests
- Query performance tests

### Phase 7: Production Readiness (Week 13-14)

**Deliverables**:
- Performance optimization
- Documentation
- Monitoring dashboards
- Rollback procedures

**Testing**:
- Full load test (10k req/s)
- Chaos engineering tests
- Failover tests

---

## Appendix A: API Compatibility Matrix

| Endpoint | Python Gateway | C++ Gateway | Status |
|----------|---------------|-------------|--------|
| `GET /health` | ✓ | ✓ | Compatible |
| `GET /api/health` | ✓ | ✓ | Compatible |
| `GET /api/config` | ✓ | ✓ | Compatible |
| `GET /api/market` | ✓ | ✓ | Compatible |
| `POST /api/order` | ✓ | ✓ | Compatible |
| `POST /api/cancel` | ✓ | ✓ | Compatible |
| `GET /api/orders` | ✓ | ✓ | Compatible |
| `GET /api/orders_state` | ✓ | ✓ | Compatible |
| `GET /api/order_state` | ✓ | ✓ | Compatible |
| `GET /api/account` | ✓ | ✓ | Compatible |
| `GET /api/stream` | ✓ | ✓ | Compatible (SSE) |
| `POST /api/auth/login` | ✓ | ✓ | Compatible |
| `POST /api/auth/refresh` | ✓ | ✓ | Compatible |
| `POST /api/auth/logout` | ✓ | ✓ | Compatible |
| `GET /api/auth/keys` | ✓ | ✓ | Compatible |
| `POST /api/auth/keys` | ✓ | ✓ | Compatible |
| `DELETE /api/auth/keys` | ✓ | ✓ | Compatible |
| `GET /api/auth/roles` | ✓ | ✓ | Compatible |
| `POST /api/auth/roles` | ✓ | ✓ | Compatible |
| `GET /metrics` | ✓ | ✓ | Compatible (Prometheus) |
| `GET /api/audit/logs` | ✓ | ✓ | Compatible |
| `GET /api/audit/stats` | ✓ | ✓ | Compatible |
| `POST /api/audit/archive` | ✓ | ✓ | Compatible |

---

## Appendix B: Dependency List

### Internal Dependencies

| Component | Depends On |
|-----------|------------|
| GatewayServer | Router, AuthManager, EngineBridge, EventBroadcaster |
| Router | Middleware, Handlers |
| AuthManager | JWTManager, APIKeyManager, PermissionManager, RBACManager |
| EngineBridge | Engine (libs/engine), EventBroadcaster |
| EventBroadcaster | SSEClient |
| OrderHandler | EngineBridge, OrderStore, AuditLogger |
| SSEHandler | EventBroadcaster |

### External Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| KJ (Cap'n Proto) | 1.0+ | Async I/O, HTTP, memory management |
| OpenSSL | 3.0+ | HMAC, SHA256 for JWT |
| yyjson | 0.8+ | JSON parsing (fast) |
| spdlog | 1.12+ | Logging |

---

## Appendix C: Error Handling Strategy

### Error Categories

1. **Client Errors (4xx)**:
   - `400 Bad Request`: Invalid parameters
   - `401 Unauthorized`: Invalid/missing credentials
   - `403 Forbidden`: Insufficient permissions
   - `404 Not Found`: Resource not found
   - `429 Too Many Requests`: Rate limit exceeded

2. **Server Errors (5xx)**:
   - `500 Internal Server Error`: Unexpected errors
   - `503 Service Unavailable`: Engine down

### Error Response Format

```json
{
  "error": "error_code",
  "message": "Human-readable message",
  "details": {
    "field": "additional_info"
  }
}
```

### Exception Handling

```cpp
kj::Promise<void> Handler::handle(RequestContext& ctx) {
  try {
    // Handler logic
    co_await doWork();
  } catch (const ValidationError& e) {
    co_return ctx.sendError(400, e.what());
  } catch (const AuthError& e) {
    co_return ctx.sendError(401, e.what());
  } catch (const kj::Exception& e) {
    LOG(ERROR) << "Internal error: " << e.getDescription();
    co_return ctx.sendError(500, "internal_error");
  }
}
```

---

## Appendix D: Monitoring & Observability

### Metrics Endpoints

1. **Prometheus Metrics**: `GET /metrics`
   - Request latency histograms
   - Error counters
   - Active connections gauge

2. **Health Check**: `GET /api/health`
   - Engine connectivity
   - Database connectivity (future)
   - Memory usage

### Logging

- **Structured logging**: JSON format
- **Log levels**: DEBUG, INFO, WARN, ERROR
- **Log rotation**: Daily or size-based (100MB)

### Tracing (Future)

- OpenTelemetry integration
- Distributed tracing across services
- Request correlation IDs

---

**Document History**:
- 2026-02-27: Initial version (v1.0)
