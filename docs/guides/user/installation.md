# Installation Guide

This guide covers installing VeloZ for development and production environments.

## Quick Install (TL;DR)

**Ubuntu/Debian (One-liner):**
```bash
sudo apt update && sudo apt install -y cmake ninja-build build-essential clang-16 libssl-dev python3 git && \
git clone https://github.com/Zzzode/VeloZ.git && cd VeloZ && \
cmake --preset dev && cmake --build --preset dev-all -j$(nproc) && \
ctest --preset dev -j$(nproc) && ./scripts/run_gateway.sh dev
```

**macOS (One-liner):**
```bash
xcode-select --install && brew install cmake ninja openssl@3 && \
git clone https://github.com/Zzzode/VeloZ.git && cd VeloZ && \
cmake --preset dev && cmake --build --preset dev-all -j$(sysctl -n hw.ncpu) && \
ctest --preset dev -j$(sysctl -n hw.ncpu) && ./scripts/run_gateway.sh dev
```

**Expected result:**
- All tests pass (100%)
- ✓ Gateway runs on http://127.0.0.1:8080
- ✓ UI accessible in browser

---

## System Requirements

### Development Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Linux (Ubuntu 20.04+), macOS 12+, Windows 10+ |
| CPU | x86_64 or ARM64 |
| RAM | 4 GB minimum, 8 GB recommended |
| Disk | 2 GB for build, 500 MB for runtime |

### Production Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Ubuntu 22.04 LTS | Ubuntu 22.04 LTS |
| CPU | 4 cores | 8+ cores |
| RAM | 8 GB | 16+ GB |
| Disk | 50 GB SSD | 200+ GB NVMe |
| Network | 100 Mbps | 1 Gbps |

**Additional production components:**
- Redis 7+ (caching, rate limiting)
- PostgreSQL 15+ (order persistence)
- Prometheus + Grafana (monitoring)
- HashiCorp Vault (secrets management)

### Software Dependencies

| Dependency | Version | Required | Notes |
|------------|---------|----------|-------|
| CMake | >= 3.24 | Yes | Build system |
| C++ Compiler | C++23 capable | Yes | Clang 16+ or GCC 13+ |
| Python | >= 3.8 | Yes | Gateway runtime |
| OpenSSL | >= 1.1.1 | Recommended | TLS support for WebSocket |
| Ninja | >= 1.10 | Recommended | Faster builds |

## Quick Installation

### Ubuntu/Debian

```bash
# Install system dependencies
sudo apt update
sudo apt install -y \
    cmake \
    ninja-build \
    build-essential \
    clang-16 \
    libssl-dev \
    python3 \
    python3-pip \
    git

# Clone repository
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ

# Build
cmake --preset dev
cmake --build --preset dev -j$(nproc)

# Verify installation
./build/dev/apps/engine/veloz_engine --version
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew dependencies
brew install cmake ninja openssl@3

# Clone and build
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ

cmake --preset dev
cmake --build --preset dev -j$(sysctl -n hw.ncpu)
```

### Windows (MSVC)

```powershell
# Install Visual Studio 2022 with C++ workload
# Install CMake from https://cmake.org/download/

# Clone repository
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ

# Build with MSVC
cmake --preset dev
cmake --build --preset dev -j
```

## Docker Installation

For quick deployment or production environments, use Docker Compose.

### Prerequisites

- Docker 20.10+
- Docker Compose 2.0+

```bash
# Verify Docker installation
docker --version
docker compose version
```

### Quick Start (Gateway Only)

```bash
# Clone repository
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ

# Start gateway in simulation mode
docker compose up -d gateway

# Verify it's running
curl http://localhost:8080/health
```

### Full Stack (Production)

Start all services including monitoring and secrets management:

```bash
# Start with monitoring stack (Prometheus, Grafana, Loki, Jaeger)
docker compose --profile monitoring up -d

# Start with secrets management (Vault)
docker compose --profile secrets up -d

# Start everything
docker compose --profile full --profile monitoring --profile secrets up -d
```

