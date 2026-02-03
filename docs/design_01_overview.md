# VeloZ Crypto Quant Trading Framework: Overview

## 1. Goals and Scope

### 1.1 Goals

- Cover market data and trading for major exchanges (with Binance as baseline), including spot, USDT-margined perpetuals, and coin-margined perpetuals
- Low-latency, high-throughput market data ingestion and execution, supporting HFT/market-making/grid strategies
- Unified strategy runtime for backtest/simulation/live trading to reduce migration cost
- Complete analytics and risk loop: PnL, risk, positions, trade quality, backtest reports
- AI Agent integration: market interpretation, strategy suggestions, trade review, anomaly detection and explanation
- Visual trading UI: monitoring, strategy management, manual intervention, risk status, audit tracing

### 1.2 Non-goals (for future iterations)

- Not targeting cross-chain/on-chain DeFi trading first (on-chain executors can be added later)
- Not defaulting to fully automated unattended AI trading (human-auditable and controllable by default)
- Avoid hard dependency on a single language/framework; prioritize core-path performance and maintainability

### 1.3 Core Principles

- Event-driven: market, orders, fills, accounts, and risk flow through events
- Layered decoupling: exchange adapters, core services, strategy layer, and presentation layer are loosely coupled
- Consistency first: order state machine, position/fund reconciliation, idempotency, and replayability
- Observability first: end-to-end latency, loss/reconnect, execution receipts, error codes, and retry strategies are traceable
- Security first: least-privilege keys, isolation, signatures, and audit logs

## 2. Architecture Overview

### 2.1 Logical Architecture (Recommended: Modular Monolith + Split Services)

A modular monolith accelerates early iteration and consistency. When throughput or isolation needs grow, split the following modules into services (interfaces remain stable):

- Market Data Service
- Execution Gateway
- OMS / Portfolio
- Strategy Runtime
- Backtest & Replay
- Analytics (PnL/Risk)
- AI Agent Bridge
- UI & API

Core components communicate via two channels:

- In-process: high-performance memory queues/ring buffers (strategy runtime and market/execution events)
- Cross-process: gRPC (control plane) + message bus (data plane)

### 2.2 Data Plane and Control Plane

- Data plane (high-frequency events): Tick, OrderBook, Kline, Trade, Funding, MarkPrice, OrderUpdate, Fill, BalanceUpdate
  - Goals: low latency, stable, backpressure-aware, droppable low-priority events (e.g., UI subscriptions)
- Control plane (low-frequency commands): start/stop strategies, parameter updates, risk toggles, subscription management, permissions
  - Goals: strong consistency, auditability, retryable, stable interfaces

### 2.3 Monorepo Architecture

VeloZ uses a monorepo for a multi-language stack (C++23 core engine + Python strategy ecosystem + Web UI), aiming for unified contracts, reproducible builds, and controlled cross-module dependencies.

#### 2.3.1 Directory Layout (Recommended)

```text
VeloZ/
  apps/
    engine/                  # C++23: core engine process (market + execution + OMS + risk)
    gateway/                 # C++23: external API gateway/BFF (optional, or merge with engine)
    backtest/                # C++23 or Python: backtest/replay task executor (splittable)
    ui/                      # Web: trading UI (React)
  services/
    ai-bridge/               # AI Agent Bridge (scalable independently)
    analytics/               # analytics/reporting service (scalable independently)
  libs/
    core/                    # C++23: infrastructure (time, logging, config, pools, ringbuffer)
    market/                  # C++23: market normalization, subscription aggregation, OrderBook
    exec/                    # C++23: exchange adapters, order placement/cancel, receipts
    oms/                     # C++23: order state machine, positions/funds, reconciliation
    risk/                    # C++23: risk rule engine
    common/                  # cross-module shared code (no business coupling)
  proto/                     # gRPC/Protobuf: control-plane and cross-service contracts (single source of truth)
  python/
    strategy-sdk/            # Python: strategy SDK (subscriptions, callbacks, order abstraction)
    research/                # Python: research and experiments (features, training, backtest scripts)
  web/
    ui/                      # React frontend source (keep only one if apps/ui is used)
  infra/
    docker/                  # Dockerfile, compose, image publishing
    k8s/                     # Kubernetes manifests (optional)
  tools/
    codegen/                 # Protobuf code generation and verification
    lint/                    # unified lint/format entry
  docs/
    crypto_quant_framework_design.md
```

Directories can be adjusted per team preference, but keep three hard boundaries:

- Deployable units live in apps/ or services/
- Reusable libraries live in libs/
- Contracts live in proto/ as the single source of truth

#### 2.3.2 Dependency Rules (Strongly Recommended)

- apps/ and services/ may depend on libs/ and proto/; libs/ depend only in one direction (e.g., market → core, exec → core, oms → core) to avoid cycles
- proto/ must not depend on any business implementation code; generated code goes to each language module (C++/Python/TS)
- Python strategies depend only on the stable SDK, not engine internals, to avoid breaking live trading on engine upgrades
- UI depends only on external API/WS contracts, not internal database schemas

#### 2.3.3 Build and Tooling (Multi-language View)

- C++23:
  - CMake (primary build), recommend CMake Presets to lock compiler/flags/build type
  - vcpkg or Conan (dependency management), recommend pinning versions for reproducible builds
- Python:
  - Maintain strategy-sdk as an independent package with versioning for decoupled upgrades
  - Separate research code from production strategies (research/ vs deployable strategy packages)
- Web:
  - TypeScript; build tool can be Vite/Next.js (depending on SSR needs)
- Code generation:
  - proto/ changes trigger C++/Python/TS codegen with CI consistency checks to avoid drift

#### 2.3.4 Versioning and Release (Recommended)

- Engine and services: independent semantic versions (engine vX.Y.Z, ai-bridge vA.B.C) to avoid monolithic release cadence
- Contract versions: breaking changes in proto/ should be handled by versioned packages/namespaces or backward-compatible fields
- Strategy SDK: independent versioning and changelog; allow strategy runtime to lock SDK versions

#### 2.3.5 CI Design (Recommended)

- Change impact analysis:
  - C++ changes trigger builds/tests only for relevant libs/ and apps/
  - proto/ changes trigger full codegen verification and downstream compilation
  - web/ and python/ have independent lint/test pipelines
- Quality gates:
  - Formatting (clang-format / black / prettier)
  - Static analysis (clang-tidy / ruff / eslint)
  - Unit tests + key integration tests (order idempotency, order state machine, reconciliation consistency)
