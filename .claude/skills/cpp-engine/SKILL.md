---
name: cpp-engine
description: Guides C++ engine and library rules in VeloZ. Invoke when editing apps/engine or libs/* C++ code.
---

# C++ Engine and Libraries

## Purpose

Use this skill for all C++ changes in apps/engine and libs/*.

## Critical Rules

### No Raw Pointers (Mandatory)
- Raw T* is forbidden for ownership or nullable values.
- Use kj::Own<T> for ownership.
- Use kj::Maybe<T> for nullable values.
- Use T& for non-null references.
- Raw pointers allowed only for external C APIs or opaque handles, with explicit justification.

### KJ-First Standard
- KJ types are the default choice over std types.
- Use std only when KJ has no equivalent or required by external APIs.

### Known Std Exceptions
- std::string for OpenSSL HMAC, yyjson C API, copyable structs.
- std::format for width specifiers.
- std::filesystem for path operations.
- std::unique_ptr for custom deleters or polymorphic ownership.
- std::function for STL compatibility or recursive lambdas.
- std::map and std::vector for required ordered iteration or API returns.

## Engine Stdio Protocol (Critical)
- Inbound commands: ORDER <BUY|SELL> <symbol> <qty> <price> <client_order_id>, CANCEL <client_order_id>.
- Outbound events: NDJSON with stable type and field names.
- Timestamps: ISO 8601 UTC (e.g., 2025-01-15T10:30:00.123Z).
- New event types must use snake_case names.

## Module Boundaries
- libs/ modules depend on libs/core or one-way peers only.
- apps/ depend on libs/; libs/ must not depend on apps/.
- Public headers under libs/*/include/veloz/ are stable APIs.

## Testing Requirements
- Use KJ Test (kj::TestRunner) only.
- Test files live under libs/*/tests and use KJ_TEST macros.
