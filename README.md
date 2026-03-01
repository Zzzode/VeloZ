# VeloZ

<div align="center">

[![CI](https://github.com/Zzzode/VeloZ/actions/workflows/ci.yml/badge.svg)](https://github.com/Zzzode/VeloZ/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/Zzzode/VeloZ/branch/master/graph/badge.svg)](https://codecov.io/gh/Zzzode/VeloZ)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![Python](https://img.shields.io/badge/Python-3.10+-blue.svg?style=flat&logo=python)](https://www.python.org/)
[![CMake](https://img.shields.io/badge/CMake-3.24+-064F8C.svg?style=flat&logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg?style=flat)](LICENSE)

</div>

VeloZ is a high-performance quantitative trading framework for crypto markets, designed for low latency and high reliability. It combines a robust C++23 core engine with a modern React-based user interface and a Python gateway for flexible integration.

The project provides a complete end-to-end trading solution, including market data ingestion, strategy execution, risk management, order routing, and a cross-platform desktop application.

## Key Features

### 🚀 High-Performance Engine

- **C++23 Core**: Built for speed and efficiency using modern C++ standards.
- **Low Latency**: Event-driven architecture optimized for minimal processing time.
- **KJ Library Integration**: Uses Cap'n Proto's KJ library for async I/O and event loops.

### 🔌 Multi-Exchange Connectivity

- **Binance**: Full support for REST and WebSocket APIs.
- **Additional Adapters**: Implementations for Bybit, Coinbase, and OKX.
- **Smart Order Routing**: Intelligent routing capabilities to optimize execution.
- **Reconciliation**: Automatic state synchronization with exchanges.

### 📈 Strategy Framework

- **Built-in Strategies**:
  - **Grid Trading**: Configurable grid levels with position limits.
  - **Market Making**: Dynamic spread management.
  - **Momentum**: Multi-timeframe signal analysis.
  - **Mean Reversion**: Statistical arbitrage strategies.
  - **Trend Following**: EMA/ATR-based trend tracking.
- **Extensible Design**: Easy-to-implement interface for custom strategies.

### 🛡️ Risk Management & OMS

- **Pre-trade Checks**: Position limits, order size validation, and exposure controls.
- **Order Management System**: Centralized order state tracking and lifecycle management.
- **Latency Tracking**: Real-time monitoring of execution latency.

### 🧪 Backtesting & Simulation

- **Integrated Engine**: Event-driven backtesting engine (`libs/backtest`).
- **Optimization**: Parameter optimization using grid search and genetic algorithms.
- **Reporting**: Detailed performance metrics and trade analysis.

### 🖥️ Modern User Interface

- **React Dashboard**: Real-time monitoring and control (`apps/ui`).
- **Charting**: Integrated TradingView Lightweight Charts for visualization.
- **Desktop Installer**: Electron-based cross-platform installer (`apps/installer`) for Windows, macOS, and Linux.

## Architecture

The system is organized into a modular structure:

```
VeloZ/
├── apps/
│   ├── engine/          # C++23 trading engine executable
│   ├── gateway/         # Python HTTP gateway (BFF)
│   ├── ui/              # React/Vite web interface
│   └── installer/       # Electron-based desktop installer
├── libs/
│   ├── core/            # Infrastructure (logging, time, event loop)
│   ├── market/          # Market data handling & WebSocket adapters
│   ├── exec/            # Exchange execution adapters (Binance, Bybit, etc.)
│   ├── oms/             # Order Management System
│   ├── risk/            # Risk checks and limits
│   ├── strategy/        # Strategy implementations
│   └── backtest/        # Backtesting framework
└── docs/                # Comprehensive documentation
```

### Data Flow

1. **Market Data**: Ingested via WebSocket/REST (`libs/market`).
2. **Strategy**: Processes data and generates signals (`libs/strategy`).
3. **Risk**: Validates signals against safety rules (`libs/risk`).
4. **OMS**: Manages order lifecycle and state (`libs/oms`).
5. **Execution**: Routes orders to exchanges (`libs/exec`).
6. **Gateway/UI**: Provides real-time visibility and control.

## Quick Start

### Prerequisites

- **C++ Compiler**: GCC 14.3+ or Clang 14.0+ (C++23 support required).
- **CMake**: Version 3.24 or higher.
- **Python**: Version 3.10+.
- **Node.js**: Version 20+ (for UI/Installer).

### Build from Source

#### 1. Build Engine and Libraries

```bash
# Configure and build all C++ targets (Debug mode)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

#### 2. Build UI

```bash
cd apps/ui
npm install
npm run build
cd ../..
```

#### 3. Run Gateway + UI

```bash
# Install Python dependencies
pip3 install -r apps/gateway/requirements.txt

# Starts the gateway which spawns the engine and serves the UI
./scripts/run_gateway.sh dev
```

Open `http://127.0.0.1:8080/` in your browser.

### Build Installer (Desktop App)

To create a standalone desktop application:

```bash
# Build engine, UI, and package them
./scripts/build_installer.sh dev
```

Artifacts will be available in `apps/installer/out/make/`.

## Documentation

| Audience | Document | Description |
|----------|----------|-------------|
| **Getting Started** | [Release Notes](docs/RELEASE_NOTES.md) | v1.0 release notes and features |
| | [Changelog](CHANGELOG.md) | Version history and changes |
| | [Getting Started](docs/guides/user/getting-started.md) | Quick start guide |
| | [Installation](docs/guides/user/installation.md) | Detailed installation instructions |
| **Configuration** | [Configuration Guide](docs/guides/user/configuration.md) | Environment variables and settings |
| | [Security Config](docs/security/PRODUCTION_CONFIG_CHECKLIST.md) | Production security checklist |
| | [Vault Setup](infra/vault/README.md) | HashiCorp Vault configuration |
| **API Reference** | [HTTP API](docs/api/http_api.md) | REST API reference |
| | [SSE API](docs/api/sse_api.md) | Server-Sent Events stream |
| | [WebSocket API](docs/api/README.md) | WebSocket real-time data |
| | [Engine Protocol](docs/api/engine_protocol.md) | Engine stdio commands |
| **Deployment** | [Production Deployment](docs/deployment/production_deployment_runbook.md) | Blue-green deployment runbook |
| | [Production Architecture](docs/guides/deployment/production_architecture.md) | Infrastructure overview |
| | [High Availability](docs/guides/deployment/high_availability.md) | HA setup and failover |
| | [Disaster Recovery](docs/guides/deployment/dr_runbook.md) | Backup and recovery procedures |
| | [Monitoring](docs/guides/deployment/monitoring.md) | Observability stack setup |
| **Operations** | [Operations Runbook](docs/guides/deployment/operations_runbook.md) | Day-to-day operations |
| | [Incident Response](docs/guides/deployment/incident_response.md) | Incident handling procedures |
| | [On-Call Handbook](docs/guides/deployment/oncall_handbook.md) | On-call guide |
| | [Troubleshooting](docs/guides/deployment/troubleshooting.md) | Common issues and solutions |
| **Development** | [Development Guide](docs/guides/user/development.md) | Development environment setup |
| | [KJ Library Guide](docs/references/kjdoc/library_usage_guide.md) | KJ library usage patterns |
| | [Design Documents](docs/design/README.md) | Technical design specifications |

For more details on coding standards and contribution guidelines, see [CLAUDE.md](CLAUDE.md).
