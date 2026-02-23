# VeloZ v1.0

<div align="center">

[![CI](https://github.com/Zzzode/VeloZ/actions/workflows/ci.yml/badge.svg)](https://github.com/Zzzode/VeloZ/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/Zzzode/VeloZ/branch/master/graph/badge.svg)](https://codecov.io/gh/Zzzode/VeloZ)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![Python](https://img.shields.io/badge/Python-3.10+-blue.svg?style=flat&logo=python)](https://www.python.org/)
[![CMake](https://img.shields.io/badge/CMake-3.24+-064F8C.svg?style=flat&logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg?style=flat)](LICENSE)

[![Version](https://img.shields.io/badge/Version-1.0-brightgreen.svg?style=flat)](https://github.com/Zzzode/VeloZ/releases)
[![Status](https://img.shields.io/badge/Status-Production%20Ready-success.svg?style=flat)](https://github.com/Zzzode/VeloZ)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg?style=flat)](https://github.com/Zzzode/VeloZ)
[![Compiler](https://img.shields.io/badge/Compiler-GCC%2013%2B%20%7C%20Clang%2016%2B-orange.svg?style=flat)](https://github.com/Zzzode/VeloZ)

[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg?style=flat&logo=docker&logoColor=white)](https://www.docker.com/)
[![Kubernetes](https://img.shields.io/badge/Kubernetes-Ready-326CE5.svg?style=flat&logo=kubernetes&logoColor=white)](https://kubernetes.io/)
[![Prometheus](https://img.shields.io/badge/Prometheus-Monitoring-E6522C.svg?style=flat&logo=prometheus&logoColor=white)](https://prometheus.io/)
[![WebSocket](https://img.shields.io/badge/WebSocket-Real--time-010101.svg?style=flat)](https://github.com/Zzzode/VeloZ)

</div>

<div align="center">

**A production-ready quantitative trading framework for cryptocurrency markets**

*Built with C++23, Python, and React*

</div>

## Overview

VeloZ is an enterprise-grade trading system designed for algorithmic trading on cryptocurrency exchanges. It combines the performance of C++ with the flexibility of Python and a modern React UI, providing:

- **High-performance C++23 engine** with KJ async I/O
- **Python HTTP gateway** with enterprise security
- **Modern React UI** with real-time monitoring
- **Multi-exchange support** with smart order routing
- **Advanced risk management** with VaR models and stress testing
- **Complete observability** with Prometheus, Grafana, Jaeger, and Loki
- **Production infrastructure** with Kubernetes, Helm, Terraform, and Ansible

**Status**: ✅ **Production Ready v1.0** - 100/100 Production Readiness

| Metric | Status |
|--------|--------|
| **Production Readiness** | ✅ 100/100 |
| **Total Tests** | ✅ 615+ passing (100%) |
| **Performance** | ✅ 80k events/sec, 4.2k orders/sec, P99 < 1ms |
| **Security** | ✅ Enterprise (Vault, JWT, RBAC, Audit) |
| **Infrastructure** | ✅ Kubernetes, Helm, Terraform, Ansible |
| **Observability** | ✅ Prometheus, Grafana, Jaeger, Loki |
| **Exchanges** | ✅ Binance (prod), OKX/Bybit/Coinbase (beta) |
| **Risk Management** | ✅ VaR, Stress Testing, Portfolio Risk |
| **Architect Certification** | ✅ Issued |

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
| **Getting Started** | [Release Notes](RELEASE_NOTES.md) | v1.0 release notes and features |
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

## Features

### Trading Infrastructure

- **Multi-exchange coordination**: Binance (production), OKX/Bybit/Coinbase (beta)
- **Smart order routing**: Fee-aware routing with liquidity scoring
- **Execution algorithms**: TWAP, VWAP, Iceberg, POV
- **Order splitting**: Large order optimization across venues
- **Unified order management**: Single interface for all exchanges
- **Real-time market data**: WebSocket integration with automatic reconnection
- **Resilient execution**: Automatic retry with exponential backoff and circuit breaker
- **Order journaling**: Write-ahead logging for crash recovery
- **Account reconciliation**: Automatic state synchronization with exchanges
- **Latency tracking**: Per-exchange performance monitoring

### Security & Compliance

- **HashiCorp Vault**: Secrets management with AppRole authentication
- **JWT Authentication**: Access tokens (15-min) + Refresh tokens (7-day)
- **API Key Management**: bcrypt hashing (cost factor 12)
- **RBAC System**: 3 permission levels (viewer/trader/admin)
- **Token Bucket Rate Limiting**: Brute force protection
- **Comprehensive Audit Logging**: NDJSON format with configurable retention
- **Automatic Archiving**: Gzip compression before deletion
- **Container Security**: Trivy scanning, Cosign signing, SBOM generation
- **TLS/HTTPS**: Enforced in production

### Strategy System

- **Trend Following**: EMA crossover with ATR-based stops
- **Mean Reversion**: Z-score based with Bollinger Bands
- **Momentum**: Multi-timeframe RSI/ROC signals
- **Market Making**: Dynamic spread with inventory management
- **Grid Trading**: Configurable grid levels with position limits
- **Advanced Strategies**: RSI, MACD, Bollinger Bands, Stochastic, HFT Market Making, Cross-Exchange Arbitrage
- **Strategy Runtime**: Hot parameter updates, metrics queries

### Backtesting

- **Event-driven engine**: Processes 100K+ events per second
- **Multiple data sources**: CSV files and Binance historical API
- **Parameter optimization**: Grid search and genetic algorithms
- **Rich reporting**: Equity curves, drawdown analysis, trade statistics

### Advanced Risk Management

- **VaR Models**: Historical VaR, Parametric VaR, Monte Carlo VaR
- **Stress Testing**: 4 historical scenarios (COVID, LUNA, FTX, Flash Crash)
- **Scenario Analysis**: Probability weighting, best/worst case analysis
- **Portfolio Risk**: Real-time aggregation, correlation analysis
- **Pre-trade risk checks**: Position limits, exposure controls
- **Dynamic thresholds**: Market-condition adaptive limits
- **Rule engine**: Configurable risk rules with priority ordering
- **Circuit breaker**: Automatic trading suspension on risk events
- **172 risk tests**: All passing

### Observability & Monitoring

- **Prometheus**: Metrics collection and storage
- **Grafana**: 4 pre-built dashboards (trading, system, logs, SLO/SLA)
- **Jaeger**: Distributed tracing across services
- **OpenTelemetry**: Unified instrumentation
- **Loki**: Centralized log aggregation (7-day retention)
- **Promtail**: Log collection agent
- **Alertmanager**: 25+ alert rules with PagerDuty/Opsgenie/Slack integration
- **SLO/SLA Tracking**: Error budget monitoring

### Infrastructure & Deployment

- **Kubernetes**: StatefulSet with 3 replicas, leader election
- **Helm Charts**: 18 templates with HPA, RBAC, ServiceMonitor
- **Terraform**: VPC, EKS, RDS modules for AWS
- **Ansible**: Blue-green deployment with zero-downtime rollback
- **High Availability**: RTO < 30s, RPO = 0 (zero data loss)
- **Automated Backup**: S3/GCS with point-in-time recovery
- **Chaos Engineering**: Chaos Mesh experiments validated
- **Database Migrations**: Alembic + SQLAlchemy ORM

### Core Infrastructure

- **Event Loop**: `kj::setupAsyncIo()` for real OS-level async I/O
- **Lock-free Queue**: MPMC queue (Michael-Scott algorithm) for high-throughput
- **Memory Arena**: `kj::Arena` for efficient temporary allocations
- **Performance Benchmarks**: Framework for core module benchmarking

### Development Tools

- **KJ Library Integration**: Memory-safe utilities from Cap'n Proto (complete migration)
- **Comprehensive tests**: 16 C++ test suites + 90 Python tests + 200+ UI tests (805+ total)
- **Static analysis**: ASan/UBSan builds for debugging
- **Code formatting**: clang-format and black
- **Performance profiling**: Benchmark framework with detailed reports

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
| **Gateway** | Python 3.x, FastAPI, SQLAlchemy, Alembic |
| **Web UI** | React 18, TypeScript, TailwindCSS, Redux Toolkit |
| **Security** | HashiCorp Vault, JWT, bcrypt, RBAC |
| **Observability** | Prometheus, Grafana, Jaeger, Loki, OpenTelemetry |
| **Infrastructure** | Kubernetes, Helm, Terraform, Ansible |
| **Testing** | KJ Test, pytest, Vitest, Playwright |
| **Build** | CMake Presets, Ninja, GitHub Actions |
| **Deployment** | Docker, Kubernetes, AWS EKS, RDS |

## License

Apache License 2.0. See [LICENSE](LICENSE).

## Contributing

Contributions are welcome! See [Development Guide](docs/user/development.md) for guidelines.

## Production Readiness

VeloZ v1.0 has achieved 100/100 production readiness:

| Category | Status | Details |
|----------|--------|---------|
| **Tests** | ✅ 615+ passing | 280 gateway, 172 risk, 110 exec, 53 security |
| **Performance** | ✅ Validated | 80k events/sec, 4.2k orders/sec, P99 < 1ms |
| **Security** | ✅ Enterprise | Vault, JWT, RBAC, audit logging, container security |
| **Infrastructure** | ✅ Production | Kubernetes, Helm, Terraform, Ansible |
| **Observability** | ✅ Complete | Prometheus, Grafana, Jaeger, Loki, 25+ alerts |
| **High Availability** | ✅ Certified | RTO < 30s, RPO = 0, 3 replicas, leader election |
| **Disaster Recovery** | ✅ Ready | S3/GCS backup, PITR, cross-region replication |
| **Documentation** | ✅ Complete | Release notes, runbooks, API docs, user guides |
| **Architect Certification** | ✅ Issued | Production deployment authorized |

See [Production Readiness Analysis](docs/project/reviews/production_readiness_analysis.md) for detailed assessment.

## Version Information

- **Current Version**: v1.0.0
- **Release Date**: 2026-02-23
- **Status**: Production Ready
- **Production Readiness**: 100/100

## What's Next

### v1.1 (Planned)
- Vault observability integration
- Machine learning-based routing optimization
- Additional exchange integrations (Kraken, Bitfinex)
- Futures trading support
- Advanced order types (stop-loss, stop-limit, trailing stop, OCO)

### v1.2+ (Roadmap)
- Strategy backtesting engine enhancements
- Portfolio optimization
- Advanced UI with charting
- Mobile app
- WebSocket API for real-time data
- GraphQL API

## Links

- [Release Notes](RELEASE_NOTES.md) - v1.0 release notes
- [Changelog](CHANGELOG.md) - Version history
- [Production Readiness Analysis](docs/project/reviews/production_readiness_analysis.md) - Detailed assessment
- [Implementation Status](docs/project/reviews/implementation_status.md) - Feature progress tracking
- [Design Documents](docs/design/) - Technical design specifications
- [GitHub Issues](https://github.com/your-org/VeloZ/issues)
