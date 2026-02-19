---
name: gateway
description: Guides the Python gateway’s engine-stdio bridge and HTTP/SSE API rules. Invoke when modifying apps/gateway or API behavior.
---

# Gateway (Python)

## Purpose

Use this skill for changes in apps/gateway or API behavior.

## Responsibilities
- Spawn the C++ engine with --stdio.
- Bridge engine stdin (text commands) and stdout (NDJSON) to HTTP JSON + SSE.
- Assign monotonic _id to SSE events and keep this stable.

## Environment Variables
- Market: VELOZ_MARKET_SOURCE, VELOZ_MARKET_SYMBOL, VELOZ_BINANCE_BASE_URL
- Execution: VELOZ_EXECUTION_MODE, VELOZ_BINANCE_TRADE_BASE_URL, VELOZ_BINANCE_API_KEY, VELOZ_BINANCE_API_SECRET, VELOZ_BINANCE_WS_BASE_URL
- Never log or echo secrets.

## Testing
- Tests use Python unittest:
  python3 -m unittest discover -s apps/gateway -p 'test_*.py'
