# P0 Features Architecture: Personal Trader Readiness

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase

---

## Executive Summary

This document series defines the architecture for P0 features that will make VeloZ ready for personal traders (non-technical users). These features transform VeloZ from a developer-focused framework into a user-friendly trading platform.

---

## P0 Feature Overview

| # | Feature | Priority | Complexity | Est. Time |
|---|---------|----------|------------|-----------|
| 1 | [One-click Installer System](01-installer-system.md) | Critical | High | 4-6 weeks |
| 2 | [GUI Configuration System](02-gui-configuration.md) | Critical | Medium | 3-4 weeks |
| 3 | [Strategy Marketplace](03-strategy-marketplace.md) | High | High | 5-7 weeks |
| 4 | [Real-time Charting System](04-charting-system.md) | High | Medium | 3-4 weeks |
| 5 | [Security Education System](05-security-education.md) | Critical | Medium | 2-3 weeks |

**Total Estimated Time**: 17-24 weeks (with parallelization: 12-16 weeks)

---

## Current Architecture Context

### Existing Components

```
VeloZ Current Architecture
==========================

+------------------+     +------------------+     +------------------+
|   React UI       |     |  Python Gateway  |     |   C++ Engine     |
|   (apps/ui)      |<--->|  (apps/gateway)  |<--->|  (apps/engine)   |
|                  |     |                  |     |                  |
| - TypeScript     |     | - HTTP/SSE API   |     | - Market Data    |
| - Vite           |     | - JWT Auth       |     | - Execution      |
| - TailwindCSS    |     | - RBAC           |     | - OMS            |
| - Zustand        |     | - Audit Logging  |     | - Risk           |
| - TanStack Query |     | - Metrics        |     | - Strategy       |
+------------------+     +------------------+     +------------------+
         |                       |                       |
         v                       v                       v
+------------------+     +------------------+     +------------------+
| REST + SSE       |     | stdio (NDJSON)   |     | KJ Async I/O     |
| WebSocket        |     | Process Control  |     | Event Loop       |
+------------------+     +------------------+     +------------------+
```

### Technology Stack

| Layer | Current Tech | Status |
|-------|--------------|--------|
| **UI** | React 19.2, TypeScript 5.9, Vite 7.3, Tailwind 4.2 | Production-ready |
| **Gateway** | Python 3, stdlib HTTP, JWT, RBAC | Production-ready |
| **Engine** | C++23, KJ Library, CMake | Production-ready |
| **Charts** | Lightweight Charts 5.1 | Integrated |
| **State** | Zustand, TanStack Query | Integrated |

---

## P0 Architecture Principles

### 1. User-First Design
- Zero technical knowledge required for basic operations
- Progressive disclosure of advanced features
- Guided workflows for complex tasks

### 2. Security by Default
- No withdrawal permissions on API keys
- Mandatory simulated trading period
- Built-in loss limits and circuit breakers

### 3. Cross-Platform Consistency
- Identical experience on Windows/macOS/Linux
- Native OS integration (keychain, notifications)
- Responsive design for various screen sizes

### 4. Performance Requirements
- UI response time < 100ms
- Chart updates at 60fps
- Order placement latency < 50ms (UI to gateway)

### 5. Maintainability
- Clean integration with existing codebase
- Minimal changes to core engine
- Feature flags for gradual rollout

---

## Integration Architecture

### High-Level P0 Integration

