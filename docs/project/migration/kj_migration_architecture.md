# KJ Library Migration Architecture

**Version**: 2.0 (Synthesized from team analysis)
**Date**: 2026-02-15
**Authors**: Architect (synthesized from dev-core-common, dev-market-exec, dev-oms-risk-strategy findings)

## Executive Summary

This document outlines the migration strategy for replacing std library usage with KJ equivalents in the VeloZ codebase. The migration focuses on patterns where KJ provides direct equivalents and should be the default choice per project guidelines.

## Module Dependency Graph

Understanding the dependency graph is critical for safe migration order:

```
                    ┌─────────────────┐
                    │   libs/core     │  (Foundation - migrate FIRST)
                    │  - config.h     │
                    │  - error.h      │
                    │  - json.h       │
                    │  - event_loop.h │
                    │  - lockfree_queue.h │
                    │  - config_manager.h │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  libs/common    │  (Base types)
                    │  - types.h      │
                    └────────┬────────┘
                             │
           ┌─────────────────┼─────────────────┐
           │                 │                 │
    ┌──────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐
    │ libs/market │   │  libs/exec  │   │  libs/oms   │
    │ - order_book│   │ - adapters  │   │ - order_rec │
    │ - websocket │   │ - router    │   │ - position  │
    └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
           │                 │                 │
           └─────────────────┼─────────────────┘
                             │
                    ┌────────▼────────┐
                    │   libs/risk     │
                    │ - circuit_breaker│
                    │ - risk_engine   │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  libs/strategy  │
                    │ - strategy.h    │
                    │ - manager       │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  libs/backtest  │
                    │ - engine        │
                    │ - optimizer     │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │   apps/engine   │  (Application - migrate LAST)
                    └─────────────────┘
```

## std::string Migration Decision Matrix

Based on team analysis, here is the definitive guidance on std::string usage:

| Location | Current Usage | Decision | Rationale |
|----------|---------------|----------|-----------|
| `libs/common/types.h` SymbolId | `std::string value` | **KEEP** | Required for std::map key comparison operators |
| `libs/core/error.h` VeloZException | `std::string` members | **KEEP** | std::runtime_error inheritance requires std::string |
| `libs/core/config.h` Config | `std::map<std::string, Value>` | **KEEP** | Ordered iteration + std::variant compatibility |
| `libs/core/json.h` | `std::string` parameters | **KEEP** | yyjson C API compatibility |
| `apps/engine/src/command_parser.cpp` | Local std::string | **MIGRATE** | Can use kj::String internally |
| `apps/engine/src/stdio_engine.cpp` | `std::string line` | **KEEP** | std::getline compatibility |
| `libs/strategy/src/strategy_manager.cpp` | String building | **MIGRATE** | Use kj::str() for concatenation |
| Exchange adapter headers | API parameters | **MIGRATE** | Internal APIs can use kj::StringPtr |

## Migration Scope

### Priority 1: Critical Patterns (Must Fix)

#### 1.1 Improper `kj::Maybe<T>()` Usage -> `kj::none`

**Issue**: Using `kj::Maybe<T>()` instead of `kj::none` to represent empty/null values.

**Files Affected**:
| File | Line | Current Pattern | Fix |
|------|------|-----------------|-----|
| `libs/backtest/src/backtest_engine.cpp` | 48 | `return kj::Maybe<double>();` | `return kj::none;` |
| `libs/exec/src/bybit_adapter.cpp` | 324, 354 | `return kj::Maybe<double>();` | `return kj::none;` |
| `libs/exec/src/bybit_adapter.cpp` | 335, 346 | `return kj::Maybe<std::map<...>>();` | `return kj::none;` |
| `libs/exec/src/coinbase_adapter.cpp` | 344, 355, 365 | `return kj::Maybe<...>();` | `return kj::none;` |
| `libs/exec/src/okx_adapter.cpp` | 364, 374, 384, 392 | `return kj::Maybe<...>();` | `return kj::none;` |
| `libs/exec/tests/test_resilient_adapter.cpp` | 30, 45 | `return kj::Maybe<ExecutionReport>();` | `return kj::none;` |

**Migration Pattern**:
```cpp
// BEFORE (incorrect)
return kj::Maybe<T>();

// AFTER (correct)
return kj::none;
```

### Priority 2: std::optional -> kj::Maybe Migration

#### 2.1 Exchange Adapter Sync APIs

