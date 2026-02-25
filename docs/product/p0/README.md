# P0 Product Requirements: Personal Trader Accessibility

## Executive Summary

This document series defines the Product Requirements Documents (PRDs) for VeloZ P0 features that will transform VeloZ from a developer-focused quantitative trading framework into an accessible platform for personal traders (non-technical users).

**Target Timeline:** 3.5-5 months (14-20 sprints at 1 week/sprint)

**Target Users:** Personal traders who:
- Have trading experience but limited technical skills
- Cannot use command-line tools or write code
- Need visual, guided workflows for all operations
- Require strong safety guardrails to prevent costly mistakes

## Current State Analysis

### What VeloZ Has Today

| Capability | Current State | User Requirement |
|------------|---------------|------------------|
| Installation | CLI-based (cmake, git clone) | One-click installer |
| Configuration | Environment variables | Visual GUI |
| Strategies | Code-based development | Browse and configure templates |
| Charts | None | Real-time technical analysis |
| Security | Documentation-based | Interactive education |

### Gap Analysis

```
Current User Journey (Technical):
  git clone -> cmake build -> env vars -> CLI commands -> curl API

Target User Journey (Personal Trader):
  Download -> Install -> Connect Exchange -> Select Strategy -> Trade
```

## P0 Feature Suite

### 1. One-Click Installer
**PRD:** [01-one-click-installer.md](./01-one-click-installer.md)

Transform the current CLI-based installation into a native installer for Windows, macOS, and Linux with automatic dependency management and guided setup.

**Key Outcomes:**
- Installation time < 5 minutes
- Zero command-line interaction required
- Automatic updates and version management

### 2. GUI Configuration
**PRD:** [02-gui-configuration.md](./02-gui-configuration.md)

Replace environment variable configuration with a visual settings interface that guides users through exchange connection, risk parameters, and system preferences.

**Key Outcomes:**
- Visual exchange API key management
- Guided risk parameter configuration
- Real-time connection testing

### 3. Strategy Marketplace
**PRD:** [03-strategy-marketplace.md](./03-strategy-marketplace.md)

Provide a curated library of pre-built trading strategies that users can browse, configure, backtest, and deploy without writing code.

**Key Outcomes:**
- 10+ ready-to-use strategy templates
- Visual parameter configuration
- One-click backtesting and deployment

### 4. Real-Time Charts
**PRD:** [04-realtime-charts.md](./04-realtime-charts.md)

Implement professional-grade charting with candlesticks, technical indicators, drawing tools, and real-time price updates.

**Key Outcomes:**
- TradingView-quality charting experience
- 30+ technical indicators
- Multi-timeframe analysis

### 5. Security Education
**PRD:** [05-security-education.md](./05-security-education.md)

Create an interactive onboarding flow that educates users about API key security, risk management, and safe trading practices before they can trade with real funds.

**Key Outcomes:**
- Mandatory security onboarding
- Simulated trading requirement before live trading
- Progressive risk limit unlocking

## Success Metrics

### Primary KPIs

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Time to First Trade | 2+ hours | < 15 minutes | Analytics |
| Installation Success Rate | ~60% (technical users) | > 95% | Installer telemetry |
| User Retention (30-day) | N/A | > 40% | Product analytics |
| Support Tickets/User | N/A | < 0.5 | Support system |

### Secondary KPIs

| Metric | Target | Measurement |
|--------|--------|-------------|
| NPS Score | > 40 | In-app survey |
| Feature Adoption Rate | > 60% per feature | Analytics |
| Security Incident Rate | < 0.1% | Incident tracking |
| Backtest to Live Ratio | > 5:1 | Analytics |

## Timeline Overview

```
Month 1 (Sprints 1-4):
  - One-Click Installer (Windows/macOS)
  - GUI Configuration (Core screens)
  - Security Education (Onboarding flow)

Month 2 (Sprints 5-8):
  - One-Click Installer (Linux, auto-update)
  - GUI Configuration (Advanced settings)
  - Strategy Marketplace (Core templates)
  - Real-Time Charts (Basic charting)

Month 3 (Sprints 9-12):
  - Strategy Marketplace (Backtesting)
  - Real-Time Charts (Indicators)
  - Security Education (Simulated trading)

Month 4 (Sprints 13-16):
  - Strategy Marketplace (Deployment)
  - Real-Time Charts (Drawing tools)
  - Integration testing
  - Beta release

Month 5 (Sprints 17-20):
  - Bug fixes and polish
  - Performance optimization
  - Documentation
  - GA release
```

## Dependencies and Critical Path

```
                    +-------------------+
                    | One-Click         |
                    | Installer         |
                    +--------+----------+
                             |
              +--------------+--------------+
              |                             |
    +---------v---------+         +---------v---------+
    | GUI Configuration |         | Security          |
    | (Core)            |         | Education         |
    +---------+---------+         | (Onboarding)      |
              |                   +---------+---------+
              |                             |
    +---------v---------+                   |
    | Strategy          |<------------------+
    | Marketplace       |
    +---------+---------+
              |
    +---------v---------+
    | Real-Time         |
    | Charts            |
    +-------------------+
```

**Critical Path:**
1. Installer must be complete before other features can be tested by non-technical users
2. GUI Configuration is required for Strategy Marketplace (API key management)
3. Security Education must gate access to live trading

## Risk Assessment

### High Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Cross-platform installer complexity | Schedule delay | Start with macOS/Windows, defer Linux |
| Chart library performance | User experience | Evaluate TradingView widget vs custom |
| Strategy liability | Legal/reputation | Clear disclaimers, mandatory backtesting |

### Medium Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Exchange API changes | Feature breakage | Abstract adapter layer |
| User security mistakes | Support burden | Mandatory education, guardrails |
| Feature scope creep | Schedule delay | Strict MVP definitions |

### Low Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| UI/UX iteration needs | Minor delays | User testing early |
| Documentation gaps | Support tickets | Contextual help in UI |

## Resource Requirements

### Team Composition

| Role | FTE | Responsibilities |
|------|-----|------------------|
| Product Manager | 0.5 | Requirements, prioritization, user research |
| Frontend Engineer | 2.0 | UI implementation, charting |
| Backend Engineer | 1.0 | Installer, strategy engine integration |
| QA Engineer | 0.5 | Cross-platform testing |
| UX Designer | 0.5 | Wireframes, user flows |

### Infrastructure

| Component | Purpose | Cost Estimate |
|-----------|---------|---------------|
| Code signing certificates | Installer trust | $500/year |
| Analytics platform | Usage tracking | $200/month |
| Beta testing platform | User feedback | $100/month |

## Document Index

1. [One-Click Installer PRD](./01-one-click-installer.md)
2. [GUI Configuration PRD](./02-gui-configuration.md)
3. [Strategy Marketplace PRD](./03-strategy-marketplace.md)
4. [Real-Time Charts PRD](./04-realtime-charts.md)
5. [Security Education PRD](./05-security-education.md)

## Approval

| Role | Name | Date | Status |
|------|------|------|--------|
| Product Owner | | | Pending |
| Engineering Lead | | | Pending |
| UX Lead | | | Pending |

---

*Last Updated: 2026-02-25*
*Version: 1.0*
