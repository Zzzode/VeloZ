# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

VeloZ is an early-stage quantitative trading framework for crypto markets.

- **C++23 core engine** (`apps/engine`) + reusable libraries (`libs/*`)
- **Python gateway** (`apps/gateway/gateway.py`) that bridges the engine to an HTTP API
- **Static UI** (`apps/ui/index.html`) that talks to the gateway via REST + SSE

The current implementation is a minimal end-to-end skeleton: buildable engine, a gateway that spawns the engine in stdio mode, and a small UI for inspection.

Additional path-scoped Claude Code rules live in `.claude/rules/`.

- English only across the project

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
| Coverage (GCC) | `coverage` | `coverage-all` | all | Build with coverage instrumentation |
| Coverage (GCC) | `coverage` | `coverage-tests` | all test targets | Build only tests with coverage |

### Coverage

Generate code coverage reports locally:

```bash
./scripts/coverage.sh
```

This builds with the `coverage` preset, runs all tests, and generates an HTML report in `coverage_html/`.

Coverage is automatically collected in CI and uploaded to Codecov. See `docs/guides/codecov-setup.md` for setup instructions.

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
pip3 install -r apps/gateway/requirements.txt
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

- REST-style JSON endpoints (see `docs/guides/build_and_run.md` for the list).
- Server-Sent Events stream at `GET /api/stream` for realtime updates.

## Pointers to docs

**Getting Started:**
- `docs/guides/user/getting-started.md`: quick start guide
- `docs/guides/user/installation.md`: installation instructions
- `docs/guides/user/configuration.md`: environment variables and settings
- `docs/guides/user/faq.md`: frequently asked questions
- `docs/guides/user/glossary.md`: trading and technical terms

**Operations:**
- `docs/guides/user/trading-guide.md`: trading operations and order management
- `docs/guides/user/monitoring.md`: Prometheus, Grafana, observability
- `docs/guides/user/troubleshooting.md`: common issues and solutions
- `docs/guides/user/security-best-practices.md`: security configuration
- `docs/performance/latency_optimization.md`: performance tuning guide

**Development:**
- `docs/guides/build_and_run.md`: build/run/scripts and gateway endpoints
- `docs/design/README.md`: design series index
- `docs/tutorials/custom-strategy-development.md`: strategy development guide
- `docs/references/kjdoc/`: KJ library documentation (tour.md, style-guide.md, library_usage_guide.md)
- `.claude/skills/kj-library/library_usage_guide.md`: KJ library usage patterns for VeloZ
- `.claude/skills/kj-library/SKILL.md`: KJ skill (name: `kj-library`) for Claude Code recognition

## Available Skills

The following skills are automatically available to guide development:

| Skill | When Used |
|-------|-----------|
| `architecture` | Cross-module design, contracts, system structure |
| `cpp-engine` | C++ code in apps/engine or libs/* |
| `kj-library` | KJ types (kj::Own, kj::Maybe, kj::Promise), async I/O |
| `market-data` | Market module, order book, WebSocket patterns |
| `testing` | KJ Test framework and test conventions |
| `build-ci` | CMake presets, build configs, CI workflow |
| `encoding-style` | Code style, naming, formatting conventions |
| `gateway` | Python gateway, HTTP/SSE API behavior |
| `ui` | Static UI and gateway integration |

Skills are automatically invoked when relevant. Use `/skill-name` to invoke manually.

## KJ Library Usage (CRITICAL - DEFAULT CHOICE)

**VeloZ strictly uses KJ library from Cap'n Proto as the default choice over C++ standard library.** This is a project-wide architectural decision that applies to all new code.

### KJ Skill for Detailed Reference

**For comprehensive KJ library guidance, invoke the `kj-library` skill defined in `.claude/skills/kj-library/SKILL.md`**. The skill contains:

- Complete type mappings (std → KJ)
- Event loop and async I/O patterns
- Network operations
- Memory management patterns
- Code examples and migration guides
- System requirements (C++20+, Clang 14.0+/GCC 14.3+)

### Quick KJ Reference

| Category | KJ (Default) | Std (Only when KJ unavailable) |
|----------|--------------|----------------------------------|
| **Owned pointers** | `kj::Own<T>` with `kj::heap<T>()` | `std::unique_ptr` only if KJ not applicable |
| **Nullable values** | `kj::Maybe<T>` with `KJ_IF_SOME` | `std::optional` only for external API compatibility |
| **Strings** | `kj::String`, `kj::StringPtr`, `"_kj"` | `std::string` only for external APIs requiring it |
| **Dynamic arrays** | `kj::Array<T>`, `kj::Vector<T>` | `std::vector` only for algorithms requiring iterator semantics |
| **Thread sync** | `kj::Mutex`, `kj::MutexGuarded<T>` | `std::mutex` only if KJ not suitable |
| **Functions/callables** | `kj::Function<T>` | `std::function` only if KJ not applicable |
| **Exceptions** | `kj::Exception`, `KJ_ASSERT`, etc. | `std::exception` only for external exception compatibility |
| **Async patterns** | `kj::Promise<T>` with `.then()` | `std::future`, `std::promise` only if required by external API |

### Mandatory Rule

**ALWAYS check KJ first before considering std library types.** Only use std types when:

1. KJ does not provide equivalent functionality
2. External API compatibility requires std types
3. Third-party library integration requires std types

When using std types, add a comment explaining why KJ was not suitable.

### Known std Library Requirements (Cannot Migrate)

Based on the KJ migration analysis, the following std library usages are required and cannot be migrated:

| std Type | Reason | Example Files |
|----------|--------|---------------|
| `std::string` | OpenSSL HMAC API | exec/binance_adapter.cpp, hmac_wrapper.cpp |
| `std::string` | yyjson C API | core/json.cpp |
| `std::string` | Copyable structs (kj::String not copyable) | market/market_quality.h (Anomaly) |
| `std::format` | Width specifiers ({:04d}, {:02d}) | core/logger.cpp, core/time.cpp |
| `std::filesystem` | File path operations | core/config_manager.cpp, backtest/data_source.cpp |
| `std::unique_ptr` | Custom deleters (kj::Own lacks support) | core/memory.h (ObjectPool) |
| `std::unique_ptr` | Polymorphic ownership | core/config_manager.h, core/logger.h |
| `std::function` | STL container compatibility | core/event_loop.h |
| `std::function` | Recursive lambdas | backtest/optimizer.cpp |
| `std::map` | Ordered iteration | core/metrics.h (Prometheus export) |
| `std::vector` | API return types | core/json.h, core/metrics.h |

### KJ Limitations

- `kj::str()` does not support width/precision specifiers (use `std::format` for `{:04d}`, `{:.2f}`)
- `kj::Own` does not support custom deleters or `.release()` method
- `kj::String` is not copyable (use `std::string` for copyable structs)
- `kj::Function` is not copyable (structs containing it must be move-only)

## Documentation conventions

The `docs/` directory is a collection of design and usage documents.

- **Language**: English only across the project
- **Responsibilities**: capture architectural, interface, and constraint evolution
- **Change note**: update documentation alongside architecture or protocol changes