**Files Affected**:
| File | Functions |
|------|-----------|
| `libs/exec/include/veloz/exec/binance_adapter.h` | `get_current_price`, `get_order_book`, `get_recent_trades`, `get_account_balance` |
| `libs/exec/include/veloz/exec/bybit_adapter.h` | Same functions |
| `libs/exec/include/veloz/exec/coinbase_adapter.h` | Same functions |
| `libs/exec/include/veloz/exec/okx_adapter.h` | Same functions |
| `libs/exec/src/binance_adapter.cpp` | Lines 698-729 |
| `libs/exec/src/bybit_adapter.cpp` | Lines 358-373 |
| `libs/exec/src/coinbase_adapter.cpp` | Lines 375-390 |
| `libs/exec/src/okx_adapter.cpp` | Lines 396-411 |

**Migration Pattern**:
```cpp
// BEFORE
std::optional<double> get_current_price(...);
return std::nullopt;

// AFTER
kj::Maybe<double> get_current_price(...);
return kj::none;
```

#### 2.2 Config Manager

**File**: `libs/core/include/veloz/core/config_manager.h`

**Patterns**:
- Line 256: `std::optional<T> get()` -> `kj::Maybe<T> get()`
- Line 258: `std::nullopt` -> `kj::none`
- Line 284: `std::optional<T> default_value()` -> `kj::Maybe<T> default_value()`
- Line 286: `std::nullopt` -> `kj::none`

#### 2.3 Lockfree Queue

**File**: `libs/core/include/veloz/core/lockfree_queue.h`

**Pattern**:
- Line 259: `std::optional<T> pop()` -> `kj::Maybe<T> pop()`
- Line 272: `std::nullopt` -> `kj::none`

### Priority 3: std::string -> kj::String Migration

#### 3.1 Common Types (SymbolId)

**File**: `libs/common/include/veloz/common/types.h`

**Current**:
```cpp
struct SymbolId final {
  std::string value;
  // ...
};
```

**Target**: Keep `std::string` for now due to STL container compatibility (std::map keys require std::string for comparison operators). Add comment explaining rationale.

#### 3.2 Engine Components

**Files**:
| File | Patterns |
|------|----------|
| `apps/engine/src/command_parser.cpp` | Local `std::string` variables for parsing |
| `apps/engine/src/main.cpp` | `std::vector<std::string> args` |
| `apps/engine/src/stdio_engine.cpp` | `std::string line` for getline |
| `apps/engine/src/engine_state.cpp` | String conversions for map keys |

**Decision**: These use `std::string` for:
1. `std::istringstream` compatibility (C++ standard library)
2. `std::getline` compatibility
3. `std::unordered_map` key compatibility

**Migration**: Convert internal string handling where possible, keep std::string at API boundaries.

#### 3.3 Strategy Manager

**File**: `libs/strategy/src/strategy_manager.cpp`

**Patterns**:
- Line 37: `std::string type_name(...)` - for logging
- Line 61: `std::string strategy_id = "strat-" + ...` - ID generation

**Migration**: Use `kj::str()` for string building, convert to std::string only at map boundaries.

### Priority 4: Container Migrations

#### 4.1 std::map Usage Analysis

**Keep std::map** (ordered iteration required):
- `libs/market/include/veloz/market/order_book.h`: `std::map<double, double>` for price-ordered book
- `libs/core/include/veloz/core/config.h`: `std::map<std::string, Value>` for ordered config
- `libs/strategy/include/veloz/strategy/strategy.h`: `std::map<kj::String, double>` for parameters

**Migrate to kj::HashMap** (unordered OK):
- `libs/core/include/veloz/core/metrics.h`: Lines 252-254 - metric registries
- `libs/strategy/include/veloz/strategy/strategy.h`: Lines 433-434 - strategy/factory maps

#### 4.2 std::unordered_map -> kj::HashMap

**Files**:
| File | Line | Current | Migration |
|------|------|---------|-----------|
| `apps/engine/include/veloz/engine/engine_state.h` | 74, 76 | `std::unordered_map<std::string, ...>` | `kj::HashMap<kj::String, ...>` |
| `libs/core/include/veloz/core/config_manager.h` | 707, 708 | `std::unordered_map<std::string, ...>` | `kj::HashMap<kj::String, ...>` |
| `libs/core/include/veloz/core/event_loop.h` | 225, 226 | `std::unordered_map<uint64_t, ...>` | `kj::HashMap<uint64_t, ...>` |
| `libs/oms/include/veloz/oms/order_record.h` | 81 | `std::unordered_map<std::string, ...>` | `kj::HashMap<kj::String, ...>` |
| `libs/risk/include/veloz/risk/risk_engine.h` | 282, 288 | `std::unordered_map<...>` | `kj::HashMap<...>` |

