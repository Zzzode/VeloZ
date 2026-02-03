# VeloZ Crypto Quant Trading Framework: Data Model and Contracts

## 5. Data Model and Contract Summary

### 5.1 Core Entities

- Symbol: unified instrument (market type, contract multiplier, precision, min order size)
- Order: internal order (clientOrderId, venueOrderId, state machine, timestamps)
- Fill: trade record (price, qty, fees, liquidity role)
- Position: position (side, size, avg price, unrealized PnL, margin)
- Account: balances and margin info
- StrategyRun: run instance (param version, initiator, environment, backtest window)

### 5.2 Event Timeline (Replayable)

- MarketEvent: market events
- TradingEvent: order receipts/fills/balance updates
- SystemEvent: disconnects, reconnects, rate limiting, risk triggers, exceptions

System must ensure:

- Events are persistable (at least trades and key market summaries)
- Events are replayable (backtest reproduction and incident analysis)
