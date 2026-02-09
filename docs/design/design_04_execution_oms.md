# VeloZ Crypto Quant Trading Framework: Execution and OMS

## 3.2 High-Performance Execution (Execution / OMS)

### 3.2.1 Scope

- Place/cancel/modify orders (mapped by exchange capability)
- Account, position, balance sync (periodic REST + WS incremental)
- Order state machine:
  - New → Accepted → PartiallyFilled → Filled / Canceled / Rejected / Expired
  - Handle async receipts and out-of-order updates
- Routing:
  - Separate spot vs derivatives routes
  - Multi-exchange best execution/slicing (later iteration)
- Risk pre-check:
  - Available funds, max leverage, max position, max order rate, max order size
  - Price protection (deviation/slippage caps/market protection)
  - Kill Switch: one-click stop and forced protection mode

### 3.2.2 Low-Latency Critical Path

Optimize the “market → strategy → order → receipt” path:

- Single-thread event loop + lock-free queues (reduce contention)
- Pre-allocated object pools (reduce GC/alloc)
- Batch sending and cancel merging when supported by exchanges
- Network layer:
  - Long-lived connections
  - TLS session reuse
  - Close-to-exchange deployment

### 3.2.3 Idempotency and Consistency

- clientOrderId: generated internally and unique for idempotency and traceability
- Order Journal: write-ahead log (local WAL or DB) for order state recovery
- Account reconciliation:
  - periodic full pull (REST) + realtime incremental (WS)
  - inconsistencies trigger “freeze strategy + auto repair/manual intervention”

### 3.2.4 Trading Interface (Recommended)

Expose a unified interface to strategies:

- place_order(symbol, side, type, qty, price?, tif?, reduce_only?, post_only?, client_order_id?)
- cancel_order(order_id or client_order_id)
- cancel_all(symbol?)
- amend_order(order_id, qty?, price?)
- get_position(symbol)
- get_account()

Strategies only depend on these interfaces, not exchange SDKs.

### 3.2.5 Implementation Details (Recommended)

- Order state machine: explicit transitions, reject invalid state changes, track last update timestamp
- Idempotency: clientOrderId must be unique and stored with every exchange receipt
- Routing: route by venue/market type, enforce capability constraints (IOC/FOK/GTX)
- Receipt ordering: accept out-of-order updates and reconcile via sequence/time
- WAL: persist every order and receipt before applying to in-memory state