#### 4.3 std::vector Analysis

**Keep std::vector** (iterator/algorithm support required):
- `libs/strategy/include/veloz/strategy/advanced_strategies.h`: Price history vectors
- `libs/core/include/veloz/core/config.h`: Array config values in std::variant
- `libs/backtest/src/optimizer.cpp`: Algorithm-heavy code

**Migrate to kj::Vector**:
- Simple dynamic arrays without algorithm requirements
- Internal buffers that don't need STL compatibility

### Priority 5: Thread Synchronization

#### 5.1 std::mutex -> kj::Mutex

**Files**:
| File | Lines | Notes |
|------|-------|-------|
| `apps/engine/src/engine_state.cpp` | Multiple | With std::unordered_map |
| `apps/engine/include/veloz/engine/engine_state.h` | 72, 75 | Member mutexes |
| `libs/core/include/veloz/core/config_manager.h` | 532, 709, 843 | Config thread safety |
| `libs/backtest/src/data_source.cpp` | 804 | Rate limiting |

**Migration Pattern**:
```cpp
// BEFORE
mutable std::mutex mu_;
std::lock_guard<std::mutex> lock(mu_);

// AFTER
mutable kj::Mutex mu_;
auto lock = mu_.lockExclusive();
```

### Priority 6: Function Types

#### 6.1 std::function -> kj::Function

**Analysis**: Many `std::function` usages are for:
1. STL container compatibility (priority_queue comparators)
2. Callback APIs with external libraries
3. Custom deleters in std::unique_ptr

**Migrate**:
| File | Pattern | Migration |
|------|---------|-----------|
| `libs/core/include/veloz/core/event_loop.h` | `EventFilter`, `EventRouter` | Keep std::function (STL compatibility) |
| `libs/core/include/veloz/core/config_manager.h` | `ConfigValidator`, callbacks | Migrate to `kj::Function` |
| `libs/risk/include/veloz/risk/circuit_breaker.h` | `StateChangeCallback`, `HealthCheckCallback` | Migrate to `kj::Function` |

### Priority 7: Smart Pointers

#### 7.1 std::unique_ptr -> kj::Own

**Files with polymorphic ownership** (keep std::unique_ptr):
- `libs/core/include/veloz/core/logger.h`: LogFormatter, LogOutput
- `libs/core/include/veloz/core/config_manager.h`: ConfigItemBase

**Migrate to kj::Own**:
- Internal ownership without polymorphism
- PIMPL patterns where possible

#### 7.2 std::shared_ptr Analysis

**Keep std::shared_ptr** (shared ownership semantics required):
- Strategy interfaces: `std::shared_ptr<IStrategy>`
- Factory patterns: `std::shared_ptr<IStrategyFactory>`
- Data sources: `std::shared_ptr<IDataSource>`

**Note**: KJ uses `kj::Own` for unique ownership. For shared ownership, KJ provides `kj::Rc` but it's less commonly used. Keep std::shared_ptr for external API compatibility.

## Module Dependency Order

Migration should proceed in this order to minimize breakage:

```
1. libs/core (foundation)
   ├── error.h/cpp (std::string for std::runtime_error compatibility)
   ├── config.h/cpp
   ├── config_manager.h/cpp
   ├── json.h/cpp
   ├── event_loop.h/cpp
   └── lockfree_queue.h

2. libs/common
   └── types.h (SymbolId - keep std::string for map compatibility)

3. libs/market
   ├── order_book.h/cpp
   └── other market components

4. libs/exec
   ├── exchange_adapter.h
   ├── binance_adapter.h/cpp
   ├── bybit_adapter.h/cpp
   ├── coinbase_adapter.h/cpp
   ├── okx_adapter.h/cpp
   └── resilient_adapter.h/cpp

5. libs/oms
   └── order_record.h/cpp

6. libs/risk
   ├── circuit_breaker.h/cpp
   └── risk_engine.h/cpp

7. libs/strategy
   └── strategy.h, strategy_manager.cpp

8. libs/backtest
   └── backtest_engine.cpp, optimizer.cpp

9. apps/engine
   └── All engine components
```

## Migration Checklist by File

### Immediate Fixes (Priority 1)

