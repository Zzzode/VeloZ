# VeloZ Crypto Quant Trading Framework: Technology Choices

## 4. VeloZ Technology Choices (Replaceable)

This section provides recommended options, alternatives, and reasoning (performance/reliability/ecosystem). For MVP, prioritize the recommended choices in each section.

### 4.1 Infrastructure and Runtime

- OS: Linux (Ubuntu/Debian), use LTS in production
- Time sync: chrony + NTP; optional PTP (higher precision in same datacenter)
- Containerization: Docker; orchestration via Kubernetes (or docker-compose first)
- Edge gateway: Nginx / Envoy (TLS termination, reverse proxy, rate limiting, WAF optional)
- Config management: env vars + config files (YAML/TOML) + remote config center (optional Consul)

### 4.2 Core Languages and Engineering Shape

- Core low-latency (market/execution/OMS/event bus): C++23 (preferred)
  - Reasons: controllable latency, extreme performance, mature systems/networking ecosystem, suited for HFT hot path (pools, zero-copy, lock-free, CPU affinity/NUMA)
  - Async model: Boost.Asio (or standalone Asio) event loop; optional C++20/23 coroutines (compiler/library maturity dependent)
  - Engineering:
    - Build system: CMake
    - Dependency management: vcpkg or Conan
    - ABI strategy: unified toolchain and stdlib version across core services
- Strategy research and iteration: Python
  - Control-plane communication: gRPC + shared memory/IPC (optional for extreme latency)
- UI: Web (React) + BFF (optional)

### 4.3 Exchange Integration (REST/WS) and Network Stack

Choose mature network stacks by language to ensure connection stability and observability:

- C++23:
  - Networking/event loop: Boost.Asio (or standalone Asio)
  - HTTP:
    - Boost.Beast (deep Asio integration, controllable pooling/timeouts)
    - Optional: libcurl (stable, non-critical paths/tools)
  - WebSocket:
    - Boost.Beast WebSocket (unified stack for rate limiting/reconnect/metrics)
    - Optional: uWebSockets (higher throughput in extreme cases)
  - TLS: OpenSSL (or BoringSSL depending on distro/compliance)
  - JSON:
    - simdjson (high-throughput parsing, hot market path)
    - nlohmann/json (convenient for non-critical paths/config)

Key strategies:

- Connection reuse: long-lived connections, HTTP keep-alive, TLS session reuse
- Auto-reconnect: exponential backoff + jitter; resync with snapshots after reconnect
- Rate limiting: token buckets per endpoint/key with queues and degradation (drop low-priority subs)

### 4.4 Communication, Serialization, and Event Bus

- Control plane (low-frequency strong typing): gRPC + Protobuf
  - Benefits: evolvable interfaces, cross-language, easy auth/audit
- Data plane (high-frequency streams): prefer NATS JetStream; Kafka for larger scale
  - NATS: low latency, lightweight ops, good for market/trade event distribution
  - Kafka: high throughput and durability, good for massive historical pipelines/analytics
- In-process queues: ring buffers/lock-free queues
  - C++: boost::lockfree, folly ProducerConsumerQueue, or custom SPSC/MPSC RingBuffer (pool + cacheline alignment)
- Serialization formats:
  - Internal events: Protobuf (cross-process) + binary structs (in-process)
  - Historical storage: Parquet (columnar, good for backtest/analysis) or compressed JSON archives (optional)

### 4.5 Market Data Technology Choices

- OrderBook:
  - In-memory structure: price-level map (BTree/skiplist/balanced tree wrapper) + TopN view
  - Snapshot/delta alignment: verify by exchange sequence (u/U), resnapshot on gap
- Kline aggregation:
  - Realtime aggregation: rolling window + incremental updates (avoid rescans)
  - Indicators: use pandas/numpy for low frequency; implement core indicators in engine for HFT
- Market cache:
  - Redis: hot markets, TopN, recent trades for UI
  - Memory: critical strategy subscriptions (minimize serialization/copy)

### 4.6 Execution / OMS Technology Choices

- Order state and idempotency:
  - clientOrderId: unified generation (snowflake/ULID/time+random) for traceability and idempotency
  - Local WAL: RocksDB (embedded) or direct PostgreSQL (simpler ops)
- User data stream (account/order receipts):
  - WebSocket primary, REST periodic full reconciliation
  - Recovery: reconciliation as authority (freeze strategy → reconcile → resume)
