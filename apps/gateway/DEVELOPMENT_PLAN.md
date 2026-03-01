# VeloZ C++ Gateway Development Plan and Design Proposal

## 1. Scope and Goals

### Goals
- Replace the Python gateway with a production-ready C++ gateway without breaking the existing API surface.
- Maintain compatibility with the current UI and documentation.
- Provide low-latency, high-throughput request handling using KJ async I/O.
- Preserve the engine protocol contract (stdio NDJSON) until a formal service contract exists.
- Keep the architecture modular for future split into services.

### Non-Goals (Current Phase)
- Introducing new API endpoints beyond the existing Python gateway surface.
- Changing the engine’s protocol or introducing new protocol formats.
- Rewriting UI or strategy SDKs.

## 2. Current State and Gaps

### Implemented (Status as of 2026-02)

| Component | Files | Status | Notes |
|-----------|-------|--------|-------|
| **Router** | `router.cpp/h` | ✅ Complete | Parameterized routes, method matching, 405 handling |
| **Middleware** | `middleware/*.cpp` | ✅ Complete | Auth, CORS, RateLimiter, Metrics all implemented |
| **Auth Module** | `auth/*.cpp` | ✅ Complete | JwtManager, ApiKeyManager, RBAC, AuthManager |
| **Audit Module** | `audit/*.cpp` | ✅ Complete | AuditLogger, AuditStore |
| **Event Broadcaster** | `bridge/event_broadcaster.cpp` | ✅ Complete | SSE fanout with history replay |
| **SSE Handler** | `handlers/sse_handler.cpp` | ✅ Complete | Keepalive, Last-Event-ID support |
| **Health Handler** | `handlers/health_handler.cpp` | ✅ Complete | Simple and detailed health checks |
| **Auth Handler** | `handlers/auth_handler.cpp` | ✅ Complete | Login, refresh, logout, API keys |
| **Order Handler** | `handlers/order_handler.cpp` | ✅ Complete | Submit, list, get, cancel orders |
| **Account Handler** | `handlers/account_handler.cpp` | ✅ Complete | Account state, positions |
| **Config Handler** | `handlers/config_handler.cpp` | ✅ Complete | Get/update config |
| **Market Handler** | `handlers/market_handler.cpp` | ✅ Complete | Market data queries |
| **Static Handler** | `handlers/static_handler.cpp` | ✅ Complete | Static file serving, SPA fallback |
| **Metrics Handler** | `handlers/metrics_handler.cpp` | ✅ Complete | Prometheus metrics endpoint |
| **Audit Handler** | `handlers/audit_handler.cpp` | ✅ Complete | Audit log queries |
| **Engine Bridge** | `bridge/engine_bridge.cpp` | ⚠️ Partial | Event queue exists, but no real engine connection |
| **GatewayServer** | `gateway_server.cpp` | ❌ Not Wired | Only hardcoded health check, no router dispatch |
| **main.cpp** | `main.cpp` | ❌ Stubs Only | All initialization functions are empty placeholders |

### Critical Gaps

#### G1: HTTP Server Not Wired (Highest Priority)
**Location**: `gateway_server.cpp:14-24`

```cpp
// Current implementation only returns hardcoded response
kj::Promise<void> GatewayServer::request(...) {
  if (method == kj::HttpMethod::GET && url == "/api/control/health"_kj) {
    // Only this endpoint works
  }
  return response.sendError(404, "Not Found"_kj, headerTable_);
}
```

**Impact**: Router, Middleware, and Handlers are implemented but not invoked.

#### G2: Components Not Instantiated in main.cpp
**Location**: `main.cpp:385-532`

All initialization functions are empty:
```cpp
void initializeRouter() {
  // router_ = kj::heap<Router>();  // Commented out
  KJ_LOG(INFO, "Router initialized");
}
```

**Impact**: No components are created; gateway cannot function.

#### G3: Engine Bridge Not Connected to Real Process
**Location**: `engine_bridge.cpp:150-226`

Order operations simulate events in memory:
```cpp
kj::Promise<void> EngineBridge::place_order(...) {
  // Creates mock event, no stdio communication
  BridgeEvent event = create_order_event(order_state);
  event_queue_.push(kj::mv(event));
}
```

**Impact**: No real trading functionality.