- [ ] `libs/backtest/src/backtest_engine.cpp:48` - `kj::Maybe<double>()` -> `kj::none`
- [ ] `libs/exec/src/bybit_adapter.cpp:324,335,346,354` - `kj::Maybe<T>()` -> `kj::none`
- [ ] `libs/exec/src/coinbase_adapter.cpp:344,355,365` - `kj::Maybe<T>()` -> `kj::none`
- [ ] `libs/exec/src/okx_adapter.cpp:364,374,384,392` - `kj::Maybe<T>()` -> `kj::none`
- [ ] `libs/exec/tests/test_resilient_adapter.cpp:30,45` - `kj::Maybe<T>()` -> `kj::none`

### Phase 1: std::optional -> kj::Maybe

- [ ] `libs/exec/include/veloz/exec/binance_adapter.h` - Sync API return types
- [ ] `libs/exec/include/veloz/exec/bybit_adapter.h` - Sync API return types
- [ ] `libs/exec/include/veloz/exec/coinbase_adapter.h` - Sync API return types
- [ ] `libs/exec/include/veloz/exec/okx_adapter.h` - Sync API return types
- [ ] `libs/exec/src/*.cpp` - Corresponding implementations
- [ ] `libs/core/include/veloz/core/config_manager.h` - ConfigItem::get(), default_value()
- [ ] `libs/core/include/veloz/core/lockfree_queue.h` - pop() return type

### Phase 2: Container Migrations

- [ ] `apps/engine/include/veloz/engine/engine_state.h` - HashMap migration
- [ ] `libs/core/include/veloz/core/config_manager.h` - HashMap migration
- [ ] `libs/core/include/veloz/core/event_loop.h` - HashMap migration
- [ ] `libs/oms/include/veloz/oms/order_record.h` - HashMap migration
- [ ] `libs/risk/include/veloz/risk/risk_engine.h` - HashMap migration

### Phase 3: Thread Synchronization

- [ ] `apps/engine/include/veloz/engine/engine_state.h` - kj::Mutex
- [ ] `apps/engine/src/engine_state.cpp` - kj::Mutex usage
- [ ] `libs/core/include/veloz/core/config_manager.h` - kj::Mutex

### Phase 4: Function Types

- [ ] `libs/core/include/veloz/core/config_manager.h` - ConfigValidator, callbacks
- [ ] `libs/risk/include/veloz/risk/circuit_breaker.h` - Callbacks

## Exceptions (Keep std Library)

The following patterns should **NOT** be migrated. Each must include a justification comment in the code.

### Justified std Usage Reference Table

| std Type | Reason | Required Justification Comment |
|----------|--------|-------------------------------|
| `std::atomic<bool>` | Lightweight flags (KJ MutexGuarded has overhead) | `// std::atomic for lightweight flag (KJ MutexGuarded has overhead)` |
| `std::atomic<T>` | Hot-path counters (mutex overhead unacceptable) | `// std::atomic for hot-path counter (KJ lacks atomic primitives)` |
| `std::chrono::system_clock` | Wall clock timestamps (KJ time is async I/O only) | `// std::chrono for wall clock timestamps (KJ time is async I/O only)` |
| `std::random_device` | RNG (KJ lacks random number generation) | `// std::random_device for RNG (KJ lacks RNG)` |
| `std::mt19937` | RNG engine (KJ lacks random number generation) | `// std::mt19937 for RNG (KJ lacks RNG)` |
| `std::abs` | Math operation | `// std::abs for math operations (standard C++ library)` |
| `std::ceil` | Math operation | `// std::ceil for math operations (standard C++ library)` |
| `std::stod` | String to double parsing | `// std::stod for string parsing (standard C++ library)` |
| `std::min`/`std::max` | Comparisons | `// std::min/max for comparisons (standard C++ library)` |
| `std::sort` | Algorithm on arrays | `// std::sort algorithm (works on kj::Array)` |
| `std::memcpy` | C library function | `// std::memcpy - C library function` |
| `std::string` | OpenSSL HMAC API | `// std::string for OpenSSL API compatibility` |
| `std::string` | yyjson C API | `// std::string for yyjson C API` |
| `std::string` | std::format, std::filesystem | `// std::string for std::format/filesystem` |
| `std::string` | std::runtime_error inheritance | `// std::string for std::runtime_error compatibility` |
| `std::string` | std::map keys | `// std::string for std::map key compatibility` |
| `std::string` | std::getline | `// std::string for std::getline compatibility` |
| `std::map` | Ordered iteration required | `// std::map for ordered iteration (kj::TreeMap alternative available)` |
| `std::shared_ptr` | Shared ownership semantics | `// std::shared_ptr for shared ownership (KJ uses kj::Own for unique)` |
| `std::function` | STL container compatibility | `// std::function for STL container compatibility` |

