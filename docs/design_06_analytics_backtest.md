# VeloZ Crypto Quant Trading Framework: Analytics and Backtest

## 3.4 Analytics (PnL / Risk / Backtest)

### 3.4.1 Core Metrics

- Returns and risk:
  - Total return, annualized return, volatility, Sharpe, Sortino, max drawdown, Calmar
  - Daily/weekly/monthly return distribution, skew/kurtosis, tail risk (VaR/CVaR optional)
- Trade quality:
  - Fill rate, cancel rate, average holding time, slippage, fees, impact
  - Profit/loss ratio, win rate, expectancy, longest losing streak
- Portfolio dimension:
  - Multi-asset decomposition, capital usage, leverage utilization, risk budget usage

### 3.4.2 Backtest Engine Design

- Data sources:
  - Kline-level backtest (mid/low frequency)
  - Tick/OrderBook backtest (HFT; higher cost)
- Matching models:
  - Market: next trade or order-book impact model
  - Limit: order-book penetration/queue position approximation (configurable)
- Cost models:
  - Fees (maker/taker)
  - Funding rates (derivatives)
  - Slippage models (linear/segment/impact)
- Event replay:
  - Reuse strategy runtime model (BacktestEnv)
  - Support accelerated replay / realtime replay / custom window replay

### 3.4.3 Storage and Query (Recommended)

- Raw high-frequency data: ClickHouse (columnar, high-throughput writes, fast queries)
- Structured business data: PostgreSQL (orders, fills, account snapshots, strategy versions, audit)
- Cache: Redis (subscriptions, hot markets, sessions)
- Cold storage: object storage (compressed archive, partitioned by day/market)

### 3.4.4 Implementation Details (Recommended)

- Online PnL: update on each fill with fee and funding adjustments
- Backtest reports: store both aggregated metrics and raw curve points
- Cross-check: reconcile PnL between execution receipts and account snapshots
