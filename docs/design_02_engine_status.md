# VeloZ Crypto Quant Trading Framework: Engine Implementation Status

## 3.0 Implementation Status and Best Practices

This repository is an early-stage open-source skeleton, not a full production-grade engine yet. The current engine focuses on a minimal, inspectable event loop (market → order → fill) and basic OMS/account aggregation to validate end-to-end flow. The goal of this section is to define how each module should be implemented to reach industry-grade reliability and performance.

Best-practice principles (implementation-oriented):

- Deterministic state machines: order/position/account must be driven by a single source of truth, and events must be replayable.
- Separation of fast-path vs slow-path: market/order handling uses lock-free or minimal-lock data structures; reconciliation and reporting run off the fast path.
- Observability by default: every critical state transition must have timestamped events for audit and replay.
- Backpressure & drop policies: UI subscriptions and low-priority streams must not impact trading or risk checks.
- Idempotency everywhere: every external interaction (REST/WS) and internal event application must be safely repeatable.

### 3.0.1 Engine Implementation Breakdown (Target)

- apps/engine
  - Process lifecycle: signal handling, clean shutdown, watchdogs, and health checks
  - Engine application shell: configuration, mode selection, and lifecycle orchestration
  - Stdio simulation runtime: command parsing, market simulation, and fill simulation
  - State aggregation: order state + account state in-memory, with periodic snapshot/export
  - Event emission: structured market/order/fill/account events
- libs/core
  - Event loop and task scheduling
  - Time utilities and monotonic clock wrappers
  - Logging and structured trace scaffolding
- libs/market
  - Market event normalization and adapters
  - Book builder (snapshot + delta, depth aggregation)
  - Market subscriptions and fan-out
- libs/exec
  - Execution interface, exchange routing, and adapter boundary
  - Idempotent order placement/cancel semantics
- libs/oms
  - Order state machine, position and account aggregation
  - Reconciliation hooks and state export
- libs/risk
  - Pre-trade checks (funds/limits/price protection)
  - Post-trade validation and circuit breakers

### 3.0.2 Engine Framework (Current)

- apps/engine/src/main.cpp
  - Argument parsing and process bootstrapping
- apps/engine/src/engine_app.cpp
  - Mode switch (stdio vs service) and lifecycle management
- apps/engine/src/stdio_engine.cpp
  - Stdio simulation loop with market and fill threads
- apps/engine/src/engine_state.cpp
  - Order state aggregation, account balance updates, pending order management
- apps/engine/src/command_parser.cpp
  - Text command parsing for ORDER/CANCEL
- apps/engine/src/event_emitter.cpp
  - JSON event emission (market, order_update, fill, order_state, account, error)

### 3.0.3 Engine Implementation Roadmap (Concrete)

- Replace stdio command parsing with a binary or gRPC command channel
- Introduce an internal event bus (SPSC/MPSC ring buffers) for market and execution events
- Add a deterministic simulator with pluggable matching models (market/limit, latency, slippage)
- Implement full order journal and recovery replay (WAL + replay on startup)
- Implement account reconciliation loop (REST snapshot + WS delta) with freeze-on-divergence policy