### Service Profiles

| Profile | Services | Use Case |
|---------|----------|----------|
| (default) | gateway | Development, testing |
| `full` | gateway, redis, postgres | Production with persistence |
| `monitoring` | prometheus, grafana, loki, alertmanager, jaeger | Observability |
| `secrets` | vault, vault-init | Secrets management |

**Profile Combinations:**

```bash
# Gateway only (development/testing)
docker compose up -d gateway

# Gateway with persistence (production data)
docker compose --profile full up -d

# Gateway with observability (metrics, logs, tracing)
docker compose --profile monitoring up -d

# Gateway with secrets management (Vault)
docker compose --profile secrets up -d

# Complete production stack
docker compose --profile full --profile monitoring --profile secrets up -d
```

**Service Dependencies:**

- `redis` and `postgres` (in `full` profile) provide persistent storage
- `vault` (in `secrets` profile) provides secure credential storage
- Monitoring services (in `monitoring` profile) provide observability and alerting

### Access Services

| Service | URL | Default Credentials |
|---------|-----|---------------------|
| Gateway | http://localhost:8080 | - |
| Grafana | http://localhost:3000 | admin / admin |
| Prometheus | http://localhost:9090 | - |
| Jaeger | http://localhost:16686 | - |
| Vault | http://localhost:8200 | Token: veloz-dev-token |

### Configuration

Create a `.env` file for custom configuration:

```bash
# .env
VELOZ_PORT=8080
VELOZ_MARKET_SOURCE=binance_rest
VELOZ_BINANCE_API_KEY=<your-api-key>
VELOZ_BINANCE_API_SECRET=<your-api-secret>
GRAFANA_PASSWORD=secure_password
POSTGRES_PASSWORD=secure_password
```

### Stop Services

```bash
# Stop all services
docker compose --profile full --profile monitoring --profile secrets down

# Stop and remove volumes (WARNING: deletes data)
docker compose --profile full --profile monitoring --profile secrets down -v
```

For detailed Docker configuration, see the [docker-compose.yml](../../../docker-compose.yml) file.

---

## KJ Library Dependencies

VeloZ uses the KJ library (v1.3.0) from Cap'n Proto as its core C++ foundation. KJ is automatically fetched and built via CMake FetchContent.

### Automatically Fetched Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| KJ | From Cap'n Proto | Core utilities, async I/O, memory management |
| KJ Test | From Cap'n Proto | Testing framework |
| yyjson | Latest | JSON parsing |

### KJ Features Used

- `kj::MutexGuarded<T>` - Thread-safe state management
- `kj::Own<T>` - Owned pointers (replaces `std::unique_ptr`)
- `kj::Maybe<T>` - Nullable values (replaces `std::optional`)
- `kj::String` / `kj::StringPtr` - String handling
- `kj::Array<T>` / `kj::Vector<T>` - Dynamic arrays
- `kj::Promise<T>` - Async operations
- `kj::AsyncIoContext` - Async I/O (WebSocket, HTTP)

### Manual KJ Installation (Optional)

If you prefer to use a system-installed KJ library:

```bash
# Clone Cap'n Proto
git clone https://github.com/capnproto/capnproto.git
cd capnproto/c++

# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build

# Configure VeloZ to use system KJ
cmake --preset dev -DUSE_SYSTEM_KJ=ON
```

## Build Presets

VeloZ provides CMake presets for different build configurations:

| Preset | Description | Use Case |
|--------|-------------|----------|
| `dev` | Debug build with symbols | Development |
| `release` | Optimized release build | Production |
| `asan` | AddressSanitizer enabled | Memory debugging |

### Development Build

```bash
cmake --preset dev
cmake --build --preset dev -j$(nproc)
```

Output: `build/dev/apps/engine/veloz_engine`

### Release Build

```bash
cmake --preset release
cmake --build --preset release -j$(nproc)
```

Output: `build/release/apps/engine/veloz_engine`

### ASan Build (Memory Debugging)

