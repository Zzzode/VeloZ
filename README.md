# VeloZ

VeloZ is a functional quantitative trading framework for crypto markets. It provides a C++23 event-driven engine, Python gateway, and web UI with support for multiple exchanges.

**Status**: ~90% Complete - Core infrastructure and modules implemented. See [Implementation Status](docs/reviews/implementation_status.md).

## Quick Start

### Dependencies

**Required:**
- CMake >= 3.24
- C++23-capable compiler (Clang 16+ or GCC 13+)
- Python 3.x (for gateway)

**Automatically Fetched via CMake:**
- KJ Library (v1.3.0) - Core C++ utilities from Cap'n Proto
- GoogleTest - Testing framework
- yyjson - High-performance JSON parser

### Installation

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y cmake ninja-build clang libssl-dev python3 python3-pip
```

**macOS:**
```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja llvm python3
```

### Build

```bash
# Configure (development preset)
cmake --preset dev

# Build all targets
cmake --build --preset dev-all -j$(nproc)

# Output: build/dev/apps/engine/veloz_engine
```

### Run

```bash
# Option 1: Gateway + Web UI (Recommended)
./scripts/run_gateway.sh dev

# Option 2: Engine only
./scripts/run_engine.sh dev
```

The gateway listens on `http://127.0.0.1:8080/` by default.

## Documentation

| Category | Description |
|----------|-------------|
| [User Guide](docs/user/) | Getting started, configuration, and development |
| [API Reference](docs/api/) | HTTP API, SSE stream, and engine protocol |
| [KJ Library Guide](docs/kj/) | KJ library usage patterns and migration status |
| [Deployment Guide](docs/deployment/) | Production deployment and operations |
| [Design Documents](docs/design/) | Architecture and system design |
| [Implementation Status](docs/reviews/implementation_status.md) | Feature progress tracking |
| [Migration Docs](docs/migration/) | KJ library migration documentation |

### Key Documents

- **[Quick Start Guide](docs/user/getting-started.md)** - Get up and running quickly
- **[HTTP API Reference](docs/api/http_api.md)** - REST API endpoints and usage
- **[Backtest Guide](docs/user/backtest.md)** - Backtesting strategies
- **[KJ Library Guide](docs/kj/library_usage_guide.md)** - KJ library usage patterns
- **[Production Deployment](docs/deployment/production_architecture.md)** - Deploy to production
- **[Development Guide](docs/user/development.md)** - Set up development environment
- **[Implementation Status](docs/reviews/implementation_status.md)** - Current feature progress

## Repository Structure

```
VeloZ/
├── apps/
│   ├── engine/          # C++23 engine executable
│   ├── gateway/         # Python HTTP gateway
│   └── ui/              # Web UI (HTML/JS)
├── libs/
│   ├── core/            # Infrastructure (logging, event loop, memory, etc.)
│   ├── market/           # Market data module (WebSocket, order book, kline)
│   ├── exec/            # Execution module (exchange adapters, order routing)
│   ├── oms/              # Order management system (records, positions, WAL)
│   ├── risk/             # Risk management module (engine, circuit breaker)
│   ├── strategy/         # Strategy framework (base, advanced, manager)
│   └── backtest/         # Backtesting system (engine, analyzer, optimizer, reporter)
├── docs/                 # Documentation
├── scripts/              # Build and run scripts
└── .github/              # CI/CD configuration
```

## Features

### Module Status

| Module | Status | Description |
|--------|--------|-------------|
| Core Engine | ✅ Implemented | C++23 event-driven trading engine with stdio commands |
| Market Data | ✅ Implemented | Binance, Bybit, Coinbase, OKX WebSocket/REST integration |
| Execution System | ✅ Implemented | Multiple exchange adapters with resilient reconnection |
| Order Management | ✅ Implemented | Order state tracking, positions, and WAL for journaling |
| Risk Management | ✅ Implemented | Risk engine, circuit breaker, and metrics |
| Gateway API | ✅ Implemented | REST API with SSE stream |
| Web UI | ✅ Implemented | Real-time trading interface |
| Backtest System | ✅ Implemented | Grid search optimizer, HTML/JSON reports |
| Strategy Framework | ✅ Implemented | Base strategy, advanced strategies, manager |
| KJ Library Integration | ✅ Complete | All modules use KJ types and patterns |

### Supported Exchanges

| Exchange | Status | Features |
|----------|--------|-----------|
| Binance | ✅ Implemented | REST + WebSocket, testnet support |
| Bybit | ✅ Implemented | REST + WebSocket, order management |
| Coinbase | ✅ Implemented | REST with JWT authentication |
| OKX | ✅ Implemented | REST with signature handling |
| Resilient Adapter | ✅ Implemented | Automatic reconnection, rate limiting, fallback |

### Key Technical Features

- **KJ Library**: Complete integration for memory safety, async I/O, and performance
- **Lock-Free Data Structures**: SPSC queue, optimized event loop variants
- **Timer Wheel**: Efficient timer management for high-frequency operations
- **Retry Policy**: Exponential backoff for resilient network operations
- **WebSocket**: Custom KJ-based implementation with TLS support
- **Order WAL**: Write-ahead logging for order journaling
- **Circuit Breaker**: Risk control with automatic trading suspension

## Configuration

### Environment Variables

Configure VeloZ behavior using environment variables:

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset (dev/release/asan) |
| `VELOZ_MARKET_SOURCE` | `sim` | Market data: `sim`, `binance_rest`, or WebSocket |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol |
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution: `sim_engine` or exchange adapters |

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

## Testing

### Build and Run Tests

```bash
# Build with tests
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Run all tests
ctest --preset dev -j$(nproc)

# Run specific module tests
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/exec/veloz_exec_tests
```

**Current Test Coverage**: 146 tests passing

## Development

### Code Style

- **C++**: 4-space indentation, PascalCase classes, snake_case functions
- **Python**: PEP 8 compliance, use `black` formatter
- **Formatting**: `./scripts/format.sh` for C++, `black` for Python

### KJ Library Usage

VeloZ uses KJ library as the primary choice for C++ types. All modules have been migrated from std library to KJ equivalents:

```cpp
// Owned pointers
kj::Own<T> ptr = kj::heap<T>(args...);

// Optional values
kj::Maybe<T> maybe = kj::none;
KJ_IF_SOME(value, maybe) { use(value); }

// Dynamic arrays
kj::Vector<T> vec;
vec.add(value);
auto arr = vec.releaseAsArray();

// Hash maps
kj::HashMap<Key, Value> map;
map.insert(key, value);
KJ_IF_SOME(v, map.find(key)) { use(v); }

// Thread-safe state
kj::MutexGuarded<State> state;
auto lock = state.lockExclusive();
```

See [KJ Library Guide](docs/kj/library_usage_guide.md) for detailed usage patterns.

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Contributing

See [Development Guide](docs/user/development.md) for contribution guidelines.
