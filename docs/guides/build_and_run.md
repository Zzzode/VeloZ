# VeloZ Build and Run

This document describes local build, run, testing, formatting, and CI behavior for VeloZ. The repository includes a C++23 engine with multiple library modules and a static UI with comprehensive test coverage.

## 1. Environment Requirements

### 1.1 Required

- Linux / macOS (Windows works too, but this doc uses Unix-like examples)
- CMake >= 3.24
- Ninja build system
- A C++23-capable compiler
  - Linux: Clang 16+ or GCC 13+
  - macOS: Clang 16+ (via Xcode or Homebrew)

### 1.2 Optional

- clang-format (for `./scripts/format.sh`)
- Node.js >= 18 (for UI testing with Vitest)

## 2. Build with CMake Presets (Recommended)

The repository provides [CMakePresets.json](../../CMakePresets.json) with three configure presets and multiple build presets for targeted builds.

### 2.1 Configure Presets

| Preset | Description | Build Type | Output Directory |
|--------|-------------|------------|------------------|
| `dev` | Development build | Debug | `build/dev/` |
| `release` | Production build | Release | `build/release/` |
| `asan` | AddressSanitizer | Debug (Clang) | `build/asan/` |

### 2.2 Build Presets

Each configure preset has corresponding build presets for targeted builds:

| Build Preset | Targets | Description |
|--------------|---------|-------------|
| `dev-all` | all | Build everything (engine + libs + tests) |
| `dev-engine` | veloz_engine | Build only the engine executable |
| `dev-libs` | veloz_core, veloz_common, veloz_market, veloz_exec, veloz_oms, veloz_risk, veloz_strategy, veloz_backtest | Build all libraries |
| `dev-tests` | veloz_core_tests, veloz_market_tests, veloz_exec_tests, veloz_oms_tests, veloz_risk_tests, veloz_strategy_tests, veloz_backtest_tests | Build all test executables |

Replace `dev` with `release` or `asan` for the corresponding preset variants (e.g., `release-all`, `asan-tests`).

### 2.3 Build Commands

#### Build Everything (Recommended for First Build)

```bash
# Development build
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

# Build only libraries (no tests)
cmake --build --preset dev-libs -j$(nproc)

# Build only tests
cmake --build --preset dev-tests -j$(nproc)
```

### 2.4 Build Outputs

After a successful dev build:

- Engine executable: `build/dev/apps/engine/veloz_engine`
- Libraries: `build/dev/libs/<module>/libveloz_<module>.a`
- Test executables: `build/dev/libs/<module>/veloz_<module>_tests`
- Compile commands: `build/dev/compile_commands.json`

### 2.5 Convenience Wrapper Script

```bash
./scripts/build.sh dev      # Development build
./scripts/build.sh release  # Release build
./scripts/build.sh asan     # ASan build
```

## 3. Run Tests

### 3.1 C++ Test Suites (CTest)

VeloZ includes 16 test suites covering all modules:

| Test Suite | Module | Description |
|------------|--------|-------------|
| veloz_core_tests | core | Logger, config, time, event loop |
| veloz_market_tests | market | Market events, order book, quality analyzer |
| veloz_websocket_tests | market | WebSocket client tests |
| veloz_rest_client_tests | market | REST client tests |
| veloz_exec_tests | exec | Exchange adapters, reconciliation |
| veloz_oms_tests | oms | Order store, position management |
| veloz_risk_tests | risk | Risk metrics, thresholds, rule engine |
| veloz_strategy_tests | strategy | Strategy implementations |
| veloz_backtest_tests | backtest | Backtest engine, data sources |
| test_command_parser | engine | Command parsing tests |
| test_stdio_engine | engine | Stdio engine tests |
| test_http_service | engine | HTTP service tests |
| veloz_integration_backtest_tests | integration | Backtest integration tests |
| veloz_integration_wal_tests | integration | WAL recovery tests |
| veloz_integration_strategy_tests | integration | Strategy integration tests |
| veloz_integration_engine_tests | integration | Engine integration tests |

#### Run All Tests

```bash
# Configure and build first (if not already done)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Run all tests with CTest
ctest --preset dev -j$(nproc)
```

#### Run Individual Test Suites

```bash
# Run specific test executable directly
./build/dev/libs/core/veloz_core_tests
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/exec/veloz_exec_tests
./build/dev/libs/oms/veloz_oms_tests
./build/dev/libs/risk/veloz_risk_tests
./build/dev/libs/strategy/veloz_strategy_tests
./build/dev/libs/backtest/veloz_backtest_tests

# Run with verbose output
./build/dev/libs/risk/veloz_risk_tests --verbose
```

#### Run Tests by Pattern

```bash
# Run tests matching a pattern
ctest --preset dev -R "risk"      # Run risk-related tests
ctest --preset dev -R "strategy"  # Run strategy-related tests
ctest --preset dev -R "integration" # Run integration tests
```

### 3.2 UI Tests (Vitest + Playwright)

The UI module is a React 19.2 + TypeScript 5.9 application with comprehensive test coverage:
- **551 unit and integration tests** (Vitest)
- **33 E2E tests** (Playwright)

#### Setup (First Time)

```bash
cd apps/ui
npm install
```

#### Run Unit Tests

```bash
cd apps/ui

# Run all tests once
npm test

# Run tests in watch mode (for development)
npm test -- --watch

# Run tests with coverage report
npm run test:coverage
```

#### Run E2E Tests

