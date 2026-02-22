# Getting Started

Welcome to VeloZ! This guide will help you get up and running quickly with the complete quantitative trading framework.

## Quick Start

The fastest way to try VeloZ is to build and run the gateway with the UI:

```bash
# Build the project (using development preset)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Run the gateway (includes the web UI)
./scripts/run_gateway.sh dev
```

The gateway will start on `http://127.0.0.1:8080/`. Open this URL in your browser to access the trading UI.

---

## Prerequisites

### System Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| **OS** | Linux, macOS, Windows | Linux (Ubuntu 22.04+) or macOS 13+ |
| **CMake** | 3.24 | 3.28+ |
| **Compiler** | Clang 16+ or GCC 13+ | Clang 18+ |
| **RAM** | 4 GB | 8 GB+ |
| **Disk** | 2 GB | 5 GB+ |
| **Python** | 3.8+ | 3.11+ |
| **Node.js** | 18+ (for UI tests) | 20+ |

### Dependencies (Automatically Fetched)

VeloZ automatically fetches these dependencies via CMake FetchContent:

- **KJ Library** (from Cap'n Proto) - Core C++ utilities, async I/O
- **KJ Test** - Testing framework
- **yyjson** - High-performance JSON parser
- **OpenSSL** - TLS support for WebSocket connections

No manual dependency installation is required.

### Optional Dependencies

| Dependency | Purpose | Installation |
|------------|---------|--------------|
| clang-format | Code formatting | `brew install clang-format` (macOS) |
| Node.js | UI testing | `brew install node` (macOS) |
| Ninja | Faster builds | `brew install ninja` (macOS) |

---

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/your-org/veloz.git
cd veloz
```

### 2. Build the Project

```bash
# Configure (creates build directory)
cmake --preset dev

# Build all targets (engine, libraries, tests)
cmake --build --preset dev-all -j$(nproc)
```

### 3. Verify Installation

```bash
# Run all tests
ctest --preset dev -j$(nproc)

# Expected output: 16 tests passed
```

### 4. Start the Gateway

```bash
./scripts/run_gateway.sh dev
```

Open `http://127.0.0.1:8080/` in your browser.

---

## Component Overview

VeloZ consists of several components that work together:

```
+----------------+     +----------------+     +----------------+
|    Web UI      | <-> |    Gateway     | <-> |    Engine      |
|  (Browser)     |     |   (Python)     |     |    (C++)       |
+----------------+     +----------------+     +----------------+
                              |
                              v
                       +----------------+
                       |   Exchange     |
                       |   Adapters     |
                       +----------------+
                              |
                              v
                       +----------------+
                       |   Binance/     |
                       |   OKX/Bybit    |
                       +----------------+
```

| Component | Description | Location |
|-----------|-------------|----------|
| **Engine** | C++ core with strategies, risk, OMS | `apps/engine/` |
| **Gateway** | Python HTTP/SSE bridge | `apps/gateway/` |
| **Web UI** | Static HTML/JS interface | `apps/ui/` |
| **Libraries** | Reusable modules | `libs/` |

### Library Modules

| Module | Description | Key Features |
|--------|-------------|--------------|
| `core` | Infrastructure | Logger, config, time, event loop |
| `common` | Shared types | Venues, symbols, enums |
| `market` | Market data | Order book, kline aggregator, quality analyzer |
| `exec` | Execution | Exchange adapters (Binance, OKX, Bybit, Coinbase), reconciliation |
| `oms` | Order management | Order store, position tracking |
| `risk` | Risk management | Metrics, dynamic thresholds, rule engine |
| `strategy` | Trading strategies | Trend following, mean reversion, momentum, grid |
| `backtest` | Backtesting | Engine, data sources, optimizer |

---

## Quick Start Guides

### A. Run with Simulated Data (Default)

Perfect for learning and development:

```bash
./scripts/run_gateway.sh dev
```

Features:
- Simulated market data (no API keys needed)
- Simulated order execution
- Full UI functionality

### B. Run with Live Binance Market Data

Use real market data with simulated execution:

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
./scripts/run_gateway.sh dev
```

### C. Run with Binance Testnet Trading

Trade on Binance testnet (requires API keys):

```bash
# Get testnet API keys from https://testnet.binance.vision/
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
./scripts/run_gateway.sh dev
```

### D. Run a Backtest

Test a strategy against historical data:

```bash
# Via UI: Navigate to Backtest tab, configure parameters, click "Run"

# Via API:
curl -X POST http://127.0.0.1:8080/api/backtest/run \
  -H "Content-Type: application/json" \
  -d '{
    "strategy": "trend_following",
    "symbol": "BTCUSDT",
    "start_time": 1704067200000,
    "end_time": 1706745600000,
    "initial_capital": 10000,
    "parameters": {
      "fast_period": 10,
      "slow_period": 20
    }
  }'
