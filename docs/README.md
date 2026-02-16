# VeloZ Documentation

Welcome to VeloZ documentation. This directory contains all documentation for the quantitative trading framework.

> **KJ Migration Status**: **COMPLETE** ✅ - All modules have been migrated to KJ library patterns. See [KJ Migration](migration/README.md) for details.

## Quick Navigation

- [Getting Started](user/getting-started.md) - Quick start guide for new users
- [HTTP API Reference](api/http_api.md) - REST API endpoints and usage
- [Build and Run](build_and_run.md) - Build instructions and scripts
- [Implementation Status](reviews/implementation_status.md) - Feature progress tracking
- [KJ Library Guide](kj/library_usage_guide.md) - KJ library usage patterns

## Documentation Structure

```
docs/
├── README.md              # This file - documentation index
├── build_and_run.md       # Build instructions and scripts
├── AGENTS.md             # Documentation writing guidelines
├── kj_migration_architecture.md  # KJ migration architecture document
├── api/                  # API Documentation
│   ├── README.md         # API overview
│   ├── http_api.md      # REST API reference
│   ├── sse_api.md       # Server-Sent Events stream
│   └── engine_protocol.md # Engine stdio commands
├── user/                 # User Documentation
│   ├── README.md         # User documentation index
│   ├── getting-started.md # Quick start guide
│   ├── installation.md  # Installation instructions
│   ├── configuration.md  # Configuration options
│   ├── development.md    # Development environment setup
│   ├── backtest.md      # Backtesting user guide
│   └── requirements.md  # Product requirements and roadmap
├── design/               # Design Documents
│   ├── README.md         # Design document index
│   ├── design_01_overview.md
│   ├── design_02_engine_status.md
│   ├── design_03_market_data.md
│   ├── design_04_execution_oms.md
│   ├── design_05_strategy_runtime.md
│   ├── design_06_analytics_backtest.md
│   ├── design_07_ai_bridge.md
│   ├── design_08_trading_ui.md
│   ├── design_09_technology_choices.md
│   ├── design_10_data_model_contracts.md
│   ├── design_11_ops_roadmap.md
│   └── design_12_core_architecture.md
├── plans/                # Implementation Plans
│   ├── README.md         # Plans overview
│   ├── architecture.md   # Core architecture plan
│   ├── implementation/   # Implementation plans
│   ├── optimization/     # Optimization plans
│   ├── deployment/      # Deployment plans
│   └── sprints/        # Sprint plans
├── migration/            # KJ Migration Documentation
│   ├── README.md         # Migration status and guide
│   ├── migration_plan.md  # Detailed migration plan
│   ├── std_inventory.md  # Standard library usage inventory
│   └── kj_library_migration.md  # KJ library patterns
├── kj/                  # KJ Library Documentation
│   └── library_usage_guide.md  # KJ usage guide
├── kjdoc/               # KJ Library Reference
│   ├── index.md         # KJ documentation index
│   ├── tour.md          # KJ library tour
│   └── style-guide.md   # KJ style guide
├── deployment/           # Deployment Documentation
│   ├── README.md         # Deployment overview
│   ├── production_architecture.md  # Production deployment
│   ├── monitoring.md     # Monitoring and observability
│   ├── ci_cd.md         # CI/CD pipeline
│   ├── backup_recovery.md # Backup procedures
│   └── troubleshooting.md  # Common issues and solutions
└── reviews/              # Reviews and Status
    ├── README.md         # Reviews overview
    ├── architecture_review.md  # Architecture evaluation
    └── implementation_status.md  # Implementation progress
```

## Key Documents

### For New Users
1. **[Getting Started](user/getting-started.md)** - Begin here to set up and run VeloZ
2. **[HTTP API Reference](api/http_api.md)** - Learn how to interact with VeloZ via REST API
3. **[Configuration Guide](user/configuration.md)** - Configure environment variables and settings

### For Developers
1. **[Development Guide](user/development.md)** - Set up development environment
2. **[Build and Run](build_and_run.md)** - Build instructions and available presets
3. **[KJ Library Guide](kj/library_usage_guide.md)** - KJ library usage patterns (REQUIRED for VeloZ development)
4. **[Implementation Status](reviews/implementation_status.md)** - Feature completion status

### For Contributors
1. **[AGENTS.md](AGENTS.md)** - Documentation writing guidelines
2. **[Plans](plans/)** - Implementation roadmaps and progress

### KJ Library Resources
1. **[KJ Migration Status](migration/README.md)** - Complete migration status
2. **[KJ Library Guide](kj/library_usage_guide.md)** - Comprehensive usage guide
3. **[KJ Skill](kj/skill.md)** - KJ skill for Claude Code
4. **[KJ Documentation](kjdoc/)** - Official KJ library reference

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
| Implementation Plans | Current | Detailed plans with accurate progress tracking (~85% complete) |
| Deployment Docs | Current | Production, monitoring, CI/CD |
| KJ Migration | Complete | All modules migrated |

## Related Links

- [Repository Root](../) - Return to project root
- [CLAUDE.md](../CLAUDE.md) - Claude Code project instructions
- [README](../README.md) - Project README
