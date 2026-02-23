# Quick Start Guide

Get VeloZ v1.0 running in 5 minutes.

**Time:** 5 minutes | **Level:** Beginner

> **VeloZ v1.0** is production-ready with multi-exchange trading, smart order routing, VaR risk management, and complete observability.

---

## Prerequisites Check

Before starting, verify you have:

| Requirement | Check Command | Minimum Version |
|-------------|---------------|-----------------|
| CMake | `cmake --version` | 3.24+ |
| Clang/GCC | `clang++ --version` | Clang 16+ / GCC 13+ |
| Python | `python3 --version` | 3.8+ |
| Git | `git --version` | Any |

```bash
# Quick check all prerequisites
cmake --version && clang++ --version && python3 --version && git --version
```

If any are missing, see [Installation Guide](installation.md) for setup instructions.

---

## Step 1: Clone and Build

```bash
# Clone the repository
git clone https://github.com/Zzzode/VeloZ.git
cd VeloZ

# Configure and build
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

**Expected Output:**
```
[100%] Built target veloz_engine
[100%] Built target veloz_market_tests
...
```

Build takes 2-3 minutes on first run.

> **Issue?** If build fails, run `cmake --preset dev` again to see detailed errors.

---

## Step 2: Run Tests

Verify the build is working:

```bash
ctest --preset dev -j$(nproc)
```

**Expected Output:**
```
100% tests passed, 0 tests failed out of 615
```

> **Issue?** If tests fail, check [Troubleshooting](troubleshooting.md#startup-issues).

---

## Step 3: Start the Gateway

```bash
./scripts/run_gateway.sh dev
```

**Expected Output:**
```
Starting VeloZ Gateway...
Engine started in stdio mode
Gateway listening on http://127.0.0.1:8080
```

Keep this terminal open. The gateway must remain running.

> **Issue?** "Address already in use" - Another process is using port 8080. Run `lsof -i :8080` to find it.

---

## Step 4: Access the UI

Open your browser to:

```
http://127.0.0.1:8080/
```

You should see the VeloZ trading dashboard with:
- Market data panel
- Order entry form
- Position display
- Order history

> **Issue?** Page not loading? Verify gateway is running and check browser console for errors.

---

## Step 5: Verify System Health

Open a new terminal and check system status:

```bash
curl http://127.0.0.1:8080/api/health
```

**Expected Output:**
```json
{
  "status": "healthy",
  "engine": "running",
  "uptime_seconds": 30,
  "version": "1.0.0"
}
```

---

## Step 6: Place Your First Order

Place a test order via the API:

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

**Expected Output:**
```json
{
  "ok": true,
  "client_order_id": "web-1708704000000",
  "venue_order_id": "sim-12345678"
}
```

In simulation mode, orders fill immediately.

---

## Step 7: View Results

Check your order status:

```bash
curl http://127.0.0.1:8080/api/orders_state
```

**Expected Output:**
```json
{
  "orders": [
    {
      "client_order_id": "web-1708704000000",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "status": "FILLED",
      "qty": 0.001,
      "filled_qty": 0.001
    }
  ]
}
```

Check your position:

```bash
curl http://127.0.0.1:8080/api/positions
```

**Expected Output:**
```json
{
  "positions": [
    {
      "symbol": "BTCUSDT",
      "size": 0.001,
      "entry_price": 50000.0,
      "unrealized_pnl": 0.0
    }
  ]
}
```

---

## Quick Reference

| Action | Command |
|--------|---------|
| Start gateway | `./scripts/run_gateway.sh dev` |
| Check health | `curl http://127.0.0.1:8080/api/health` |
| Place order | `curl -X POST http://127.0.0.1:8080/api/order -H "Content-Type: application/json" -d '{...}'` |
| View orders | `curl http://127.0.0.1:8080/api/orders_state` |
| View positions | `curl http://127.0.0.1:8080/api/positions` |
| Stop gateway | `Ctrl+C` in gateway terminal |

---

## Common Issues

| Problem | Solution |
|---------|----------|
| Build fails | Run `cmake --preset dev` to see errors |
| Port 8080 in use | `lsof -i :8080` then `kill <PID>` |
| Connection refused | Ensure gateway is running |
| Order rejected | Check error message in response |

---

## Next Steps

Now that VeloZ is running, explore these resources:

### Tutorials
- [Your First Trade](../../tutorials/first-trade.md) - Detailed trading walkthrough
- [Production Deployment](../../tutorials/production-deployment.md) - Deploy to production
- [Custom Strategy Development](../../tutorials/custom-strategy-development.md) - Build your own strategies

### User Guides
- [Getting Started](getting-started.md) - Comprehensive setup guide
- [Trading Guide](trading-guide.md) - Order management, positions, execution algorithms
- [API Usage Guide](api-usage-guide.md) - Practical API examples with Python client
- [Configuration Guide](configuration.md) - All configuration options
- [Risk Management](risk-management.md) - VaR, stress testing, circuit breakers
- [Monitoring Guide](monitoring.md) - Prometheus, Grafana, alerting

### Reference
- [HTTP API Reference](../../api/http_api.md) - Complete API documentation
- [Best Practices](best-practices.md) - Security, trading, and operational best practices
- [Troubleshooting](troubleshooting.md) - Common issues and solutions

---

## Summary

You have successfully:

1. Built VeloZ from source
2. Started the gateway in simulation mode
3. Accessed the web UI
4. Placed and verified a test order

VeloZ is now ready for development and testing. For production use, see [Best Practices](best-practices.md).
