# VeloZ Build and Run

This document describes local build, run, formatting, and CI behavior for VeloZ. The repository currently focuses on a C++23 engine skeleton, with more modules to be added incrementally.

## 1. Environment Requirements

### 1.1 Required

- Linux / macOS (Windows works too, but this doc uses Unix-like examples)
- CMake >= 3.24
- A C++23-capable compiler
  - Linux: Clang 16+ or GCC 13+ (depending on what is available on your system)

### 1.2 Optional

- clang-format (for `./scripts/format.sh`)

## 2. Build with CMake Presets (Recommended)

The repository provides [CMakePresets.json](../CMakePresets.json):

- dev: Debug + generate compile_commands.json
- release: Release + generate compile_commands.json
- asan: Clang + ASan/UBSan (local self-check)

### 2.1 Dev Build

```bash
cmake --preset dev
cmake --build --preset dev -j
```

Build outputs are located by default at:

- `build/dev/`
- Engine executable: `build/dev/apps/engine/veloz_engine`

### 2.2 Release Build

```bash
cmake --preset release
cmake --build --preset release -j
```

### 2.3 ASan Self-Check Build (Optional)

```bash
cmake --preset asan
cmake --build --preset asan -j
```

## 3. Run

### 3.1 Run Engine Directly

```bash
./build/dev/apps/engine/veloz_engine
```

The engine is currently a smoke-test skeleton: it periodically prints heartbeat and exits gracefully on SIGINT/SIGTERM.

### 3.2 Run via Script (Recommended)

```bash
./scripts/run_engine.sh dev
```

The script does the following:

- Calls `./scripts/build.sh <preset>` first
- Runs the engine and uses `timeout` for a short smoke test

### 3.3 Run Gateway and UI (Minimal Integration)

The gateway uses Python stdlib to start an HTTP service and bridges to the engine via stdio:

```bash
./scripts/run_gateway.sh dev
```

Default listen address:

- `http://127.0.0.1:8080/`

If you open `apps/ui/index.html` directly via `file://` (e.g. IDE preview), set `API Base` at the top of the page to `http://127.0.0.1:8080`, or set it via URL params:

- `.../index.html?api=http://127.0.0.1:8080`

Available endpoints:

- `GET /health`
- `GET /api/config`
- `GET /api/market`
- `GET /api/orders`
- `GET /api/orders_state` (order state view)
- `GET /api/order_state?client_order_id=...`
- `GET /api/stream` (SSE: realtime event stream)
- `GET /api/execution/ping`
- `GET /api/account`
- `POST /api/order` (JSON or x-www-form-urlencoded)
- `POST /api/cancel` (cancel order)

Order state view notes:

- `orders` is an event list (useful for debugging and replay)
- `orders_state` is a merged state view (for UI/strategy queries), derived from `fill` into `PARTIALLY_FILLED/FILLED`

### 3.4 Switch Market Source (Optional: Binance REST Polling)

Market data is simulated by the engine by default (`sim`). You can switch to Binance public REST polling via env vars before starting the gateway:

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com
./scripts/run_gateway.sh dev
```

### 3.5 Switch Execution Channel (Optional: Binance Spot Testnet REST + User Stream)

Order placement/cancel goes through local engine simulation by default (`sim_engine`). You can switch to Binance Spot Testnet REST (requires API key/secret):

```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=...
export VELOZ_BINANCE_API_SECRET=...
./scripts/run_gateway.sh dev
```

Notes:

- For security, the gateway does not print or echo the API secret
- The gateway automatically obtains a listenKey and connects to the user data stream (WS) to receive `executionReport` (order/fill reports) and `outboundAccountPosition` (balance updates), and exposes:
  - `GET /api/account`
- SSE: `event: account`
To override the default WS URL, set:

```bash
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws
```

## 4. Common Scripts

### 4.1 Build Scripts

```bash
./scripts/build.sh dev
./scripts/build.sh release
./scripts/build.sh asan
```

Script: [build.sh](../scripts/build.sh)

### 4.2 Format Script

```bash
./scripts/format.sh
```

Script: [format.sh](../scripts/format.sh)

Notes:

- Scans `*.h/*.hpp/*.cc/*.cpp/*.cxx` under `apps/` and `libs/` by default
- Formats in place (`clang-format -i`)

## 5. CI Behavior

GitHub Actions workflow: [ci.yml](../.github/workflows/ci.yml)

Current CI does three things:

- Configure with `cmake --preset dev`
- Build with `cmake --build --preset dev -j`
- Run engine smoke test (`timeout 3s`)

## 6. FAQ

### 6.1 `cmake --preset` Not Found

Make sure your local CMake version is >= 3.19 (this repo requires >= 3.24).

### 6.2 `clang-format` Missing

The format script exits with an error. Install clang-format or skip the formatting script for now.
