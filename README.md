# VeloZ

VeloZ is a quantitative trading framework for crypto markets (C++23 core engine + multi-language ecosystem). It aims to provide a unified backtest/simulation/live trading runtime model and an evolvable engineering structure for low-latency critical paths.

## Status

The repository is in an early stage and already provides a buildable C++23 engine skeleton, base module split (core/market/exec/oms/risk), and standardized build/CI configuration.

## Quick Start

### Build (Recommended: CMake Presets)

```bash
cmake --preset dev
cmake --build --preset dev -j
```

### Run Engine (Smoke Test)

```bash
./scripts/run_engine.sh dev
```

### Run Gateway + UI (Minimal Integration)

```bash
./scripts/run_gateway.sh dev
```

Default listen address: `http://127.0.0.1:8080/`.
If you open `apps/ui/index.html` directly via file://, set API Base at the top of the page to `http://127.0.0.1:8080`.

Optional: switch gateway market source (Binance REST polling; offline environments fall back to simulated market data):

```bash
VELOZ_MARKET_SOURCE=binance_rest ./scripts/run_gateway.sh dev
```

Optional: switch execution channel (Binance Spot Testnet REST; requires API key/secret):

```bash
VELOZ_EXECUTION_MODE=binance_testnet_spot ./scripts/run_gateway.sh dev
```

### Format C++ Code

```bash
./scripts/format.sh
```

See [Build and Run](docs/build_and_run.md) for more details.

## Repository Structure (Summary)

- apps/engine: C++23 engine executable (currently a skeleton)
- libs/core: infrastructure (logging, time, event loop)
- libs/market: market event model (placeholder)
- libs/exec: trading interface model (placeholder)
- libs/oms: order record and state aggregation (minimal implementation)
- libs/risk: risk checks (minimal implementation)
- docs: design and usage documents

## Design Documents

- [crypto_quant_framework_design.md](docs/crypto_quant_framework_design.md)

## License

Apache License 2.0. See [LICENSE](LICENSE).