#### G4: Missing API Endpoints
See Section 4.1 for detailed coverage matrix.

#### G5: Scripts Still Use Python Gateway
**Location**: `scripts/run_gateway.sh`

No option to switch to C++ gateway.

### API Endpoint Coverage Matrix

| Endpoint | Python Gateway | C++ Handler | Route Registered | Status |
|----------|---------------|-------------|------------------|--------|
| `GET /health` | ✅ | HealthHandler | ❌ | Handler ready |
| `GET /api/health` | ✅ | HealthHandler | ❌ | Handler ready |
| `GET /api/market` | ✅ | MarketHandler | ❌ | Handler ready |
| `GET /api/orders` | ✅ | OrderHandler | ❌ | Handler ready |
| `GET /api/orders_state` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `GET /api/order_state` | ✅ | OrderHandler | ❌ | Handler ready (different route) |
| `GET /api/stream` | ✅ | SseHandler | ❌ | Handler ready |
| `GET /api/account` | ✅ | AccountHandler | ❌ | Handler ready |
| `GET /api/config` | ✅ | ConfigHandler | ❌ | Handler ready |
| `GET /api/execution/ping` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `POST /api/order` | ✅ | OrderHandler | ❌ | Handler ready |
| `POST /api/cancel` | ✅ | OrderHandler | ❌ | Handler ready (different route) |
| `POST /api/auth/login` | ✅ | AuthHandler | ❌ | Handler ready |
| `POST /api/auth/refresh` | ✅ | AuthHandler | ❌ | Handler ready |
| `POST /api/auth/logout` | ✅ | AuthHandler | ❌ | Handler ready |
| `GET /api/auth/keys` | ✅ | AuthHandler | ❌ | Handler ready |
| `POST /api/auth/keys` | ✅ | AuthHandler | ❌ | Handler ready |
| `GET /api/auth/roles` | ✅ | RoleHandler | ❌ | Handler exists, not in CMake |
| `POST /api/auth/roles` | ✅ | RoleHandler | ❌ | Handler exists, not in CMake |
| `GET /api/audit/logs` | ✅ | AuditHandler | ❌ | Handler ready |
| `GET /api/audit/stats` | ✅ | AuditHandler | ❌ | Handler ready |
| `POST /api/audit/archive` | ✅ | AuditHandler | ❌ | Handler ready |
| `GET /api/exchange-keys` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `POST /api/exchange-keys` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `POST /api/exchange-keys/test` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `GET /api/settings` | ✅ | ConfigHandler | ❌ | Handler ready (different path) |
| `POST /api/settings` | ✅ | ConfigHandler | ❌ | Handler ready (different path) |
| `POST /api/settings/import` | ✅ | ❌ Missing | ❌ | Needs implementation |
| `GET /api/settings/export` | ✅ | ❌ Missing | ❌ | Needs implementation |
| SPA fallback | ✅ | StaticHandler | ❌ | Handler ready |

**Coverage Summary**: 22/29 endpoints have handlers (76%), 0 routes registered (0%)

## 3. Target Architecture

### High-Level Flow
1. HTTP request arrives at `GatewayServer` (KJ HttpService).
2. Request is wrapped into `RequestContext`.
3. Middleware chain runs (auth, rate limit, CORS, audit, metrics).
4. Router matches the endpoint and dispatches to a handler.
5. Handler calls `EngineBridge` or configuration services.
6. Response is serialized and returned, or SSE stream is attached.

### Engine Bridge Contract
- Keep current stdio protocol: gateway writes text commands to engine stdin and reads NDJSON events from stdout.
- Maintain event types: market, order_update, fill, error, account, order_state.
- Provide backpressure-aware broadcast to SSE subscribers.

### Module Responsibilities
- `GatewayServer`: KJ HTTP service implementation.
- `Router`: route matching and dispatch.
- `Middleware`: auth, rate limit, CORS, audit, metrics.
- `Handlers`: request logic and response formatting.
- `EngineBridge`: process lifecycle, stdio protocol, state caching.
- `EventBroadcaster`: SSE fanout and replay.

## 4. API Compatibility Targets

Match the Python gateway surface in `apps/gateway/gateway.py` and `docs/guides/build_and_run.md`.