```bash
cmake --preset asan
cmake --build --preset asan -j$(nproc)
```

## Build Targets

Build specific components instead of everything:

```bash
# Build only engine
cmake --build --preset dev-engine -j$(nproc)

# Build only libraries
cmake --build --preset dev-libs -j$(nproc)

# Build only tests
cmake --build --preset dev-tests -j$(nproc)
```

## Running Tests

```bash
# Run all tests
ctest --preset dev -j$(nproc)

# Run specific test suite
./build/dev/libs/core/veloz_core_tests
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/backtest/veloz_backtest_tests
```

## Environment Variables

Configure VeloZ behavior via environment variables:

### Gateway Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset to use |

### Market Data Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_MARKET_SOURCE` | `sim` | Market source: `sim` or `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol |
| `VELOZ_BINANCE_BASE_URL` | `https://api.binance.com` | Binance REST API URL |

### Execution Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://testnet.binance.vision` | Binance trade API URL |
| `VELOZ_BINANCE_API_KEY` | (empty) | Binance API key |
| `VELOZ_BINANCE_API_SECRET` | (empty) | Binance API secret |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://testnet.binance.vision/ws` | Binance WebSocket URL |

### Authentication Configuration (Optional)

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_AUTH_ENABLED` | `false` | Enable authentication |
| `VELOZ_JWT_SECRET` | (auto-generated) | JWT signing secret (min 32 chars) |
| `VELOZ_TOKEN_EXPIRY` | `3600` | Access token expiry (seconds) |
| `VELOZ_ADMIN_PASSWORD` | (empty) | Admin user password |
| `VELOZ_RATE_LIMIT_CAPACITY` | `100` | Rate limit capacity |
| `VELOZ_RATE_LIMIT_REFILL` | `10.0` | Rate limit refill rate |

### Audit Logging Configuration (Optional)

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_AUDIT_LOG_ENABLED` | `true` | Enable audit logging |
| `VELOZ_AUDIT_LOG_FILE` | (stderr) | Audit log file path |
| `VELOZ_AUDIT_LOG_RETENTION_DAYS` | `90` | Log retention period |

### Reconciliation Configuration (Optional)

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_RECONCILIATION_ENABLED` | `true` | Enable reconciliation |
| `VELOZ_RECONCILIATION_INTERVAL` | `30` | Reconciliation interval (seconds) |
| `VELOZ_AUTO_CANCEL_ORPHANED` | `false` | Auto-cancel orphaned orders |

## Verification

After installation, verify everything works:

```bash
# Check engine starts
./build/dev/apps/engine/veloz_engine --help

# Run smoke test
./scripts/run_engine.sh dev

# Start gateway and UI
./scripts/run_gateway.sh dev
# Open http://127.0.0.1:8080 in browser

# Run test suite
ctest --preset dev -j$(nproc)
```

**Expected Results:**
- Engine starts without errors
- All tests pass (100% pass rate)
- Gateway accessible at http://127.0.0.1:8080
- UI loads in browser

## Post-Installation Setup

### Optional: Enable Authentication

For production or multi-user environments:

```bash
# Generate secure JWT secret (min 32 characters)
export VELOZ_JWT_SECRET=$(openssl rand -base64 32)

# Set admin password
export VELOZ_ADMIN_PASSWORD=your_secure_password

# Enable authentication
export VELOZ_AUTH_ENABLED=true

# Configure audit logging
export VELOZ_AUDIT_LOG_ENABLED=true
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log
export VELOZ_AUDIT_LOG_RETENTION_DAYS=90

# Start gateway
./scripts/run_gateway.sh dev
```

**Audit log setup for production:**

```bash
# Create audit log directory with proper permissions
sudo mkdir -p /var/log/veloz/audit
sudo chown veloz:veloz /var/log/veloz/audit