### Migration Options for Atomic Counters

For counters that don't need atomic performance:
```cpp
// Use kj::MutexGuarded<T> for non-hot-path counters
kj::MutexGuarded<int> counter;

// Read
int value = counter.lockExclusive()->get();

// Write
counter.lockExclusive()->set(newValue);
```

For hot-path counters and lightweight flags, keep `std::atomic<T>` with justification comment.

### Legacy Exception List (Deprecated - Use Table Above)

1. **std::string for std::map keys**: Required for comparison operators
2. **std::string for std::runtime_error**: C++ exception hierarchy requirement
3. **std::string for std::variant**: Type compatibility
4. **std::vector in std::variant**: Config value types
5. **std::function for STL containers**: priority_queue, etc.
6. **std::shared_ptr for interfaces**: Shared ownership semantics
7. **std::unique_ptr with custom deleters**: Memory pool patterns
8. **std::map for ordered iteration**: Order book, config

## Testing Strategy

1. **Unit Tests**: Run existing tests after each file migration
2. **Build Verification**: `cmake --preset dev && cmake --build --preset dev-all`
3. **Test Execution**: `ctest --preset dev`
4. **Integration Tests**: Run gateway smoke tests

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| API breakage | High | Migrate internal code first, keep external APIs stable |
| Build failures | Medium | Incremental migration with continuous testing |
| Runtime errors | Medium | Comprehensive test coverage before migration |
| Performance regression | Low | KJ types are generally more efficient |

## Estimated Effort

| Phase | Files | Complexity |
|-------|-------|------------|
| Priority 1 (kj::none) | 5 | Low - simple text replacement |
| Priority 2 (std::optional) | 8 | Medium - API signature changes |
| Priority 3 (strings) | 10+ | High - cascading changes |
| Priority 4 (containers) | 8 | High - API and usage changes |
| Priority 5 (mutex) | 5 | Medium - lock pattern changes |
| Priority 6 (functions) | 4 | Medium - signature changes |
| Priority 7 (pointers) | Limited | Low - selective migration |

## Developer Assignment Matrix

Based on the dependency graph and team expertise:

### dev-core-common (Foundation Layer)

| File | Migration Type | Priority | Complexity |
|------|---------------|----------|------------|
| `libs/core/include/veloz/core/config_manager.h:256-286` | std::optional -> kj::Maybe | P2 | Medium |
| `libs/core/include/veloz/core/lockfree_queue.h:259,272` | std::optional -> kj::Maybe | P2 | Low |
| `libs/core/include/veloz/core/event_loop.h:225-226` | std::unordered_map -> kj::HashMap | P4 | Medium |
| `libs/core/include/veloz/core/config_manager.h:707-708` | std::unordered_map -> kj::HashMap | P4 | Medium |
| `libs/core/include/veloz/core/config_manager.h:532,709,843` | std::mutex -> kj::Mutex | P5 | Medium |
| `libs/common/include/veloz/common/types.h` | Review only | - | Keep std::string |

### dev-market-exec (Exchange Layer)

| File | Migration Type | Priority | Complexity |
|------|---------------|----------|------------|
| `libs/exec/src/bybit_adapter.cpp:324,335,346,354` | kj::Maybe<T>() -> kj::none | P1 | Low |
| `libs/exec/src/coinbase_adapter.cpp:344,355,365` | kj::Maybe<T>() -> kj::none | P1 | Low |
| `libs/exec/src/okx_adapter.cpp:364,374,384,392` | kj::Maybe<T>() -> kj::none | P1 | Low |
| `libs/exec/tests/test_resilient_adapter.cpp:30,45` | kj::Maybe<T>() -> kj::none | P1 | Low |
| `libs/exec/include/veloz/exec/*_adapter.h` | std::optional -> kj::Maybe | P2 | Medium |
| `libs/exec/src/*_adapter.cpp` | std::optional -> kj::Maybe | P2 | Medium |
| `libs/market/include/veloz/market/order_book.h` | Review only | - | Keep std::map (ordered) |

### dev-oms-risk-strategy (Business Logic Layer)

