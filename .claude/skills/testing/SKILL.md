---
name: testing
description: Use when writing or modifying tests in VeloZ. Covers KJ Test framework, assertions, fixtures, async testing, mocking, and test organization. Invoke for all test-related tasks.
---

# Testing

## Overview

VeloZ uses **KJ Test** (from Cap'n Proto) for all testing. No GoogleTest, Catch2, or other frameworks.

**Core principle**: Tests should be fast, deterministic, and use KJ types.

## When to Use

Invoke this skill when:
- Writing new tests or test files
- Setting up test infrastructure
- Running tests or checking coverage
- Debugging test failures
- Creating mock components

## Test Framework Setup

### Headers and Dependencies

```cpp
#include "kj/test.h"  // Main test header
#include "veloz/module/header.h"  // Code under test
```

### CMake Configuration

```cmake
# In libs/<module>/CMakeLists.txt
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_<module>_tests
    tests/test_file1.cpp
    tests/test_file2.cpp
  )
  target_link_libraries(veloz_<module>_tests
    PRIVATE veloz_<module> kj-test
  )
  add_test(NAME <module>_tests COMMAND veloz_<module>_tests)
endif()
```

**Test target naming**: `veloz_<module>_tests` (e.g., `veloz_core_tests`)

## Test Registration and Structure

### Basic Test Structure

```cpp
namespace {

using namespace veloz::module;

KJ_TEST("ModuleName: descriptive test case") {
  // Setup
  auto obj = kj::heap<SomeClass>();

  // Execution
  obj->do_something();

  // Verification
  KJ_EXPECT(obj->is_valid());
}

} // namespace
```

### Test Naming Convention

Format: `"ModuleName: specific functionality or scenario"`

**Examples:**
- `"JSON: Parse simple object"`
- `"OrderBook: Apply snapshot with gaps"`
- `"EngineBridge: place_order with limit order"`
- `"EventLoop: Post task with priority"`

### Multiple Test Cases

Each `KJ_TEST()` is independent - no shared state between tests:

```cpp
KJ_TEST("OrderBook: Add bid level") {
  OrderBook book;
  book.add_bid(50000.0, 1.5);
  KJ_EXPECT(book.best_bid().price == 50000.0);
}

KJ_TEST("OrderBook: Add ask level") {
  OrderBook book;  // Fresh instance
  book.add_ask(51000.0, 2.0);
  KJ_EXPECT(book.best_ask().price == 51000.0);
}
```

## Assertion Macros

### Basic Assertions

| Macro | Behavior | Use Case |
|-------|----------|----------|
| `KJ_EXPECT(condition)` | Continues on failure | Non-critical checks |
| `KJ_EXPECT(value == expected)` | Continues on failure | Equality checks |
| `KJ_ASSERT(condition)` | Stops test on failure | Critical invariants |
| `KJ_FAIL_EXPECT("message")` | Explicit failure | Unreachable code paths |
| `KJ_FAIL_REQUIRE("message")` | Fatal explicit failure | Invalid test state |

### KJ-Specific Assertions

```cpp
// String comparison
KJ_EXPECT(str == "expected"_kj);

// Maybe contains value
KJ_EXPECT(maybe_value != kj::none);

// Exception expected
bool threw = false;
KJ_IF_SOME(e, kj::runCatchingExceptions([&]() {
  risky_operation();
})) {
  threw = true;
  KJ_EXPECT(e.getDescription().contains("expected error"));
}
KJ_EXPECT(threw);
```

### Assertion Messages

```cpp
KJ_EXPECT(value == expected, "Custom message with context", value, expected);
KJ_FAIL_EXPECT("Unexpected state: ", state);
```

## KJ Maybe Handling

### Pattern: KJ_IF_SOME

```cpp
KJ_TEST("Maybe: extract value") {
  kj::Maybe<int> maybe_value = some_function();

  KJ_IF_SOME(value, maybe_value) {
    // Value is present
    KJ_EXPECT(value == 42);
  }
  else {
    // Value is absent
    KJ_FAIL_EXPECT("Expected value but got none");
  }
}
```

### Pattern: Maybe Comparison

```cpp
// Check Maybe has value
KJ_EXPECT(maybe_value != kj::none);

// Check Maybe is empty
KJ_EXPECT(maybe_value == kj::none);

// Compare Maybe contents
kj::Maybe<int> result = compute();
KJ_IF_SOME(val, result) {
  KJ_EXPECT(val == expected);
}
```

## Exception Testing

### Pattern: Expected Exceptions

```cpp
KJ_TEST("Network: Connection timeout throws") {
  bool threw = false;

  KJ_IF_SOME(e, kj::runCatchingExceptions([&]() {
    NetworkClient client("invalid-host:9999");
    client.connect();  // Should throw
  })) {
    threw = true;
    // Verify exception type or message
    KJ_EXPECT(e.getDescription().contains("timeout"));
  }

  KJ_EXPECT(threw, "Expected exception but none thrown");
}
```

### Pattern: No Exception Expected

```cpp
KJ_TEST("JSON: Parse valid JSON") {
  kj::Maybe<kj::Exception> exception =
    kj::runCatchingExceptions([&]() {
      auto json = parse_json("{\"key\": \"value\"}");
      KJ_EXPECT(json.is_object());
    });

  KJ_EXPECT(exception == kj::none, "Unexpected exception");
}
```

## Async Testing

### Pattern: Event Loop with wait()

```cpp
KJ_TEST("Async: Promise resolution") {
  auto io = kj::setupAsyncIo();

  auto promise = some_async_operation()
    .then([](int result) {
      return result * 2;
    });

  int value = promise.wait(io.waitScope);
  KJ_EXPECT(value == 42);
}
```

### Pattern: Multiple Promises

```cpp
KJ_TEST("Async: Concurrent operations") {
  auto io = kj::setupAsyncIo();

  auto p1 = async_op1();
  auto p2 = async_op2();

  auto combined = kj::joinPromises(kj::arr(
    kj::mv(p1),
    kj::mv(p2)
  ));

  auto results = combined.wait(io.waitScope);
  KJ_EXPECT(results[0] == expected1);
  KJ_EXPECT(results[1] == expected2);
}
```

### Pattern: Timer-Based Tests

```cpp
KJ_TEST("Timer: Delayed execution") {
  auto io = kj::setupAsyncIo();

  auto start = kj::systemPreciseMonotonicClock().now();

  io.provider->getTimer()
    .afterDelay(100 * kj::MILLISECONDS)
    .wait(io.waitScope);

  auto elapsed = kj::systemPreciseMonotonicClock().now() - start;
  KJ_EXPECT(elapsed >= 100 * kj::MILLISECONDS);
}
```

## Fixtures and Setup/Teardown

### Pattern: RAII Fixture

```cpp
class TestFixture {
public:
  TestFixture() : fs_(kj::newFilesystem()) {
    // Setup temporary directory
    auto tmp = fs_->getCurrentPath().append("_test_temp");
    fs_->getRoot().openSubdir(tmp, kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
    test_dir_ = fs_->getRoot().openSubdir(tmp, kj::WriteMode::MODIFY);
  }

  ~TestFixture() {
    // Cleanup happens automatically via RAII
  }

  kj::Directory& test_dir() { return *test_dir_; }

private:
  kj::Own<kj::Filesystem> fs_;
  kj::Own<kj::Directory> test_dir_;
};

KJ_TEST("FileIO: Write and read") {
  TestFixture fixture;

  auto file = fixture.test_dir().openFile("test.txt",
    kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
  file->writeAll("test data"_kj);

  auto content = file->readAllText();
  KJ_EXPECT(content == "test data"_kj);
}
```

### Pattern: Test Context

```cpp
struct OrderBookTestContext {
  OrderBook book;
  kj::Vector<BookLevel> bids;
  kj::Vector<BookLevel> asks;

  void add_test_data() {
    bids.add(BookLevel{50000.0, 1.5});
    bids.add(BookLevel{49900.0, 2.0});
    book.apply_snapshot(bids, asks);
  }
};

KJ_TEST("OrderBook: Snapshot with gaps") {
  OrderBookTestContext ctx;
  ctx.add_test_data();

  KJ_EXPECT(ctx.book.best_bid().price == 50000.0);
}
```

## Mock Components and Integration Tests

### Mock Pattern

```cpp
class MockMarketDataManager {
public:
  using EventCallback = kj::Function<void(const MarketEvent&)>;

  void inject_event(const MarketEvent& event) {
    auto lock = state_.lockExclusive();
    lock->events.add(event);
    if (lock->callback) {
      lock->callback(event);
    }
  }

  void set_event_callback(EventCallback callback) {
    state_.lockExclusive()->callback = kj::mv(callback);
  }

  [[nodiscard]] size_t injected_event_count() const {
    return state_.lockShared()->events.size();
  }

private:
  struct State {
    kj::Vector<MarketEvent> events;
    kj::Maybe<EventCallback> callback;
  };
  mutable kj::MutexGuarded<State> state_;
};
```

### Test Harness Pattern

```cpp
// tests/integration/test_harness.h
class IntegrationTestHarness {
public:
  IntegrationTestHarness()
      : market_data_(kj::heap<MockMarketDataManager>()),
        strategy_runtime_(kj::heap<MockStrategyRuntime>()),
        oms_(kj::heap<MockOMS>()) {
    wire_components();
  }

  void wire_components() {
    market_data_->set_event_callback([this](const MarketEvent& e) {
      strategy_runtime_->on_market_event(e);
    });
  }

  MockMarketDataManager& market_data() { return *market_data_; }
  MockStrategyRuntime& strategy_runtime() { return *strategy_runtime_; }

private:
  kj::Own<MockMarketDataManager> market_data_;
  kj::Own<MockStrategyRuntime> strategy_runtime_;
  kj::Own<MockOMS> oms_;
};

// In test file
KJ_TEST("Integration: Strategy receives market events") {
  IntegrationTestHarness harness;

  MarketEvent event;
  event.symbol = "BTC-USDT"_kj;
  harness.market_data().inject_event(event);

  KJ_EXPECT(harness.strategy_runtime().event_count() == 1);
}
```

## Test Organization

### Directory Structure

```
libs/<module>/
  tests/
    test_class1.cpp
    test_class2.cpp
    CMakeLists.txt (optional)

apps/<app>/
  tests/
    test_handler1.cpp
    test_handler2.cpp
    integration/
      test_full_flow.cpp

tests/
  integration/
    test_engine_integration.cpp
    test_backtest_e2e.cpp
  load/
    load_test_main.cpp
```

### Test File Naming

- Unit tests: `test_<class_or_module>.cpp`
- Integration tests: `test_<scenario>_integration.cpp`
- Load tests: `<scenario>_load_test.cpp`

### One Test File Per Class

```cpp
// test_order_book.cpp
namespace {

KJ_TEST("OrderBook: Add bid level") { ... }
KJ_TEST("OrderBook: Apply snapshot") { ... }
KJ_TEST("OrderBook: Handle gap") { ... }

} // namespace
```

## Running Tests

### CTest Commands

```bash
# All tests
ctest --preset dev -j$(nproc)

# Specific test
ctest --preset dev -R test_order_book

# Verbose output
ctest --preset dev -V

# With AddressSanitizer
ctest --test-dir build/asan -j$(nproc)

# Load tests with labels
ctest --preset dev -L load
```

### Direct Execution

```bash
# Run test binary directly
./build/dev/libs/core/veloz_core_tests

# Filter tests by name
./build/dev/libs/core/veloz_core_tests --filter "JSON*"
```

### Load Test Execution

```bash
# Quick test (30 seconds)
./build/dev/tests/load/veloz_load_tests quick

# Full test (5 minutes)
./build/dev/tests/load/veloz_load_tests full

# Sustained test (hours)
./build/dev/tests/load/veloz_load_tests sustained --hours 24

# Stress test
./build/dev/tests/load/veloz_load_tests stress
```

## Test Coverage

### Coverage Expectations

| Module | Required Coverage |
|--------|-------------------|
| **Core** | EventLoop, Logger, Config, MemoryPool, JSON, Metrics, Retry, TimerWheel |
| **Market** | OrderBook, WebSocket, Subscriptions, KlineAggregator, QualityMonitoring |
| **Exec** | Adapters (mock tests), OrderRouter, Reconciliation |
| **OMS** | OrderRecord, WAL, Position |
| **Risk** | CircuitBreaker, RiskEngine |
| **Strategy** | StrategyManager, Lifecycle |

### Coverage Commands

```bash
# Generate coverage report
cmake --preset coverage
cmake --build --preset coverage-all -j$(nproc)
ctest --test-dir build/coverage -j$(nproc)
./scripts/coverage.sh  # Generates HTML in coverage_html/
```

## Common Patterns

### Pattern: Test Data Builders

```cpp
namespace {

BookLevel make_level(double price, double qty) {
  return BookLevel{price, qty};
}

Order make_test_order(kj::StringPtr id = "test-order"_kj) {
  return Order{
    .id = kj::str(id),
    .symbol = "BTC-USDT"_kj,
    .side = OrderSide::BUY,
    .quantity = 1.0,
    .price = 50000.0
  };
}

KJ_TEST("OrderBook: Multiple levels") {
  OrderBook book;
  book.add_bid(make_level(50000.0, 1.5));
  book.add_bid(make_level(49900.0, 2.0));

  KJ_EXPECT(book.best_bid().price == 50000.0);
}

} // namespace
```

### Pattern: Thread Safety Testing

```cpp
KJ_TEST("ThreadSafe: Concurrent access") {
  ThreadSafeQueue<int> queue;

  kj::Thread producer([&]() {
    for (int i = 0; i < 100; ++i) {
      queue.push(i);
    }
  });

  int consumed = 0;
  kj::Thread consumer([&]() {
    for (int i = 0; i < 100; ++i) {
      auto value = queue.pop();
      if (value != kj::none) {
        ++consumed;
      }
    }
  });

  producer.join();
  consumer.join();

  KJ_EXPECT(consumed == 100);
}
```

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| Using GoogleTest/Catch2 | Use KJ Test (`kj/test.h`) |
| Asserting `maybe_value == expected` | Use `KJ_IF_SOME` to extract value first |
| Blocking in async test | Use `promise.wait(io.waitScope)` |
| Shared state between tests | Each `KJ_TEST()` is independent - no globals |
| Forgetting `"_kj"` suffix on strings | Add `"_kj"` for `kj::StringPtr` literals |
| Not testing error paths | Use `kj::runCatchingExceptions()` for exception testing |
| Over-mocking | Prefer integration tests with real components when possible |

## Self-Evolution Mechanism

This skill includes patterns discovered from actual test files. To keep it updated:

### Detection Triggers
1. **New test utility** added to `tests/` → Document utility pattern
2. **New mock pattern** in integration tests → Add to mock section
3. **Coverage gap** identified → Update coverage expectations
4. **New CMake test configuration** → Update CMake setup section

### Update Process
1. Review recent test file additions
2. Identify new patterns or utilities
3. Update SKILL.md with examples
4. Verify patterns are consistent across test files

## Quick Reference

### Test Structure
```cpp
#include "kj/test.h"

namespace {

KJ_TEST("Module: Description") {
  // Setup
  auto obj = kj::heap<SomeClass>();

  // Execution & Verification
  KJ_EXPECT(obj->is_valid());
  KJ_IF_SOME(val, obj->maybe_value()) {
    KJ_EXPECT(val == expected);
  }
}

} // namespace
```

### Running Tests
```bash
ctest --preset dev -j$(nproc)        # All tests
ctest --preset dev -R test_name       # Specific test
./build/dev/libs/core/veloz_core_tests  # Direct
```

### Key Patterns
- **Maybe**: `KJ_IF_SOME(value, maybe) { ... }`
- **Exception**: `kj::runCatchingExceptions([&]() { ... })`
- **Async**: `promise.wait(io.waitScope)`
- **Mock**: Hand-rolled classes with `kj::MutexGuarded<State>`