# Configure log rotation (optional, VeloZ handles retention internally)
# Logs are stored in NDJSON format for easy parsing
# Archives are automatically compressed with gzip
```

**Retention policies by log type:**
- Auth logs: 90 days (login, logout, token refresh)
- Order logs: 365 days (order placement and execution)
- API key logs: 365 days (key creation and revocation)
- Access logs: 14 days (general API access)

### Optional: Configure Binance Integration

For live market data or testnet trading:

```bash
# Get API keys from https://testnet.binance.vision/

# Configure Binance
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=<your-api-key>
export VELOZ_BINANCE_API_SECRET=<your-api-secret>

# Start gateway
./scripts/run_gateway.sh dev
```

### Optional: Enable Reconciliation

For production trading with automatic order state synchronization:

```bash
export VELOZ_RECONCILIATION_ENABLED=true
export VELOZ_RECONCILIATION_INTERVAL=30
export VELOZ_AUTO_CANCEL_ORPHANED=true
./scripts/run_gateway.sh dev
```

### Create Systemd Service (Production)

For production deployments, create a systemd service:

```bash
# Create service file
sudo tee /etc/systemd/system/veloz.service > /dev/null <<EOF
[Unit]
Description=VeloZ Trading Gateway
After=network.target

[Service]
Type=simple
User=veloz
WorkingDirectory=/opt/veloz
Environment="VELOZ_PRESET=release"
Environment="VELOZ_AUTH_ENABLED=true"
Environment="VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log"
ExecStart=/opt/veloz/scripts/run_gateway.sh
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable veloz
sudo systemctl start veloz

# Check status
sudo systemctl status veloz
```

## Troubleshooting

### CMake version too old

```bash
# Check version
cmake --version

# Ubuntu: Install newer CMake
sudo apt remove cmake
pip3 install cmake
```

### Compiler doesn't support C++23

```bash
# Ubuntu: Install Clang 16
sudo apt install clang-16
export CC=clang-16
export CXX=clang++-16
cmake --preset dev
```

### OpenSSL not found

```bash
# Ubuntu
sudo apt install libssl-dev

# macOS
brew install openssl@3
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

### Build fails with KJ errors

```bash
# Clean and rebuild
rm -rf build/
cmake --preset dev
cmake --build --preset dev -j$(nproc)
```

### Tests fail

```bash
# Run with verbose output
ctest --preset dev --output-on-failure

# Run specific test
./build/dev/libs/core/veloz_core_tests
```

### Gateway fails to start

**Symptom:** `./scripts/run_gateway.sh dev` exits immediately

**Diagnosis:**
```bash
# Check if engine binary exists
ls -la build/dev/apps/engine/veloz_engine

# Check Python version
python3 --version  # Should be 3.8+

# Check port availability
lsof -i :8080 || netstat -an | grep 8080
```

**Solutions:**
```bash
# If engine not built
cmake --build --preset dev-all -j$(nproc)

# If port in use
export VELOZ_PORT=8081
./scripts/run_gateway.sh dev

# If Python too old
# Ubuntu: sudo apt install python3.11
# macOS: brew install python@3.11
```

### Authentication not working

**Symptom:** Login returns 401 or auth endpoints return 500

**Diagnosis:**
```bash
# Check if auth is enabled
echo $VELOZ_AUTH_ENABLED

# Check if admin password is set
echo $VELOZ_ADMIN_PASSWORD
```

**Solutions:**
```bash
# Enable authentication properly
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_password
export VELOZ_JWT_SECRET=$(openssl rand -base64 32)
./scripts/run_gateway.sh dev
```

### Audit logs not appearing

**Symptom:** Audit events not being logged or log file empty

**Diagnosis:**
```bash
# Check if audit logging is enabled
echo $VELOZ_AUDIT_LOG_ENABLED

# Check if log directory exists and is writable
ls -la /var/log/veloz/audit/

# Check if log file is being written
tail -f /var/log/veloz/audit.log
```

**Solutions:**
```bash
# Enable audit logging
export VELOZ_AUDIT_LOG_ENABLED=true

# Create log directory with proper permissions
sudo mkdir -p /var/log/veloz/audit
sudo chown $(whoami):$(whoami) /var/log/veloz/audit

# Set log file path
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit.log

# Restart gateway
./scripts/run_gateway.sh dev
```

