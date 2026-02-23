# Configuration Guide

This document describes all configurable options for VeloZ v1.0.

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
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode (see table below) |

**Supported Execution Modes:**

| Mode | Description |
|------|-------------|
| `sim_engine` | Simulated execution (no real orders) |
| `binance_testnet_spot` | Binance Spot testnet |
| `binance_spot` | Binance Spot production |
| `okx_testnet` | OKX testnet |
| `okx_spot` | OKX production |
| `bybit_testnet` | Bybit testnet |
| `bybit_spot` | Bybit production |
| `coinbase_sandbox` | Coinbase sandbox |
| `coinbase_spot` | Coinbase production |

---

### OKX Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_OKX_API_KEY` | (empty) | OKX API key |
| `VELOZ_OKX_API_SECRET` | (empty) | OKX API secret |
| `VELOZ_OKX_PASSPHRASE` | (empty) | OKX API passphrase (required) |
| `VELOZ_OKX_BASE_URL` | `https://www.okx.com` | OKX REST API base URL |
| `VELOZ_OKX_WS_URL` | `wss://ws.okx.com:8443/ws/v5/public` | OKX WebSocket URL |
| `VELOZ_OKX_TRADE_URL` | `https://www.okx.com` | OKX trade API URL |

**OKX Testnet URLs:**
```bash
export VELOZ_OKX_BASE_URL=https://www.okx.com
export VELOZ_OKX_TRADE_URL=https://www.okx.com
export VELOZ_OKX_WS_URL=wss://wspap.okx.com:8443/ws/v5/public?brokerId=9999
```

**Note:** OKX requires a passphrase in addition to API key and secret.

---

### Bybit Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_BYBIT_API_KEY` | (empty) | Bybit API key |
| `VELOZ_BYBIT_API_SECRET` | (empty) | Bybit API secret |
| `VELOZ_BYBIT_BASE_URL` | `https://api.bybit.com` | Bybit REST API base URL |
| `VELOZ_BYBIT_WS_URL` | `wss://stream.bybit.com/v5/public/spot` | Bybit WebSocket URL |
| `VELOZ_BYBIT_TRADE_URL` | `https://api.bybit.com` | Bybit trade API URL |

**Bybit Testnet URLs:**
```bash
export VELOZ_BYBIT_BASE_URL=https://api-testnet.bybit.com
export VELOZ_BYBIT_TRADE_URL=https://api-testnet.bybit.com
export VELOZ_BYBIT_WS_URL=wss://stream-testnet.bybit.com/v5/public/spot
```

---

### Coinbase Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_COINBASE_API_KEY` | (empty) | Coinbase API key |
| `VELOZ_COINBASE_API_SECRET` | (empty) | Coinbase API secret |
| `VELOZ_COINBASE_BASE_URL` | `https://api.coinbase.com` | Coinbase REST API base URL |
| `VELOZ_COINBASE_WS_URL` | `wss://ws-feed.exchange.coinbase.com` | Coinbase WebSocket URL |
| `VELOZ_COINBASE_TRADE_URL` | `https://api.exchange.coinbase.com` | Coinbase trade API URL |

**Coinbase Sandbox URLs:**
```bash
export VELOZ_COINBASE_BASE_URL=https://api-public.sandbox.exchange.coinbase.com
export VELOZ_COINBASE_TRADE_URL=https://api-public.sandbox.exchange.coinbase.com
export VELOZ_COINBASE_WS_URL=wss://ws-feed-public.sandbox.exchange.coinbase.com
```

**Note:** Coinbase uses JWT-based authentication for some endpoints.

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

---

### WAL (Write-Ahead Log) Configuration

