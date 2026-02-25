# Troubleshooting Guide

This guide helps you diagnose and resolve common issues with VeloZ.

## Table of Contents

1. [Quick Reference](#quick-reference)
2. [Startup Issues](#startup-issues)
3. [Connection Problems](#connection-problems)
4. [Order and Trading Issues](#order-and-trading-issues)
5. [Data Issues](#data-issues)
6. [Performance Issues](#performance-issues)
7. [Authentication Issues](#authentication-issues)
8. [Debug Mode](#debug-mode)
9. [Log Analysis](#log-analysis)
10. [When to Seek Help](#when-to-seek-help)

---

## Quick Reference

| Symptom | Likely Cause | Quick Fix |
|---------|--------------|-----------|
| Gateway won't start | Port in use | `lsof -i :8080` and kill process |
| Connection refused | Gateway not running | Start gateway first |
| Order rejected | Insufficient balance | Check account balance |
| No market data | Feed disconnected | Check exchange connection |
| High latency | Network issues | Check connectivity |
| Authentication failed | Invalid credentials | Verify API keys |
| Circuit breaker open | Risk limit exceeded | Review and reset |

---

## Startup Issues

### Gateway fails to start

**Symptom:** Error message when running `./scripts/run_gateway.sh`

#### Port already in use

**Error:**
```
OSError: [Errno 48] Address already in use
```

**Diagnosis:**
```bash
# Find process using port 8080
lsof -i :8080
```

**Solution:**
```bash
# Kill the process using the port
kill <PID>

# Or use a different port
export VELOZ_PORT=8081
./scripts/run_gateway.sh dev
```

#### Engine binary not found

**Error:**
```
FileNotFoundError: Engine binary not found at build/dev/apps/engine/veloz_engine
```

**Diagnosis:**
```bash
# Check if binary exists
ls -la build/dev/apps/engine/veloz_engine
```

**Solution:**
```bash
# Rebuild the project
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

#### Missing dependencies

**Error:**
```
ImportError: No module named 'aiohttp'
```

**Solution:**
```bash
# Install Python dependencies
pip install -r apps/gateway/requirements.txt
```

### Engine crashes on startup

**Symptom:** Engine starts but immediately exits

**Diagnosis:**
```bash
# Run engine directly to see error
./build/dev/apps/engine/veloz_engine --stdio
```

**Common causes:**
- Invalid configuration file
- Missing environment variables
- Corrupted build

**Solution:**
```bash
# Clean rebuild
rm -rf build/dev
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

---

## Connection Problems

### Cannot connect to gateway

**Symptom:** `curl: (7) Failed to connect to 127.0.0.1 port 8080`

**Diagnosis:**
1. Check if gateway is running
2. Verify correct port
3. Check firewall settings

**Solution:**
```bash
# Verify gateway is running
ps aux | grep gateway

# Check if port is listening
netstat -an | grep 8080

# Test local connection
curl http://127.0.0.1:8080/api/health
```

### Exchange connection failed

**Symptom:** Exchange status shows "disconnected"

**Diagnosis:**
```bash
# Check exchange status
curl http://127.0.0.1:8080/api/health

# Test exchange connectivity directly
curl https://api.binance.com/api/v3/ping
```

**Common causes:**
- Invalid API credentials
- Network connectivity issues
- Exchange maintenance
- IP not whitelisted

**Solution:**
```bash
# Verify credentials are set
echo $VELOZ_BINANCE_API_KEY

# Test with correct endpoint
export VELOZ_BINANCE_BASE_URL=https://api.binance.com
```

### WebSocket disconnections

**Symptom:** Market data stops updating, SSE stream disconnects

**Diagnosis:**
```bash
# Check connection status
curl http://127.0.0.1:8080/api/health

# Monitor WebSocket reconnection in logs
tail -f /var/log/veloz/gateway.log | grep -i websocket
```

**Solution:**
- Check network stability
- Verify firewall allows WebSocket connections
- Increase timeout settings

```bash
export VELOZ_WS_TIMEOUT=30000
export VELOZ_WS_RECONNECT_DELAY=5000
```

### SSL/TLS errors

**Symptom:** SSL certificate verification failed

**Error:**
```
ssl.SSLCertVerificationError: certificate verify failed
```

**Solution:**
```bash
# Update CA certificates
# macOS
brew install ca-certificates

# Ubuntu
sudo apt update && sudo apt install ca-certificates

# Verify SSL works
curl -v https://api.binance.com/api/v3/ping
```

---

## Order and Trading Issues

### Order rejected

**Symptom:** API returns `{"ok": false, "error": "..."}`

#### Insufficient balance

**Error:**
```json
{"ok": false, "error": "insufficient_balance"}
```

**Diagnosis:**
```bash
# Check account balance
curl http://127.0.0.1:8080/api/account
```

**Solution:**
- Deposit funds to exchange account
- Reduce order size
- Check if funds are locked in open orders

#### Order size exceeded

**Error:**
```json
{"ok": false, "error": "order_size_exceeded", "max_allowed": 0.1}
```

**Solution:**
```bash
# Check current limits
curl http://127.0.0.1:8080/api/risk/config

# Adjust limits if appropriate
curl -X POST http://127.0.0.1:8080/api/risk/config \
  -H "Content-Type: application/json" \
  -d '{"max_order_size": 0.5}'
```

#### Price deviation

**Error:**
```json
{"ok": false, "error": "price_deviation_exceeded"}
```

**Diagnosis:** Order price is too far from current market price

**Solution:**
- Use a price closer to market
- Adjust price deviation limit

```bash
export VELOZ_MAX_PRICE_DEVIATION=0.10  # 10%
```

### Order stuck in pending

**Symptom:** Order shows "NEW" status but never fills

**Diagnosis:**
```bash
# Check order state
curl "http://127.0.0.1:8080/api/order_state?client_order_id=YOUR_ORDER_ID"

# Check market price
curl http://127.0.0.1:8080/api/market
```

**Common causes:**
- Limit price too far from market
- Low liquidity
- Exchange issues

**Solution:**
- Cancel and resubmit at market price
- Use market order if available
- Check exchange order book

### Orders not syncing

**Symptom:** Local order state differs from exchange

**Diagnosis:**
```bash
# Trigger reconciliation
curl -X POST http://127.0.0.1:8080/api/reconcile

# Check for discrepancies
curl http://127.0.0.1:8080/api/reconcile/discrepancies
```

**Solution:**
```bash
# Force full reconciliation
curl -X POST http://127.0.0.1:8080/api/reconcile?force=true
```

### Circuit breaker blocking orders

**Symptom:** All orders rejected, circuit breaker is open

**Diagnosis:**
```bash
# Check circuit breaker status
curl http://127.0.0.1:8080/api/risk/circuit-breaker
```

**Solution:**
1. Understand why it triggered (check logs)
2. Address the underlying issue
3. Reset if appropriate

```bash
curl -X POST http://127.0.0.1:8080/api/risk/circuit-breaker/reset
```

---

## Data Issues

### No market data

**Symptom:** Market endpoint returns empty or stale data

**Diagnosis:**
```bash
# Check market data
curl http://127.0.0.1:8080/api/market

# Check data source configuration
curl http://127.0.0.1:8080/api/config
```

**Common causes:**
- Market feed disconnected
- Wrong symbol configured
- Exchange API issues

**Solution:**
```bash
# Verify market source
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT

# Restart gateway
./scripts/run_gateway.sh dev
```

### Stale prices

**Symptom:** Timestamps are old, prices not updating

**Diagnosis:**
```bash
# Check timestamp in market data
curl http://127.0.0.1:8080/api/market | jq '.timestamp'

# Compare with current time
date +%s000
```

**Solution:**
- Check WebSocket connection
- Verify exchange is operational
- Restart market data feed

### Position mismatch

**Symptom:** Local positions don't match exchange

**Diagnosis:**
```bash
# Get local positions
curl http://127.0.0.1:8080/api/positions

# Trigger reconciliation
curl -X POST http://127.0.0.1:8080/api/reconcile
```

**Solution:**
- Run reconciliation
- Check for missed fill events
- Review trade history

### Backtest data errors

**Symptom:** Backtest fails with data errors

**Error:**
```
DataError: Invalid data format in file
```

**Diagnosis:**
```bash
# Check data file format
head -5 data/btcusdt_1h.csv

# Verify CSV structure
wc -l data/btcusdt_1h.csv
```

**Solution:**
- Ensure correct CSV format (timestamp, open, high, low, close, volume)
- Check for missing values
- Verify timestamp format (Unix milliseconds)

---

## Performance Issues

### High latency

**Symptom:** Order execution takes longer than expected

**Diagnosis:**
```bash
# Check system stats
curl http://127.0.0.1:8080/api/stats

# Monitor latency
curl http://127.0.0.1:8080/api/stats | jq '.performance'
```

**Common causes:**
- Network congestion
- High system load
- Exchange rate limiting

**Solution:**
- Check network connectivity to exchange
- Reduce order frequency
- Use release build for production

```bash
cmake --preset release
cmake --build --preset release-all -j$(nproc)
```

### High memory usage

**Symptom:** Process using excessive memory

**Diagnosis:**
```bash
# Check memory usage
ps aux | grep veloz

# Monitor over time
top -p $(pgrep veloz_engine)
```

**Solution:**
- Clear old order history
- Limit in-memory data
- Restart periodically if needed

```bash
# Clear old orders (if API available)
curl -X POST http://127.0.0.1:8080/api/maintenance/clear-history?older_than=7d
```

### High CPU usage

**Symptom:** CPU constantly at high utilization

**Diagnosis:**
```bash
# Check CPU usage
top -p $(pgrep veloz_engine)

# Profile if needed
perf top -p $(pgrep veloz_engine)
```

**Common causes:**
- Tight polling loops
- Excessive logging
- Debug build in production

**Solution:**
- Use release build
- Reduce log verbosity
- Check for runaway processes

### Slow backtest

**Symptom:** Backtest takes much longer than expected

**Diagnosis:**
```bash
# Check backtest progress
curl "http://127.0.0.1:8080/api/backtest/status?id=YOUR_BACKTEST_ID"
```

**Solution:**
- Use release build
- Reduce data size for initial testing
- Use parallel optimization

---

## Authentication Issues

### Login failed

**Symptom:** Cannot authenticate with gateway

**Error:**
```json
{"error": "invalid_credentials"}
```

**Diagnosis:**
- Verify username and password
- Check if authentication is enabled

**Solution:**
```bash
# Verify auth configuration
echo $VELOZ_AUTH_ENABLED
echo $VELOZ_ADMIN_PASSWORD

# Test login
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"YOUR_PASSWORD"}'
```

### Token expired

**Symptom:** API returns 401 Unauthorized

**Error:**
```json
{"error": "token_expired"}
```

**Solution:**
```bash
# Refresh token
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"YOUR_REFRESH_TOKEN"}'
```

### API key invalid

**Symptom:** Exchange API returns authentication error

**Error:**
```json
{"code": -2015, "msg": "Invalid API-key, IP, or permissions for action"}
```

**Diagnosis:**
- Verify API key is correct
- Check IP whitelist on exchange
- Verify permissions

**Solution:**
```bash
# Verify key is set correctly
echo $VELOZ_BINANCE_API_KEY | head -c 10

# Test with exchange directly
curl -H "X-MBX-APIKEY: $VELOZ_BINANCE_API_KEY" \
  https://api.binance.com/api/v3/account
```

---

## Debug Mode

### Enable debug logging

```bash
# Set debug log level
export VELOZ_LOG_LEVEL=debug

# Start gateway with debug output
./scripts/run_gateway.sh dev 2>&1 | tee debug.log
```

### Engine debug mode

```bash
# Run engine with verbose output
./build/dev/apps/engine/veloz_engine --stdio --verbose
```

### Network debugging

```bash
# Trace HTTP requests
curl -v http://127.0.0.1:8080/api/health

# Monitor network traffic
tcpdump -i any port 8080 -w capture.pcap
```

### Memory debugging

```bash
# Build with AddressSanitizer
cmake --preset asan
cmake --build --preset asan-all -j$(nproc)

# Run with sanitizer
./build/asan/apps/engine/veloz_engine --stdio
```

---

## Log Analysis

### Log locations

| Component | Default Location |
|-----------|------------------|
| Gateway | stdout / `gateway.log` |
| Engine | stdout / `engine.log` |
| Audit | `audit.log` |

### Common log patterns

**Successful order:**
```
INFO: Order placed: client_order_id=web-123, symbol=BTCUSDT, side=BUY
INFO: Order filled: client_order_id=web-123, filled_qty=0.001
```

**Connection issue:**
```
WARNING: WebSocket disconnected, reconnecting...
ERROR: Failed to connect to exchange: Connection refused
INFO: WebSocket reconnected successfully
```

**Risk alert:**
```
WARNING: Drawdown at 8%, approaching limit of 10%
ERROR: Circuit breaker triggered: max_drawdown_exceeded
```

### Filtering logs

```bash
# Find errors
grep -i error gateway.log

# Find specific order
grep "client_order_id=web-123" gateway.log

# Find connection issues
grep -i "connect\|disconnect\|reconnect" gateway.log

# Find risk events
grep -i "risk\|circuit\|drawdown" gateway.log
```

### Log rotation

```bash
# Configure logrotate
cat > /etc/logrotate.d/veloz << EOF
/var/log/veloz/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
}
EOF
```

---

## When to Seek Help

### Before asking for help

1. **Check this guide** - Most common issues are covered
2. **Search existing issues** - Someone may have solved it
3. **Gather information:**
   - VeloZ version
   - Operating system
   - Error messages (full text)
   - Steps to reproduce
   - Relevant logs

### Information to include

```markdown
**Environment:**
- VeloZ version: 1.0.0
- OS: Ubuntu 22.04
- Build type: dev/release

**Problem:**
Brief description of the issue

**Steps to reproduce:**
1. Step one
2. Step two
3. Step three

**Expected behavior:**
What should happen

**Actual behavior:**
What actually happens

**Error messages:**
```
Paste full error message here
```

**Relevant logs:**
```
Paste relevant log entries here
```
```

### Where to get help

| Resource | Use For |
|----------|---------|
| [GitHub Issues](https://github.com/Zzzode/VeloZ/issues) | Bug reports, feature requests |
| Documentation | Reference and guides |
| Logs | Debugging specific issues |

### Emergency procedures

**If trading is affected:**
1. Stop the gateway immediately
2. Cancel all open orders on exchange directly
3. Document the issue
4. Do not restart until issue is understood

```bash
# Emergency stop
pkill -f veloz

# Cancel orders directly on exchange (if needed)
# Use exchange web interface or API directly
```

---

## Summary

Most issues fall into these categories:

| Category | First Step |
|----------|------------|
| Startup | Check build and dependencies |
| Connection | Verify network and credentials |
| Orders | Check balance and limits |
| Data | Verify feed connection |
| Performance | Use release build |
| Auth | Verify credentials |

When in doubt:
1. Check the logs
2. Restart the gateway
3. Run reconciliation
4. Consult this guide

---

## Related Documentation

- [FAQ](faq.md) - Frequently asked questions and quick answers
- [Monitoring Guide](monitoring.md) - Metrics, dashboards, and alerting
- [Best Practices Guide](best-practices.md) - Recommendations for production environments
- [Configuration Guide](configuration.md) - Environment variables and settings
- [Glossary](glossary.md) - Definitions of trading and technical terms
