# P0 Feature Test Strategy

This directory contains comprehensive test documentation for the 5 P0 features required to make VeloZ ready for personal traders.

## Overview

| Feature | Document | Priority | Status |
|---------|----------|----------|--------|
| One-click Installer | [01-installer-testing.md](./01-installer-testing.md) | P0 | Draft |
| GUI Configuration | [02-gui-configuration-testing.md](./02-gui-configuration-testing.md) | P0 | Draft |
| Strategy Marketplace | [03-strategy-marketplace-testing.md](./03-strategy-marketplace-testing.md) | P0 | Draft |
| Real-time Charts | [04-realtime-charts-testing.md](./04-realtime-charts-testing.md) | P0 | Draft |
| Security Education | [05-security-education-testing.md](./05-security-education-testing.md) | P0 | Draft |

## Testing Principles

1. **Comprehensive**: Cover all user scenarios including edge cases
2. **Automated**: Minimize manual testing through automation
3. **Realistic**: Test with real exchange connections (testnet)
4. **Performance**: Validate latency and throughput requirements
5. **Security**: Protect user funds and data at all times

## Test Environment Matrix

| Environment | Purpose | Exchange Connection |
|-------------|---------|---------------------|
| Local Dev | Unit tests, component tests | Mock |
| CI/CD | Automated regression | Mock + Testnet |
| Staging | Integration, E2E | Testnet |
| Pre-prod | UAT, Load testing | Testnet |
| Production | Smoke tests only | Mainnet (read-only) |

## Quality Gates

All P0 features must pass the following quality gates before release:

### Gate 1: Unit Test Coverage
- Minimum 80% line coverage
- 100% coverage for critical paths (order execution, risk checks)

### Gate 2: Integration Tests
- All component interactions tested
- API contract validation
- Database migration verification

### Gate 3: Performance Tests
- Latency P99 within SLA
- Throughput meets requirements
- Memory leak detection passed

### Gate 4: Security Tests
- OWASP Top 10 vulnerabilities checked
- Credential storage audit passed
- API authentication verified

### Gate 5: User Acceptance
- All UAT scenarios passed
- Usability feedback incorporated
- Documentation complete

## Bug Tracking Process

### Severity Levels

| Level | Description | Response Time | Resolution Time |
|-------|-------------|---------------|-----------------|
| S0 | Data loss, fund loss, security breach | Immediate | 4 hours |
| S1 | Feature completely broken | 2 hours | 24 hours |
| S2 | Feature partially broken, workaround exists | 8 hours | 72 hours |
| S3 | Minor issue, cosmetic | 24 hours | Next sprint |

### Bug Lifecycle

1. **New**: Bug reported with reproduction steps
2. **Triaged**: Severity assigned, owner assigned
3. **In Progress**: Fix being developed
4. **In Review**: Code review and testing
5. **Verified**: QA verified fix works
6. **Closed**: Deployed to production

## Test Automation Framework

VeloZ uses the following testing frameworks:

### C++ Tests (KJ Test Framework)
- Location: `libs/*/tests/`, `apps/engine/tests/`
- Run: `ctest --preset dev`
- Coverage: `./scripts/coverage.sh`

### Python Tests (pytest)
- Location: `apps/gateway/tests/`
- Run: `pytest apps/gateway/tests/`

### UI Tests (Playwright)
- Location: `apps/ui/tests/`
- Run: `npx playwright test`

### Load Tests
- Location: `tests/load/`
- Run: `./build/dev/tests/load/veloz_production_load_test --quick`

## Related Documentation

- [Build and Run Guide](../../guides/build_and_run.md)
- [CI Configuration](../../../.github/workflows/ci.yml)
- [Test Utilities](../../../tests/test_utilities.h)
- [Integration Test Harness](../../../tests/integration/test_harness.h)