VeloZ uses a Write-Ahead Log for durability and crash recovery.

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WAL_ENABLED` | `true` | Enable WAL for durability |
| `VELOZ_WAL_DIR` | `/var/lib/veloz/wal` | WAL directory path |
| `VELOZ_WAL_SYNC_MODE` | `fsync` | Sync mode: `fsync` (durable) or `async` (fast) |
| `VELOZ_WAL_MAX_SIZE_MB` | `100` | Maximum WAL file size before rotation |
| `VELOZ_WAL_MAX_FILES` | `10` | Maximum number of WAL files to retain |
| `VELOZ_WAL_CHECKPOINT_INTERVAL` | `60` | Checkpoint interval in seconds |

**Sync Modes:**

| Mode | Durability | Performance | Use Case |
|------|------------|-------------|----------|
| `fsync` | High | Lower | Production (recommended) |
| `async` | Lower | Higher | Development, backtesting |

**Example:**
```bash
export VELOZ_WAL_ENABLED=true
export VELOZ_WAL_DIR=/data/veloz/wal
export VELOZ_WAL_SYNC_MODE=fsync
export VELOZ_WAL_MAX_SIZE_MB=256
```

---

### Performance Configuration

Tune VeloZ for your workload and hardware.

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WORKER_THREADS` | `4` | Number of worker threads |
| `VELOZ_EVENT_LOOP_THREADS` | `2` | Number of event loop threads |
| `VELOZ_CONNECTION_POOL_SIZE` | `10` | HTTP connection pool size |
| `VELOZ_REQUEST_TIMEOUT_MS` | `30000` | Request timeout in milliseconds |
| `VELOZ_MEMORY_POOL_SIZE` | `536870912` | Memory pool size in bytes (512MB) |
| `VELOZ_ORDER_RATE_LIMIT` | `10` | Max orders per second |
| `VELOZ_LOG_LEVEL` | `INFO` | Log level: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR` |

**Performance Tuning Guidelines:**

| Workload | Worker Threads | Event Loop Threads | Memory Pool |
|----------|----------------|-------------------|-------------|
| Light (< 10 orders/s) | 2 | 1 | 256MB |
| Medium (10-100 orders/s) | 4 | 2 | 512MB |
| Heavy (> 100 orders/s) | 8 | 4 | 1GB |

**Example (High Performance):**
```bash
export VELOZ_WORKER_THREADS=8
export VELOZ_EVENT_LOOP_THREADS=4
export VELOZ_MEMORY_POOL_SIZE=1073741824
export VELOZ_ORDER_RATE_LIMIT=100
```

---

### Secrets Management

VeloZ supports multiple methods for managing sensitive credentials.

#### Environment Variables (Default)

Set secrets directly as environment variables:

```bash
export VELOZ_BINANCE_API_KEY=your_api_key
export VELOZ_BINANCE_API_SECRET=your_api_secret
```

**Security Note:** Avoid storing secrets in shell history. Use:
```bash
read -s VELOZ_BINANCE_API_SECRET && export VELOZ_BINANCE_API_SECRET
```

#### HashiCorp Vault Integration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_VAULT_ENABLED` | `false` | Enable Vault integration |
| `VELOZ_VAULT_ADDR` | `http://127.0.0.1:8200` | Vault server address |
| `VELOZ_VAULT_TOKEN` | (empty) | Vault authentication token |
| `VELOZ_VAULT_PATH` | `secret/data/veloz` | Vault secrets path |
| `VELOZ_VAULT_ROLE` | `veloz` | Vault AppRole role name |

**Vault Setup:**
```bash
# Enable Vault
export VELOZ_VAULT_ENABLED=true
export VELOZ_VAULT_ADDR=https://vault.example.com:8200
export VELOZ_VAULT_TOKEN=s.xxxxxxxxxxxxxxxx

# Or use AppRole authentication
export VELOZ_VAULT_ROLE=veloz
export VELOZ_VAULT_SECRET_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

**Store secrets in Vault:**
```bash
vault kv put secret/veloz \
  binance_api_key=your_key \
  binance_api_secret=your_secret \
  okx_api_key=your_key \
  okx_api_secret=your_secret \
  okx_passphrase=your_passphrase
```

#### Kubernetes Secrets Integration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_K8S_SECRETS_ENABLED` | `false` | Enable K8s Secrets integration |
| `VELOZ_K8S_SECRET_NAME` | `veloz-secrets` | Kubernetes Secret name |
| `VELOZ_K8S_NAMESPACE` | `default` | Kubernetes namespace |

**Kubernetes Secret Example:**
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: veloz-secrets
  namespace: veloz
type: Opaque
stringData:
  VELOZ_BINANCE_API_KEY: "your_api_key"
  VELOZ_BINANCE_API_SECRET: "your_api_secret"
  VELOZ_OKX_API_KEY: "your_okx_key"
  VELOZ_OKX_API_SECRET: "your_okx_secret"
  VELOZ_OKX_PASSPHRASE: "your_passphrase"
