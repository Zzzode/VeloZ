# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

VeloZ is an early-stage quantitative trading framework for crypto markets.

- **C++23 core engine** (`apps/engine`) + reusable libraries (`libs/*`)
- **Python gateway** (`apps/gateway/gateway.py`) that bridges the engine to an HTTP API
- **Static UI** (`apps/ui/index.html`) that talks to the gateway via REST + SSE

The current implementation is a minimal end-to-end skeleton: buildable engine, a gateway that spawns the engine in stdio mode, and a small UI for inspection.

Additional path-scoped Claude Code rules live in `.claude/rules/`.

## Common commands

### Build (CMake Presets)

```bash
cmake --preset dev
cmake --build --preset dev -j
```

Other presets (see `CMakePresets.json`): `release`, `asan`.

Convenience wrapper:

```bash
./scripts/build.sh dev
```

### Run (smoke tests)

Run engine directly (after dev build):

```bash
./build/dev/apps/engine/veloz_engine
```

Run engine via script (builds first; runs a short `timeout 3s` smoke test):

```bash
./scripts/run_engine.sh dev
```

Run gateway + UI (builds first; starts `apps/gateway/gateway.py`):

```bash
./scripts/run_gateway.sh dev
```

Gateway listens on `http://127.0.0.1:8080/`. If opening `apps/ui/index.html` via `file://`, set the UI “API Base” to `http://127.0.0.1:8080`.

### Format (C++)

```bash
./scripts/format.sh
```

### Tests / CI

There are currently **no CTest-based unit tests** (no `ctest`, `enable_testing()`, or `add_test()` usage).

CI runs a configure/build + engine smoke run:

```bash
cmake --preset dev
cmake --build --preset dev -j
timeout 3s ./build/dev/apps/engine/veloz_engine || true
```

(See `.github/workflows/ci.yml`.)

## Architecture and data flow

### Top-level module layout

- `apps/engine`: C++ engine executable.
- `libs/core`: infrastructure (logging/time/event-loop primitives).
- `libs/market`: market event model (placeholder/early).
- `libs/exec`: execution/trading interface model (placeholder/early).
- `libs/oms`: order state + aggregation (minimal implementation).
- `libs/risk`: risk checks (minimal implementation).
- `apps/gateway`: Python HTTP gateway (BFF) that spawns and controls engine.
- `apps/ui`: static HTML UI.

Top-level CMake includes: `libs/{common,core,market,exec,oms,risk}` and `apps/engine`.

### Engine entrypoints

- `apps/engine/src/main.cpp`: parses flags (notably `--stdio`) and starts `EngineApp`.
- `apps/engine/src/engine_app.cpp`: selects `run_stdio()` vs `run_service()`.
- `apps/engine/src/stdio_engine.cpp`: stdio loop; parses inbound commands and emits events.
- `apps/engine/src/command_parser.cpp`: parses `ORDER ...` / `CANCEL ...` text commands.
- `apps/engine/src/event_emitter.cpp`: emits newline-delimited JSON events (NDJSON).

### Gateway/UI protocol (current)

**Engine ↔ Gateway** (process boundary):
- Gateway → Engine: plain-text commands over **stdin** (`ORDER ...`, `CANCEL ...`).
- Engine → Gateway: **NDJSON** over **stdout**; each line is a JSON object with a `type` field.

**Gateway ↔ UI**:
- REST-style JSON endpoints (see `docs/build_and_run.md` for the list).
- Server-Sent Events stream at `GET /api/stream` for realtime updates.

## Pointers to docs

- `docs/build_and_run.md`: build/run/scripts and gateway endpoints
- `docs/crypto_quant_framework_design.md`: design series index