```

---

## Configuration Examples

### Development Configuration

```bash
# Default - no configuration needed
./scripts/run_gateway.sh dev
```

### Production Configuration

```bash
export VELOZ_PRESET=release
export VELOZ_PORT=8443
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
./scripts/run_gateway.sh
```

### Multi-Symbol Configuration

```bash
export VELOZ_MARKET_SYMBOL=BTCUSDT,ETHUSDT,BNBUSDT
./scripts/run_gateway.sh dev
```

### Full Binance Integration

```bash
# Market data from Binance
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com

# Trading on Binance testnet
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=your_api_key
export VELOZ_BINANCE_API_SECRET=your_api_secret

# WebSocket for user data stream
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws

./scripts/run_gateway.sh dev
```

---

## Common Workflows

### Workflow 1: Strategy Development

1. **Create a strategy** in `libs/strategy/src/`
2. **Write tests** in `libs/strategy/tests/`
3. **Build and test**:
   ```bash
   cmake --build --preset dev-tests -j$(nproc)
   ./build/dev/libs/strategy/veloz_strategy_tests
   ```
4. **Backtest** via UI or API
5. **Optimize parameters** using grid search or genetic algorithm

### Workflow 2: Backtesting

1. **Prepare data**: Download or generate historical data
2. **Configure backtest**: Set strategy, parameters, time range
3. **Run backtest**: Via UI Backtest tab or API
4. **Analyze results**: Review equity curve, metrics, trade history
5. **Optimize**: Adjust parameters based on results

```bash
# Download Binance historical data
curl -X POST http://127.0.0.1:8080/api/backtest/download \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT",
    "interval": "1h",
    "start_time": 1704067200000,
    "end_time": 1706745600000,
    "output_path": "data/btcusdt_1h.csv"
  }'

# Run backtest
curl -X POST http://127.0.0.1:8080/api/backtest/run \
  -H "Content-Type: application/json" \
  -d '{
    "strategy": "momentum",
    "data_source": "csv",
    "data_path": "data/btcusdt_1h.csv",
    "initial_capital": 10000
  }'

# Get results
curl http://127.0.0.1:8080/api/backtest/results
```

### Workflow 3: Live Trading (Testnet)

1. **Get API keys** from Binance testnet
2. **Configure environment**:
   ```bash
   export VELOZ_EXECUTION_MODE=binance_testnet_spot
   export VELOZ_BINANCE_API_KEY=your_key
   export VELOZ_BINANCE_API_SECRET=your_secret
   ```
3. **Start gateway**: `./scripts/run_gateway.sh dev`
4. **Monitor** via UI or SSE stream
5. **Place orders** via UI or API

### Workflow 4: Risk Monitoring

1. **Configure risk thresholds** in strategy config
2. **Monitor positions** via Positions tab
3. **View risk metrics** in real-time
4. **Set alerts** for drawdown, exposure limits

---

## Using the Web UI

### Order Book Tab

- Real-time bid/ask display
- Depth visualization
- Spread calculations
- Place orders directly

### Positions Tab

- Current positions with PnL
- Unrealized and realized PnL
- Position history
- Close positions

### Strategies Tab

- View active strategies
- Start/stop strategies
- Configure parameters
- Monitor performance

### Backtest Tab

- Configure backtest parameters
- Run backtests
- View equity curves
- Analyze trade history
- Compare parameter sets

---

## API Quick Reference

### Health and Status

```bash
# Health check
curl http://127.0.0.1:8080/health

