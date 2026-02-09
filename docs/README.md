# VeloZ Documentation

Welcome to the VeloZ documentation. This directory contains all documentation for the quantitative trading framework.

## Quick Navigation

- [Getting Started](user/getting-started.md) - Quick start guide for new users
- [HTTP API Reference](api/http-api.md) - REST API endpoints and usage
- [Build and Run](build_and_run.md) - Build instructions and scripts

## Documentation Structure

```
docs/
├── README.md              # This file - documentation index
├── build_and_run.md       # Build instructions and scripts
├── AGENTS.md             # Documentation writing guidelines
├── api/                  # API Documentation
│   ├── README.md         # API overview
│   └── http-api.md      # REST API reference
├── user/                 # User Documentation
│   ├── README.md         # User documentation index
│   └── getting-started.md # Quick start guide
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
│   └── design_11_ops_roadmap.md
├── plans/                # Implementation Plans
│   ├── README.md         # Plans overview
│   ├── architecture.md   # Core architecture plan
│   ├── implementation/   # Implementation plans
│   ├── optimization/     # Optimization plans
│   └── deployment/      # Deployment plans
└── reviews/              # Reviews and Status
    ├── README.md         # Reviews overview
    ├── ARCHITECTURE_REVIEW.md
    └── IMPLEMENTATION_STATUS.md
```

## Key Documents

### For New Users
1. **[Getting Started](user/getting-started.md)** - Begin here to set up and run VeloZ
2. **[HTTP API Reference](api/http-api.md)** - Learn how to interact with VeloZ via REST API

### For Developers
1. **[Build and Run](build_and_run.md)** - How to build and run the project
2. **[Design Documents](design/)** - Architecture and system design
3. **[Implementation Plans](plans/)** - Development roadmaps and progress

### For Contributors
1. **[AGENTS.md](AGENTS.md)** - Documentation writing guidelines

## Project Overview

VeloZ is an early-stage quantitative trading framework for crypto markets with:

- **C++23 Core Engine** - High-performance trading engine
- **Python Gateway** - HTTP API bridge to the engine
- **Web UI** - Static HTML/JS interface for monitoring and trading

## Related Links

- [Repository Root](../) - Return to project root
- [CLAUDE.md](../CLAUDE.md) - Claude Code project instructions
