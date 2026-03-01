---
name: architecture
description: Guides VeloZ architecture, module boundaries, and dependency rules. Invoke when changing cross-module design, contracts, or system structure.
---

# Architecture

## Purpose

Use this skill when making architectural decisions or changes that affect module boundaries, dependencies, or system structure.

## Core Architectural Principles

### Event-Driven Architecture
- All communication flows through immutable events ordered by timestamp.
- Typical flow: market data → signals → order updates → fills.
- Event types: market, order_update, fill, order_state, account, error.

### Layered Decoupling
- Presentation: apps/ui, apps/gateway
- Control plane: services/*, proto/
- Data plane: apps/engine (market/execution/OMS/risk)

### Modular Monolith with Split Readiness
- Current state: modular monolith for iteration speed.
- Future state: split into services with stable proto/ contracts.

### Consistency First
- Order state derived from event streams.
- Idempotent event processing and replayability.

### Observability First
- Structured logging with correlation IDs.
- Metrics at module boundaries.

### Security First
- No secrets in code.
- Secrets via environment variables only.
- Audit logging for sensitive operations.

## Module Boundaries

### Directory Structure
- apps/: deployable application units
- libs/: reusable core libraries
- services/: scalable services (future)
- proto/: single source of truth for contracts
- python/: strategy SDK and research
- infra/: deployment configuration
- tools/: codegen and tooling

### Dependency Rules
- libs/ modules depend on libs/core or one-way peers only.
- No circular dependencies in libs/.
- apps/ depend on libs/ and proto/, not services/ internals.
- proto/ must not depend on business implementation code.
- python/strategy-sdk is versioned independently.

## Interface Stability
- Public headers in libs/*/include/veloz/ are stable APIs.
- Breaking changes require major version bump.
- Deprecations require a 2-release notice.

## Data Plane vs Control Plane
- Data plane: low-latency event flow, backpressure-aware, droppable low-priority events.
- Control plane: configuration, auditability, retryable operations, stable contracts.

## Design Patterns to Prefer
- RAII for resource ownership.
- Strategy pattern for pluggable algorithms.
- Composite and builder patterns for hierarchical config.
- Observer pattern for change notifications.