| File | Migration Type | Priority | Complexity |
|------|---------------|----------|------------|
| `libs/backtest/src/backtest_engine.cpp:48` | kj::Maybe<T>() -> kj::none | P1 | Low |
| `libs/oms/include/veloz/oms/order_record.h:81` | std::unordered_map -> kj::HashMap | P4 | Medium |
| `libs/risk/include/veloz/risk/risk_engine.h:282,288` | std::unordered_map -> kj::HashMap | P4 | Medium |
| `libs/risk/include/veloz/risk/circuit_breaker.h:60-61` | std::function -> kj::Function | P6 | Medium |
| `libs/strategy/include/veloz/strategy/strategy.h` | Review only | - | Keep std::map, std::shared_ptr |

## Phased Migration Plan

### Phase 1: Immediate Fixes (Day 1)
**Goal**: Fix all improper kj::Maybe<T>() usage

All developers can work in parallel:
- dev-market-exec: bybit, coinbase, okx adapters, test_resilient_adapter
- dev-oms-risk-strategy: backtest_engine

**Success Criteria**:
- All `kj::Maybe<T>()` replaced with `kj::none`
- Build passes: `cmake --build --preset dev-all`
- Tests pass: `ctest --preset dev`

### Phase 2: std::optional Migration (Day 2-3)
**Goal**: Replace std::optional with kj::Maybe

**Order**:
1. dev-core-common: config_manager.h, lockfree_queue.h (foundation)
2. dev-market-exec: exchange adapter headers and implementations (depends on core)

**Success Criteria**:
- No `#include <optional>` in migrated files
- No `std::nullopt` in migrated files
- All tests pass

### Phase 3: Container Migration (Day 4-5)
**Goal**: Replace std::unordered_map with kj::HashMap where appropriate

**Order**:
1. dev-core-common: event_loop.h, config_manager.h
2. dev-oms-risk-strategy: order_record.h, risk_engine.h

**Success Criteria**:
- kj::HashMap used for unordered lookups
- std::map retained only for ordered iteration
- All tests pass

### Phase 4: Thread Synchronization (Day 6)
**Goal**: Replace std::mutex with kj::Mutex

**Order**:
1. dev-core-common: config_manager.h
2. apps/engine: engine_state.h/cpp (after core migration)

**Success Criteria**:
- kj::Mutex with lockExclusive() pattern
- No std::lock_guard in migrated code
- Thread safety tests pass

### Phase 5: Function Types (Day 7)
**Goal**: Replace std::function with kj::Function where appropriate

**Files**:
- dev-core-common: config_manager.h callbacks
- dev-oms-risk-strategy: circuit_breaker.h callbacks

**Success Criteria**:
- kj::Function for internal callbacks
- std::function retained for STL container compatibility
- All tests pass

## Verification Commands

After each phase, run:

```bash
# Build
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Test
ctest --preset dev -j$(nproc)

# Smoke test
./scripts/run_engine.sh dev
```

## Conclusion

The migration should prioritize:
1. **Immediate**: Fix improper `kj::Maybe<T>()` usage (5 files) - ALL DEVELOPERS
2. **Short-term**: Migrate `std::optional` to `kj::Maybe` (8 files) - CORE THEN EXEC
3. **Medium-term**: Container and mutex migrations - CORE THEN OTHERS
4. **Long-term**: String and function type migrations where beneficial

Keep std library types where they provide necessary functionality (ordered maps, exception hierarchy, STL container compatibility).

## Appendix: KJ Pattern Quick Reference

```cpp
// kj::Maybe (nullable values)
kj::Maybe<T> value = kj::none;           // Empty
kj::Maybe<T> value = someValue;          // With value
KJ_IF_SOME(v, maybeValue) { use(v); }    // Safe access

// kj::HashMap (unordered map)
kj::HashMap<kj::String, Value> map;
map.insert(kj::str("key"), value);
KJ_IF_SOME(v, map.find("key"_kj)) { use(v); }

// kj::Mutex (thread synchronization)
kj::Mutex mutex;
auto lock = mutex.lockExclusive();       // Exclusive lock
auto lock = mutex.lockShared();          // Shared lock

// kj::Function (callable wrapper)
kj::Function<void(int)> callback;
callback = [](../../int x) { /* ... */ };

// kj::String (string handling)
kj::String str = kj::str("hello ", name);  // Concatenation
kj::StringPtr ptr = str;                    // Non-owning view
auto cstr = str.cStr();                     // C string access
```

## Document History

| Date | Version | Changes |
|------|---------|---------|
| 2026-02-15 | 2.0 | Initial architecture plan synthesized from team analysis |
| 2026-02-15 | 2.1 | Added justified std usage reference table with required justification comments; added migration options for atomic counters; clarified kj::MutexGuarded usage pattern |
