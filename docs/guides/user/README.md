# VeloZ v1.0 User Documentation

Complete documentation for VeloZ, the production-ready quantitative trading framework.

> **Production Status:** VeloZ v1.0 is production-ready with 615+ tests passing, 80k events/sec performance, and enterprise-grade security.

---

## Quick Start

**Get VeloZ running in 5 minutes:**

```bash
git clone https://github.com/veloz-trading/veloz.git
cd veloz
cmake --preset dev && cmake --build --preset dev-all -j$(nproc)
./scripts/run_gateway.sh dev
```

Open **http://127.0.0.1:8080** in your browser.

See [Quick Start Guide](quick-start.md) for detailed steps with expected output.

### Key Features

| Feature | Description |
|---------|-------------|
| **Multi-Exchange Trading** | Binance (production), OKX, Bybit, Coinbase (beta) |
| **Smart Order Routing** | Fee-aware routing with latency tracking |
| **Execution Algorithms** | TWAP, VWAP, Iceberg, POV with order splitting |
| **Advanced Risk Management** | VaR (Historical, Parametric, Monte Carlo), stress testing |
| **Enterprise Security** | JWT + API keys, RBAC, Vault integration, audit logging |
| **Complete Observability** | Prometheus, Grafana, Jaeger, Loki, 25+ alert rules |

---

## User Guides

### Getting Started

| Guide | Description |
|-------|-------------|
| [Installation Guide](installation.md) | System requirements, dependencies, build instructions |
| [Quick Start Guide](quick-start.md) | 5-minute setup with step-by-step commands |
| [Getting Started](getting-started.md) | Comprehensive setup with all configuration options |
| [Configuration Guide](configuration.md) | Environment variables, secrets management, tuning |

### Trading Operations

| Guide | Description |
|-------|-------------|
| [Trading Guide](trading-guide.md) | Order types, position management, execution algorithms, backtesting |
| [API Usage Guide](api-usage-guide.md) | Practical API examples with Python client |
| [Binance Integration](binance.md) | Binance-specific setup, testnet, WebSocket |

### System Management

| Guide | Description |
|-------|-------------|
| [Risk Management Guide](risk-management.md) | VaR, stress testing, circuit breakers, alerts |
| [Monitoring Guide](monitoring.md) | Prometheus, Grafana, Jaeger, Loki setup |
| [Troubleshooting Guide](troubleshooting.md) | Common issues and solutions |
| [Best Practices](best-practices.md) | Security, trading, risk, performance best practices |

### Development

| Guide | Description |
|-------|-------------|
| [Development Guide](development.md) | Development environment, testing, debugging |

---

## Tutorials

Step-by-step tutorials for hands-on learning.

| Tutorial | Time | Level | Description |
|----------|------|-------|-------------|
| [Your First Trade](../../tutorials/first-trade.md) | 15 min | Beginner | Place your first order, monitor fills, check positions |
| [Multi-Exchange Arbitrage](../../tutorials/multi-exchange-arbitrage.md) | 25 min | Intermediate | Trade across multiple exchanges with smart routing |
| [Production Deployment](../../tutorials/production-deployment.md) | 45 min | Intermediate | Deploy VeloZ with systemd, Nginx, TLS |
| [Custom Strategy Development](../../tutorials/custom-strategy-development.md) | 60 min | Advanced | Build, test, and deploy custom trading strategies |

**Note:** For detailed information on risk management, monitoring, and backtesting, see the corresponding User Guides above.

---

## Learning Paths

Recommended documentation sequences based on your role.

### New User Path

Get started with VeloZ from scratch.

```
Installation Guide → Quick Start Guide → Your First Trade Tutorial
       ↓                    ↓                      ↓
   Build VeloZ        Run gateway           Place first order
```

1. [Installation Guide](installation.md) - Build VeloZ from source
2. [Quick Start Guide](quick-start.md) - Start the gateway and UI
3. [Your First Trade](../../tutorials/first-trade.md) - Place and monitor orders

### Active Trader Path

Master trading operations and API usage.

```
Trading Guide → API Usage Guide → Risk Management → Binance Integration
      ↓               ↓                  ↓                  ↓
 Order types    Python client      VaR & limits       Live trading
```

1. [Trading Guide](trading-guide.md) - Order types, positions, execution algorithms
2. [API Usage Guide](api-usage-guide.md) - REST API, SSE streaming, Python client
3. [Risk Management Guide](risk-management.md) - VaR, stress testing, circuit breakers
4. [Binance Integration](binance.md) - Connect to live exchange

