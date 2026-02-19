---
paths:
  - "**"
  - "apps/**"
  - "libs/**"
  - "services/**"
  - "proto/**"
  - "python/**"
  - "infra/**"
---

# Architecture (Module Boundaries and Design Principles)

## Overview

This skill provides guidance for architectural decisions, module boundaries, dependency patterns, and design principles in VeloZ.

## Core Architectural Principles

### 1. Event-Driven Architecture

**All communication flows through events:**

- Market data → Trading signals → Order updates → Fill notifications
- Events are immutable, ordered by timestamp
- Event types: `market`, `order_update`, `fill`, `order_state`, `account`, `error`

### 2. Layered Decoupling

**Three layers with clear boundaries:**

```
┌─────────────────────────────────────────────────────┐
│  Presentation Layer (apps/ui, apps/gateway) │
└─────────────────────────────────────────────────────┘
                      ↓ HTTP/SSE/stdio
┌─────────────────────────────────────────────────────┐
│  Control Plane (services/*, proto/)          │
└─────────────────────────────────────────────────────┘
                      ↓ gRPC/message bus
┌─────────────────────────────────────────────────────┐
│  Data Plane (apps/engine)                  │
│  Market Data + Execution + OMS + Risk       │
└─────────────────────────────────────────────────────┘
```

### 3. Modular Monolith with Split Readiness

**Current state: Modular monolith (accelerate early iteration)**
**Future state: Can split into independent services (when isolation needed)**

- Modular monolith accelerates early development
- Clear module boundaries allow future service extraction
- Maintain stable interfaces (proto/) for smooth migration

### 4. Consistency First

**Single source of truth through order state machine:**

- Order state computed from events, not stored
- Idempotent event processing
- Replayable for debugging and recovery

### 5. Observability First

**End-to-end tracing, metrics, and logging:**

- Every operation should be measurable
- Structured logging with correlation IDs
- Metrics collection at module boundaries

### 6. Security First

**Least-privilege design, secrets management:**

- No secrets in code
- Secrets via environment variables only
- Audit logging for sensitive operations

## Module Boundaries

### Directory Structure

```
VeloZ/
├── apps/              # Deployable application units
│   ├── engine/        # C++23: core engine process
│   ├── gateway/       # Python: HTTP gateway/BFF
│   ├── backtest/       # C++23: backtest/replay executor
│   └── ui/           # Web: trading UI (static HTML)
├── libs/              # Reusable core library set
│   ├── common/        # Cross-module shared code
│   ├── core/          # Infrastructure (logging, time, config, pools)
│   ├── market/         # Market data (order book, websocket, subscriptions)
│   ├── exec/           # Exchange adapters, order placement/cancel
│   ├── oms/            # Order state machine, positions/funds
│   ├── risk/           # Risk rule engine
│   ├── strategy/       # Strategy runtime, signal processing
│   └── backtest/       # Backtest engine, optimizer, reporter
├── services/          # Scalable services (future)
│   ├── ai-bridge/     # AI Agent Bridge
│   └── analytics/       # Analytics/reporting service
├── proto/             # gRPC/Protobuf: control-plane contracts
├── python/            # Python ecosystem
│   ├── strategy-sdk/   # Strategy SDK
│   └── research/       # Research and experiments
├── infra/             # Infrastructure code
│   ├── docker/         # Docker configuration
│   └── k8s/           # Kubernetes manifests
└── tools/             # Development tools
    └── codegen/        # Code generation and verification
```

### Module AGENTS.md Files

Each top-level directory contains `AGENTS.md` with module-specific guidelines:

| Directory | AGENTS.md Content |
|-----------|------------------|
| `apps/` | - Language: English only<br>- Position: deployable application units<br>- Responsibilities: compose libs/ and proto/ to provide process-level entrypoints<br>- Dependencies: may depend on libs/ and proto/; avoid direct dependency on services/ internals<br>- Change note: new apps should update build and run scripts |
| `libs/` | - Language: English only<br>- Position: reusable core library set<br>- Submodules: core/market/exec/oms/risk/common<br>- Dependencies: keep one-way dependencies and avoid cycles<br>- Change note: public API changes should be assessed for apps/ and services/ impact |
| `python/` | - Position: Python strategy ecosystem<br>- Submodules: strategy-sdk, research<br>- Dependencies: keep one-way dependencies (libs/ → python/)<br>- Change note: maintain strategy-sdk as independent package |
| `services/` | - Language: English only<br>- Position: independent services (ai-bridge, analytics)<br>- Dependencies: may depend on proto/ only; services communicate via gRPC/message bus |
| `proto/` | - Position: single source of truth (contracts)<br>- Submodules: generated C++/Python/TS code<br>- Dependencies: must not depend on any business implementation code; generated code goes to each language module |

