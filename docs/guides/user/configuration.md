# Configuration Guide

This document describes all configurable options for VeloZ.

## Environment Variables

VeloZ behavior can be configured via environment variables before starting the gateway.

### Gateway Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host address |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset to use: `dev`, `release`, or `asan` |

### Market Data Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_MARKET_SOURCE` | `sim` | Market data source: `sim` (simulated) or `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol to track |

### Binance Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_BINANCE_BASE_URL` | `https://api.binance.com` | Binance REST API base URL (for market data) |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://testnet.binance.vision/ws` | Binance WebSocket base URL (for user data stream) |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://testnet.binance.vision` | Binance trade API base URL |
| `VELOZ_BINANCE_API_KEY` | (empty) | Binance API key (required for trading) |
| `VELOZ_BINANCE_API_SECRET` | (empty) | Binance API secret (required for trading) |

### Execution Mode Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode: `sim_engine` (simulated) or `binance_testnet_spot` (Binance testnet) |

### Authentication Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_AUTH_ENABLED` | `false` | Enable authentication: `true` or `false` |
| `VELOZ_JWT_SECRET` | (auto-generated) | JWT signing secret (keep secure) |
| `VELOZ_TOKEN_EXPIRY` | `3600` | Access token expiry in seconds (1 hour) |
| `VELOZ_ADMIN_PASSWORD` | (empty) | Admin user password for login |
| `VELOZ_RATE_LIMIT_CAPACITY` | `100` | Rate limit bucket capacity |
| `VELOZ_RATE_LIMIT_REFILL` | `10.0` | Rate limit refill rate (tokens/second) |

### Audit Logging Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_AUDIT_LOG_ENABLED` | `true` | Enable audit logging (when auth is enabled) |
| `VELOZ_AUDIT_LOG_FILE` | (stderr) | Audit log file path |
| `VELOZ_AUDIT_LOG_RETENTION_DAYS` | `90` | Default audit log retention period in days |

**Retention policies by log type:**

| Log Type | Retention | Description |
|----------|-----------|-------------|
| `auth` | 90 days | Login, logout, token refresh events |
| `order` | 365 days | Order placement and execution events |
| `api_key` | 365 days | API key creation and revocation |
| `error` | 30 days | Error events |
| `access` | 14 days | General API access logs |

**Audit log storage:**
- Logs are stored in NDJSON format (newline-delimited JSON)
- Default location: `/var/log/veloz/audit/`
- Archives stored in: `/var/log/veloz/audit/archive/`
- Old logs are compressed with gzip before deletion

### Reconciliation Configuration

| Variable | Default | Description |
|-----------|---------|-------------|
| `VELOZ_RECONCILIATION_INTERVAL` | `30` | Reconciliation interval in seconds |
| `VELOZ_AUTO_CANCEL_ORPHANED` | `false` | Automatically cancel orphaned orders |
| `VELOZ_RECONCILIATION_ENABLED` | `true` | Enable automatic reconciliation |

## Configuration Examples

### Local Development (Default)

No environment variables needed:

```bash
./scripts/run_gateway.sh dev
```

This uses:
- Simulated market data
- Simulated execution engine
- Default port 8080

### Binance Market Data Only

Use real Binance market data with simulated execution:

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com
./scripts/run_gateway.sh dev
```

### Binance Testnet Trading

Trade on Binance testnet with real execution:

```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
./scripts/run_gateway.sh dev
```

### Custom Port

Run gateway on a custom port:

```bash
export VELOZ_PORT=9000
./scripts/run_gateway.sh dev
```

### Production Build

Use release build instead of debug:

```bash
export VELOZ_PRESET=release
./scripts/run_gateway.sh
```

### Authentication Enabled

Enable authentication with JWT tokens and API keys:

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
export VELOZ_JWT_SECRET=your_secret_key_min_32_chars
export VELOZ_TOKEN_EXPIRY=3600
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log
./scripts/run_gateway.sh dev
```

### Reconciliation Enabled

Enable automatic order reconciliation:

```bash
export VELOZ_RECONCILIATION_ENABLED=true
export VELOZ_RECONCILIATION_INTERVAL=30
export VELOZ_AUTO_CANCEL_ORPHANED=true
./scripts/run_gateway.sh dev
```

### Combined Configuration

All options together:

```bash
export VELOZ_PORT=8443
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=ETHUSDT
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_key
export VELOZ_BINANCE_API_SECRET=your_secret
export VELOZ_PRESET=release
./scripts/run_gateway.sh
```

## Binance Testnet Setup

To get Binance testnet API credentials:

1. **Sign up**: https://testnet.binance.vision/
2. **Generate API Key**: Go to API Management → Create API
3. **Save Keys**: Copy API Key and Secret Key securely

**Important**: Never share your API Secret Key publicly.

## Configuration Validation

The gateway validates configuration on startup:

- Engine executable must exist in build directory
- Binance API keys are required when using `binance_testnet_spot` mode
- Invalid execution modes default to `sim_engine`

## Troubleshooting

### Port Already in Use

If you see "Address already in use" error:

```bash
# macOS/Linux
lsof -i :8080

# Kill the process or use different port
export VELOZ_PORT=8081
```

### Engine Not Found

If you see "engine not found" error:

```bash
# Ensure you've built the project
cmake --preset dev
cmake --build --preset dev -j

# Verify engine exists
ls build/dev/apps/engine/veloz_engine
```

### Binance Connection Errors

If you see market source errors:

- Verify `VELOZ_BINANCE_BASE_URL` is correct
- Check network connectivity to Binance
- Ensure testnet URL is correct for trading mode

## Related

- [Getting Started](getting-started.md) - Quick start guide
- [HTTP API Reference](../../api/http-api.md) - API endpoints
- [Build and Run](../build_and_run.md) - Build instructions
