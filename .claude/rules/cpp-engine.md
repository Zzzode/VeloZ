---
paths:
  - "apps/engine/**"
  - "libs/**"
  - "CMakeLists.txt"
  - "CMakePresets.json"
  - "scripts/build.sh"
  - "scripts/run_engine.sh"
---

# C++ Engine / Libraries (Module Notes)

## Overview

This skill provides guidance for C++ engine and library development in VeloZ. The C++ core engine provides fundamental infrastructure for quantitative trading, implementing high-performance, type-safe, and production-ready utilities.

## Critical Rules

### NO RAW POINTERS (MANDATORY)

**Entire project prohibits raw pointers (T*)**. All nullable pointers must use `kj::Maybe<T>` or KJ-owned types:

| Raw Pointer (FORBIDDEN) | KJ Alternative (REQUIRED) |
|-------------------------|----------------------------|
| `T*` (nullable) | `kj::Maybe<T>` or `kj::Own<T>` |
| `T*` (non-nullable) | `T&` or `kj::Own<T>` |
| `T*` (function return, nullable) | `kj::Maybe<T>` |
| `T*` (function return, non-null) | `kj::Own<T>` or `T&` |
| `nullptr` check | `KJ_IF_SOME(value, ...) { ... }` |

### Examples

```cpp
// ❌ FORBIDDEN - Raw pointer (nullable)
T* findObject();
if (obj != nullptr) {
    obj->doSomething();
}

// ✅ REQUIRED - KJ Maybe (optional value)
kj::Maybe<T> findObject();
KJ_IF_SOME(obj, findObject()) {
    obj->doSomething();  // obj is T& type
}

// ❌ FORBIDDEN - Raw pointer ownership
T* createObject();
void destroyObject(T* obj);

// ✅ REQUIRED - KJ Own
kj::Own<T> createObject();
// No destroyObject needed - kj::Own<T> auto-deletes
```

### Allowed Exceptions (documentation required)

Raw pointers are ONLY allowed in these cases, and MUST be documented with a comment:

1. **External C API** - When interfacing with C libraries
   ```cpp
   // Using raw pointer for external C API: c_library_func()
   extern "C" void c_library_func(void* ctx);
   ```

2. **Opaque types** - When working with platform-specific opaque handles
   ```cpp
   // Using raw pointer for platform handle: socket_t
   using socket_t = void*;  // Platform-specific opaque type
   ```

3. **KJ internal** - Within kj::Own<T>, kj::Maybe<T> implementations
   ```cpp
   // KJ internal implementation detail
   T* ptr;  // Inside Own<T> implementation only
   ```

## Engine Stdio Protocol (CRITICAL)

The gateway runs the engine in **`--stdio` mode**. Keep `--stdio` behavior and stdio wire protocol compatible when changing engine code.

### Inbound Commands (stdin) - Plain Text

- `ORDER <BUY|SELL> <symbol> <qty> <price> <client_order_id>`
- `CANCEL <client_order_id>`

### Outbound Events (stdout) - NDJSON

Outbound events are **NDJSON** (one JSON object per line). The gateway/UI depend on stable `type` and field names.

**Common `type` values:**
- `market` - Market data updates
- `order_update` - Order state changes
- `fill` - Trade execution events
- `order_state` - Order state summary
- `account` - Account/balance updates
- `error` - Error events

**Protocol Stability Requirements:**
- Do not change field names in existing event types
- New event types should follow naming convention: `snake_case`
- All timestamps use ISO 8601 format: `2025-01-15T10:30:00.123Z`

### Key Entrypoints

| File | Purpose |
|------|-----------|
| `apps/engine/src/main.cpp` | Flag parsing, entry point |
| `apps/engine/src/engine_app.cpp` | Mode switch (stdio vs service) |
| `apps/engine/src/stdio_engine.cpp` | Stdio loop, command processing |
| `apps/engine/src/command_parser.cpp` | ORDER/CANCEL parsing |
| `apps/engine/src/event_emitter.cpp` | Event JSON emission |

## Module Structure and Boundaries

### Directory Structure

```
libs/
├── common/        # Cross-module shared code (no business coupling)
├── core/          # Infrastructure (logging, time, config, pools)
├── market/         # Market data (order book, websocket, subscriptions)
├── exec/           # Exchange adapters, order placement/cancel, receipts
├── oms/            # Order state machine, positions/funds, reconciliation
├── risk/           # Risk rule engine
└── strategy/       # Strategy runtime and signal processing
```

### Module Dependency Rules

- **`libs/` components may depend on `libs/common/`** only (for shared utilities)
- **`libs/` dependency direction is one-way only**: `market → core`, `exec → core`, `oms → core`
- **No circular dependencies between `libs/` modules**
- **`apps/` depend on `libs/`** as needed
- **`libs/` modules must NOT depend on `apps/`**

### Interface Stability

