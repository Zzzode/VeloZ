---
paths:
  - "apps/ui/**"
---

# UI (module notes)

- `apps/ui/index.html` is a **static** UI (no Node/Vite build in this repo yet).
- It talks to the gateway via:
  - REST-style JSON endpoints (e.g. `/api/market`, `/api/orders_state`, `POST /api/order`, `POST /api/cancel`)
  - Server-Sent Events: `GET /api/stream` via `EventSource`
- SSE event names are aligned with engine/gateway event `type` values (e.g. `market`, `order_update`, `fill`, `account`, `error`); keep them consistent across layers.
- Default gateway address is `http://127.0.0.1:8080/`. If opening the HTML via `file://`, the UI needs an explicit API base (see `docs/build_and_run.md`).