### Required Endpoints
- `GET /health`
- `GET /api/health`
- `GET /api/market`
- `GET /api/orders`
- `GET /api/orders_state`
- `GET /api/order_state?client_order_id=...`
- `GET /api/stream`
- `GET /api/account`
- `GET /api/config`
- `GET /api/execution/ping`
- `POST /api/order`
- `POST /api/cancel`
- `POST /api/auth/login`
- `POST /api/auth/refresh`
- `POST /api/auth/logout`
- `GET /api/auth/keys`
- `POST /api/auth/keys`
- `GET /api/auth/roles`
- `POST /api/auth/roles`
- `GET /api/audit/logs`
- `GET /api/audit/stats`
- `POST /api/audit/archive`
- `GET /api/exchange-keys`
- `POST /api/exchange-keys`
- `POST /api/exchange-keys/test`
- `GET /api/settings`
- `POST /api/settings`
- `POST /api/settings/import`
- `GET /api/settings/export`
- SPA fallback for non-API paths (serve UI index.html)

## 5. Design Details

### 5.1 HTTP Server and Request Context
- Implement `GatewayServer` as a `kj::HttpService`.
- Convert request into `RequestContext` using `request_context.h`.
- Ensure a single authoritative `RequestContext` definition, remove duplicate.

### 5.2 Router and Middleware
- Build a middleware chain that wraps route handler invocation.
- Always produce `405 Method Not Allowed` with `Allow` header when path exists.
- Support `OPTIONS` for CORS preflight.

### 5.3 Engine Bridge
- Spawn the engine process using the same preset workflow as Python gateway.
- Manage stdin/stdout pipes and parse NDJSON events.
- Provide:
  - order submission/cancel
  - query endpoints backed by cached state
  - event replay ring buffer for SSE
- Provide health: engine connectivity and last event timestamp.

### 5.4 SSE Event Stream
- `GET /api/stream` with `last_id` support.
- Keep-alive tick every `sse_keepalive_interval_ms`.
- Backpressure-aware broadcast with bounded queue per client.

### 5.5 Auth and RBAC
- Reuse current JWT and API key semantics.
- Implement permission checks at middleware or handler boundary.
- Include `admin` role and enforced endpoints mapping.

### 5.6 Metrics and Audit
- Emit Prometheus metrics for request counts, latency, and SSE connections.
- Audit log records for security-sensitive actions.

### 5.7 Static UI Serving
- Serve static assets from `apps/ui` and fallback to `index.html` for SPA routes.
- Respect `VELOZ_STATIC_DIR` and cache settings.

## 6. Implementation Plan (Milestones)

### M1: Build and Compile Stability ✅ COMPLETE
- [x] Fix KJ misuse in handlers.
- [x] Resolve duplicate `RequestContext` definition.
- [x] Ensure `veloz_gateway` and tests compile under dev preset.
- [x] Unit test framework operational.

### M2: HTTP Server and Routing

#### M2.1: GatewayServer Request Dispatch
**Priority**: Critical | **Estimate**: Core work

- [ ] **Rewrite `GatewayServer::request()`** (`gateway_server.cpp`)
  - Create `RequestContext` from incoming request
  - Extract path, method, headers, and body stream
  - Parse client IP from `X-Forwarded-For` or socket address

- [ ] **Implement middleware chain execution**
  - Define `MiddlewareChain` class or use vector of middleware
  - Execute in order: CORS → RateLimit → Auth → Audit → Metrics
  - Handle middleware short-circuit (auth failure, rate limit exceeded)

- [ ] **Call Router for request dispatch**
  - Use `Router::match(method, path)` to find handler
  - Pass extracted path parameters to handler
  - Handle 404 (no route) and 405 (wrong method) responses

- [ ] **OPTIONS and CORS preflight handling**
  - Return 204 with CORS headers for OPTIONS requests
  - Include `Allow` header with permitted methods for 405 responses

#### M2.2: Component Instantiation in main.cpp
**Priority**: Critical | **Estimate**: Core work

- [ ] **Uncomment and fix all initialization functions**
  ```cpp
  void initializeMetrics() {
    metrics_registry_ = kj::heap<MetricsRegistry>();
  }
  void initializeRouter() {
    router_ = kj::heap<Router>();
    register_all_routes();
  }
  // ... etc
  ```