```bash
cd apps/ui

# Install Playwright browsers (first time only)
npx playwright install

# Run E2E tests
npm run test:e2e

# Run E2E tests in UI mode (interactive)
npm run test:e2e:ui
```

#### Test Coverage

| Test Type | Count | Pass Rate |
|-----------|-------|-----------|
| Unit/Integration Tests | 551 | 100% |
| E2E Tests | 33 | 100% |
| **Total** | **584** | **100%** |

For detailed UI setup and testing instructions, see [UI Setup and Run Guide](./ui_setup_and_run.md).

## 4. Run Engine

### 4.1 Run Engine Directly

```bash
# Run in default mode
./build/dev/apps/engine/veloz_engine

# Run in stdio mode (for gateway integration)
./build/dev/apps/engine/veloz_engine --stdio

# Run in service mode (HTTP server)
./build/dev/apps/engine/veloz_engine --service --port 8081
```

### 4.2 Run via Script (Recommended)

```bash
./scripts/run_engine.sh dev
```

The script:
- Calls `./scripts/build.sh <preset>` first
- Runs the engine with a short smoke test (`timeout 3s`)

#### Dry Run Mode

To see what the script would do without actually executing:

```bash
./scripts/run_engine.sh --dry-run
./scripts/run_engine.sh dev --dry-run
```

### 4.3 Engine Service Mode

The engine supports HTTP service mode for direct API access:

```bash
# Start engine in service mode
./build/dev/apps/engine/veloz_engine --service --port 8081
```

Service mode endpoints:
- `GET /health` - Health check
- `GET /api/status` - Engine status
- `GET /api/strategies` - List strategies
- `POST /api/strategy/start` - Start a strategy
- `POST /api/strategy/stop` - Stop a strategy

### 4.4 Run Gateway and UI (Full Stack)

The gateway uses Python stdlib to start an HTTP service and bridges to the engine via stdio:

```bash
# Terminal 1: Start gateway
./scripts/run_gateway.sh dev
```

#### Dry Run Mode

To see what the script would do without actually executing:

```bash
./scripts/run_gateway.sh --dry-run
./scripts/run_gateway.sh dev --dry-run
```

Default listen address: `http://127.0.0.1:8080/`

```bash
# Terminal 2: Start UI development server
cd apps/ui
npm install  # First time only
npm run dev
```

The UI will be available at `http://localhost:5173/` and will connect to the gateway at `http://127.0.0.1:8080/`.

For detailed UI setup instructions, see [UI Setup and Run Guide](./ui_setup_and_run.md).

### 4.5 Gateway API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Health check |
| `/api/config` | GET | Engine configuration |
| `/api/market` | GET | Market data |
| `/api/orders` | GET | Order event list |
| `/api/orders_state` | GET | Merged order state view |
| `/api/order_state?client_order_id=...` | GET | Single order state |
| `/api/stream` | GET | SSE realtime event stream |
| `/api/execution/ping` | GET | Execution ping |
| `/api/account` | GET | Account information |
| `/api/order` | POST | Place order (JSON or form) |
| `/api/cancel` | POST | Cancel order |
| `/api/backtest/run` | POST | Run backtest |
| `/api/backtest/results` | GET | List backtest results |
| `/api/backtest/results/{id}` | GET | Get backtest result |

### 4.6 Switch Market Source (Binance REST Polling)

Market data is simulated by the engine by default (`sim`). Switch to Binance public REST polling:

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com
./scripts/run_gateway.sh dev
```

### 4.7 Switch Execution Channel (Binance Spot Testnet)

Order placement/cancel goes through local engine simulation by default (`sim_engine`). Switch to Binance Spot Testnet REST:

```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=...
export VELOZ_BINANCE_API_SECRET=...
./scripts/run_gateway.sh dev
```

Notes:
- The gateway does not print or echo the API secret
- The gateway automatically obtains a listenKey and connects to the user data stream (WS)
- To override the default WS URL:

```bash
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws
```

## 5. Common Scripts

### 5.1 Build Scripts

```bash
./scripts/build.sh dev      # Development build
./scripts/build.sh release  # Release build
./scripts/build.sh asan     # ASan build
```

### 5.2 Format Script

```bash
./scripts/format.sh
```

Notes:
- Scans `*.h/*.hpp/*.cc/*.cpp/*.cxx` under `apps/` and `libs/`
- Formats in place (`clang-format -i`)

## 6. CI Behavior

GitHub Actions workflow: [ci.yml](../../.github/workflows/ci.yml)

CI runs:
1. Configure with `cmake --preset dev`
2. Build with `cmake --build --preset dev-all -j`
3. Run all tests with `ctest --preset dev -j$(nproc)`

All 16 test suites must pass for CI to succeed.

## 7. FAQ

### 7.1 `cmake --preset` Not Found

Make sure your local CMake version is >= 3.19 (this repo requires >= 3.24).

### 7.2 `clang-format` Missing

The format script exits with an error. Install clang-format or skip the formatting script.

### 7.3 UI Tests Fail with "vitest not found"

Run `npm install` in the `apps/ui` directory first:

```bash
cd apps/ui
npm install
npm test
```

### 7.4 Build Fails with KJ/Cap'n Proto Errors

Ensure you have the KJ library installed. On macOS:

```bash
brew install capnproto
```

On Ubuntu/Debian:

```bash
sudo apt-get install libcapnp-dev
```

### 7.5 Tests Timeout

Increase the timeout in CTest:

```bash
ctest --preset dev --timeout 120
```

Or run individual test executables directly for more control.