**Note:** Audit logging requires authentication to be enabled (`VELOZ_AUTH_ENABLED=true`).

### Binance API errors

**Symptom:** Orders fail with "binance_not_configured" or API errors

**Common causes:**
1. API keys not set
2. Using mainnet keys with testnet URLs (or vice versa)
3. API key permissions insufficient
4. Rate limit exceeded

**Solutions:**
```bash
# Verify configuration
echo "API Key: $VELOZ_BINANCE_API_KEY"
echo "Trade URL: $VELOZ_BINANCE_TRADE_BASE_URL"

# For testnet, ensure testnet URL
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws

# For mainnet (CAUTION: real money)
export VELOZ_BINANCE_TRADE_BASE_URL=https://api.binance.com
export VELOZ_BINANCE_WS_BASE_URL=wss://stream.binance.com:9443/ws

# Check API key permissions on Binance website
# Required: Spot & Margin Trading enabled
```

### Build takes too long

**Symptom:** Build takes > 10 minutes

**Solutions:**
```bash
# Use Ninja instead of Make (faster)
sudo apt install ninja-build  # Ubuntu
brew install ninja            # macOS

# Build specific targets only
cmake --build --preset dev-engine -j$(nproc)  # Just engine

# Use ccache for faster rebuilds
sudo apt install ccache  # Ubuntu
brew install ccache      # macOS
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### Memory issues during build

**Symptom:** Compiler killed, "out of memory" errors

**Solutions:**
```bash
# Reduce parallel jobs
cmake --build --preset dev-all -j2  # Use only 2 cores

# Or build sequentially
cmake --build --preset dev-all -j1

# Check available memory
free -h  # Linux
vm_stat  # macOS
```

### WebSocket connection fails

**Symptom:** Market data not updating, WebSocket errors in logs

**Diagnosis:**
```bash
# Test WebSocket connectivity
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Host: testnet.binance.vision" \
  https://testnet.binance.vision/ws
```

**Solutions:**
```bash
# Check OpenSSL is installed
openssl version

# Verify WebSocket URL is correct
echo $VELOZ_BINANCE_WS_BASE_URL

# Check firewall/proxy settings
# Ensure outbound WebSocket connections allowed
```

## Installation Verification Checklist

After installation, verify everything works:

- [ ] CMake version >= 3.24: `cmake --version`
- [ ] Compiler supports C++23: `clang++ --version` or `g++ --version`
- [ ] Python >= 3.8: `python3 --version`
- [ ] OpenSSL installed: `openssl version`
- [ ] Build succeeds: `cmake --build --preset dev-all -j$(nproc)`
- [ ] All tests pass: `ctest --preset dev -j$(nproc)`
- [ ] Engine starts: `./build/dev/apps/engine/veloz_engine --help`
- [ ] Gateway starts: `./scripts/run_gateway.sh dev`
- [ ] UI loads: Open http://127.0.0.1:8080 in browser
- [ ] Health check: `curl http://127.0.0.1:8080/health` returns `{"ok":true}`

## Next Steps

### Getting Started
- [Quick Start Guide](quick-start.md) - Get running in 5 minutes
- [Your First Trade Tutorial](../../tutorials/first-trade.md) - Place your first order

### Configuration
- [Configuration Guide](configuration.md) - Environment variables and settings
- [Binance Integration](binance.md) - Connect to Binance exchange

### Production
- [Best Practices](best-practices.md) - Security, trading, and deployment recommendations
- [Monitoring Guide](monitoring.md) - Set up observability stack
- [Troubleshooting](troubleshooting.md) - Common issues and solutions

### Development
- [Development Guide](development.md) - Contributing to VeloZ
- [API Documentation](../../api/README.md) - Complete API reference

### Reference
- [Glossary](glossary.md) - Definitions of trading, risk, and technical terms