- [ ] **Register all routes** (create `register_routes()` function)
  ```cpp
  void register_routes() {
    // Health
    router_->add_route(kj::HttpMethod::GET, "/health", health_handler_);
    router_->add_route(kj::HttpMethod::GET, "/api/health", health_handler_);

    // Market
    router_->add_route(kj::HttpMethod::GET, "/api/market", market_handler_);

    // Orders
    router_->add_route(kj::HttpMethod::GET, "/api/orders", order_handler_);
    router_->add_route(kj::HttpMethod::GET, "/api/order_state", order_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/order", order_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/cancel", order_handler_);

    // Auth
    router_->add_route(kj::HttpMethod::POST, "/api/auth/login", auth_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/auth/refresh", auth_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/auth/logout", auth_handler_);
    router_->add_route(kj::HttpMethod::GET, "/api/auth/keys", auth_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/auth/keys", auth_handler_);

    // Account
    router_->add_route(kj::HttpMethod::GET, "/api/account", account_handler_);

    // Config/Settings
    router_->add_route(kj::HttpMethod::GET, "/api/config", config_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/config", config_handler_);

    // SSE
    router_->add_route(kj::HttpMethod::GET, "/api/stream", sse_handler_);

    // Audit
    router_->add_route(kj::HttpMethod::GET, "/api/audit/logs", audit_handler_);
    router_->add_route(kj::HttpMethod::GET, "/api/audit/stats", audit_handler_);
    router_->add_route(kj::HttpMethod::POST, "/api/audit/archive", audit_handler_);

    // Static/SPA fallback (catch-all for non-API paths)
    router_->add_fallback(static_handler_);
  }
  ```

- [ ] **Create and start kj::HttpServer**
  ```cpp
  kj::Promise<void> run() {
    kj::HttpServer server(io_.provider->getTimer(), headerTable_, *gateway_service_);
    auto listener = io_.provider->getNetwork().parseAddress(config_.host, config_.port)
        .then([this, &server](kj::Own<kj::NetworkAddress> addr) {
          return server.listenHttp(*addr);
        });
    // ... handle shutdown
  }
  ```

- [ ] **Wire GatewayServer to use Router and Middleware**
  - Store references to Router and MiddlewareChain in GatewayServer
  - Pass request through middleware then to router

#### M2.3: Static File Serving and SPA Fallback
**Priority**: Medium | **Depends on**: M2.2

- [ ] **Configure static file path**
  - Use `VELOZ_STATIC_DIR` environment variable
  - Default to `./apps/ui`

- [ ] **Implement SPA fallback**
  - For any non-API route (not starting with `/api/` or `/health`)
  - Serve `index.html` with proper content-type

- [ ] **Add cache headers for static assets**
  - Use `VELOZ_STATIC_CACHE_ENABLED` and `VELOZ_STATIC_CACHE_MAX_AGE`

### M3: Engine Bridge Integration

#### M3.1: Process Management
**Priority**: High | **Estimate**: Significant work

- [ ] **Create `SubprocessHandle` class** (new file: `bridge/subprocess.h`)
  ```cpp
  class SubprocessHandle {
  public:
    kj::Promise<void> spawn(kj::StringPtr command, kj::Array<kj::StringPtr> args);
    kj::AsyncOutputStream& stdin();
    kj::AsyncInputStream& stdout();
    kj::Promise<int> waitExit();
    void kill();
  };
  ```

- [ ] **Implement process spawning**
  - Use `fork()`/`exec()` on Unix, `CreateProcess()` on Windows
  - Create pipes for stdin/stdout
  - Set process to use `--stdio` mode

- [ ] **Handle process lifecycle**
  - Graceful shutdown on SIGTERM
  - Restart on unexpected exit (with backoff)
  - Health monitoring

#### M3.2: NDJSON Protocol Implementation
**Priority**: High | **Depends on**: M3.1

- [ ] **Implement command formatting** (engine protocol)
  ```
  ORDER side=<buy|sell> symbol=<SYMBOL> qty=<QTY> [price=<PRICE>] [client_order_id=<ID>]
  CANCEL client_order_id=<ID>
  ```

- [ ] **Implement event parsing**
  - Parse JSON lines from engine stdout
  - Event types: `market`, `order_update`, `fill`, `error`, `account`, `order_state`
  - Validate required fields per event type

