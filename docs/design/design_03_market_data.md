# VeloZ Crypto Quant Trading Framework: Market Data

## 3.1 High-Performance Market Data (Market Data)

### 3.1.1 Scope

- Multi-exchange support (initial: Binance; extend to OKX, Bybit, Gate, Coinbase, etc.)
- Unified market types:
  - Spot: Trades, OrderBook (depth), BestBidAsk, Kline, Ticker, AggTrade
  - USDT-margined perps: same + FundingRate, MarkPrice, IndexPrice, OpenInterest
  - Coin-margined perps: same (note contract face value and settlement currency)
- Subscription management: aggregate and deduplicate by strategy/UI
- Data normalization: exchange symbols, price/qty precision, contract multipliers
- Data quality: sequence validation, snapshot+delta alignment, reconnect, latency monitoring, loss metrics

### 3.1.2 Key Design Points

#### (1) Exchange Adapter Layer

- REST: initial snapshot, trading rules, historical Klines, account/position fetch
- WebSocket: realtime market and user data streams (if shared with trading)
- Rate limits/quotas: token buckets per API key/IP/endpoint with built-in queuing and degradation

#### (2) OrderBook Rebuild

- Snapshot + incremental diff: merge strictly by exchange sequence; detect gaps and re-snapshot
- In-memory representation: ordered price levels (array/skiplist/balanced tree wrappers), expose:
  - TopN (1/5/20/100 levels)
  - Full depth (market making/microstructure strategies)
  - Spread, weighted mid, impact cost, and derived metrics

#### (3) Latency and Sync

- Local timestamps: receive time, parse finish, publish time
- Exchange timestamps: event time + server time calibration (periodic ping)
- Metrics: end-to-end P99/P999 latency, reconnect count, message QPS, drop rate

### 3.1.3 Market Event Contract (Recommended)

Use a unified internal event model to avoid direct dependency on exchange fields:

- MarketEvent
  - type: Trade | Book | Kline | Ticker | MarkPrice | FundingRate | ...
  - venue: Binance | OKX | ...
  - market: Spot | LinearPerp(USDT) | InversePerp(Coin) | ...
  - symbol: normalized SymbolId
  - ts_exchange / ts_recv / ts_pub
  - payload: data structure per event type

### 3.1.4 Implementation Details (Recommended)

- Market adapter split: REST for snapshots/rules and WS for incremental updates, with clear retry/backoff policies
- Sequence validation: maintain exchange sequence numbers and trigger resnapshot on gaps
- Data normalization: enforce symbol mapping, precision, and contract multiplier before publish
- OrderBook design: price-level map + TopN cache, with immutable snapshots for readers
- Metrics: per-venue latency (recv→publish), drop rates, reconnect counters
