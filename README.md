# VeloZ

VeloZ is an early-stage quantitative trading framework for crypto markets with a C++23 core engine, Python gateway, and web UI. It aims to provide a unified backtest/simulation/live trading runtime model and an evolvable engineering structure.

## Quick Start

### Dependencies

Required:

- CMake >= 3.24
- C++23-capable compiler (Clang 16+ or GCC 13+)

Optional:

- OpenSSL (for TLS support in WebSocket connections via KJ)

Ubuntu/Debian:

```bash
sudo apt install cmake ninja-build build-essential clang libgtest-dev libssl-dev
```

### Build

```bash
cmake --preset dev
cmake --build --preset dev -j
```

### Run

```bash
# Engine only
./scripts/run_engine.sh dev

# Gateway + Web UI
./scripts/run_gateway.sh dev
```

The gateway listens on `http://127.0.0.1:8080/` by default.

## Documentation

| Category | Description |
|----------|-------------|
| [User Guide](docs/user/) | Getting started, configuration, and development |
| [API Reference](docs/api/) | HTTP API, SSE stream, and engine protocol |
| [Deployment Guide](docs/deployment/) | Production deployment and operations |
| [Design Documents](docs/design/) | Architecture and system design |
| [Implementation Plans](docs/plans/) | Development roadmaps with progress |
| [Build & Run](docs/build_and_run.md) | Build instructions and scripts |

### Key Documents

- **[Quick Start Guide](docs/user/getting-started.md)** - Get up and running quickly
- **[Installation Guide](docs/user/installation.md)** - Detailed installation instructions
- **[HTTP API Reference](docs/api/http_api.md)** - REST API endpoints and usage
- **[Backtest Guide](docs/user/backtest.md)** - Backtesting strategies
- **[Configuration Guide](docs/user/configuration.md)** - Environment variables and options
- **[Production Deployment](docs/deployment/production_architecture.md)** - Deploy to production
- **[Development Guide](docs/user/development.md)** - Set up development environment
- **[CHANGELOG](CHANGELOG.md)** - Release notes and changes

## Repository Structure

```
VeloZ/
├── apps/
│   ├── engine/          # C++23 engine executable
│   ├── gateway/         # Python HTTP gateway
│   └── ui/              # Web UI (HTML/JS)
├── libs/
│   ├── core/            # Infrastructure (logging, event loop, etc.)
│   ├── market/           # Market data module
│   ├── exec/            # Execution module
│   ├── oms/              # Order management system
│   ├── risk/             # Risk management module
│   └── strategy/         # Strategy framework
├── docs/                 # Documentation
├── scripts/              # Build and run scripts
├── tests/                # Test suites
└── .github/              # CI/CD configuration
```

## Features

| Module | Status | Description |
|--------|--------|-------------|
| Core Engine | Implemented | C++23 event-driven trading engine |
| Market Data | Partial | Simulated + Binance REST polling |
| Execution System | Implemented | Simulated + Binance testnet |
| Order Management | Implemented | Order state tracking and aggregation |
| Risk Management | Partial | Basic risk checks and circuit breaker |
| Gateway API | Implemented | REST API with SSE stream |
| Web UI | Implemented | Real-time trading interface |

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Contributing

See [Development Guide](docs/user/development.md) for contribution guidelines.