```
                                    P0 Features Integration
                                    =======================

+-----------------------------------------------------------------------------------+
|                                    User Layer                                      |
+-----------------------------------------------------------------------------------+
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
|  | Installer   |  | Setup       |  | Strategy    |  | Charting    |  | Security  ||
|  | Wizard      |  | Wizard      |  | Marketplace |  | System      |  | Education ||
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                                  React UI Layer                                    |
+-----------------------------------------------------------------------------------+
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
|  | Config      |  | Keychain    |  | Strategy    |  | Chart       |  | Tutorial  ||
|  | Manager     |  | Integration |  | Browser     |  | Components  |  | System    ||
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                                Python Gateway Layer                                |
+-----------------------------------------------------------------------------------+
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
|  | Config      |  | Credential  |  | Strategy    |  | Market Data |  | User      ||
|  | Persistence |  | Vault       |  | Repository  |  | Aggregator  |  | Progress  ||
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                                 C++ Engine Layer                                   |
+-----------------------------------------------------------------------------------+
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
|  | Config      |  | Strategy    |  | Backtest    |  | Risk        |  | Simulated ||
|  | Manager     |  | Runtime     |  | Engine      |  | Engine      |  | Trading   ||
|  +-------------+  +-------------+  +-------------+  +-------------+  +-----------+|
+-----------------------------------------------------------------------------------+
```

---

## Data Flow Overview

### Configuration Flow

```
User Input (GUI)
       |
       v
+------------------+
| React Config UI  |
| - Form validation|
| - Preview        |
+------------------+
       |
       v (REST API)
+------------------+
| Gateway Config   |
| - Encryption     |
| - Validation     |
| - Persistence    |
+------------------+
       |
       v (stdio)
+------------------+
| Engine Config    |
| - Hot reload     |
| - Type-safe      |
+------------------+
```

### Strategy Execution Flow

```
Strategy Selection (Marketplace)
       |
       v
+------------------+
| Parameter Config |
| - Validation     |
| - Risk limits    |
+------------------+
       |
       v
+------------------+
| Backtest Runner  |
| - Historical data|
| - Performance    |
+------------------+
       |
       v (if approved)
+------------------+
| Live Execution   |
| - Risk checks    |
| - Monitoring     |
+------------------+
```

---

## Security Architecture

### Defense in Depth

```
Layer 1: User Education
+------------------+
| - Onboarding     |
| - Risk warnings  |
| - Checklists     |
+------------------+
       |
       v
Layer 2: Application Security
+------------------+
| - Input valid.   |
| - Rate limiting  |
| - Audit logging  |
+------------------+
       |
       v
Layer 3: Credential Security
+------------------+
| - OS Keychain    |
| - Encryption     |
| - No withdrawals |
+------------------+
       |
       v
Layer 4: Trading Safety
+------------------+
| - Loss limits    |
| - Circuit breaker|
| - Simulated mode |
+------------------+
```

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-4)
- Installer framework setup
- Configuration system backend
- Security education framework

### Phase 2: Core Features (Weeks 5-10)
- GUI configuration wizard
- Strategy marketplace backend
- Charting system integration

### Phase 3: Integration (Weeks 11-14)
- End-to-end testing
- Cross-platform validation
- Performance optimization

### Phase 4: Polish (Weeks 15-16)
- User testing
- Documentation
- Release preparation

---

## Success Metrics

| Metric | Target |
|--------|--------|
| Installation success rate | > 95% |
| First trade completion rate | > 80% |
| User retention (30 days) | > 60% |
| Support tickets per user | < 0.5 |
| Security incident rate | 0 |

---

## Document Index

1. [One-click Installer System](01-installer-system.md)
   - Cross-platform packaging
   - Auto-update mechanism
   - Dependency management

2. [GUI Configuration System](02-gui-configuration.md)
   - Settings storage
   - API key management
   - First-run wizard

3. [Strategy Marketplace](03-strategy-marketplace.md)
   - Strategy templates
   - Parameter system
   - Performance tracking

4. [Real-time Charting System](04-charting-system.md)
   - Chart library integration
   - Technical indicators
   - Order placement

5. [Security Education System](05-security-education.md)
   - Onboarding wizard
   - Simulated trading
   - Loss limits

---

## Related Documentation

- [Core Architecture](../design_12_core_architecture.md)
- [UI Architecture](../ui-react-architecture.md)
- [Security Best Practices](../../guides/user/security-best-practices.md)
- [Trading Guide](../../guides/user/trading-guide.md)
