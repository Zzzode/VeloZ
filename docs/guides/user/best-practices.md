# Best Practices Guide

This guide provides recommendations for running VeloZ securely and efficiently in production environments.

## Table of Contents

1. [Security Best Practices](#security-best-practices)
2. [Trading Best Practices](#trading-best-practices)
3. [Risk Management Best Practices](#risk-management-best-practices)
4. [Performance Best Practices](#performance-best-practices)
5. [Monitoring Best Practices](#monitoring-best-practices)
6. [Backup and Recovery](#backup-and-recovery)
7. [Production Deployment Checklist](#production-deployment-checklist)

---

## Security Best Practices

### API Key Management

**Store credentials securely:**
- Never commit API keys to version control
- Use environment variables or secret management systems
- Rotate keys regularly (monthly recommended)

```bash
# Use environment variables (good)
export VELOZ_BINANCE_API_KEY=$(cat /secure/path/binance_key)
export VELOZ_BINANCE_API_SECRET=$(cat /secure/path/binance_secret)

# Never hardcode in scripts (bad)
# VELOZ_BINANCE_API_KEY="abc123..."  # DON'T DO THIS
```

**Restrict API key permissions:**
- Enable only required permissions (spot trading, not withdrawals)
- Use IP whitelisting on exchange API keys
- Create separate keys for different environments (testnet, production)

| Environment | Permissions | IP Restriction |
|-------------|-------------|----------------|
| Development | Read only | None |
| Testnet | Spot trading | Office IP |
| Production | Spot trading | Server IP only |

### Authentication Configuration

**Enable authentication in production:**

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=$(openssl rand -base64 32)
export VELOZ_JWT_SECRET=$(openssl rand -base64 64)
```

**Use strong secrets:**
- Minimum 32 characters for passwords
- Minimum 64 characters for JWT secrets
- Generate cryptographically random values

**Token management:**
- Set appropriate token expiration (1 hour for access, 7 days for refresh)
- Implement token refresh before expiration
- Revoke tokens on logout

### Network Security

**Use HTTPS in production:**
- Deploy behind a reverse proxy (nginx, Caddy)
- Use valid TLS certificates (Let's Encrypt)
- Redirect HTTP to HTTPS

```nginx
# nginx configuration example
server {
    listen 443 ssl;
    server_name veloz.yourdomain.com;

    ssl_certificate /etc/letsencrypt/live/veloz.yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/veloz.yourdomain.com/privkey.pem;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

**Firewall configuration:**
- Allow only necessary ports (443 for HTTPS)
- Block direct access to port 8080 from external networks
- Use VPN for administrative access

### Audit Logging

**Enable comprehensive audit logging:**

```bash
export VELOZ_AUDIT_ENABLED=true
export VELOZ_AUDIT_LOG_PATH=/var/log/veloz/audit.log
```

**Log retention:**
- Retain audit logs for minimum 90 days
- Archive older logs to cold storage
- Implement log rotation to prevent disk exhaustion

---

## Trading Best Practices

### Order Management

**Use client order IDs consistently:**
- Generate unique, traceable IDs
- Include timestamp and strategy identifier
- Store mapping for reconciliation

```bash
# Good: Descriptive, unique client_order_id
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "client_order_id": "momentum-btc-1708704000000",
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.01,
    "price": 50000.0
  }'
```

**Handle order states properly:**

| State | Action |
|-------|--------|
| `NEW` | Order accepted, wait for fill |
| `PARTIALLY_FILLED` | Update position tracking |
| `FILLED` | Complete position update |
| `CANCELED` | Remove from active orders |
| `REJECTED` | Log reason, alert if unexpected |

**Implement idempotency:**
- Use unique client_order_id for each order intent
- Check for existing orders before placing new ones
- Handle duplicate responses gracefully

### Position Management

**Track positions accurately:**
- Reconcile with exchange state regularly
- Handle partial fills correctly
- Account for fees in P&L calculations

**Avoid over-leveraging:**
- Start with low leverage (1-2x)
- Increase gradually based on performance
- Never exceed exchange maximum

**Size positions appropriately:**

```
Position Size = (Account Balance * Risk Per Trade) / Stop Loss Distance
```

Example:
- Account: $10,000
- Risk per trade: 2% ($200)
- Stop loss: 5% from entry
- Position size: $200 / 0.05 = $4,000

### Order Execution

**Use limit orders for better execution:**
- Avoid market orders in volatile conditions
- Set reasonable price limits
- Consider using post-only orders to avoid taker fees

**Handle rate limits:**
- Implement exponential backoff on rate limit errors
- Batch orders when possible
- Monitor rate limit headers in responses

**Verify fills:**
- Confirm fill prices match expectations
- Check for slippage on large orders
- Alert on significant price deviation

---

## Risk Management Best Practices

### Position Limits

**Set conservative limits initially:**

```json
{
  "risk_limits": {
    "max_position_size": 0.5,
    "max_order_size": 0.1,
    "max_leverage": 2.0,
    "max_concentration_pct": 30.0
  }
}
```

**Adjust based on experience:**
- Review limits monthly
- Increase gradually after consistent performance
- Reduce during high volatility periods

### Stop-Loss Configuration

**Always use stop-losses:**
- Set stop-loss before entering positions
- Use percentage-based stops (2-5% typical)
- Consider volatility-adjusted stops

```bash
# Enable stop-loss protection
export VELOZ_STOP_LOSS_ENABLED=true
export VELOZ_STOP_LOSS_PCT=0.03  # 3% stop-loss
```

**Avoid common mistakes:**
- Don't set stops too tight (normal volatility will trigger)
- Don't set stops too wide (defeats the purpose)
- Don't move stops further from entry (only tighten)

### Drawdown Management

**Set maximum drawdown limits:**

| Level | Drawdown | Action |
|-------|----------|--------|
| Warning | 5% | Review positions |
| Caution | 10% | Reduce position sizes |
| Critical | 15% | Halt new trades |
| Emergency | 20% | Close all positions |

**Implement circuit breakers:**

```bash
export VELOZ_CIRCUIT_BREAKER_ENABLED=true
export VELOZ_MAX_DRAWDOWN_PCT=0.15
export VELOZ_MAX_DAILY_LOSS=1000
```

### Diversification

**Spread risk across assets:**
- Trade multiple uncorrelated pairs
- Limit single-asset concentration to 30%
- Monitor correlation between positions

**Use multiple strategies:**
- Combine trend-following and mean-reversion
- Allocate risk budget per strategy
- Rebalance periodically

---

## Performance Best Practices

### System Resources

**Allocate sufficient resources:**

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 2 cores | 4+ cores |
| RAM | 4 GB | 8+ GB |
| Disk | 20 GB SSD | 100+ GB NVMe |
| Network | 100 Mbps | 1 Gbps |

**Optimize for latency:**
- Deploy close to exchange servers
- Use dedicated network connections
- Minimize network hops

### Build Configuration

**Use release builds in production:**

```bash
# Development (debug symbols, slower)
cmake --preset dev
cmake --build --preset dev-all

# Production (optimized, faster)
cmake --preset release
cmake --build --preset release-all
```

**Enable compiler optimizations:**
- Use `-O3` optimization level
- Enable link-time optimization (LTO)
- Use native CPU instructions when possible

### Connection Management

**Maintain stable connections:**
- Implement automatic reconnection
- Use connection pooling for HTTP
- Monitor connection health

**Handle disconnections gracefully:**
- Queue orders during brief disconnections
- Cancel pending orders on extended outages
- Alert on connection failures

### Memory Management

**Monitor memory usage:**
- Set memory limits for the process
- Implement memory leak detection in testing
- Use memory-efficient data structures

**Avoid memory issues:**
- Clear old order history periodically
- Limit in-memory trade history
- Use disk-based storage for historical data

---

## Monitoring Best Practices

### Health Checks

**Implement comprehensive health checks:**

```bash
# Check all components
curl http://127.0.0.1:8080/api/health
```

**Monitor critical metrics:**

| Metric | Warning Threshold | Critical Threshold |
|--------|-------------------|-------------------|
| Latency | > 100ms | > 500ms |
| Error rate | > 1% | > 5% |
| Memory usage | > 70% | > 90% |
| CPU usage | > 70% | > 90% |

### Alerting

**Configure alerts for critical events:**
- Order rejections
- Connection failures
- Risk threshold breaches
- System errors

**Use multiple notification channels:**
- Email for non-urgent alerts
- SMS/Slack for critical alerts
- PagerDuty for emergencies

```bash
export VELOZ_ALERT_WEBHOOK_URL=https://hooks.slack.com/services/xxx
export VELOZ_ALERT_EMAIL=alerts@yourdomain.com
```

### Logging

**Configure appropriate log levels:**

| Environment | Log Level | Retention |
|-------------|-----------|-----------|
| Development | DEBUG | 1 day |
| Staging | INFO | 7 days |
| Production | INFO | 30 days |

**Structure logs for analysis:**
- Use JSON format for machine parsing
- Include correlation IDs
- Add relevant context (order ID, symbol)

### Dashboards

**Create monitoring dashboards for:**
- System health overview
- Trading performance metrics
- Risk metrics and alerts
- Order flow visualization

**Review dashboards regularly:**
- Daily: Quick health check
- Weekly: Performance review
- Monthly: Trend analysis

---

## Backup and Recovery

### Data Backup

**Back up critical data:**
- Configuration files
- API keys (encrypted)
- Trade history
- Strategy parameters

**Backup schedule:**

| Data Type | Frequency | Retention |
|-----------|-----------|-----------|
| Configuration | On change | 30 versions |
| Trade history | Daily | 1 year |
| Logs | Daily | 90 days |
| Database | Hourly | 7 days |

### Disaster Recovery

**Document recovery procedures:**
1. Identify failure scenario
2. Assess impact
3. Execute recovery steps
4. Verify system health
5. Post-incident review

**Test recovery regularly:**
- Monthly: Configuration restore
- Quarterly: Full system recovery
- Annually: Disaster simulation

### Failover Configuration

**Implement redundancy:**
- Multiple gateway instances
- Database replication
- Geographic distribution

**Automatic failover:**
- Health check-based failover
- DNS-based load balancing
- Stateless design for easy scaling

### State Recovery

**Handle state after restart:**
1. Load configuration
2. Reconnect to exchanges
3. Reconcile order state
4. Resume trading

```bash
# Trigger reconciliation after restart
curl -X POST http://127.0.0.1:8080/api/reconcile
```

---

## Production Deployment Checklist

Use this checklist before deploying to production.

### Pre-Deployment

- [ ] **Security**
  - [ ] Authentication enabled
  - [ ] Strong JWT secret configured
  - [ ] API keys stored securely
  - [ ] HTTPS configured
  - [ ] Firewall rules in place

- [ ] **Configuration**
  - [ ] Production API endpoints configured
  - [ ] Correct exchange credentials
  - [ ] Risk limits set appropriately
  - [ ] Logging configured
  - [ ] Audit trail enabled

- [ ] **Testing**
  - [ ] All unit tests passing
  - [ ] Integration tests completed
  - [ ] Testnet trading verified
  - [ ] Failover tested
  - [ ] Load testing completed

### Deployment

- [ ] **Infrastructure**
  - [ ] Sufficient resources allocated
  - [ ] Monitoring agents installed
  - [ ] Log aggregation configured
  - [ ] Backup system ready
  - [ ] Alerting configured

- [ ] **Application**
  - [ ] Release build deployed
  - [ ] Configuration verified
  - [ ] Health checks passing
  - [ ] Connections established
  - [ ] Initial reconciliation complete

### Post-Deployment

- [ ] **Verification**
  - [ ] Place test order (small size)
  - [ ] Verify order fills correctly
  - [ ] Check position tracking
  - [ ] Confirm risk limits enforced
  - [ ] Verify alerts working

- [ ] **Documentation**
  - [ ] Deployment documented
  - [ ] Runbook updated
  - [ ] Team notified
  - [ ] On-call schedule confirmed

### Ongoing Operations

- [ ] **Daily**
  - [ ] Review system health
  - [ ] Check for alerts
  - [ ] Verify positions reconciled

- [ ] **Weekly**
  - [ ] Review trading performance
  - [ ] Check log errors
  - [ ] Verify backups successful

- [ ] **Monthly**
  - [ ] Rotate API keys
  - [ ] Review risk limits
  - [ ] Update dependencies
  - [ ] Test recovery procedures

---

## Summary

Following these best practices helps ensure:

| Area | Benefit |
|------|---------|
| Security | Protect credentials and prevent unauthorized access |
| Trading | Execute orders reliably and manage positions accurately |
| Risk | Limit losses and protect capital |
| Performance | Maintain low latency and high availability |
| Monitoring | Detect issues early and respond quickly |
| Backup | Recover from failures with minimal data loss |

Remember: **Start conservative, monitor closely, and adjust gradually.**

---

## Related Documentation

- [Configuration Guide](configuration.md) - Environment variables and settings
- [Monitoring Guide](monitoring.md) - Prometheus, Grafana, and alerting setup
- [Risk Management Guide](risk-management.md) - VaR, stress testing, and circuit breakers
- [Troubleshooting Guide](troubleshooting.md) - Diagnosing and resolving common issues
- [Glossary](glossary.md) - Definitions of trading and technical terms
