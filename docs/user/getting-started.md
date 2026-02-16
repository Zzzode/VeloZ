# Getting Started

Welcome to VeloZ! This guide will help you get up and running quickly.

## Quick Start

The fastest way to try VeloZ is to build and run the gateway with the UI:

```bash
# Build the project (using development preset)
cmake --preset dev
cmake --build --preset dev -j

# Run the gateway (includes the web UI)
./scripts/run_gateway.sh dev
```

The gateway will start on `http://127.0.0.1:8080/`. Open this URL in your browser to access the trading UI.

---

## Environment Requirements

- **OS**: Linux, macOS, or Windows
- **CMake**: >= 3.24
- **Compiler**: C++23-capable (Clang 16+ or GCC 13+)
- **Python**: 3.x (for gateway, usually pre-installed)

### Dependencies (Automatically Fetched)

VeloZ automatically fetches these dependencies via CMake FetchContent:

- **KJ Library** (from Cap'n Proto) - Core C++ utilities
- **KJ Test** - Testing framework
- **yyjson** - High-performance JSON parser
- **OpenSSL** - TLS support for WebSocket

No manual dependency installation is required.

---

## Build Options

### Development Build

```bash
cmake --preset dev
cmake --build --preset dev -j
```

Output: `build/dev/apps/engine/veloz_engine`

### Release Build

```bash
cmake --preset release
cmake --build --preset release -j
```

Output: `build/release/apps/engine/veloz_engine`

### AddressSanitizer Build (for debugging)

```bash
cmake --preset asan
cmake --build --preset asan -j
```

---

## Running VeloZ

### Option 1: Run Engine Directly

```bash
./build/dev/apps/engine/veloz_engine
```

The engine starts in stdio mode and processes text commands (`ORDER ...`, `CANCEL ...`).

### Option 2: Run Gateway with Web UI (Recommended)

```bash
./scripts/run_gateway.sh dev
```

This starts:

- The C++ engine (in stdio mode)
- A Python HTTP gateway
- A web UI accessible at `http://127.0.0.1:8080/`

---

## Using the Web UI

1. Open `http://127.0.0.1:8080/` in your browser

2. The UI shows:
   - Current market data (BTC/USDT by default)
   - Order book view
   - Your recent orders
   - Account balance

3. Place an order:
   - Select Buy or Sell
   - Enter quantity and price
   - Click "Place Order"

4. Cancel an order:
   - Click "Cancel" next to any pending order

---

## API Endpoints

You can also interact with VeloZ via REST API:

```bash
# Health check
curl http://127.0.0.1:8080/health

# Get market data
curl http://127.0.0.1:8080/api/market

# Get account state
curl http://127.0.0.1:8080/api/account

# Get all orders
curl http://127.0.0.1:8080/api/orders_state

# Get specific order
curl "http://127.0.0.1:8080/api/order_state?client_order_id=web-1234567890"

# Place an order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{"side":"BUY","symbol":"BTCUSDT","qty":0.001,"price":50000.0}'

# Cancel an order
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{"client_order_id":"web-1234567890"}'
```

---

## Configuration

### Environment Variables

Configure VeloZ behavior using environment variables before running the gateway:

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset (dev/release/asan) |
| `VELOZ_MARKET_SOURCE` | `sim` | Market data: `sim` or `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol |
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution: `sim_engine` or `binance_testnet_spot` |

### Example: Use Real Binance Market Data

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
./scripts/run_gateway.sh dev
```

### Example: Trade on Binance Testnet

```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_api_key
export VELOZ_BINANCE_API_SECRET=your_api_secret
./scripts/run_gateway.sh dev
```

---

## Next Steps

- Read the [HTTP API Reference](../api/http_api.md) for detailed API documentation
- Check the [Backtest User Guide](backtest.md) for backtesting strategies
- Explore the [design documents](../design/) for architecture details
- Check the [implementation plans](../plans/) for development roadmap

---

## Troubleshooting

### Build fails with CMake version error

Ensure CMake >= 3.24 is installed:

```bash
cmake --version
```

### Engine executable not found

Make sure you ran the build command first:

```bash
cmake --preset dev
cmake --build --preset dev -j
```

### Gateway cannot start

Check if port 8080 is already in use:

```bash
# macOS/Linux
lsof -i :8080

# Or use a different port
export VELOZ_PORT=8081
./scripts/run_gateway.sh dev
```