- **Public headers** in `libs/*/include/veloz/` are stable APIs
- Breaking changes require major version bump
- Additions are backward compatible
- Deprecations require 2-release notice period

## KJ Library Usage (DEFAULT CHOICE)

**VeloZ strictly uses KJ library from Cap'n Proto as the default choice over C++ standard library.**

For comprehensive KJ guidance, invoke the `kj-library` skill at `docs/kj/SKILL.md`.

## Known std Library Requirements (Cannot Migrate)

The following std library usages are required and cannot be migrated:

| std Type | Reason | Example Files |
|----------|--------|---------------|
| `std::string` | OpenSSL HMAC API | exec/binance_adapter.cpp, hmac_wrapper.cpp |
| `std::string` | yyjson C API | core/json.cpp |
| `std::string` | Copyable structs (kj::String not copyable) | market/market_quality.h (Anomaly) |
| `std::format` | Width specifiers ({:04d}, {:02d}) | core/logger.cpp, core/time.cpp |
| `std::filesystem` | File path operations | core/config_manager.cpp, backtest/data_source.cpp |
| `std::unique_ptr` | Custom deleters (kj::Own lacks support) | core/memory.h (ObjectPool) |
| `std::unique_ptr` | Polymorphic ownership | core/config_manager.h, core/logger.h |
| `std::function` | STL container compatibility | core/event_loop.h |
| `std::function` | Recursive lambdas | backtest/optimizer.cpp |
| `std::map` | Ordered iteration | core/metrics.h (Prometheus export) |
| `std::vector` | API return types | core/json.h, core/metrics.h |

## Core Component Design Patterns

### Event Loop (`event_loop.h/cpp`)

**Design Pattern**: Single-threaded event loop with priority queue

**Key Features:**
- 4-level priority system (Low, Normal, High, Critical)
- Event tags for filtering and routing
- Delayed task execution
- Built-in statistics tracking

**Thread Safety:**
- `kj::Mutex` or `kj::MutexGuarded<T>` for filter state
- `std::condition_variable` for queue coordination (requires `std::mutex`)

### Logging System (`logger.h/cpp`)

**Design Pattern**: Strategy pattern for formatters, composite pattern for outputs

**Key Features:**
- 6-level severity hierarchy (Trace/Debug/Info/Warn/Error/Critical/Off)
- Multiple output destinations (Console, File, Multi)
- Custom formatters (Text, JSON)
- Log rotation (Size-based, Time-based, Both)

**Thread Safety:**
- Each formatter is stateless (safe to share)
- Multi-output uses `kj::MutexGuarded<T>` for synchronization

### Memory Management (`memory_pool.h/cpp`)

**Design Pattern**: Object pool pattern, allocator pattern

**Key Features:**
- Fixed-size memory pools for specific types
- Block allocation to reduce fragmentation
- STL-compatible allocator (`PoolAllocator<T>`)
- Memory usage monitoring (`MemoryMonitor`)
- Allocation site tracking

**Thread Safety:**
- Each pool has `kj::MutexGuarded<T>` for protection
- Atomic statistics counters

### JSON Processing (`json.h/cpp`)

**Design Pattern**: RAII wrapper with zero-copy access where possible

**Key Features:**
- Wrap yyjson library (MIT-licensed, high performance)
- Type-safe parsing (`parse_as<T>()`)
- Compile-time type checking
- JSON builder for fluent API
- Array iteration with type inference
- String view access for zero-copy

## Build Integration

### Build Outputs

- Follow CMake presets: `build/<preset>/`
- Engine executable: `build/<preset>/apps/engine/veloz_engine`
- Libraries: `build/<preset>/libs/veloz_*.a`

### Dependencies

**Core library dependencies:**
- KJ library (from `libs/core/kj/`)
- yyjson (JSON parsing)
- CURL (optional, for REST API)
- OpenSSL (for TLS support in WebSocket module)

## Testing Requirements

### Unit Tests

- Each library module has `tests/test_*.cpp` files
- Use KJ Test framework (`kj::TestRunner`), NOT GoogleTest
- Test filenames: `<module>-test.cpp` pattern in test subdirectory
- Use `KJ_TEST` macros for test assertions

### Test Organization

```
libs/
├── core/
│   └── tests/
│       ├── test_event_loop.cpp
│       ├── test_logger.cpp
│       ├── test_memory_pool.cpp
│       └── ...
├── market/
│   └── tests/
│       └── test_*.cpp
└── ...
```

## Files to Check

- `/Users/bytedance/Develop/VeloZ/libs/core/kj/` - KJ library source code (headers)
- `/Users/bytedance/Develop/VeloZ/docs/kj/SKILL.md` - KJ library skill for comprehensive KJ guidance
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/` - KJ library documentation
- `/Users/bytedance/Develop/VeloZ/CMakeLists.txt` - Build configuration
- `/Users/bytedance/Develop/VeloZ/.claude/rules/cpp-engine.md` - This file