- [ ] **Implement state caching**
  - Cache order states from `order_update` events
  - Cache account state from `account` events
  - Cache market data from `market` events

- [ ] **Wire to EngineBridge methods**
  - `place_order()`: format and write command to stdin
  - `cancel_order()`: format and write cancel command
  - `get_order()`: return from cache
  - `get_orders()`: return all from cache

#### M3.3: Event Broadcasting Integration
**Priority**: Medium | **Depends on**: M3.2

- [ ] **Connect EngineBridge to EventBroadcaster**
  - On each parsed event, broadcast to SSE subscribers
  - Maintain event ID sequence for replay

- [ ] **Implement ring buffer for history replay**
  - Store last N events (configurable via `event_queue_capacity`)
  - Support `Last-Event-ID` header for reconnection

### M4: Full API Coverage

#### M4.1: Missing Endpoints
**Priority**: Medium | **Depends on**: M2.2

| Endpoint | Implementation Notes |
|----------|---------------------|
| `GET /api/orders_state` | Batch query for multiple order states. Accept `client_order_ids` query param or POST body. |
| `GET /api/execution/ping` | Simple ping to verify engine connectivity. Return `{"pong": true, "engine_connected": bool}`. |
| `GET /api/exchange-keys` | List encrypted exchange API keys. Use `keychain.py` logic. |
| `POST /api/exchange-keys` | Store encrypted exchange API key. |
| `POST /api/exchange-keys/test` | Test exchange API key validity. |
| `POST /api/settings/import` | Import settings from JSON file/body. |
| `GET /api/settings/export` | Export settings as JSON. |

#### M4.2: Role Management Handler
**Priority**: Low | **Depends on**: M2.2

- [ ] **Add `role_handler.cpp` to CMakeLists.txt**
  ```cmake
  src/handlers/role_handler.cpp
  ```

- [ ] **Register role routes**
  ```cpp
  router_->add_route(kj::HttpMethod::GET, "/api/auth/roles", role_handler_);
  router_->add_route(kj::HttpMethod::POST, "/api/auth/roles", role_handler_);
  ```

- [ ] **Implement role CRUD operations**
  - List roles
  - Create role with permissions
  - Update role
  - Delete role (admin only)

#### M4.3: Settings Alias Routes
**Priority**: Low | **Depends on**: M2.2

- [ ] **Add `/api/settings` routes pointing to ConfigHandler**
  ```cpp
  router_->add_route(kj::HttpMethod::GET, "/api/settings", config_handler_);
  router_->add_route(kj::HttpMethod::POST, "/api/settings", config_handler_);
  ```

### M5: SSE and Observability

#### M5.1: SSE Integration
**Priority**: Medium | **Depends on**: M3.3

- [x] SSE handler implementation (`sse_handler.cpp`)
- [x] Event broadcaster (`event_broadcaster.cpp`)
- [ ] Wire to EngineBridge event stream
- [ ] Implement keepalive timer
- [ ] Test reconnection with `Last-Event-ID`

#### M5.2: Metrics Endpoint
**Priority**: Medium

- [x] Metrics handler exists (`metrics_handler.cpp`)
- [ ] Expose Prometheus-compatible metrics at `/metrics`
- [ ] Metrics to include:
  - `veloz_http_requests_total{method, path, status}`
  - `veloz_http_request_duration_seconds`
  - `veloz_sse_connections_active`
  - `veloz_engine_events_total{type}`
  - `veloz_engine_latency_seconds`

#### M5.3: Integration Tests
**Priority**: Medium | **Depends on**: M2, M3

- [ ] End-to-end test: login → place order → verify via SSE
- [ ] Engine stdio mock for testing
- [ ] Concurrent load test
- [ ] Graceful shutdown test

### M6: Integration and Tooling

#### M6.1: Run Script Update
**Priority**: Low | **Depends on**: M2

- [ ] Update `scripts/run_gateway.sh`
  ```bash
  # Add gateway selection
  GATEWAY_TYPE=${VELOZ_GATEWAY_TYPE:-python}

  if [ "$GATEWAY_TYPE" = "cpp" ]; then
    exec ./build/dev/apps/gateway_cpp/veloz_gateway
  else
    exec python3 apps/gateway/gateway.py
  fi
  ```

#### M6.2: Deployment Scripts
**Priority**: Low