## Dependency Rules

### Core Dependency Direction

```
┌─────────────────────────────────────────────────────┐
│              libs/core (infrastructure)               │
│    ↑              ↑              ↑                │
│    │              │              │                │
│ libs/market ← libs/exec ← libs/oms         │
│    │              │              │                │
│    ↓              ↓              ↓                │
└─────────────────────────────────────────────────────┘
         All depend on libs/core (infrastructure only)
```

### Dependency Principles

1. **One-way dependencies only**: `libs/` components depend on `libs/core/` or each other in one direction only
2. **No circular dependencies**: `libs/` modules must not have circular dependencies
3. **Apps depend on libs only**: `apps/` modules may depend on `libs/` but must NOT depend on `services/`
4. **Proto dependency isolation**: `proto/` must NOT depend on any business implementation code
5. **Python SDK isolation**: `python/strategy-sdk` maintains version independently of `libs/`

### External Dependency Integration

| External Library | Integration Point | Notes |
|----------------|------------------|-------|
| KJ library | `libs/core/kj/` | Embedded source, version 2.0.0 |
| yyjson | `libs/core/CMakeLists.txt` | JSON parsing, MIT licensed |
| OpenSSL | `libs/market/CMakeLists.txt` | TLS support for WebSocket |
| CURL | `libs/core/CMakeLists.txt` | REST API (optional) |

## Interface Stability

### Public API Contracts

Libraries expose public APIs through `libs/*/include/veloz/`:

- **Breaking changes require major version bump**: Semantic versioning (X.Y.Z)
- **Additions are backward compatible**: New fields, new methods
- **Deprecations require 2-release notice period**: Allow migration time

### Module Interface Contracts

Each module defines stable interfaces for:

| Module | Public Interface | Breaking Change Process |
|---------|-----------------|---------------------|
| Market | `veloz/market/SubscriptionManager`, `veloz/market/OrderBook` | Design review, version bump, update docs |
| Exec | `veloz/exec::OrderRouter`, `veloz/exec::ExchangeAdapter` | Adapter abstraction, update implementations |
| OMS | `veloz/oms::OrderStore`, `veloz/oms::OrderWAL` | State machine interface, event replay |
| Risk | `veloz/risk::CircuitBreaker`, `veloz/risk::RiskEngine` | Rule configuration, threshold updates |
| Strategy | `veloz/strategy::StrategyManager`, `veloz/strategy::Strategy` | Signal interface, lifecycle hooks |
| Backtest | `veloz/backtest::BacktestEngine`, `veloz::backtest::Optimizer` | Data source, optimization parameters |

## Data Plane vs Control Plane

### Data Plane (High-Frequency Events)

**Purpose**: Low-latency, high-throughput market and execution data

**Components**:
- In-process: Strategy runtime, market data, execution, OMS, risk
- Communication: Memory queues, ring buffers (zero-copy where possible)

**Goals**:
- Latency < 1ms for order submission
- Throughput > 100k events/second
- Backpressure awareness
- Droppable low-priority events (UI subscriptions)

**Channels**:
- Market events: tick, order book updates, trades, klines
- Execution events: order submissions, fills, rejections
- Risk events: limit checks, circuit breaker triggers

### Control Plane (Low-Frequency Commands)

**Purpose**: Configuration, control, and management operations

**Components**:
- In-process: Config manager, strategy controls
- Cross-process: gRPC services (future), message bus

**Goals**:
- Strong consistency across services
- Auditability of all operations
- Retry capability for transient failures
- Stable interfaces across versions

**Operations**:
- Start/stop strategies
- Parameter updates
- Risk toggles
- Subscription management
- Permission management

### Cross-Process Communication

**Current**: In-process (shared memory)

**Future (Split Services)**: gRPC control plane + message bus (data plane)

## Design Patterns

### RAII (Resource Acquisition Is Initialization)

