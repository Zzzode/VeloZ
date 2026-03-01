---
paths:
  - "libs/*/tests/*.cpp"
  - "tests/integration/*.cpp"
  - "libs/core/kj/kj-test.h"
---

# Testing (Test Framework and Coverage)

## Overview

This skill provides guidance for test development in VeloZ. All tests use KJ Test framework (`kj::TestRunner`), NOT GoogleTest.

## Test Framework (MANDATORY)

### KJ Test Framework (DEFAULT)

**All tests MUST use KJ Test framework, NOT GoogleTest.**

| GoogleTest (FORBIDDEN) | KJ Test (REQUIRED) |
|---------------------|--------------------|
| `TEST()`, `TEST_F()` | `KJ_TEST()` |
| `EXPECT_EQ()`, `EXPECT_NE()` | `KJ_EXPECT()`, `KJ_ASSERT()` |
| `GoogleTest` | `kj::TestRunner` |
| `gmock` | KJ mocking (if needed) |

### Test Macro Reference

```cpp
// KJ Test assertion macro
KJ_TEST("test_name") {
    // test code
}

// Test expectations
KJ_ASSERT(condition, "description");      // Fatal if false (debug only)
KJ_EXPECT(condition, "description");      // Non-fatal assertion
```

## Test File Organization

### Test File Naming

Test files must follow `<module>-test.cpp` pattern in test subdirectory:

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
├── exec/
│   └── tests/
│       └── test_*.cpp
└── ...
```

### CMake Test Configuration

```cmake
# Enable testing early
if(VELOZ_BUILD_TESTS)
    enable_testing()
endif()

# Test executable naming
add_executable(veloz_core_tests
    tests/test_event_loop.cpp
    tests/test_logger.cpp
    tests/test_memory_pool.cpp
    ...
    )

# Link KJ Test framework
target_link_libraries(veloz_core_tests
    PRIVATE
      veloz_core
      kj-test    # KJ Test framework
    )

add_test(NAME veloz_core_tests COMMAND veloz_core_tests)
```

## Test Coverage Requirements

### Unit Test Coverage

Each library module should have comprehensive unit tests for:

| Module | Required Coverage |
|---------|------------------|
| Core | Event loop, logger, config, memory pool, JSON, metrics, retry, timer wheel |
| Market | Order book, WebSocket, subscriptions, market quality, kline aggregation |
| Exec | Exchange adapters, order router, client order ID, resilient adapter, reconciliation |
| OMS | Order records, order WAL, positions |
| Risk | Circuit breaker, risk engine |
| Strategy | Strategy manager, advanced strategies |

### Integration Tests

Integration tests are located in `tests/integration/`:

```
tests/integration/
├── test_backtest_e2e.cpp
├── test_multi_strategy.cpp
└── test_order_wal_recovery.cpp
```

**Integration test requirements:**
- Test cross-module interactions
- Test data flow between engine, OMS, market data, execution
- Test order lifecycle end-to-end
- Test failure and recovery scenarios

## Test Best Practices

### Test Structure

```cpp
// Good test structure
KJ_TEST("ComponentName_Description") {
    // Setup phase
    // Arrange test data and preconditions

    // Execute phase
    // Call the code being tested

    // Assert phase
    // Verify expected outcomes
}
```

### KJ Type Handling in Tests

```cpp
// Test with KJ Maybe
kj::Maybe<int> maybeValue = computeValue();
KJ_IF_SOME(value, maybeValue) {
    KJ_EXPECT(value == 42, "computed value should be 42");
}

// Test with KJ Own
kj::Own<MyObject> obj = kj::heap<MyObject>(/* args */);
KJ_EXPECT(obj != nullptr, "owned object should not be null");
```

### Test File Checklist

Each test file should:

- [ ] Include `kj/common.h>` for KJ types
- [ ] Include module header being tested
- [ ] Use `KJ_TEST()` macro for test cases
- [ ] Test both success and failure paths
- [ ] Test edge cases (empty input, null values, boundaries)
- [ ] Test thread safety if applicable
- [ ] Clean up test resources

## Running Tests

### Local Test Execution

```bash
# Run all tests with CTest preset
ctest --preset dev -j$(nproc)

# Run tests from specific build directory
ctest --test-dir build/asan -j$(nproc)

# Run specific test
ctest -R veloz_core_tests -T "test_name"
```

### CI Test Execution

The CI pipeline runs tests automatically:

```yaml
dev-build:
  steps:
    - name: Run all tests
      run: ctest --preset dev -j$(nproc)
```

## Test Quality Gates

### Coverage Requirements

- [ ] Unit test coverage > 80% for core modules
- [ ] Unit test coverage > 70% for market/exec modules
- [ ] Integration tests for critical paths

### Quality Metrics

- [ ] All tests pass on clean build
- [ ] ASan/UBSan builds run without issues
- [ ] Test execution time < 5 minutes for unit tests

## Common Test Patterns

### Mock Objects

When mocking external dependencies, use interfaces and test doubles:

```cpp
// Good: Use interface for mocking
class MockOrderAdapter : public OrderAdapter {
public:
    kj::Own<Order> lastOrder;

    kj::Promise< kj::Own<Order>> submitOrder(kj::Own<Order> order) override {
        lastOrder = kj::mv(order);
        return kj::Promise<kj::Own<Order>>(kj::mv(lastOrder));
    }
};

