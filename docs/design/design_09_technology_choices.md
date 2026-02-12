# VeloZ Crypto Quant Trading Framework: Technology Choices

This document reflects the current repository implementation. Planned or optional technologies are
listed separately.

## 4.1 Current Technology Choices (Implemented)

### Core Languages and Build

- Core engine and libraries: C++23 with CMake presets
- Gateway: Python 3 standard library (`http.server`, `urllib`)
- UI: Static HTML/JS (`apps/ui/index.html`)
- JSON: yyjson wrapper in `libs/core/json.h`

### Runtime Integration

- Engine ↔ Gateway: stdio text commands and NDJSON events
- Gateway ↔ UI: REST + SSE (`/api/stream`)
- Configuration: JSON config files via `ConfigManager` and `Config`

### Infrastructure and Ops

- Target OS: Linux
- Containerization/orchestration: not present in the repository
- Authentication and rate limiting: not implemented
- Observability: basic logging utilities in `libs/core/logger.h`

## 4.2 Current Subsystems (Partial Implementations)

- Market data: event types, order book scaffold, subscription manager, WS scaffold
- Execution: order models, router, binance adapter scaffold
- OMS: order record and position structures
- Risk: basic risk engine + circuit breaker
- Strategy: base strategy interfaces, manager scaffold
- Backtest: engine, data sources, analyzer, reporter, optimizer (as standalone libs)

## 4.3 Planned or Optional Choices (Not Implemented Yet)

These items are design ideas and are not present in the current codebase.

- gRPC/Protobuf control plane
- WebSocket market ingestion and order book rebuild with sequence validation
- Event bus / ring buffers for high-frequency paths
- Persistence layer (WAL, replay, database integration)
- Full authentication, authorization, and rate limiting
- Containerization and deployment stack (Docker/Kubernetes)
- AI agent bridge and vector retrieval stack
