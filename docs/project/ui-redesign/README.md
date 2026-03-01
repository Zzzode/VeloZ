# VeloZ UI Redesign Documentation

## Overview

This directory contains comprehensive UI/UX design documentation for the VeloZ trading platform redesign project.

## Documents

| Document | Description |
|----------|-------------|
| [01-ui-architecture.md](./01-ui-architecture.md) | Application architecture, technology stack, folder structure, state management |
| [02-design-system.md](./02-design-system.md) | Colors, typography, spacing, component tokens, Tailwind theme |
| [03-component-specs.md](./03-component-specs.md) | Detailed specifications for all UI components |
| [04-user-flows.md](./04-user-flows.md) | User flow diagrams for all major features |
| [05-accessibility.md](./05-accessibility.md) | WCAG 2.1 AA compliance requirements |

## Design Summary

### Technology Stack

| Layer | Technology |
|-------|------------|
| Language | TypeScript 5.4+ |
| Framework | React 19.x |
| Build Tool | Vite 7.x |
| Styling | Tailwind CSS 4.x |
| UI Components | Headless UI + Custom Components |
| Icons | Lucide React |
| State Management | Zustand + TanStack Query |
| Forms | React Hook Form + Zod |
| Charts | Recharts |
| Testing | Vitest + React Testing Library |

### Key Features

1. **Authentication**
   - JWT login with access/refresh tokens
   - Automatic token refresh
   - Session expiration handling

2. **Role-Based Access Control**
   - Viewer: Read-only access
   - Trader: Read + write access
   - Admin: Full access + audit logs

3. **Real-Time Updates**
   - SSE for order book, positions, orders
   - WebSocket option for market data
   - Connection status indicators

4. **Trading Interface**
   - Order book with depth visualization
   - Order form with validation
   - Position tracking with PnL
   - Strategy management

5. **Admin Features**
   - Audit log viewer with filters
   - Export functionality
   - System monitoring

### Design Principles

1. **Clarity First** - Trading interfaces must be instantly readable
2. **Information Density** - Optimize for data visibility
3. **Real-Time Feedback** - Immediate visual feedback for all actions
4. **Error Prevention** - Confirmations for destructive actions
5. **Accessibility** - WCAG 2.1 AA compliance

### Color System

| Purpose | Light Mode | Dark Mode |
|---------|------------|-----------|
| Buy/Long | `#52c41a` | `#49aa19` |
| Sell/Short | `#ff4d4f` | `#dc4446` |
| Profit | `#52c41a` | `#49aa19` |
| Loss | `#ff4d4f` | `#dc4446` |
| Primary | `#1890ff` | `#177ddc` |

### Component Hierarchy

```
App
├── ErrorBoundary
├── QueryProvider (TanStack Query)
├── ToastProvider
└── AppRouter
    ├── LoginPage
    └── ProtectedRoute
        └── MainLayout
            ├── Header
            ├── Sidebar
            └── Content (Outlet)
                ├── Dashboard
                ├── Trading
                ├── Market
                ├── Strategies
                ├── Backtest
                └── Settings
```

## Implementation Status

### Completed
- Project scaffolding (Vite + React + TypeScript)
- Tailwind CSS 4.x theme configuration
- Core UI components (Button, Card, Input, Select, Modal, Toast, Spinner)
- Layout components (Header, Sidebar, Footer, MainLayout)
- API client with interceptors and token refresh
- SSE client for real-time updates
- WebSocket clients for market/order data
- Authentication flow (Login, ProtectedRoute, authStore)
- Router with lazy loading

### In Progress
- Feature page implementations (Dashboard, Trading, Market, Strategies, Backtest)
- Trading-specific components (OrderBook, OrderForm, PositionTable)

### Pending
- Comprehensive testing suite
- Dark mode implementation
- Accessibility audit
- Performance optimization

## Related Documents

- [Implementation Status](../reviews/implementation_status.md) - Backend feature status
- [HTTP API Reference](../../api/http_api.md) - API documentation
- [Project Overview](../../crypto_quant_framework_design.md) - System design
