# VeloZ Crypto Quant Trading Framework: Trading UI

## 3.6 Trading UI

### 3.6.1 Core Pages

- Market and depth:
  - Kline, order book, trades, funding/mark price (derivatives)
- Trading and positions:
  - Order panel (limit/market/conditional mapping)
  - Current positions, open orders, fills, capital usage, risk
- Strategy center:
  - Strategy list, parameter config, start/stop, version management
  - Realtime strategy metrics and alerts
- Backtest and reports:
  - Backtest task management, comparisons, metrics and curves
- AI assistant:
  - Market analysis panel, review panel, traceable conversation logs
- System monitoring:
  - Latency, disconnects, error codes, queue backlog, resource usage

### 3.6.2 Permissions and Audit

- RBAC: admin/trader/researcher/read-only
- Audit: full logging for critical actions (start strategy, change params, manual order, risk toggle)

### 3.6.3 Implementation Details (Recommended)

- UI subscribes only to aggregated state views, not raw exchange feeds
- All UI actions are routed through the control plane with audit trace IDs
