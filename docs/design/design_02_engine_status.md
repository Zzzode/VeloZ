# VeloZ Crypto Quant Trading Framework: Engine Implementation Status

## 3.1 Implementation Status (Current - Feb 2026)

The engine is a minimal stdio-driven runtime with a small in-process state machine and NDJSON
event emission. The service mode exists as a placeholder loop. Integration is done through the
Python gateway that spawns the engine in stdio mode.

### 3.1.1 Project Status Summary

**Current Phase:** Minimal end-to-end skeleton (engine + gateway + UI)

**Implemented (Current):**
- ✅ Stdio command parsing for ORDER/CANCEL/QUERY
- ✅ Stdio event output (`engine_started`, `order_received`, `cancel_received`, `query_received`)
- ✅ EngineState with balances, order reservation, pending fills, and order state updates
- ✅ Risk checks via `libs/risk` wired into order placement
- ✅ NDJSON event helpers for market/order/fill/account/error
- ✅ Gateway stdio bridge + REST + SSE stream

**In Progress / Partial:**
- ⚠️ Service mode runtime (placeholder loop + heartbeat)
- ⚠️ Exchange integrations (Binance REST optional; WS scaffolding only)
- ⚠️ Persistence and recovery (no WAL/replay yet)
- ⚠️ Strategy runtime integration

### 3.1.2 Runtime Behavior (Current)

- Engine is launched with `--stdio`
- Commands arrive via stdin; events are emitted to stdout as NDJSON
- Orders are accepted/rejected by `EngineState` and risk checks
- Pending orders simulate fills using due timestamps
- Order/account snapshots are derived from in-memory state

### 3.1.3 Engine Implementation Breakdown (Current)

#### apps/engine
- **main.cpp:** Argument parsing, enables stdio mode
- **engine_app.cpp:** stdio vs service mode, signal handling
- **stdio_engine.cpp:** Parses commands and emits command events
- **command_parser.cpp:** ORDER/CANCEL/QUERY parsing
- **engine_state.cpp:** Order acceptance, cancellations, pending fills, balances
- **event_emitter.cpp:** NDJSON emission helpers for market/order/fill/account/error

#### libs/core
- Event loop, logger, time utilities, config manager, JSON wrapper, metrics, memory utilities, error
  types

#### libs/oms / libs/risk / libs/exec / libs/market
- **OMS:** Order records and positions (basic)
- **Risk:** Risk engine and circuit breaker (basic checks)
- **Exec:** Order request/response models, client order IDs, adapter scaffolding
- **Market:** Market event types, order book scaffolding, subscription manager, WebSocket scaffold

### 3.1.4 Engine ↔ Gateway Integration

- Gateway starts the engine with `--stdio` and sends ORDER/CANCEL commands
- Engine emits NDJSON events consumed by the gateway and exposed over SSE
- REST endpoints and protocol details are documented in `docs/build_and_run.md` and
  `docs/api/engine_protocol.md`

### 3.1.5 Gaps vs Design

- Service mode networking stack is not implemented
- WebSocket market ingestion and order book rebuild are pending
- Persistence (order WAL + replay) is not implemented
- Strategy runtime/SDK is not integrated into the engine