```

**Mount in Deployment:**
```yaml
spec:
  containers:
    - name: veloz
      envFrom:
        - secretRef:
            name: veloz-secrets
```

#### Secrets Priority Order

VeloZ loads secrets in this priority order (highest first):
1. Environment variables
2. HashiCorp Vault
3. Kubernetes Secrets
4. Configuration file

---

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

### OKX Testnet Trading

Trade on OKX testnet:

```bash
export VELOZ_EXECUTION_MODE=okx_testnet
export VELOZ_OKX_API_KEY=your_okx_api_key
export VELOZ_OKX_API_SECRET=your_okx_api_secret
export VELOZ_OKX_PASSPHRASE=your_passphrase
export VELOZ_OKX_TRADE_URL=https://www.okx.com
./scripts/run_gateway.sh dev
```

### Bybit Testnet Trading

Trade on Bybit testnet:

```bash
export VELOZ_EXECUTION_MODE=bybit_testnet
export VELOZ_BYBIT_API_KEY=your_bybit_api_key
export VELOZ_BYBIT_API_SECRET=your_bybit_api_secret
export VELOZ_BYBIT_BASE_URL=https://api-testnet.bybit.com
export VELOZ_BYBIT_TRADE_URL=https://api-testnet.bybit.com
./scripts/run_gateway.sh dev
```

### Coinbase Sandbox Trading

Trade on Coinbase sandbox:

```bash
export VELOZ_EXECUTION_MODE=coinbase_sandbox
export VELOZ_COINBASE_API_KEY=your_coinbase_api_key
export VELOZ_COINBASE_API_SECRET=your_coinbase_api_secret
export VELOZ_COINBASE_TRADE_URL=https://api-public.sandbox.exchange.coinbase.com
./scripts/run_gateway.sh dev
```

### Production with Vault Secrets

Production setup with HashiCorp Vault:

```bash
export VELOZ_PRESET=release
export VELOZ_EXECUTION_MODE=binance_spot
export VELOZ_VAULT_ENABLED=true
export VELOZ_VAULT_ADDR=https://vault.example.com:8200
export VELOZ_VAULT_TOKEN=s.xxxxxxxxxxxxxxxx
export VELOZ_WAL_ENABLED=true
export VELOZ_WAL_SYNC_MODE=fsync
./scripts/run_gateway.sh
```

### High-Performance Configuration

Optimized for high-frequency trading:

```bash
export VELOZ_WORKER_THREADS=8
export VELOZ_EVENT_LOOP_THREADS=4
export VELOZ_MEMORY_POOL_SIZE=1073741824
export VELOZ_ORDER_RATE_LIMIT=100
export VELOZ_LOG_LEVEL=WARN
export VELOZ_WAL_SYNC_MODE=async
./scripts/run_gateway.sh release
```

## Exchange Testnet Setup

### Binance Testnet

1. **Sign up**: https://testnet.binance.vision/
2. **Generate API Key**: Go to API Management -> Create API
3. **Save Keys**: Copy API Key and Secret Key securely

### OKX Testnet

1. **Sign up**: https://www.okx.com/ (use demo trading)
2. **Enable Demo Trading**: Account -> Demo Trading
3. **Generate API Key**: API -> Create API Key
4. **Save Keys**: Copy API Key, Secret Key, and Passphrase

### Bybit Testnet

1. **Sign up**: https://testnet.bybit.com/
2. **Generate API Key**: Account -> API Management -> Create New Key
3. **Save Keys**: Copy API Key and Secret Key

### Coinbase Sandbox

1. **Sign up**: https://public.sandbox.exchange.coinbase.com/
2. **Generate API Key**: Settings -> API -> Create API Key
3. **Save Keys**: Copy API Key and Secret Key

**Important**: Never share your API Secret Keys publicly. Never commit credentials to version control.

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
- [HTTP API Reference](../../api/http_api.md) - API endpoints
- [Monitoring Guide](monitoring.md) - Metrics and observability
- [Binance Integration](binance.md) - Binance-specific configuration
- [Build and Run](../build_and_run.md) - Build instructions