# Engine status
curl http://127.0.0.1:8080/api/config
```

### Market Data

```bash
# Get current market data
curl http://127.0.0.1:8080/api/market

# Subscribe to real-time updates (SSE)
curl http://127.0.0.1:8080/api/stream
```

### Orders

```bash
# Place order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{"side":"BUY","symbol":"BTCUSDT","qty":0.001,"price":50000.0}'

# Get all orders
curl http://127.0.0.1:8080/api/orders_state

# Cancel order
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{"client_order_id":"web-1234567890"}'
```

### Account

```bash
# Get account info
curl http://127.0.0.1:8080/api/account
```

### Backtest

```bash
# Run backtest
curl -X POST http://127.0.0.1:8080/api/backtest/run \
  -H "Content-Type: application/json" \
  -d '{"strategy":"trend_following","symbol":"BTCUSDT"}'

# List results
curl http://127.0.0.1:8080/api/backtest/results

# Get specific result
curl http://127.0.0.1:8080/api/backtest/results/{id}
```

---

---

## Authentication (Optional)

VeloZ supports optional authentication for secure API access.

### Enabling Authentication

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
./scripts/run_gateway.sh dev
```

### Quick Authentication Example

**1. Login:**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_secure_password"}'
```

**Response:**
```json
{
  "access_token": "eyJ...",
  "refresh_token": "eyJ...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**2. Use access token:**
```bash
curl http://127.0.0.1:8080/api/orders_state \
  -H "Authorization: Bearer YOUR_ACCESS_TOKEN"
```

**3. Refresh token when expired:**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"YOUR_REFRESH_TOKEN"}'
```

### API Keys for Automation

For automated trading bots, use API keys instead of JWT tokens:

**Create API key (requires admin token):**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer ADMIN_ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"trading-bot","permissions":["read","write"]}'
```

**Use API key:**
```bash
curl http://127.0.0.1:8080/api/market \
  -H "X-API-Key: veloz_abc123..."
```

### Permission Levels

- **read**: View market data, orders, positions
- **write**: Place and cancel orders
- **admin**: Manage users and API keys

See [User Guide](README.md#authentication--security) for complete authentication documentation.

### Audit Logging

When authentication is enabled, security events are automatically logged:

**Enable audit logging:**
```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_AUDIT_LOG_ENABLED=true
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log
./scripts/run_gateway.sh dev
```

**Logged events include:**
- Login/logout attempts (success and failure)
- Token refresh operations
- API key creation and revocation
- Permission denied events

**Retention policies:**
- Auth logs: 90 days
- Order logs: 365 days
- API key logs: 365 days
- Access logs: 14 days

See [Configuration Guide](configuration.md#audit-logging-configuration) for detailed audit configuration.

---

## Next Steps

- **[User Guide](README.md)** - Complete user documentation
- **[Configuration Guide](configuration.md)** - All configuration options
- **[Backtest User Guide](backtest.md)** - Detailed backtesting documentation
- **[Binance Integration](binance.md)** - Binance-specific setup
- **[Build and Run](../build_and_run.md)** - Detailed build instructions
- **[HTTP API Reference](../../api/http_api.md)** - Complete API documentation

---

## Troubleshooting

### Build fails with CMake version error

Ensure CMake >= 3.24 is installed:

```bash
cmake --version
# If too old, upgrade:
# macOS: brew install cmake
# Ubuntu: snap install cmake --classic
```

### Engine executable not found

Make sure you ran the build command first:

```bash
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
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

### Tests fail

Run tests individually to identify the issue:

```bash
# Run specific test suite
./build/dev/libs/strategy/veloz_strategy_tests --verbose

# Run with CTest for more details
ctest --preset dev --output-on-failure
```

### Binance connection errors

- Verify API keys are correct
- Check network connectivity
- Ensure you're using testnet URLs for testnet keys
- Check rate limits (1200 requests/minute)

### UI not loading

- Ensure gateway is running
- Check browser console for errors
- Verify API base URL is correct
- Try clearing browser cache
