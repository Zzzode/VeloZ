# Implementation Status

This document tracks the implementation status of VeloZ features and components based on the
current repository state.

**Last Updated**: 2026-02-13 (Repository sync with current codebase)

## Summary

VeloZ is an early-stage, minimal end-to-end skeleton: a C++ engine, a Python HTTP gateway that
spawns the engine in stdio mode, and a static UI. Most modules exist as standalone libraries with
partial implementations and limited integration.

## Core Infrastructure

| Feature | Status | Notes |
|----------|--------|--------|
| Event Loop | Implemented | `libs/core/event_loop.h` provides a basic event loop |
| Logging | Implemented | `libs/core/logger.h` with console output and text formatter |
| Time Utilities | Implemented | `libs/core/time.h` |
| Memory Pool | Implemented | `libs/core/memory_pool.h` fixed-size pool and stats |
| Metrics | Implemented | `libs/core/metrics.h` counters and gauges |
| Configuration | Partial | JSON load/save and hierarchical groups; YAML load is not implemented |
| Error Handling | Implemented | `libs/core/error.h` exception types |
| JSON Utilities | Implemented | `libs/core/json.h` wrapper over yyjson |
| Tests | Implemented | GoogleTest coverage for core modules |

## Engine Runtime

| Feature | Status | Notes |
|----------|--------|--------|
| Stdio Command Parsing | Implemented | ORDER/CANCEL/QUERY commands via `command_parser.cpp` |
| Stdio Event Output | Implemented | Emits `engine_started`, `order_received`, `cancel_received`, `query_received` |
| Engine State | Partial | Basic balances, order reservation, pending fills, order state tracking |
| Risk Checks | Partial | Pre-trade checks in `libs/risk` integrated into `EngineState` |
| Service Mode | Partial | Placeholder loop + heartbeat in `engine_app.cpp` |
| Event Emitter | Implemented | NDJSON helpers for market/order/fill/account/error |

## API & Gateway

| Feature | Status | Notes |
|----------|--------|--------|
| HTTP Gateway | Implemented | `apps/gateway/gateway.py` |
| REST API | Implemented | Endpoints listed in `docs/build_and_run.md` |
| SSE Event Stream | Implemented | `GET /api/stream` |
| Engine Stdio Bridge | Implemented | Gateway spawns engine with `--stdio` |
| Market Source Switch | Partial | Sim default; Binance REST polling optional |
| Authentication | Not Started | No auth layer |
| Rate Limiting | Not Started | Not implemented |

## User Interface

| Feature | Status | Notes |
|----------|--------|--------|
| Web UI | Implemented | `apps/ui/index.html` minimal inspection UI |
| Market Data Display | Implemented | Updates via SSE |
| Order Actions | Implemented | Place/cancel orders |
| Account Display | Implemented | Balance view |
| Strategy UI | Not Started | No strategy management UI |

## Libraries (Standalone / Partial Integration)

| Module | Status | Notes |
|--------|--------|--------|
| Market | Partial | Event types, subscription manager, basic order book, WS scaffold |
| Exec | Partial | Order models, client order IDs, router, binance adapter scaffold |
| OMS | Partial | Order records and positions in `libs/oms` |
| Risk | Partial | Risk engine and circuit breaker are basic |
| Strategy | Partial | Base strategy and manager exist; not wired to engine |
| Backtest | Partial | Backtest engine, data sources, analyzer, reporter, optimizer exist as libs |

## Documentation

| Document | Status | Notes |
|----------|--------|--------|
| HTTP API Reference | Current | Based on gateway implementation |
| SSE API Documentation | Current | Event stream documented |
| Engine Protocol | Current | Stdio commands documented |
| Build and Run Guide | Current | `docs/build_and_run.md` |
| Design Documents | Current | Some sections are future-facing |

## Status Legend

| Status | Description |
|--------|-------------|
| Implemented | Feature is complete and functional |
| Partial | Feature is partially implemented |
| Not Started | Feature has not been implemented |

## Design Gaps (Features in Design Docs but Not Implemented)

### From design_01_overview.md
- gRPC/Protobuf contracts (no `proto/` directory)
- Python Strategy SDK (no `python/strategy-sdk/`)
- AI Bridge Service (no `services/ai-bridge/`)
- Analytics Service (no `services/analytics/`)
- Message Bus/Ring Buffers (no lock-free queue implementation)
- WAL (Write-Ahead Log) for order journaling

### From design_03_market_data.md
- Order book rebuild with sequence validation
- Rate limiting with token buckets
- Latency calibration with exchange time sync

### From design_04_execution_oms.md
- Order WAL/Journal
- Account reconciliation loop
- Batch sending and cancel merging

### From design_05_strategy_runtime.md
- Environment abstraction (LiveEnv/PaperEnv/BacktestEnv)
- Hot parameter updates
- Versioned parameters

### From design_07_ai_bridge.md
- AI Agent Bridge service
- Authorization/masking layer
- AI workflows for market analysis and trade review

## Next Priority Items

1. **Stdio Runtime Hardening** - Deterministic fills, richer order state, error handling
2. **Market Data Wiring** - WebSocket ingestion and order book rebuild
3. **Persistence & Recovery** - Order WAL + replay
4. **Gateway Hardening** - Auth and rate limiting
5. **Strategy Runtime** - Engine-level strategy hooks or SDK
