---
name: testing
description: Defines the KJ Test framework and testing conventions in VeloZ. Invoke when adding or modifying tests or test configs.
---

# Testing

## Purpose

Use this skill whenever writing or changing tests.

## Test Framework (Mandatory)
- All tests use KJ Test (kj::TestRunner), not GoogleTest.
- Use KJ_TEST, KJ_EXPECT, KJ_ASSERT macros.

## Test Organization
- Tests live under libs/*/tests or tests/integration.
- CMake test executables follow veloz_<module>_tests naming.

## Coverage Expectations
- Core modules: event loop, logger, config, memory pool, JSON, metrics, retry, timer wheel.
- Market: order book, websocket, subscriptions, kline aggregation, quality monitoring.
- Exec: adapters, order router, reconciliation.
- OMS: order records, WAL, positions.
- Risk: circuit breaker, risk engine.
- Strategy: strategy manager, lifecycle.

## Running Tests
- ctest --preset dev -j$(nproc)
- ctest --test-dir build/asan -j$(nproc)