All resources must use RAII:

```cpp
// Example: KJ Own (preferred over std::unique_ptr)
kj::Own<MyResource> resource = kj::heap<MyResource>(/* args */);
// Resource auto-deleted when resource goes out of scope
```

### Strategy Pattern

Used for pluggable algorithms/formatters:

```cpp
// Formatter interface (Strategy pattern)
class LogFormatter {
    virtual std::string format(const LogEntry& entry) const = 0;
};

// Output destination (Strategy pattern)
class LogOutput {
    virtual void write(const std::string& formatted, const LogEntry& entry) = 0;
};
```

### Composite Pattern

Used for hierarchical organization:

```cpp
// Config groups (Composite pattern)
class ConfigGroup {
    void add_item(kj::Own<ConfigItemBase> item);
    void add_group(kj::Own<ConfigGroup> group);
};
```

### Builder Pattern

Used for fluent construction APIs:

```cpp
// JSON builder (Builder pattern)
class JsonBuilder {
    static JsonBuilder object();
    JsonBuilder& put(const std::string& key, const T& value);
    std::string build(bool pretty = false) const;
};
```

### Observer Pattern

Used for change notifications:

```cpp
// Config change callbacks (Observer pattern)
class ConfigManager {
    void add_callback(ConfigChangeCallback<T> callback);
};
```

## Thread Safety Strategy

### Fine-Grained Locking

- Each component has its own mutex (avoid lock contention)
- Lock only data that is being accessed, not entire object graph
- Use `kj::MutexGuarded<T>` for protecting single values
- Atomic counters for statistics (lock-free reads)

### Lock Ordering

- Always acquire locks in consistent order to prevent deadlocks
- Use RAII lock guards (`kj::Mutex::Guard`, `std::scoped_lock`)
- Minimize lock scope (release as soon as possible)

## Performance Considerations

### Zero-Copy Where Possible

- Use `kj::StringPtr` instead of copying strings
- Pass `kj::ArrayPtr` for array views
- Avoid heap allocations in hot paths

### Memory Efficiency

- Use object pools (`FixedSizeMemoryPool`) for frequently allocated types
- Prefer stack allocation for small objects
- Preallocate buffers for known workloads

### Lock-Free Reads

- Use atomic counters for statistics
- Use lock-free data structures where possible

## Error Handling Strategy

### Exceptions vs Optional Returns

```cpp
// Use exceptions for truly exceptional conditions
throw std::runtime_error("unexpected error");

// Use optional for expected failures
std::optional<T> tryOperation();  // Returns empty on failure
```

### Validation Functions

Pre-check inputs before operations:

```cpp
// Config validation
bool validateQuantity(double qty) {
    return qty > 0 && qty <= 1000000;
}
```

## Documentation Structure

### Required Documentation

- `product_requirements.md`: Detailed product requirements and feature specifications
- `strategy_development_guide.md`: Comprehensive guide for developing strategies
- `API.md`: Complete API documentation with examples
- `deployment_operations.md`: Deployment and operations documentation
- `build_and_run.md`: Instructions for building and running the system
- `design_*.md`: Architecture and design documentation

## Migration Guidelines

### Service Split Readiness

**Criteria for splitting into separate service:**

1. **Independent scalability needs**: Service needs to scale independently
2. **Isolation requirements**: Service requires separate deployment or resource boundaries
3. **Team ownership**: Different team owns and operates the service
4. **Clear interface**: Stable contract defined in proto/

**Preparation requirements:**

- All communication goes through proto/ contracts
- Service interface clearly defined
- No direct dependencies on engine internals
- Independent build and deploy process

## Files to Check

- `/Users/bytedance/Develop/VeloZ/docs/design/design_01_overview.md` - Design overview and principles
- `/Users/bytedance/Develop/VeloZ/docs/design/design_12_core_architecture.md` - Core module architecture analysis
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/` - KJ library documentation
- `/Users/bytedance/Develop/VeloZ/docs/kj/SKILL.md` - KJ library skill
- `/Users/bytedance/Develop/VeloZ/.claude/rules/cpp-engine.md` - C++ engine rules
- `/Users/bytedance/Develop/VeloZ/CMakeLists.txt` - Root build configuration
- `*/AGENTS.md` - Module-specific guidelines (in each top-level directory)