- [ ] Fix canary deploy binary name (`veloz_gateway` vs expected name)
- [ ] Update `scripts/deploy_gateway_canary.sh`
- [ ] Update `scripts/health_check_gateway.sh` for C++ endpoints

#### M6.3: Documentation
**Priority**: Low

- [ ] Update `docs/guides/build_and_run.md` with C++ gateway instructions
- [ ] Document environment variables for configuration
- [ ] Add troubleshooting section for common issues

## 7. Testing Strategy

### Unit Tests
- Router matching and parameter extraction.
- Auth and RBAC checks.
- Rate limiting and CORS.

### Integration Tests
- End-to-end flow: auth -> order -> SSE.
- Engine stdio protocol simulation.
- Concurrent request load.

### Build and CI
- `cmake --preset dev`
- `cmake --build --preset dev-all -j$(nproc)`
- `ctest --preset dev -j$(nproc)`

## 8. Rollout Plan

1. Ship C++ gateway behind a flag in `scripts/run_gateway.sh`.
2. Canary deploy C++ gateway on a different port.
3. Compare functional parity and metrics.
4. Switch UI and tools to new gateway.
5. Remove Python gateway only after stability is proven.

## 9. Risks and Mitigations

- **Protocol mismatch**: validate NDJSON fields against engine outputs.
- **API drift**: keep compatibility tests against Python gateway responses.
- **Performance regression**: run benchmarks and track latency/throughput.
- **Operational gaps**: ensure logging, metrics, and audit are complete.

## 10. Acceptance Criteria

### M2 Acceptance (HTTP Server)
- [ ] Gateway starts and listens on configured port (default 8080)
- [ ] `curl http://localhost:8080/health` returns `{"status":"ok"}`
- [ ] `curl http://localhost:8080/api/health` returns detailed health with engine status
- [ ] `curl http://localhost:8080/api/market` returns market data (or appropriate error)
- [ ] All registered endpoints return expected responses (not 404 for registered routes)
- [ ] 405 responses include `Allow` header
- [ ] OPTIONS requests return proper CORS headers
- [ ] SPA fallback serves `index.html` for non-API routes

### M3 Acceptance (Engine Integration)
- [ ] Engine process spawns successfully with `--stdio` flag
- [ ] `POST /api/order` sends command to engine via stdin
- [ ] Engine events (NDJSON) are parsed and cached
- [ ] `GET /api/orders` returns cached order states
- [ ] SSE clients receive real-time events from engine

### M4 Acceptance (API Coverage)
- [ ] All 29 endpoints in Section 4 return expected response format
- [ ] Response JSON matches Python gateway schema
- [ ] Error responses include `{"error": "message"}` format
- [ ] Rate limiting returns 429 with `Retry-After` header

### M5 Acceptance (SSE & Observability)
- [ ] SSE connection stays alive with keepalive messages
- [ ] `Last-Event-ID` header triggers event replay
- [ ] Prometheus metrics available at `/metrics`
- [ ] Audit logs record all security-sensitive actions

### M6 Acceptance (Tooling)
- [ ] `VELOZ_GATEWAY_TYPE=cpp ./scripts/run_gateway.sh` starts C++ gateway
- [ ] UI operates end-to-end without modification
- [ ] All integration tests pass

## 11. Implementation Progress Summary

| Milestone | Status | Completion |
|-----------|--------|------------|
| M1: Build Stability | ✅ Complete | 100% |
| M2: HTTP Server | 🔴 Not Started | 0% |
| M3: Engine Bridge | 🔴 Not Started | 0% |
| M4: API Coverage | 🟡 Partial | 76% (handlers exist) |
| M5: SSE & Observability | 🟡 Partial | 50% (handlers exist, not wired) |
| M6: Tooling | 🔴 Not Started | 0% |

**Overall Progress: ~35%**

### Next Steps (Recommended Order)
1. **M2.1**: Rewrite `GatewayServer::request()` to call Router
2. **M2.2**: Uncomment and fix component initialization in `main.cpp`
3. **M2.3**: Wire static file serving and SPA fallback
4. **Test**: Verify basic HTTP flow works
5. **M3.1-M3.3**: Implement engine process integration
6. **M4.1**: Add missing endpoints
7. **M5**: Complete SSE and metrics integration
8. **M6**: Update scripts and documentation