### System Operator Path

Deploy and operate VeloZ in production.

```
Configuration → Monitoring → Production Deployment → Best Practices
      ↓             ↓                 ↓                    ↓
  Env vars      Grafana          systemd/Nginx        Security
```

1. [Configuration Guide](configuration.md) - Environment variables, secrets
2. [Monitoring Guide](monitoring.md) - Prometheus, Grafana, alerting
3. [Production Deployment](../../tutorials/production-deployment.md) - systemd, Nginx, TLS
4. [Best Practices](best-practices.md) - Security, performance, operations

### Strategy Developer Path

Build and deploy custom trading strategies.

```
Custom Strategy → Trading Guide → API Usage → Risk Management
       ↓              ↓             ↓              ↓
  C++ strategy    Backtesting   Integration    Risk limits
```

1. [Custom Strategy Development](../../tutorials/custom-strategy-development.md) - Build strategies
2. [Trading Guide](trading-guide.md) - Backtesting and optimization
3. [API Usage Guide](api-usage-guide.md) - Integrate via API
4. [Risk Management Guide](risk-management.md) - Configure risk limits

---

## Quick Reference

### Essential Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `VELOZ_PORT` | Gateway HTTP port | `8080` |
| `VELOZ_EXECUTION_MODE` | Execution mode | `sim_engine` |
| `VELOZ_MARKET_SOURCE` | Market data source | `sim` |
| `VELOZ_MARKET_SYMBOL` | Trading symbol(s) | `BTCUSDT` |
| `VELOZ_AUTH_ENABLED` | Enable authentication | `false` |
| `VELOZ_BINANCE_API_KEY` | Binance API key | - |
| `VELOZ_BINANCE_API_SECRET` | Binance API secret | - |

See [Configuration Guide](configuration.md) for complete reference.

### Essential API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | System health check |
| `/api/market` | GET | Current market data |
| `/api/order` | POST | Place new order |
| `/api/cancel` | POST | Cancel order |
| `/api/orders_state` | GET | All orders status |
| `/api/positions` | GET | Current positions |
| `/api/stream` | GET | SSE event stream |
| `/api/risk/metrics` | GET | Risk metrics |
| `/metrics` | GET | Prometheus metrics |

See [HTTP API Reference](../../api/http_api.md) for complete documentation.

### Common Commands

```bash
# Build
cmake --preset dev && cmake --build --preset dev-all -j$(nproc)

# Run tests
ctest --preset dev -j$(nproc)

# Start gateway
./scripts/run_gateway.sh dev

# Health check
curl http://127.0.0.1:8080/health

# Place order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{"side":"BUY","symbol":"BTCUSDT","qty":0.001,"price":50000}'

# View orders
curl http://127.0.0.1:8080/api/orders_state

# Subscribe to events
curl -N http://127.0.0.1:8080/api/stream
```

---

## Authentication Quick Start

### Enable Authentication

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
export VELOZ_JWT_SECRET=$(openssl rand -base64 64)
./scripts/run_gateway.sh dev
```

### Login and Use Token

```bash
# Login
TOKEN=$(curl -s -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_secure_password"}' | jq -r '.access_token')

# Use token
curl http://127.0.0.1:8080/api/orders_state \
  -H "Authorization: Bearer $TOKEN"
```

### Create API Key for Automation

```bash
# Create API key (requires admin token)
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"trading-bot","permissions":["read","write"]}'

# Use API key
curl http://127.0.0.1:8080/api/market \
  -H "X-API-Key: veloz_abc123..."
```

### Permission Levels

| Permission | Access |
|------------|--------|
| `read` | View market data, orders, positions |
| `write` | Place and cancel orders |
| `admin` | Manage users, API keys, system config |

---

## Multi-Exchange Trading

### Supported Exchanges

| Exchange | Status | Spot | Futures |
|----------|--------|------|---------|
| Binance | Production | Yes | Planned |
| OKX | Beta | Yes | Planned |
| Bybit | Beta | Yes | Planned |
| Coinbase | Beta | Yes | - |

### Quick Setup

```bash
# Configure multiple exchanges
export VELOZ_EXECUTION_MODE=multi_exchange
export VELOZ_EXCHANGES=binance,okx

# Binance credentials
export VELOZ_BINANCE_API_KEY=your_key
export VELOZ_BINANCE_API_SECRET=your_secret

