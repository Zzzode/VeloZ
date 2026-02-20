# VeloZ Documentation

Welcome to VeloZ documentation. This directory contains all documentation for the quantitative trading framework.

> **Project Status**: **PRODUCTION READY** ✅ - All 7 phases complete (33/33 tasks). See [Implementation Status](project/reviews/implementation_status.md) for details.

## Production Status

| Metric | Status |
|--------|--------|
| **Roadmap Completion** | 33/33 tasks (100%) |
| **Build Status** | 79/79 targets clean |
| **C++ Test Suites** | 16/16 passing |
| **UI Tests** | 200+ passing |
| **KJ Migration** | Complete |

### Feature Summary

**Exchange Adapters (4):**
- Binance (spot/futures)
- OKX
- Bybit
- Coinbase

**Trading Strategies (5):**
- Trend Following (EMA crossover)
- Mean Reversion (Z-score)
- Momentum (ROC/RSI)
- Market Making (bid/ask spread)
- Grid Trading (price levels)

**Core Systems:**
- Complete backtest engine with parameter optimization
- Risk management system with dynamic thresholds and rule engine
- Order management with WAL persistence and reconciliation
- Real-time market data with WebSocket integration

**User Interface (4 views):**
- Real-time Order Book display
- Position and PnL tracking
- Strategy configuration
- Backtest result visualization

---

> **KJ Migration Status**: **COMPLETE** ✅ - All modules have been migrated to KJ library patterns. See [KJ Migration](project/migration/README.md) for details.

## Quick Navigation

- [Getting Started](guides/user/getting-started.md) - Quick start guide for new users
- [HTTP API Reference](api/http_api.md) - REST API endpoints and usage
- [Build and Run](guides/build_and_run.md) - Build instructions and scripts
- [Implementation Status](project/reviews/implementation_status.md) - Feature progress tracking
- [KJ Library Guide](references/kjdoc/library_usage_guide.md) - KJ library usage patterns

## Documentation Structure

```
docs/
├── README.md              # This file - documentation index
├── api/                  # API Documentation
│   ├── README.md         # API overview
│   ├── exchange_api_reference.md # Exchange Adapter APIs
│   ├── http_api.md      # REST API reference
│   ├── sse_api.md       # Server-Sent Events stream
│   └── engine_protocol.md # Engine stdio commands
├── design/               # Design Documents
│   ├── README.md         # Design document index
│   └── design_*.md      # Detailed design specs
├── guides/               # Guides & Manuals
│   ├── build_and_run.md  # Build instructions
│   ├── user/             # User Documentation
│   │   ├── README.md
│   │   ├── getting-started.md
│   │   ├── configuration.md
│   │   └── ...
│   └── deployment/       # Deployment Guides
│       ├── README.md
│       ├── production_architecture.md
│       └── ...
├── project/              # Project Management
│   ├── plans/            # Implementation Plans
│   ├── requirements/     # Requirements
│   ├── reviews/          # Reviews & Status
│   └── migration/        # Migration Docs
│       ├── README.md
│       └── kj_migration_architecture.md
└── references/           # External References
    └── kjdoc/            # KJ Library Docs
        ├── index.md
        ├── library_usage_guide.md
        └── ...
```

## Key Documents

### For New Users
1. **[Getting Started](guides/user/getting-started.md)** - Begin here to set up and run VeloZ
2. **[HTTP API Reference](api/http_api.md)** - Learn how to interact with VeloZ via REST API
3. **[Configuration Guide](guides/user/configuration.md)** - Configure environment variables and settings

### For Developers
1. **[Development Guide](guides/user/development.md)** - Set up development environment
2. **[Build and Run](guides/build_and_run.md)** - Build instructions and available presets
3. **[KJ Library Guide](references/kjdoc/library_usage_guide.md)** - KJ library usage patterns (REQUIRED for VeloZ development)
4. **[Implementation Status](project/reviews/implementation_status.md)** - Feature completion status

### For Contributors
1. **[Plans](project/plans)** - Implementation roadmaps and progress

### KJ Library Resources
1. **[KJ Migration Status](project/migration/README.md)** - Complete migration status
2. **[KJ Library Guide](references/kjdoc/library_usage_guide.md)** - Comprehensive usage guide
3. **[KJ Documentation](references/kjdoc)** - Official KJ library reference

## Project Overview

VeloZ is a quantitative trading framework for crypto markets with:

- **C++23 Core Engine** - High-performance trading engine with KJ library
- **Python Gateway** - HTTP API bridge to engine
- **Web UI** - Static HTML/JS interface for monitoring and trading
- **KJ Library** - Memory-safe utilities from Cap'n Proto (COMPLETE migration)

### Key Architecture Decisions

| Component | Technology | Status |
|-----------|-------------|---------|
| Core Utilities | KJ Library | ✅ Complete |
| Testing Framework | KJ Test | ✅ Complete |
| Memory Management | kj::Own, kj::Maybe, kj::Vector | ✅ Complete |
| Strings | kj::String, kj::StringPtr | ✅ Complete |
| Threading | kj::Mutex, kj::Thread | ✅ Complete |

## Documentation Status

| Section | Status | Notes |
|---------|--------|-------|
| User Guides | Current | Getting started, installation, configuration |
| API Reference | Current | HTTP, SSE, Engine protocol |
| Design Docs | Current | Comprehensive design documents covering all aspects |
| Implementation Plans | Complete | All 7 phases delivered (33/33 tasks - 100%) |
| Deployment Docs | Current | Production, monitoring, CI/CD |
| KJ Migration | Complete | All modules migrated |

## Phase Completion Summary

| Phase | Description | Tasks | Status |
|-------|-------------|-------|--------|
| Phase 1 | Core Engine | 5/5 | ✅ Complete |
| Phase 2 | Market Data | 5/5 | ✅ Complete |
| Phase 3 | Execution | 5/5 | ✅ Complete |
| Phase 4 | Strategy | 5/5 | ✅ Complete |
| Phase 5 | Backtest | 5/5 | ✅ Complete |
| Phase 6 | Risk/OMS | 4/4 | ✅ Complete |
| Phase 7 | UI | 4/4 | ✅ Complete |
| **Total** | | **33/33** | **100%** |

## Related Links

- [Repository Root](..) - Return to project root
- [CLAUDE.md](../CLAUDE.md) - Claude Code project instructions
- [README](../README.md) - Project README
