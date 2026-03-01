---
name: cpp-engine
description: Use when editing C++ code in apps/engine, apps/gateway_cpp, or libs/* directories. Covers project structure, naming conventions, build system, and architecture. For KJ library rules, see kj-library skill.
---

# C++ Engine and Libraries

## Overview

VeloZ uses **C++23** with KJ library (from Cap'n Proto) as the default choice over std library. All C++ code in `apps/engine`, `apps/gateway_cpp`, and `libs/*` must follow these rules.

**Core principle**: KJ-first, no raw pointers for ownership, event-driven async I/O.

## When to Use

Invoke this skill when:
- Writing or modifying C++ code in apps/ or libs/
- Creating new modules, handlers, or libraries
- Setting up tests or CMake configuration
- Understanding project structure and architecture

**For detailed KJ library usage rules (type mapping, async patterns, I/O operations):** invoke the **kj-library** skill.

## Module Structure

### Application Executables
| Directory | Purpose |
|-----------|---------|
| `apps/engine` | C++ trading engine (stdio/HTTP modes) |
| `apps/gateway_cpp` | HTTP gateway with REST API, SSE, auth, middleware |
| `apps/gateway` | Python gateway (legacy) |
| `apps/ui` | Static HTML UI (TypeScript/Vue) |

### Reusable Libraries (libs/)
| Library | Purpose |
|---------|---------|
| `core` | EventLoop, Logger, Metrics, JSON, Time, MemoryPool, Config |
| `common` | Shared types (header-only) |
| `market` | OrderBook, WebSocket, MarketEvent, Subscriptions |
| `exec` | ExchangeAdapter, OrderRouter, SmartOrderRouter |
| `oms` | OrderRecord, OrderStore, Position, OrderWAL |
| `risk` | RiskEngine, CircuitBreaker, RiskMetrics, VaR |
| `strategy` | StrategyManager, BaseStrategy, trading strategies |
| `backtest` | BacktestEngine, DataSource, Analyzer, Optimizer |

### Directory Layout
```
libs/<module>/
  include/veloz/<module>/  # Public headers
  src/                     # Implementation
  tests/                   # KJ tests (test_*.cpp)
  CMakeLists.txt           # Build config
```

## Critical Rules

### 1. KJ Library (Mandatory)

**KJ is the default choice over std library.** No exceptions.

- Use `kj::Own<T>` for ownership (not `std::unique_ptr`)
- Use `kj::Maybe<T>` for nullable values (not `std::optional`)
- Use `kj::String` / `kj::StringPtr` for strings (not `std::string`)
- Use `kj::Vector<T>` for dynamic arrays (not `std::vector`)
- Use `kj::MutexGuarded<T>` for thread safety (not `std::mutex`)
- Use `kj::Promise<T>` for async (not `std::future`)

**For detailed KJ rules, type mappings, and usage patterns:** invoke the **kj-library** skill.

**When external APIs force std types:**
- Add comment explaining which API requires it
- Use std types only at API boundaries
- Convert back to KJ immediately
- See kj-library skill for complete external API constraint handling

### 2. No Raw Pointers (Mandatory)

Raw `T*` is forbidden for ownership or nullable values.
- Use `kj::Own<T>` for ownership
- Use `kj::Maybe<T>` for nullable values
- Use `T&` for non-null references

**Exceptions**: Raw pointers allowed only for external C APIs or opaque handles (with explicit comment).

### 3. No `using namespace` (Mandatory)

**Never use `using namespace` directives.** Always use fully qualified names.

```cpp
// ❌ BAD: Pollutes namespace
using namespace kj;
using namespace veloz::core;
String str = "test";

// ✅ GOOD: Fully qualified names
kj::String str = "test"_kj;
veloz::core::EventLoop loop;

// ✅ GOOD: Use namespace aliases for long names
namespace core = veloz::core;
core::EventLoop loop;
```

**Why:**
- Prevents namespace pollution
- Makes dependencies explicit
- Avoids ambiguous symbol resolution
- Improves code readability

**Exceptions**: None. Not even in `.cpp` files.

## Naming Conventions

| Category | Convention | Example |
|----------|------------|---------|
| **Namespaces** | `veloz::<module>` | `veloz::core`, `veloz::gateway` |
| **Classes** | PascalCase | `OrderBook`, `AuthHandler` |
| **Functions** | snake_case | `place_order()`, `handle_login()` |
| **Methods** | snake_case | `get_signals()`, `process_event()` |
| **Member variables** | trailing underscore | `jwt_`, `api_keys_`, `audit_` |
| **Constants** | UPPER_SNAKE_CASE | `MAX_ORDER_SIZE` |
| **Enums** | PascalCase enum, UPPER_SNAKE values | `enum class OrderSide { BUY, SELL }` |

### File Naming
- Headers: `<name>.h` (not `.hpp`)
- Sources: `<name>.cpp`
- Tests: `test_<name>.cpp`
- All lowercase with underscores: `order_book.h`, `auth_handler.cpp`

## Build System

### CMake Presets
```bash
# Development (Debug)
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Release
cmake --preset release
cmake --build --preset release-all -j$(nproc)

# AddressSanitizer (Clang)
cmake --preset asan
cmake --build --preset asan-all -j$(nproc)

# Coverage (GCC)
cmake --preset coverage
cmake --build --preset coverage-all -j$(nproc)
```

### Build Targets
- Libraries: `veloz_core`, `veloz_market`, `veloz_exec`, etc.
- Executables: `veloz_engine`, `veloz_gateway`
- Tests: `veloz_<module>_tests`

### Target-Specific Builds
```bash
cmake --build --preset dev-engine    # Engine only
cmake --build --preset dev-libs      # Libraries only
cmake --build --preset dev-tests     # Tests only
```

## Testing Requirements

**For comprehensive testing guidance:** invoke **testing** skill.

**Overview:**
- **Mandatory**: KJ Test (`kj/test.h`)
- **Not allowed**: GoogleTest, Catch2

**Key Areas Covered:**
- Test framework setup (KJ Test headers, CMake configuration)
- Test registration and structure (KJ_TEST, assertions)
- KJ-specific patterns (Maybe handling, exception testing, async testing)
- Fixtures and setup/teardown patterns
- Mock components and integration test harnesses
- Test organization and naming conventions
- Running tests (ctest, direct execution)
- Load testing framework

**See testing skill for:**
- Detailed assertion macros reference
- Async testing patterns with kj::Promise
- Mock component patterns
- Integration test organization
- Test coverage expectations

## Async I/O Patterns

VeloZ uses event-driven async I/O with KJ's promise-based async framework.

**For detailed async patterns, error handling, and promise chaining:** invoke **kj-library** skill.

## Thread Safety Patterns

Use KJ's thread-safe types for concurrent access.

**For detailed thread safety patterns, mutex usage, and lock-free data structures:** invoke **kj-library** skill.

## Gateway C++ Architecture

### Handler Pattern
```cpp
// apps/gateway_cpp/src/handlers/<name>_handler.h
class MyHandler {
public:
  explicit MyHandler(Dependency& dep);

  // Non-copyable, non-movable
  MyHandler(const MyHandler&) = delete;
  MyHandler& operator=(const MyHandler&) = delete;

  // Endpoint handlers
  kj::Promise<void> handleGet(RequestContext& ctx);
  kj::Promise<void> handlePost(RequestContext& ctx);

private:
  Dependency& dep_;
};
```

### Router Registration
```cpp
// apps/gateway_cpp/src/router.h
router.addRoute("GET", "/api/resource", handler, middleware);
router.addRoute("POST", "/api/resource", handler, middleware);
```

### Middleware Stack
```cpp
// Middleware runs in registration order
auto middleware = kj::heap<AuthMiddleware>(jwt_manager);
router.addRoute("GET", "/api/protected", handler, middleware);
```

## Engine Stdio Protocol (Critical)

### Inbound Commands (Gateway → Engine)
```
ORDER <BUY|SELL> <symbol> <qty> <price> <client_order_id>
CANCEL <client_order_id>
```

### Outbound Events (Engine → Gateway)
NDJSON format with stable `type` field:
```json
{"type":"order_accepted","order_id":"...","timestamp":"2025-01-15T10:30:00.123Z"}
{"type":"order_rejected","reason":"...","timestamp":"2025-01-15T10:30:00.123Z"}
```

**Timestamps**: ISO 8601 UTC (`2025-01-15T10:30:00.123Z`)

**New event types**: Must use `snake_case` names

## Module Boundaries

### Dependency Rules
- `libs/` modules depend on `libs/core` or one-way peers only
- `apps/` depend on `libs/`
- `libs/` must **never** depend on `apps/`
- Circular dependencies forbidden

### Public API Stability
- Public headers: `libs/*/include/veloz/<module>/`
- Public APIs are stable contracts
- Internal headers: `libs/*/src/`
- Internal APIs can change freely

## Common Patterns

### RAII Resource Management
```cpp
class FileHandle {
public:
  explicit FileHandle(kj::StringPtr path) : fd_(open(path.cStr())) {}
  ~FileHandle() { if (fd_ >= 0) close(fd_); }

  // Non-copyable
  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;

  // Movable
  FileHandle(FileHandle&& other) : fd_(other.fd_) { other.fd_ = -1; }

private:
  int fd_;
};
```

### Factory Pattern
```cpp
class ExchangeAdapterFactory {
public:
  static kj::Own<ExchangeAdapter> create(Venue venue) {
    switch (venue) {
      case Venue::BINANCE:
        return kj::heap<BinanceAdapter>();
      case Venue::OKX:
        return kj::heap<OkxAdapter>();
      default:
        KJ_FAIL_REQUIRE("Unknown venue", venue);
    }
  }
};
```

### Builder Pattern
```cpp
class ConfigBuilder {
public:
  ConfigBuilder& set_name(kj::StringPtr name) {
    name_ = kj::str(name);
    return *this;
  }

  ConfigBuilder& set_timeout(kj::Duration timeout) {
    timeout_ = timeout;
    return *this;
  }

  Config build() {
    return Config(kj::mv(name_), timeout_);
  }

private:
  kj::String name_ = "default"_kj;
  kj::Duration timeout_ = 30 * kj::SECONDS;
};
```

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| Using `std::string` | Use `kj::String` or `kj::StringPtr` (see kj-library skill) |
| Raw pointer `T*` for ownership | Use `kj::Own<T>` with `kj::heap<T>()` (see kj-library skill) |
| `std::optional<T>` for nullable | Use `kj::Maybe<T>` with `KJ_IF_SOME` (see kj-library skill) |
| GoogleTest / Catch2 | Use KJ Test (`kj/test.h`) (see testing skill) |
| Std mutex for thread safety | Use `kj::MutexGuarded<T>` (see kj-library skill) |
| Using std for convenience | STOP. Use KJ. External API only if forced (see kj-library skill) |
| Copyable class with `kj::String` | Make class move-only (delete copy constructor) |
| Circular lib dependencies | Refactor to one-way dependency (see architecture skill) |

## Self-Evolution Mechanism

This skill includes a self-update mechanism to detect codebase changes:

### Detection Triggers
Run these checks to identify skill drift:
1. **New library module** added to `libs/` → Update module structure table
2. **New std exception** found in code → Add to std exceptions table
3. **Naming pattern violation** → Update naming conventions
4. **New build preset** in CMakeLists → Update build system section
5. **New architectural pattern** → Add to common patterns

### Update Process
When skill drifts from codebase:
1. Run `grep -r "std::" libs/ apps/` to find new std usage
2. Run `ls -la libs/` to check for new modules
3. Run `cmake --list-presets` to check for new presets
4. Review recent commits for architectural changes
5. Update SKILL.md and commit with message: `"docs(skill): update cpp-engine to reflect <change>"`

### Validation
After updating:
- Run `grep -f .claude/skills/cpp-engine/SKILL.md libs/**/*.cpp` to verify examples match code
- Check that all modules in `ls libs/` are documented
- Verify std exceptions table covers all `std::` usage in codebase

## Quick Reference

### Build Commands
```bash
cmake --preset dev              # Configure
cmake --build --preset dev-all  # Build
ctest --preset dev -j$(nproc)   # Test
./scripts/format.sh             # Format
```

### Test Structure
```cpp
KJ_TEST("Module: Test case") {
  auto obj = kj::heap<SomeClass>();
  KJ_EXPECT(condition);
  KJ_EXPECT(value == expected);
}
```

**For detailed KJ test patterns and assertions:** invoke **testing** skill.

### Handler Structure
```cpp
class MyHandler {
public:
  explicit MyHandler(Dep& dep);
  MyHandler(const MyHandler&) = delete;

  kj::Promise<void> handleGet(RequestContext& ctx);
private:
  Dep& dep_;
};
```

**For complete KJ type mappings, async patterns, and I/O operations:** invoke **kj-library** skill.
