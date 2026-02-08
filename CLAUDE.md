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

#### Configure and Build All Targets

```bash
# Development build (Debug)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Release build
cmake --preset release
cmake --build --preset release-all -j$(nproc)

# ASan build (for memory error detection)
cmake --preset asan
cmake --build --preset asan-all -j$(nproc)
```

#### Build Specific Targets

```bash
# Build only the engine executable
cmake --preset dev
cmake --build --preset dev-engine -j$(nproc)

# Build only libraries
cmake --preset dev
cmake --build --preset dev-libs -j$(nproc)

# Build only tests
cmake --preset dev
cmake --build --preset dev-tests -j$(nproc)

# Release and ASan presets have similar target-specific presets
# e.g., release-engine, release-libs, release-tests, asan-engine, etc.
```

#### Convenience Wrapper

```bash
./scripts/build.sh dev
```

#### Available Presets and Targets

| Preset Type | Configure Preset | Build Preset | Targets | Description |
|-------------|-----------------|--------------|---------|-------------|
| Development | `dev` | `dev-all` | all | Build everything in Debug mode |
| Development | `dev` | `dev-engine` | veloz_engine | Build only engine executable |
| Development | `dev` | `dev-libs` | core, common, market, exec, oms, risk, strategy | Build all libraries |
| Development | `dev` | `dev-tests` | market_tests, exec_tests, oms_tests, risk_tests, strategy_tests | Build all tests |
| Release | `release` | `release-all` | all | Build everything in Release mode |
| Release | `release` | `release-engine` | veloz_engine | Build only engine executable |
| Release | `release` | `release-libs` | core, common, market, exec, oms, risk, strategy | Build all libraries |
| Release | `release` | `release-tests` | market_tests, exec_tests, oms_tests, risk_tests, strategy_tests | Build all tests |
| ASan (Clang) | `asan` | `asan-all` | all | Build with AddressSanitizer for debugging |
| ASan (Clang) | `asan` | `asan-engine` | veloz_engine | Build engine with AddressSanitizer |
| ASan (Clang) | `asan` | `asan-libs` | core, common, market, exec, oms, risk, strategy | Build libraries with AddressSanitizer |
| ASan (Clang) | `asan` | `asan-tests` | market_tests, exec_tests, oms_tests, risk_tests, strategy_tests | Build tests with AddressSanitizer |

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

#### Run Tests

```bash
# Configure and build first (if not already done)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Run all tests with CTest
ctest --preset dev -j$(nproc)

# Or run tests directly (after building)
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/exec/veloz_exec_tests
./build/dev/libs/oms/veloz_oms_tests
./build/dev/libs/risk/veloz_risk_tests
./build/dev/libs/strategy/veloz_strategy_tests
```

#### CI Configuration

CI runs all tests using the `dev` preset:

```bash
cmake --preset dev
cmake --build --preset dev-all -j
ctest --preset dev -j$(nproc)
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
