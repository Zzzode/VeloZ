---
name: market-data
description: Guides market module design, order book rules, and WebSocket patterns. Invoke when modifying libs/market or market data behavior.
---

# Market Data

## Purpose

Use this skill when working in libs/market or on market data behavior.

## Order Book Pattern
- Sequence-based reconstruction with gap detection.
- Track best bid/ask with kj::Maybe<T>.
- Spread = bestAsk.price - bestBid.price when both exist.

## WebSocket (RFC 6455)
- Custom implementation using KJ async I/O.
- TLS via OpenSSL.
- Exponential backoff reconnection with jitter.

## Subscription Management
- Hierarchical subscriptions with state transitions.
- Observer callbacks for subscription changes.

## Kline Aggregation
- Time-windowed aggregation from ticks (1m/5m/15m/1h).
- Drop stale ticks based on sequence.

## Market Quality Monitoring
- Detect anomalies: price spike, volume surge, spread widening, order imbalance.
- Report with severity and timestamps.

## Thread Safety
- Prefer kj::MutexGuarded<T> and kj::Atomic for counters.
- Avoid manual lock/unlock patterns.
