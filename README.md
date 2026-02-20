# VeloZ

A high-performance quantitative trading framework for cryptocurrency markets, built with C++23, Python, and modern web technologies.

## Overview

VeloZ is a production-ready trading system designed for algorithmic trading on cryptocurrency exchanges. It combines the performance of C++ with the flexibility of Python, providing:

- **High-performance C++23 engine** with event-driven architecture
- **Python HTTP gateway** for easy integration and API access
- **Real-time web UI** for monitoring and control
- **Multi-exchange support** with unified order management
- **Comprehensive backtesting** with strategy optimization
- **Built-in risk management** with circuit breaker protection

**Status**: Production Ready - All 7 development phases completed (33/33 tasks).

| Metric | Status |
|--------|--------|
| Build | 79/79 targets clean |
| C++ Tests | 16/16 test suites passing |
| UI Tests | 200+ tests passing |
| Exchanges | 4 adapters (Binance, OKX, Bybit, Coinbase) |
| Strategies | 5 production strategies |

## Quick Start

### Prerequisites

- CMake >= 3.24
- C++23-capable compiler (Clang 16+ or GCC 13+)
- Python 3.x

### Installation

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install -y cmake ninja-build clang libssl-dev python3
```

**macOS:**

```bash
brew install cmake ninja llvm python3
```

### Build and Run

```bash
# Configure and build
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Run with gateway + web UI
./scripts/run_gateway.sh dev
```

The gateway starts at `http://127.0.0.1:8080/`

## Architecture

```
VeloZ/
├── apps/
│   ├── engine/          # C++23 trading engine
│   ├── gateway/         # Python HTTP gateway
│   └── ui/              # Web interface
├── libs/
│   ├── core/            # Infrastructure (event loop, logging)
│   ├── market/          # Market data & WebSocket
│   ├── exec/            # Exchange execution adapters
│   ├── oms/             # Order management system
│   ├── risk/            # Risk management
│   ├── strategy/        # Strategy framework
│   └── backtest/        # Backtesting engine
└── docs/                # Documentation
```

## Documentation

| Audience | Document | Description |
|----------|----------|-------------|
| **Users** | [Getting Started](docs/user/getting-started.md) | Quick start guide |
| | [Installation](docs/user/installation.md) | Detailed installation instructions |
| | [Configuration](docs/user/configuration.md) | Environment variables and settings |
| | [Backtesting](docs/user/backtest.md) | Strategy backtesting guide |
| **Developers** | [Development Guide](docs/user/development.md) | Development environment setup |
| | [HTTP API Reference](docs/api/http_api.md) | REST API reference |
| | [SSE API](docs/api/sse_api.md) | Server-Sent Events stream |
| | [Engine Protocol](docs/api/engine_protocol.md) | Engine stdio commands |
| | [KJ Library Guide](docs/kj/library_usage_guide.md) | KJ library usage patterns |
| **Operators** | [Deployment](docs/deployment/production_architecture.md) | Production deployment |
| | [Monitoring](docs/deployment/monitoring.md) | Monitoring and observability |
| | [CI/CD](docs/deployment/ci_cd.md) | Build and release pipeline |

## Features

### Trading Infrastructure

- **Multi-exchange support**: Binance, Bybit, Coinbase, OKX
- **Unified order management**: Single interface for all exchanges
- **Real-time market data**: WebSocket integration with automatic reconnection
- **Resilient execution**: Automatic retry with exponential backoff and circuit breaker
- **Order journaling**: Write-ahead logging for crash recovery
- **Account reconciliation**: Automatic state synchronization with exchanges

### Strategy System

- **Trend Following**: EMA crossover with ATR-based stops
- **Mean Reversion**: Bollinger Bands with RSI confirmation
- **Momentum**: Multi-timeframe RSI/MACD signals
- **Market Making**: Dynamic spread with inventory management
- **Grid Trading**: Configurable grid levels with position limits

### Backtesting

- **Event-driven engine**: Processes 100K+ events per second
- **Multiple data sources**: CSV files and Binance historical API
- **Parameter optimization**: Grid search and genetic algorithms
- **Rich reporting**: Equity curves, drawdown analysis, trade statistics

### Risk Management

- **Pre-trade risk checks**: Position limits, exposure controls
- **Dynamic thresholds**: Market-condition adaptive limits
- **Rule engine**: Configurable risk rules with priority ordering
- **Circuit breaker**: Automatic trading suspension on risk events
- **Real-time metrics**: VaR, Sharpe ratio, drawdown tracking

### Development Tools

- **KJ Library Integration**: Memory-safe utilities from Cap'n Proto
- **Comprehensive tests**: 16 test suites with KJ Test framework
- **Static analysis**: ASan/UBSan builds for debugging
- **Code formatting**: clang-format and black

## Supported Exchanges

| Exchange | REST | WebSocket | Testnet |
|----------|------|-----------|---------|
| Binance | ✅ | ✅ | ✅ |
| Bybit | ✅ | ✅ | ✅ |
| Coinbase | ✅ | ✅ | - |
| OKX | ✅ | ✅ | ✅ |

## Technology Stack

| Component | Technology |
|-----------|------------|
| **Core Engine** | C++23, CMake, KJ Library (Cap'n Proto) |
| **Gateway** | Python 3.x |
| **Web UI** | HTML, CSS, JavaScript |
| **Testing** | KJ Test framework (from Cap'n Proto) |
| **Build** | CMake Presets, Ninja |
| **Deployment** | Docker, Kubernetes |

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Contributing

Contributions are welcome! See [Development Guide](docs/user/development.md) for guidelines.

## Development Phases

All 7 development phases have been completed:

| Phase | Description | Tasks |
|-------|-------------|-------|
| Phase 1 | Core Engine | 5/5 |
| Phase 2 | Market Data | 5/5 |
| Phase 3 | Execution Module | 5/5 |
| Phase 4 | Strategy System | 5/5 |
| Phase 5 | Backtest System | 5/5 |
| Phase 6 | Risk & Position | 4/4 |
| Phase 7 | UI & User Experience | 4/4 |
| **Total** | **All Phases** | **33/33** |

See [Development Roadmap](docs/plans/roadmap_team_plan.md) for detailed task breakdown.

## Links

- [Development Roadmap](docs/plans/roadmap_team_plan.md) - Complete task breakdown and milestones
- [Implementation Status](docs/reviews/implementation_status.md) - Feature progress tracking
- [KJ Migration Status](docs/migration/README.md) - KJ library migration completion
- [Design Documents](docs/design/) - Technical design specifications
- [GitHub Issues](https://github.com/your-org/VeloZ/issues)
