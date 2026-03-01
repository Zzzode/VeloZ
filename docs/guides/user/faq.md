# Frequently Asked Questions (FAQ)

This document answers common questions about VeloZ installation, configuration, trading, troubleshooting, and performance.

## Table of Contents

1. [Installation Questions](#installation-questions)
2. [Configuration Questions](#configuration-questions)
3. [Trading Questions](#trading-questions)
4. [Troubleshooting Questions](#troubleshooting-questions)
5. [Performance Questions](#performance-questions)

---

## Installation Questions

### Q: What are the minimum system requirements for VeloZ?

**A:** For development:
- OS: Linux (Ubuntu 20.04+), macOS 12+, or Windows 10+
- CPU: x86_64 or ARM64
- RAM: 4 GB minimum, 8 GB recommended
- Disk: 2 GB for build, 500 MB for runtime

For production, see [Installation Guide - System Requirements](installation.md#system-requirements).

---

### Q: Which compilers are supported?

**A:** VeloZ requires a C++23 capable compiler:
- Clang 16 or newer (recommended)
- GCC 13 or newer

Check your compiler version:
```bash
clang++ --version
# or
g++ --version
```

See [Installation Guide](installation.md#software-dependencies) for details.

---

### Q: How do I install VeloZ on Ubuntu?

**A:** Run these commands:
```bash
# Install dependencies
sudo apt update
sudo apt install -y cmake ninja-build build-essential clang-16 libssl-dev python3 git

# Clone and build
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

See [Installation Guide - Ubuntu](installation.md#ubuntudebian) for complete instructions.

---

### Q: How do I install VeloZ on macOS?

**A:** Run these commands:
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install cmake ninja openssl@3

# Clone and build
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ
cmake --preset dev
cmake --build --preset dev-all -j$(sysctl -n hw.ncpu)
```

See [Installation Guide - macOS](installation.md#macos) for complete instructions.

---

### Q: What is the difference between dev, release, and asan build presets?

**A:**
| Preset | Purpose | Use Case |
|--------|---------|----------|
| `dev` | Debug build with symbols | Development and debugging |
| `release` | Optimized build | Production deployment |
| `asan` | AddressSanitizer enabled | Memory error detection |

Use `release` for production to get optimal performance.

See [Installation Guide - Build Presets](installation.md#build-presets) for details.

---

### Q: How do I verify my installation is working?

**A:** Run the verification checklist:
```bash
# Check engine starts
./build/dev/apps/engine/veloz_engine --help

# Run all tests
ctest --preset dev -j$(nproc)

# Start gateway
./scripts/run_gateway.sh dev

# Test health endpoint
curl http://127.0.0.1:8080/health
```

All 2000+ tests should pass. See [Installation Guide - Verification](installation.md#verification).

---

### Q: Can I use Docker instead of building from source?

**A:** Yes. VeloZ provides Docker Compose configurations:
```bash
# Start gateway only
docker compose up -d gateway

# Start with monitoring stack
docker compose --profile monitoring up -d

# Start everything
docker compose --profile full --profile monitoring --profile secrets up -d
```

See [Installation Guide - Docker Installation](installation.md#docker-installation) for details.

---

### Q: What is the KJ library and why does VeloZ use it?

**A:** KJ is a C++ library from Cap'n Proto that provides:
- `kj::Own<T>` - Owned pointers (replaces `std::unique_ptr`)
- `kj::Maybe<T>` - Nullable values (replaces `std::optional`)
- `kj::String` / `kj::StringPtr` - String handling
- `kj::Promise<T>` - Async operations

VeloZ uses KJ as its default C++ foundation for memory safety and async I/O. It is automatically fetched during build.

See [Installation Guide - KJ Library Dependencies](installation.md#kj-library-dependencies).

---

### Q: How do I fix "CMake version too old" errors?

**A:** VeloZ requires CMake 3.24 or newer. Update CMake:
```bash
# Ubuntu: Install via pip
sudo apt remove cmake
pip3 install cmake

# macOS: Update via Homebrew
brew upgrade cmake

# Verify version
cmake --version
```

See [Installation Guide - Troubleshooting](installation.md#troubleshooting).

---

### Q: How do I fix "Compiler doesn't support C++23" errors?

**A:** Install a newer compiler:
```bash
# Ubuntu: Install Clang 16
sudo apt install clang-16
export CC=clang-16
export CXX=clang++-16
cmake --preset dev
```

See [Installation Guide - Troubleshooting](installation.md#compiler-doesnt-support-c23).

---

## Configuration Questions

### Q: What environment variables are required to run VeloZ?

**A:** No environment variables are required for basic operation. VeloZ uses sensible defaults:
- `VELOZ_PORT`: 8080
- `VELOZ_MARKET_SOURCE`: sim (simulated)
- `VELOZ_EXECUTION_MODE`: sim_engine

For exchange trading, you need API credentials. See [Configuration Guide](configuration.md).

---

### Q: How do I configure VeloZ for Binance testnet trading?

**A:** Set these environment variables:
```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
./scripts/run_gateway.sh dev
```

Get testnet API keys from https://testnet.binance.vision/

See [Configuration Guide - Binance Testnet Trading](configuration.md#binance-testnet-trading).

---

### Q: How do I configure multiple exchanges?

**A:** VeloZ supports Binance, OKX, Bybit, and Coinbase. Configure each exchange:
```bash
# Binance
export VELOZ_BINANCE_API_KEY=<your-api-key>
export VELOZ_BINANCE_API_SECRET=<your-api-secret>

# OKX (requires passphrase)
export VELOZ_OKX_API_KEY=<your-api-key>
export VELOZ_OKX_API_SECRET=<your-api-secret>
export VELOZ_OKX_PASSPHRASE=<your-passphrase>

# Bybit
export VELOZ_BYBIT_API_KEY=<your-api-key>
export VELOZ_BYBIT_API_SECRET=<your-api-secret>
```

See [Configuration Guide - Exchange Configuration](configuration.md#okx-configuration).

---

### Q: How do I enable authentication?

**A:** Set these environment variables:
```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
export VELOZ_JWT_SECRET=$(openssl rand -base64 64)
./scripts/run_gateway.sh dev
```

Then login to get a token:
```bash
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_secure_password"}'
```

See [Configuration Guide - Authentication Configuration](configuration.md#authentication-configuration).

---

### Q: How do I store API credentials securely?

**A:** VeloZ supports multiple secret management methods:

1. **Environment variables** (default):
   ```bash
   read -s VELOZ_BINANCE_API_SECRET && export VELOZ_BINANCE_API_SECRET
   ```

2. **HashiCorp Vault**:
   ```bash
   export VELOZ_VAULT_ENABLED=true
   export VELOZ_VAULT_ADDR=https://vault.internal:8200
   export VELOZ_VAULT_TOKEN=s.xxxxxxxxxxxxxxxx
   ```

3. **Kubernetes Secrets**:
   ```bash
   export VELOZ_K8S_SECRETS_ENABLED=true
   export VELOZ_K8S_SECRET_NAME=veloz-secrets
   ```

See [Configuration Guide - Secrets Management](configuration.md#secrets-management).

---

### Q: How do I change the gateway port?

**A:** Set the `VELOZ_PORT` environment variable:
```bash
export VELOZ_PORT=9000
./scripts/run_gateway.sh dev
```

See [Configuration Guide - Gateway Configuration](configuration.md#gateway-configuration).

---

### Q: What execution modes are available?

**A:** VeloZ supports these execution modes:

| Mode | Description |
|------|-------------|
| `sim_engine` | Simulated execution (default) |
| `binance_testnet_spot` | Binance Spot testnet |
| `binance_spot` | Binance Spot production |
| `okx_testnet` | OKX testnet |
| `okx_spot` | OKX production |
| `bybit_testnet` | Bybit testnet |
| `bybit_spot` | Bybit production |
| `coinbase_sandbox` | Coinbase sandbox |
| `coinbase_spot` | Coinbase production |

See [Configuration Guide - Execution Mode Configuration](configuration.md#execution-mode-configuration).

---

### Q: How do I enable audit logging?

**A:** Set these environment variables:
```bash
export VELOZ_AUTH_ENABLED=true  # Required for audit logging
export VELOZ_AUDIT_LOG_ENABLED=true
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log
./scripts/run_gateway.sh dev
```

See [Configuration Guide - Audit Logging Configuration](configuration.md#audit-logging-configuration).

---

### Q: How do I configure the Write-Ahead Log (WAL)?

**A:** Configure WAL for durability:
```bash
export VELOZ_WAL_ENABLED=true
export VELOZ_WAL_DIR=/var/lib/veloz/wal
export VELOZ_WAL_SYNC_MODE=fsync  # Use 'async' for development
export VELOZ_WAL_MAX_SIZE_MB=100
```

See [Configuration Guide - WAL Configuration](configuration.md#wal-write-ahead-log-configuration).

---

### Q: How do I tune VeloZ for high-performance trading?

**A:** Configure performance settings:
```bash
export VELOZ_WORKER_THREADS=8
export VELOZ_EVENT_LOOP_THREADS=4
export VELOZ_MEMORY_POOL_SIZE=1073741824  # 1GB
export VELOZ_ORDER_RATE_LIMIT=100
export VELOZ_LOG_LEVEL=WARN
```

See [Configuration Guide - Performance Configuration](configuration.md#performance-configuration).

---

## Trading Questions

### Q: How do I place an order?

**A:** Use the order API endpoint:
```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
```

See [Trading Guide - Place an Order](trading-guide.md#place-an-order).

---

### Q: How do I cancel an order?

**A:** Use the cancel API endpoint:
```bash
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{
    "client_order_id": "my-order-001",
    "symbol": "BTCUSDT"
  }'
```

See [Trading Guide - Cancel an Order](trading-guide.md#cancel-an-order).

---

### Q: What order types are supported?

**A:** VeloZ supports limit orders with execution algorithms:
- **Limit orders**: Execute at a specific price or better
- **TWAP**: Time-weighted average price execution
- **VWAP**: Volume-weighted average price execution
- **Iceberg**: Hide large order size
- **POV**: Percentage of volume execution

See [Trading Guide - Order Types](trading-guide.md#order-types-and-parameters).

---

### Q: How do I check my order status?

**A:** Query the order state endpoint:
```bash
# Single order
curl "http://127.0.0.1:8080/api/order_state?client_order_id=my-order-001"

# All orders
curl http://127.0.0.1:8080/api/orders_state
```

See [Trading Guide - Query Order Status](trading-guide.md#query-order-status).

---

### Q: How do I view my positions?

**A:** Query the positions endpoint:
```bash
# All positions
curl http://127.0.0.1:8080/api/positions

# Specific symbol
curl http://127.0.0.1:8080/api/positions/BTCUSDT
```

See [Trading Guide - View All Positions](trading-guide.md#view-all-positions).

---

### Q: What is a client_order_id and why should I use it?

**A:** A `client_order_id` is your unique identifier for an order. Benefits:
- **Idempotency**: Same ID prevents duplicate orders
- **Tracking**: Reference orders in your system
- **Reconciliation**: Match fills to your orders

Example:
```bash
CLIENT_ORDER_ID="order-$(date +%s)-$(openssl rand -hex 4)"
```

See [Trading Guide - Client Order ID](trading-guide.md#client-order-id).

---

### Q: How do I close a position?

**A:** Place an opposite order for the position size:
```bash
# Close LONG position (sell)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "SELL",
    "symbol": "BTCUSDT",
    "qty": 0.5,
    "price": 50000.0
  }'
```

See [Trading Guide - Close a Position](trading-guide.md#close-a-position).

---

### Q: How does smart order routing work?

**A:** VeloZ automatically routes orders to the best exchange based on:
- Price (35% weight)
- Fees (20% weight)
- Latency (15% weight)
- Liquidity (20% weight)
- Reliability (10% weight)

See [Trading Guide - Smart Order Routing](trading-guide.md#smart-order-routing).

---

### Q: How do I monitor orders in real-time?

**A:** Subscribe to the SSE stream:
```bash
curl -N http://127.0.0.1:8080/api/stream | grep order_update
```

Events include: `market`, `order_update`, `fill`, `account`, `error`.

See [Monitoring Guide - SSE Event Stream](monitoring.md#sse-event-stream).

---

### Q: How do I enable order reconciliation?

**A:** Set these environment variables:
```bash
export VELOZ_RECONCILIATION_ENABLED=true
export VELOZ_RECONCILIATION_INTERVAL=30
export VELOZ_AUTO_CANCEL_ORPHANED=true
./scripts/run_gateway.sh dev
```

See [Configuration Guide - Reconciliation Configuration](configuration.md#reconciliation-configuration).

---

## Troubleshooting Questions

### Q: The gateway fails to start with "Address already in use"

**A:** Another process is using port 8080. Find and kill it:
```bash
# Find process
lsof -i :8080

# Kill process
kill <PID>

# Or use a different port
export VELOZ_PORT=8081
./scripts/run_gateway.sh dev
```

See [Troubleshooting Guide - Port already in use](troubleshooting.md#port-already-in-use).

---

### Q: The gateway fails with "Engine binary not found"

**A:** The engine needs to be built first:
```bash
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)

# Verify engine exists
ls build/dev/apps/engine/veloz_engine
```

See [Troubleshooting Guide - Engine binary not found](troubleshooting.md#engine-binary-not-found).

---

### Q: My orders are being rejected with "insufficient_balance"

**A:** Check your account balance:
```bash
curl http://127.0.0.1:8080/api/account
```

Solutions:
- Deposit funds to your exchange account
- Reduce order size
- Check if funds are locked in open orders

See [Troubleshooting Guide - Insufficient balance](troubleshooting.md#insufficient-balance).

---

### Q: I'm getting "binance_not_configured" errors

**A:** Verify your Binance configuration:
```bash
# Check if API key is set
echo $VELOZ_BINANCE_API_KEY

# Ensure correct URLs for testnet
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws
```

See [Installation Guide - Binance API errors](installation.md#binance-api-errors).

---

### Q: WebSocket connections keep disconnecting

**A:** Check network stability and configuration:
```bash
# Test WebSocket connectivity
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  https://testnet.binance.vision/ws

# Increase timeout settings
export VELOZ_WS_TIMEOUT=30000
export VELOZ_WS_RECONNECT_DELAY=5000
```

See [Troubleshooting Guide - WebSocket disconnections](troubleshooting.md#websocket-disconnections).

---

### Q: My authentication token expired

**A:** Refresh your token:
```bash
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"YOUR_REFRESH_TOKEN"}'
```

See [Troubleshooting Guide - Token expired](troubleshooting.md#token-expired).

---

### Q: The circuit breaker is blocking all my orders

**A:** Check circuit breaker status and reset if appropriate:
```bash
# Check status
curl http://127.0.0.1:8080/api/risk/circuit-breaker

# Reset (only after addressing underlying issue)
curl -X POST http://127.0.0.1:8080/api/risk/circuit-breaker/reset
```

See [Troubleshooting Guide - Circuit breaker blocking orders](troubleshooting.md#circuit-breaker-blocking-orders).

---

### Q: Market data is not updating

**A:** Check market data configuration:
```bash
# Check market data
curl http://127.0.0.1:8080/api/market

# Check configuration
curl http://127.0.0.1:8080/api/config

# Verify market source
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
```

See [Troubleshooting Guide - No market data](troubleshooting.md#no-market-data).

---

### Q: How do I enable debug logging?

**A:** Set the log level to debug:
```bash
export VELOZ_LOG_LEVEL=debug
./scripts/run_gateway.sh dev 2>&1 | tee debug.log
```

See [Troubleshooting Guide - Debug Mode](troubleshooting.md#debug-mode).

---

### Q: How do I run memory debugging with AddressSanitizer?

**A:** Build with the asan preset:
```bash
cmake --preset asan
cmake --build --preset asan-all -j$(nproc)
./build/asan/apps/engine/veloz_engine --stdio
```

See [Troubleshooting Guide - Memory debugging](troubleshooting.md#memory-debugging).

---

### Q: Where are the log files located?

**A:** Default log locations:

| Component | Default Location |
|-----------|------------------|
| Gateway | stdout / `gateway.log` |
| Engine | stdout / `engine.log` |
| Audit | `audit.log` |

Configure with `VELOZ_AUDIT_LOG_FILE` environment variable.

See [Troubleshooting Guide - Log locations](troubleshooting.md#log-locations).

---

### Q: What should I do in an emergency (trading affected)?

**A:** Follow emergency procedures:
1. Stop the gateway immediately: `pkill -f veloz`
2. Cancel all open orders on exchange directly
3. Document the issue
4. Do not restart until issue is understood

See [Troubleshooting Guide - Emergency procedures](troubleshooting.md#emergency-procedures).

---

## Performance Questions

### Q: How do I optimize VeloZ for low latency?

**A:** Follow these steps:
1. Use release build: `cmake --preset release`
2. Deploy close to exchange servers
3. Use dedicated network connections
4. Configure performance settings:
   ```bash
   export VELOZ_WORKER_THREADS=8
   export VELOZ_EVENT_LOOP_THREADS=4
   export VELOZ_LOG_LEVEL=WARN
   ```

See [Best Practices - Performance Best Practices](best-practices.md#performance-best-practices).

---

### Q: What is the typical order latency?

**A:** Typical latencies by exchange:

| Exchange | Typical Latency |
|----------|-----------------|
| Binance | 10-50ms |
| OKX | 20-80ms |
| Bybit | 30-100ms |
| Coinbase | 50-150ms |

Monitor latency with: `curl http://127.0.0.1:8080/api/stats | jq '.performance'`

See [Trading Guide - Latency Considerations](trading-guide.md#latency-considerations).

---

### Q: How do I reduce memory usage?

**A:** Implement these strategies:
- Clear old order history periodically
- Limit in-memory trade history
- Use disk-based storage for historical data
- Set memory limits for the process

See [Best Practices - Memory Management](best-practices.md#memory-management).

---

### Q: The build is taking too long. How can I speed it up?

**A:** Try these optimizations:
```bash
# Use Ninja instead of Make
sudo apt install ninja-build  # Ubuntu
brew install ninja            # macOS

# Build specific targets only
cmake --build --preset dev-engine -j$(nproc)

# Use ccache for faster rebuilds
sudo apt install ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

See [Installation Guide - Build takes too long](installation.md#build-takes-too-long).

---

### Q: I'm running out of memory during build

**A:** Reduce parallel jobs:
```bash
# Use only 2 cores
cmake --build --preset dev-all -j2

# Or build sequentially
cmake --build --preset dev-all -j1

# Check available memory
free -h  # Linux
vm_stat  # macOS
```

See [Installation Guide - Memory issues during build](installation.md#memory-issues-during-build).

---

### Q: How do I monitor system performance?

**A:** Use the metrics endpoint:
```bash
# Get Prometheus metrics
curl http://127.0.0.1:8080/metrics

# Key metrics:
# veloz_latency_ms - Order processing latency
# veloz_orders_total - Total orders processed
# veloz_memory_pool_used_bytes - Memory usage
```

See [Monitoring Guide - Metrics Reference](monitoring.md#metrics-reference).

---

### Q: What resources should I allocate for production?

**A:** Recommended production resources:

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 4 cores | 8+ cores |
| RAM | 8 GB | 16+ GB |
| Disk | 50 GB SSD | 200+ GB NVMe |
| Network | 100 Mbps | 1 Gbps |

See [Installation Guide - Production Requirements](installation.md#production-requirements).

---

### Q: How do I set up Prometheus and Grafana monitoring?

**A:** Use Docker Compose:
```bash
# Start monitoring stack
docker compose --profile monitoring up -d

# Access services:
# Prometheus: http://localhost:9090
# Grafana: http://localhost:3000 (admin/admin)
```

See [Monitoring Guide - Prometheus Setup](monitoring.md#prometheus-setup).

---

### Q: What metrics should I monitor for trading performance?

**A:** Key trading metrics:

| Metric | Warning | Critical |
|--------|---------|----------|
| Order latency (p99) | > 50ms | > 100ms |
| Error rate | > 0.05/s | > 0.1/s |
| Drawdown | > 10% | > 20% |
| Leverage | > 2x | > 3x |

See [Monitoring Guide - Monitoring Best Practices](monitoring.md#monitoring-best-practices).

---

### Q: How do I configure alerting for critical events?

**A:** Configure Alertmanager with webhooks:
```bash
export VELOZ_ALERT_WEBHOOK_URL=https://hooks.slack.com/services/xxx
export VELOZ_ALERT_EMAIL=alerts@yourdomain.com
```

VeloZ includes pre-configured alerts for:
- System availability (engine down, WebSocket disconnected)
- Performance degradation (high latency, error rate)
- Risk thresholds (drawdown, leverage, VaR breach)

See [Monitoring Guide - Alerting](monitoring.md#alerting).

---

## Related Documentation

- [Installation Guide](installation.md) - Complete installation instructions
- [Configuration Guide](configuration.md) - All configuration options
- [Trading Guide](trading-guide.md) - Trading operations and order management
- [Troubleshooting Guide](troubleshooting.md) - Common issues and solutions
- [Monitoring Guide](monitoring.md) - Observability and alerting
- [Risk Management Guide](risk-management.md) - Risk controls and VaR
- [Best Practices](best-practices.md) - Production recommendations
