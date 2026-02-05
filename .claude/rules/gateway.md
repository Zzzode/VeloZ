---
paths:
  - "apps/gateway/**"
  - "scripts/run_gateway.sh"
---

# Python gateway (module notes)

- `apps/gateway/gateway.py` is the integration layer:
  - spawns the C++ engine with `--stdio`
  - bridges **engine stdio (text in / NDJSON out)** to **HTTP JSON endpoints + SSE**
- The gateway assigns a monotonic `_id` to events for the SSE stream; keep this behavior stable if refactoring buffering/replay.
- Environment variables used by the gateway (see `docs/build_and_run.md`):
  - Market: `VELOZ_MARKET_SOURCE`, `VELOZ_MARKET_SYMBOL`, `VELOZ_BINANCE_BASE_URL`
  - Execution: `VELOZ_EXECUTION_MODE`, `VELOZ_BINANCE_TRADE_BASE_URL`, `VELOZ_BINANCE_API_KEY`, `VELOZ_BINANCE_API_SECRET`, `VELOZ_BINANCE_WS_BASE_URL`
  - Avoid printing/echoing secrets.
- Tests under `apps/gateway/test_*.py` use the stdlib **`unittest`** runner:

  ```bash
  python3 -m unittest discover -s apps/gateway -p 'test_*.py'
  ```

  Note: current tests load `gateway.py` via `runpy.run_path(...)` with a hardcoded absolute path; adjust if tests fail in a different checkout location.
