---
paths:
  - "apps/ui/**"
---

# UI (module notes)

- `apps/ui/` is a **React + TypeScript** production-ready UI built with Vite.
- It talks to the gateway via:
  - REST-style JSON endpoints (e.g. `/api/market`, `/api/orders_state`, `POST /api/order`, `POST /api/cancel`)
  - Server-Sent Events: `GET /api/stream` via `EventSource`
  - WebSocket connections for real-time market data and order book updates
- SSE event names are aligned with engine/gateway event `type` values (e.g. `market`, `order_update`, `fill`, `account`, `error`); keep them consistent across layers.
- Default gateway address is `http://127.0.0.1:8080/`.
- Technology stack: React 19.2, TypeScript 5.9, Vite 7.3, Tailwind CSS 4.2, Zustand, TanStack Query
- Comprehensive test suite: 551 unit/integration tests + 33 E2E tests (100% pass rate)