- Risk engine (suggested as independent module, embeddable in gateway):
  - In-memory checks (rate/position/leverage/deviation) + async reconciliation
  - Kill Switch: force cancel-only mode at execution gateway
  - Rule expression: config-driven (YAML/TOML) + hot reload (control plane), prevent bypass

### 4.7 Strategy Runtime Technology Choices

- Strategy SDK (Python):
  - Data structures: pydantic (validation/config) or dataclasses (lighter)
  - Research computation: numpy/pandas; minimize Python in hot paths
- Parallel model:
  - Multi-strategy: process isolation (stability) or thread isolation (low overhead)
  - Unified callbacks: on_event + on_timer; control plane handles start/stop and hot params
- Config and versions:
  - Strategy package management: Python venv/poetry (research); production can freeze into images

### 4.8 Backtest / Replay / Analytics Technology Choices

- Backtest engine:
  - Event replay: time-line driven (reuse MarketEvent/TradingEvent)
  - Matching: pluggable models (Kline matching, tick matching, book penetration, queue approximation)
- Compute engine:
  - Online metrics: realtime updates in core services (PnL/positions/risk)
  - Offline analytics: ClickHouse + SQL or Spark (larger scale)
- Report output:
  - MVP: HTML (frontend rendering) or JSON metrics + curve data
  - Later: server-side PDF rendering (optional)

### 4.9 Data Storage Choices

- Structured business data (orders/fills/accounts/audit): PostgreSQL
- High-frequency market + analytics queries: ClickHouse
- Cache and light queue: Redis
- Embedded state and local logs: RocksDB (optional)
- Object storage (cold archive): S3-compatible (MinIO / AWS S3)
  - C++ clients: libpqxx (PostgreSQL), ClickHouse C++ Client, redis-plus-plus (or hiredis)

### 4.10 AI Agent Bridge Technology Choices

- LLM/Agent integration:
  - Protocol: HTTP/gRPC (unified gateway) + streaming responses (SSE/WebSocket optional)
  - Tool calls: default to read-only data tools + auditable suggestion tools
- Vector retrieval (for review/knowledge base):
  - pgvector (PostgreSQL-integrated, simpler ops) or Milvus (larger scale)
- Permissions and audit:
  - Per-request logs: prompt summary, data scope, response summary, traceId
  - Redaction: account identifiers, API keys, sensitive user fields

### 4.11 API and Authentication

- External APIs:
  - Internal: gRPC (service-to-service)
  - External: REST (OpenAPI) or GraphQL (for UI aggregation)
- Auth:
  - JWT (short-lived) + Refresh Token (long-lived)
  - RBAC: role/permission matrix (strategy start/stop, manual trading, risk toggles)
- Rate limiting:
  - Nginx/Envoy edge rate limiting + finer-grained service limits

### 4.12 UI Technology Choices

- Frontend framework: React + TypeScript
- Charting and visualization:
  - Kline: TradingView Lightweight Charts (or ECharts)
  - Monitoring: ECharts / Recharts
- State management: Redux Toolkit / Zustand
- Realtime updates: WebSocket (or SSE) + incremental updates (avoid full refresh)
- Desktop (optional): Electron (ecosystem) or Qt 6 (native + high performance)

### 4.13 Observability and Ops

- Unified tracing: OpenTelemetry (Trace/Metrics/Logs)
- Metrics: Prometheus + Grafana
- Logs: Loki (lightweight) or Elasticsearch (stronger search)
- Alerts: Alertmanager (Grafana/Prometheus ecosystem)
- C++ instrumentation libraries:
  - Logs: spdlog
  - Metrics: prometheus-cpp (or StatsD/OTel Metrics)
  - Trace: opentelemetry-cpp
- Health checks:
  - Liveness/Readiness
  - Key dependencies: exchange connectivity, latency, queue backlog, reconciliation state

### 4.14 CI/CD and Delivery

- Build: GitHub Actions / GitLab CI
- Artifacts: Docker images + semantic versions
- Release:
  - Canary/rolling upgrades (Kubernetes)
  - Rollback: keep last stable image and config

### 4.15 Security and Key Management

- Key storage: cloud KMS (preferred) or HashiCorp Vault; local system keyring optional
- Runtime isolation:
  - Separate research and live trading environments (network/permissions/data)
  - Live API keys use separate accounts, least privilege, IP allowlist
- Audit: append-only storage for critical actions (PostgreSQL append-only table / object storage archive)
