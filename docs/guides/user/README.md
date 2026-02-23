# VeloZ User Guide

Complete documentation for VeloZ users, from installation to advanced trading workflows.

## Table of Contents

### Getting Started
- [Quick Start Guide](getting-started.md) - Get up and running in 5 minutes
- [Installation Guide](installation.md) - Detailed installation instructions
- [Configuration Guide](configuration.md) - Environment variables and settings
- [UI Setup and Run Guide](../ui_setup_and_run.md) - Complete UI setup instructions

### Core Features
- [Authentication & Security](#authentication--security) - JWT tokens, API keys, permissions
- [Trading Workflows](#trading-workflows) - Place orders, manage positions
- [Backtesting](backtest.md) - Test strategies against historical da{}ta
- [Live Trading](binance.md) - Connect to Binance for live trading
- [Monitoring & Metrics](monitoring.md) - System health and performance

### Advanced Topics
- [Strategy Development](development.md) - Create custom trading strategies
- [Risk Management](#risk-management) - Configure risk controls
- [Reconciliation](#reconciliation) - Order state synchronization
- [Multi-Exchange Trading](#multi-exchange-trading) - Trade across multiple venues

### Reference
- [API Documentation](../../api/README.md) - Complete API reference
- [Troubleshooting](../deployment/troubleshooting.md) - Common issues and solutions
- [FAQ](#frequently-asked-questions) - Quick answers

---

## Authentication & Security

VeloZ supports optional authentication for secure API access.

### Enabling Authentication

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=your_secure_password
export VELOZ_JWT_SECRET=your_secret_key
./scripts/run_gateway.sh dev
```

### Authentication Methods

#### 1. JWT Tokens (Recommended for Users)

**Login to get tokens:**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"your_password"}'
```

**Response:**
```json
{
  "access_token": "eyJ...",
  "refresh_token": "eyJ...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Use access token:**
```bash
curl http://127.0.0.1:8080/api/orders_state \
  -H "Authorization: Bearer YOUR_ACCESS_TOKEN"
```

**Refresh expired token:**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"YOUR_REFRESH_TOKEN"}'
```

**Logout (revoke refresh token):**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/logout \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"YOUR_REFRESH_TOKEN"}'
```

#### 2. API Keys (Recommended for Services)

**Create API key (requires admin):**
```bash
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"trading-bot","permissions":["read","write"]}'
```

**Use API key:**
```bash
curl http://127.0.0.1:8080/api/orders_state \
  -H "X-API-Key: veloz_abc123..."
```

### Permission Levels

| Permission | Access | Use Case |
|------------|--------|----------|
| `read` | GET endpoints only | Monitoring, analytics |
| `write` | Read + POST/DELETE | Trading bots |
| `admin` | Full access | User management, system admin |

### Security Best Practices

1. **Use HTTPS in production** - Never send tokens over HTTP
2. **Store tokens securely** - Use environment variables or secure vaults
3. **Rotate API keys regularly** - Revoke and recreate keys periodically
4. **Monitor audit logs** - Review security events regularly
5. **Use least privilege** - Grant minimum required permissions

---

## Trading Workflows

### Workflow 1: Manual Trading

**1. Check market data:**
```bash
curl http://127.0.0.1:8080/api/market
```

**2. Place an order:**
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

**3. Monitor order status:**
```bash
# Via SSE stream
curl -N http://127.0.0.1:8080/api/stream

# Or poll order state
curl "http://127.0.0.1:8080/api/order_state?client_order_id=ORDER_ID"
```

**4. Check positions:**
```bash
curl http://127.0.0.1:8080/api/positions
```

**5. Cancel order if needed:**
```bash
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -d '{"client_order_id":"ORDER_ID","symbol":"BTCUSDT"}'
```

### Workflow 2: Automated Trading Bot

**Example Python bot:**
```python
import requests
import json

class VeloZBot:
    def __init__(self, base_url, api_key):
        self.base_url = base_url
        self.headers = {"X-API-Key": api_key}

    def get_market_data(self):
        r = requests.get(f"{self.base_url}/api/market", headers=self.headers)
        return r.json()

    def place_order(self, side, symbol, qty, price):
        data = {"side": side, "symbol": symbol, "qty": qty, "price": price}
        r = requests.post(
            f"{self.base_url}/api/order",
            headers={**self.headers, "Content-Type": "application/json"},
            json=data
        )
        return r.json()

    def get_positions(self):
        r = requests.get(f"{self.base_url}/api/positions", headers=self.headers)
        return r.json()

# Usage
bot = VeloZBot("http://127.0.0.1:8080", "veloz_your_api_key")
market = bot.get_market_data()
print(f"Current price: {market['price']}")

# Place order
result = bot.place_order("BUY", "BTCUSDT", 0.001, 50000.0)
print(f"Order placed: {result['client_order_id']}")
```

---

## Risk Management

### Configuring Risk Limits

Risk limits are configured per strategy. Example configuration:

```json
{
  "strategy_id": "momentum_1",
  "risk_limits": {
    "max_position_size": 1.0,
    "max_drawdown_pct": 10.0,
    "max_daily_loss": 1000.0,
    "max_order_size": 0.1,
    "stop_loss_pct": 2.0
  }
}
```

### Monitoring Risk Metrics

```bash
# Get risk metrics
curl http://127.0.0.1:8080/api/risk/metrics

# Get position risk
curl http://127.0.0.1:8080/api/positions/BTCUSDT
```

### Circuit Breakers

VeloZ includes automatic circuit breakers that halt trading when:
- Drawdown exceeds threshold
- Daily loss limit reached
- Position size exceeds maximum
- Rapid consecutive losses detected

---

## Reconciliation

Reconciliation ensures local order state matches exchange state.

### Automatic Reconciliation

Reconciliation runs automatically every 30 seconds by default.

### Manual Reconciliation

```bash
# Trigger reconciliation for all venues
curl -X POST http://127.0.0.1:8080/api/reconcile

# Reconcile specific venue
curl -X POST http://127.0.0.1:8080/api/reconcile/binance
```

### Orphaned Order Handling

Orphaned orders (on exchange but not in local state) are automatically:
1. **Detected** during reconciliation
2. **Logged** to audit trail
3. **Cancelled** (if auto_cancel_orphaned=true)

**Configuration:**
```bash
export VELOZ_RECONCILIATION_INTERVAL=30  # seconds
export VELOZ_AUTO_CANCEL_ORPHANED=true
```

---

## Multi-Exchange Trading

VeloZ supports multiple exchanges simultaneously.

### Supported Exchanges

| Exchange | Spot | Futures | Status |
|----------|------|---------|--------|
| Binance | ✓ | Planned | Production |
| OKX | ✓ | Planned | Beta |
| Bybit | ✓ | Planned | Beta |
| Coinbase | ✓ | - | Beta |

### Configuration Example

```bash
# Binance configuration
export VELOZ_BINANCE_API_KEY=your_key
export VELOZ_BINANCE_API_SECRET=your_secret

# OKX configuration
export VELOZ_OKX_API_KEY=your_key
export VELOZ_OKX_API_SECRET=your_secret
export VELOZ_OKX_PASSPHRASE=your_passphrase

./scripts/run_gateway.sh dev
```

---

## Frequently Asked Questions

### General

**Q: Is VeloZ production-ready?**
A: VeloZ is in active development. Use testnet for testing. Production use requires thorough testing and risk management.

**Q: What exchanges are supported?**
A: Currently Binance (production), OKX, Bybit, and Coinbase (beta).

**Q: Can I run multiple strategies simultaneously?**
A: Yes, VeloZ supports multiple strategies running concurrently.

### Authentication

**Q: How long do access tokens last?**
A: Access tokens expire after 1 hour (default). Use refresh tokens to get new access tokens.

**Q: How long do refresh tokens last?**
A: Refresh tokens expire after 7 days (default).

**Q: Can I revoke an API key?**
A: Yes, use `DELETE /api/auth/keys?key_id=KEY_ID` (requires admin permission).

### Trading

**Q: What order types are supported?**
A: Currently limit orders. Market orders and stop orders are planned.

**Q: How do I test without risking real money?**
A: Use simulation mode (default) or exchange testnet environments.

**Q: Can I paper trade with live market data?**
A: Yes, set `VELOZ_MARKET_SOURCE=binance_rest` and `VELOZ_EXECUTION_MODE=sim_engine`.

### Troubleshooting

**Q: Gateway won't start**
A: Check if port 8080 is in use: `lsof -i :8080`. Use a different port or kill the conflicting process.

**Q: Orders not executing**
A: Verify API keys are correct and have trading permissions. Check exchange rate limits.

**Q: UI not loading**
A: Ensure gateway is running and accessible. Check browser console for errors.

---

## Additional Resources

### Documentation
- [HTTP API Reference](../../api/http_api.md) - Complete REST API documentation
- [SSE API](../../api/sse_api.md) - Real-time event streaming
- [Engine Protocol](../../api/engine_protocol.md) - Engine command format
- [Build and Run](../build_and_run.md) - Build system and scripts

### Development
- [Strategy Development](development.md) - Create custom strategies
- [KJ Library Guide](../../kj/library_usage_guide.md) - C++ library patterns
- [Design Documents](../../design) - System architecture

### Deployment
- [Production Architecture](../deployment/production_architecture.md) - Production setup
- [Monitoring Guide](../deployment/monitoring.md) - Observability setup
- [Backup & Recovery](../deployment/backup_recovery.md) - Data protection

### Support
- [Troubleshooting Guide](../deployment/troubleshooting.md) - Common issues
- [GitHub Issues](https://github.com/your-org/VeloZ/issues) - Report bugs
- [Implementation Status](../../project/reviews/implementation_status.md) - Feature progress
