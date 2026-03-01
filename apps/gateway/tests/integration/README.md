# Gateway Integration Tests

This directory contains end-to-end integration tests for the VeloZ C++ Gateway.

## Overview

Integration tests validate the complete request flow through the gateway, testing multiple components working together. Unlike unit tests that test individual components in isolation, integration tests verify that the system works correctly as a whole.

## Test Categories

### 1. Full Flow Tests (`test_full_flow.cpp`)

Tests the complete request lifecycle from authentication through order submission to SSE notifications.

**Test Coverage:**
- Authentication flow (login, token refresh, logout)
- Order submission and state transitions
- SSE event delivery
- Error scenarios and recovery

**Performance Targets:**
- Full flow (auth → order → SSE): <500ms
- Order submission: <10ms
- SSE event delivery: <1s

### 2. Rate Limiting Tests (`test_rate_limiting_integration.cpp`)

Tests the rate limiting middleware under various scenarios.

**Test Coverage:**
- Token bucket algorithm behavior
- Rate limit enforcement
- Per-user vs per-IP limiting
- Token refill over time
- Recovery after rate limit

**Performance Targets:**
- Rate limit check: <1μs
- Token refill: <1ms

### 3. SSE Integration Tests (`test_sse_integration.cpp`)

Tests Server-Sent Events streaming functionality.

**Test Coverage:**
- Event delivery and ordering
- Event history replay
- Reconnection with Last-Event-ID
- Keep-alive mechanism
- Multiple concurrent connections
- Connection cleanup

**Performance Targets:**
- SSE connection: <100ms
- Event delivery: <10ms per event
- Max concurrent connections: 1000+

### 4. Concurrent Access Tests (`test_concurrent_access.cpp`)

Tests thread safety and race condition prevention.

**Test Coverage:**
- Concurrent HTTP requests
- Thread-safe event broadcasting
- Race condition detection
- Concurrent order submissions
- Thread-safe rate limiting
- Concurrent SSE subscriptions

**Performance Targets:**
- 100 concurrent requests: <5s total
- No race conditions or deadlocks
- Linear scalability up to 1000 connections

## Running Integration Tests

### Build and Run

```bash
# Configure and build
cmake --preset dev
cmake --build --preset dev-all

# Run integration tests
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests
```

### Run Specific Test Category

```bash
# Run only full flow tests
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests --filter="*full*"

# Run only rate limiting tests
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests --filter="*rate_limiting*"

# Run only SSE tests
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests --filter="*sse*"

# Run only concurrent access tests
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests --filter="*concurrent*"
```

### Run with Verbose Output

```bash
# Show detailed test output
./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests -v

# Show very verbose output (includes KJ_LOG statements)
KJ_DEBUG=1 ./build/dev/apps/gateway_cpp/veloz_gateway_integration_tests -vv
```

### Run with CTest

```bash
# Run all gateway tests (including integration)
ctest --preset dev

# Run only integration tests
ctest -R integration -V

# Run integration tests with timeout
ctest -R integration --timeout 300
```

## Test Environment Configuration

Integration tests use specific environment variables for configuration:

```bash
# Authentication
VELOZ_ADMIN_PASSWORD="integration_test_admin_password"
VELOZ_JWT_SECRET="test_secret_key_for_integration_tests_32_chars!"

# Gateway
VELOZ_GATEWAY_HOST="127.0.0.1"
VELOZ_GATEWAY_PORT="18080"

# Rate Limiting
VELOZ_RATE_LIMIT_CAPACITY="1000"
VELOZ_RATE_LIMIT_REFILL="100.0"
```

These are automatically set by `main.cpp` before running tests.

## Common Utilities

The test suite includes common utilities in `test_common.h`:

- **Mock Classes**: Mock HTTP request/response for testing
- **Timing Helpers**: Measure execution time in ns/μs/ms
- **JSON Helpers**: Create test JSON payloads
- **Assertion Helpers**: Custom assertions for HTTP responses
- **Performance Assertions**: Verify performance targets

## Adding New Integration Tests

To add a new integration test:

1. Create a new `.cpp` file in this directory
2. Include necessary headers:
   ```cpp
   #include "test_common.h"
   #include <kj/test.h>
   #include "../src/your_handler.h"
   ```
3. Use `KJ_TEST()` macro to define tests:
   ```cpp
   KJ_TEST("Descriptive test name") {
     // Test code here
     KJ_EXPECT(condition);
   }
   ```
4. Update `CMakeLists.txt` to include the new file in `veloz_gateway_integration_tests`
5. Run the test suite to verify

## Performance Benchmarking

Integration tests include performance assertions that log warnings when targets are not met. In debug builds, these are warnings; in release builds, they should all pass.

To run performance benchmarks:

```bash
# Build in release mode for accurate performance
cmake --preset release
cmake --build --preset release-all

# Run tests and watch for performance logs
./build/release/apps/gateway_cpp/veloz_gateway_integration_tests -v
```

## Troubleshooting

### Tests Fail in CI

- Check test timeout settings in CMakeLists.txt
- Verify all dependencies are available
- Ensure sufficient resources for concurrent tests

### Performance Tests Fail

- Performance targets may not be met in debug builds
- Run in release mode for accurate measurements
- Check system load during test execution

### Concurrent Test Failures

- Ensure thread-safe libraries are linked correctly
- Check for deadlocks or livelocks
- Verify atomic operations are used correctly

## Coverage Requirements

Integration tests should cover:

- ✅ All API endpoints tested
- ✅ Error cases covered (401, 403, 404, 429, 500)
- ✅ Concurrent access validated
- ✅ SSE streaming verified
- ✅ Rate limiting enforcement tested
- ✅ Performance targets met in release builds

## References

- [KJ Test Framework](https://capnproto.org/cxx.html#kj-test-framework)
- [Gateway Architecture](../../ARCHITECTURE.md)
- [Unit Tests](../test_main.cpp)