// Avoid: Mocking concrete implementations with complex dependencies
```

### Thread Safety Tests

```cpp
KJ_TEST("ThreadPool_SharedCounter") {
    const int threadCount = 4;
    const int iterations = 1000;

    kj::Atomic<uint64_t> counter(0);

    // Spawn threads that increment counter
    // Verify final count equals iterations * threadCount
}
```

### Exception Tests

```cpp
KJ_TEST("Exception_ErrorMessage") {
    kj::String error = "test error";
    kj::ThrowException(kj::Exception::Type::FAILED, __FILE__, __LINE__, error);
}
```

### KJ Library Integration in Tests

```cpp
// Use KJ types in test expectations
kj::Own<MyType> obj = kj::heap<MyType>(/* args */);
KJ_EXPECT(obj != nullptr, "object should be allocated");

// Use KJ String for comparisons
kj::String expected = "expected"_kj;
kj::String actual = "actual"_kj;
KJ_EXPECT(expected == actual, "strings should match");

// Use KJ Mutex for thread-safe tests
kj::MutexGuarded<int> guardedValue(0);
auto lock = guardedValue.lockExclusive();
KJ_EXPECT(*lock == 42, "value should be correct");
```

## Test File Examples

### Core Library Test Example

```cpp
// libs/core/tests/test_event_loop.cpp

#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include <kj/common.h>

KJ_TEST("EventLoop_TaskExecution") {
    veloz::core::EventLoop loop;

    // Post a simple task
    bool executed = false;
    auto future = loop.post([&]() {
        executed = true;
    }, veloz::core::EventPriority::Normal);

    // Wait for task
    kj::WaitScope waitScope;
    kj::setupAsyncIo().waitScope.wait(future.wait(waitScope));

    KJ_EXPECT(executed == true, "task should have executed");
}

KJ_TEST("EventLoop_PriorityOrdering") {
    veloz::core::EventLoop loop;

    int executionOrder[3] = {0, 0, 0};

    // Post tasks with different priorities
    loop.post([&]() { executionOrder[0]++; }, veloz::core::EventPriority::Low);
    loop.post([&]() { executionOrder[1]++; }, veloz::core::EventPriority::Normal);
    loop.post([&]() { executionOrder[2]++; }, veloz::core::EventPriority::Critical);

    // Verify critical task runs first
    KJ_EXPECT(executionOrder[0] == 1, "critical task should execute first");
    KJ_EXPECT(executionOrder[1] == 2, "normal task should execute second");
    KJ_EXPECT(executionOrder[2] == 3, "low task should execute last");
}
```

### Market Library Test Example

```cpp
// libs/market/tests/test_order_book.cpp

#include "veloz/market/order_book.h"
#include <kj/common.h>

KJ_TEST("OrderBook_BidAskUpdates") {
    veloz::market::OrderBook book;

    // Update best bid
    book.updateBid(50000.0, 1.0, 100);

    // Verify best bid is set
    KJ_IF_SOME(bestBid, book.bestBid()) {
        KJ_EXPECT(bestBid->price == 50000.0, "bid price should match");
        KJ_EXPECT(bestBid->quantity == 1.0, "bid quantity should match");
    }

    // Update best ask
    book.updateAsk(50100.0, 1.0, 100);

    // Verify spread
    KJ_IF_SOME(spread, book.spread()) {
        KJ_EXPECT(*spread == 100.0, "spread should be 100.0");
    }
}

KJ_TEST("OrderBook_SnapshotReconstruction") {
    // Test order book snapshot reconstruction from sequence
    // Test out-of-order handling
    // Test gap filling
}
```

### Exec Library Test Example

```cpp
// libs/exec/tests/test_order_router.cpp

#include "veloz/exec/order_router.h"
#include <kj/common.h>

KJ_TEST("OrderRouter_OrderRouting") {
    veloz::exec::OrderRouter router;

    kj::Maybe<veloz::exec::ExchangeAdapter*> binanceAdapter = /* get adapter */;

    // Test order routing
    kj::Own<veloz::exec::Order> order = kj::heap<veloz::exec::Order>(
        /* init order */
    );

    auto result = router.submitOrder(kj::mv(order));

    KJ_IF_SOME(result, submissionResult) {
        KJ_EXPECT(submissionResult.success == true, "order should be submitted successfully");
    }
}

KJ_TEST("OrderRouter_ErrorHandling") {
    veloz::exec::OrderRouter router;

    // Test error handling when no adapter available
    kj::Own<veloz::exec::Order> order = kj::heap<veloz::exec::Order>(
        /* init invalid order */
    );

    auto result = router.submitOrder(kj::mv(order));

    KJ_EXPECT(result.success == false, "should fail for invalid order");
}
```

## Known Test Issues and Solutions

### Common Pitfalls

1. **Race Conditions in Async Tests**
   - Problem: Test finishes before async operation completes
   - Solution: Use `kj::WaitScope` and call `.wait()` properly

2. **Memory Leaks in Tests**
   - Problem: Test allocations not freed
   - Solution: Use KJ own objects that auto-clean up

3. **Flaky Tests Due to Timing**
   - Problem: Test depends on precise timing
   - Solution: Add retry logic or timing tolerance

4. **KJ Type Incompatibility**
   - Problem: Using std types in KJ tests
   - Solution: Use `kj::String`, `kj::Maybe<T>`, `kj::Own<T>`

## Files to Check

- `/Users/bytedance/Develop/VeloZ/libs/core/kj/kj-test.h` - KJ Test framework headers
- `/Users/bytedance/Develop/VeloZ/libs/core/CMakeLists.txt` - Test build configuration
- `/Users/bytedance/Develop/VeloZ/.claude/rules/cpp-engine.md` - C++ engine rules

## References

- KJ Test Framework: Part of KJ library (`libs/core/kj/`)
- KJ Library: https://github.com/capnproto/capnproto/tree/v2/c++/src/kj