# OKX credentials
export VELOZ_OKX_API_KEY=your_key
export VELOZ_OKX_API_SECRET=your_secret
export VELOZ_OKX_PASSPHRASE=your_passphrase

./scripts/run_gateway.sh dev
```

### Smart Order Routing

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 1.0,
    "routing": "smart",
    "routing_strategy": "best_price"
  }'
```

See [Trading Guide](trading-guide.md#multi-exchange-trading) for details.

---

## Risk Management Quick Start

### Configure Risk Limits

```bash
export VELOZ_MAX_POSITION_SIZE=1.0
export VELOZ_MAX_DRAWDOWN_PCT=0.10
export VELOZ_STOP_LOSS_PCT=0.02
export VELOZ_CIRCUIT_BREAKER_ENABLED=true
```

### Monitor Risk Metrics

```bash
# Get VaR calculation
curl http://127.0.0.1:8080/api/risk/var

# Get stress test results
curl http://127.0.0.1:8080/api/risk/stress-test

# Check circuit breaker status
curl http://127.0.0.1:8080/api/risk/circuit-breaker
```

See [Risk Management Guide](risk-management.md) for complete documentation.

---

## Reference Documentation

### API Documentation

- [HTTP API Reference](../../api/http_api.md) - Complete REST API documentation
- [SSE API](../../api/sse_api.md) - Real-time event streaming
- [Engine Protocol](../../api/engine_protocol.md) - Engine command format

### Deployment Documentation

- [Production Architecture](../deployment/production_architecture.md) - System architecture
- [High Availability](../deployment/high_availability.md) - HA configuration
- [Disaster Recovery](../deployment/dr_runbook.md) - Backup and recovery

### Development Documentation

- [Build and Run](../build_and_run.md) - Build system and scripts
- [KJ Library Guide](../../references/kjdoc/library_usage_guide.md) - C++ library patterns
- [Design Documents](../../design/README.md) - System architecture

---

## Getting Help

### Troubleshooting

See [Troubleshooting Guide](troubleshooting.md) for common issues:
- Build failures
- Connection errors
- Order rejections
- UI issues

### Support Resources

- [GitHub Issues](https://github.com/veloz-trading/veloz/issues) - Report bugs
- [FAQ](#frequently-asked-questions) - Quick answers
- [Best Practices](best-practices.md) - Recommended configurations

---

## Frequently Asked Questions

### General

**Q: Is VeloZ production-ready?**
A: Yes, VeloZ v1.0 is production-ready with 615+ tests, enterprise security, and complete observability.

**Q: What exchanges are supported?**
A: Binance (production), OKX, Bybit, Coinbase (beta).

**Q: Can I run multiple strategies simultaneously?**
A: Yes, VeloZ supports multiple concurrent strategies with independent risk limits.

### Trading

**Q: What order types are supported?**
A: Limit orders with execution algorithms (TWAP, VWAP, Iceberg, POV).

**Q: How do I test without risking real money?**
A: Use simulation mode (default) or exchange testnet environments.

**Q: What is smart order routing?**
A: Automatic routing of orders to the best exchange based on price, fees, and latency.

### Security

**Q: How long do access tokens last?**
A: 1 hour (default). Use refresh tokens to get new access tokens.

**Q: Can I use API keys instead of JWT?**
A: Yes, API keys are recommended for automated trading bots.

**Q: Is audit logging available?**
A: Yes, comprehensive audit logging with configurable retention.

---

## Document Index

### All User Guides (11)

1. [Installation Guide](installation.md)
2. [Quick Start Guide](quick-start.md)
3. [Getting Started](getting-started.md)
4. [Configuration Guide](configuration.md)
5. [Trading Guide](trading-guide.md)
6. [API Usage Guide](api-usage-guide.md)
7. [Binance Integration](binance.md)
8. [Risk Management Guide](risk-management.md)
9. [Monitoring Guide](monitoring.md)
10. [Troubleshooting Guide](troubleshooting.md)
11. [Best Practices](best-practices.md)

### All Tutorials (4)

1. [Your First Trade](../../tutorials/first-trade.md) - 15 min, Beginner
2. [Multi-Exchange Arbitrage](../../tutorials/multi-exchange-arbitrage.md) - 25 min, Intermediate
3. [Production Deployment](../../tutorials/production-deployment.md) - 45 min, Intermediate
4. [Custom Strategy Development](../../tutorials/custom-strategy-development.md) - 60 min, Advanced
