# Troubleshooting Guide

This guide covers common issues and their solutions when running VeloZ in production.

## Quick Diagnostics

### Health Check

```bash
# Check gateway health
curl http://localhost:8080/health

# Check detailed health
curl http://localhost:8080/health/ready

# Check metrics
curl http://localhost:8080/metrics
```

### Log Analysis

```bash
# View recent logs
journalctl -u veloz -n 100

# Follow logs in real-time
journalctl -u veloz -f

# Search for errors
journalctl -u veloz | grep -i error

# Docker logs
docker-compose logs --tail=100 gateway
```

### Process Status

```bash
# Check process status
systemctl status veloz

# Check resource usage
top -p $(pgrep veloz_engine)

# Check open connections
ss -tlnp | grep 8080

# Check file descriptors
ls -la /proc/$(pgrep veloz_engine)/fd | wc -l
```

## Common Issues

### Gateway Won't Start

#### Symptom
Gateway fails to start or immediately exits.

#### Diagnosis
```bash
# Check logs
journalctl -u veloz -n 50

# Check port availability
ss -tlnp | grep 8080

# Check permissions
ls -la /var/lib/veloz/
```

#### Solutions

**Port already in use:**
```bash
# Find process using port
lsof -i :8080

# Kill the process or use different port
export VELOZ_PORT=8081
```

**Permission denied:**
```bash
# Fix permissions
sudo chown -R veloz:veloz /var/lib/veloz/
sudo chmod 755 /var/lib/veloz/
```

**Missing dependencies:**
```bash
# Check Python dependencies
pip3 install -r requirements.txt

# Check library dependencies
ldd build/release/apps/engine/veloz_engine
```

### Engine Crashes

#### Symptom
Engine process terminates unexpectedly.

#### Diagnosis
```bash
# Check for core dumps
ls -la /var/crash/

# Check system logs
dmesg | tail -50

# Run with ASan
./build/asan/apps/engine/veloz_engine
```

#### Solutions

**Segmentation fault:**
```bash
# Run with debugger
gdb ./veloz_engine
(gdb) run
(gdb) bt  # backtrace on crash
```

**Out of memory:**
```bash
# Check memory usage
free -h

# Increase memory limits
ulimit -v unlimited
```

**Stack overflow:**
```bash
# Increase stack size
ulimit -s 16384
```

### WebSocket Connection Issues

#### Symptom
WebSocket connections fail or disconnect frequently.

#### Diagnosis
```bash
# Check WebSocket metrics
curl http://localhost:8080/metrics | grep websocket

# Check network connectivity
ping api.binance.com
curl https://api.binance.com/api/v3/ping

# Check TLS
openssl s_client -connect stream.binance.com:9443
```

#### Solutions

**Connection refused:**
```bash
# Check firewall
sudo ufw status
sudo ufw allow out 443/tcp
sudo ufw allow out 9443/tcp
```

**TLS handshake failure:**
```bash
# Update CA certificates
sudo apt-get update
sudo apt-get install ca-certificates
sudo update-ca-certificates
```

**Rate limiting:**
```bash
# Check rate limit status
curl -I https://api.binance.com/api/v3/ping

# Implement backoff
export VELOZ_WS_RECONNECT_DELAY=5000
```

### High Latency

#### Symptom
Order processing takes longer than expected.

#### Diagnosis
```bash
# Check latency metrics
curl http://localhost:8080/metrics | grep latency

# Check event loop queue
curl http://localhost:8080/metrics | grep event_loop

# Profile CPU usage
perf top -p $(pgrep veloz_engine)
```

#### Solutions

**Event loop backlog:**
```bash
# Increase worker threads
export VELOZ_WORKER_THREADS=4
```

**Network latency:**
```bash
# Check network path
traceroute api.binance.com

# Use closer endpoint
export VELOZ_BINANCE_BASE_URL=https://api1.binance.com
```

**CPU saturation:**
```bash
# Check CPU usage
mpstat -P ALL 1

# Set CPU affinity
taskset -c 0-3 ./veloz_engine
```

### Memory Issues

#### Symptom
Memory usage grows over time or OOM kills.

#### Diagnosis
```bash
# Check memory usage
ps aux | grep veloz
cat /proc/$(pgrep veloz_engine)/status | grep -i mem

# Check memory pool
curl http://localhost:8080/metrics | grep memory_pool

# Run with ASan
./build/asan/apps/engine/veloz_engine
```

#### Solutions

**Memory leak:**
```bash
# Run with valgrind
valgrind --leak-check=full ./veloz_engine

# Run with ASan
cmake --preset asan
cmake --build --preset asan-all
./build/asan/apps/engine/veloz_engine
```

**Memory fragmentation:**
```bash
# Use jemalloc
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so ./veloz_engine
```

**Pool exhaustion:**
```bash
# Increase pool size
export VELOZ_MEMORY_POOL_SIZE=1073741824  # 1GB
```

### Order Processing Failures

#### Symptom
Orders are rejected or not processed.

#### Diagnosis
```bash
# Check order logs
journalctl -u veloz | grep -i order

# Check order state
curl http://localhost:8080/api/orders_state

# Check Binance API status
curl https://api.binance.com/api/v3/exchangeInfo
```

#### Solutions

**Invalid parameters:**
```bash
# Check order format
curl -X POST http://localhost:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{"side":"BUY","symbol":"BTCUSDT","qty":0.001,"price":50000}'
```

**Insufficient balance:**
```bash
# Check account balance
curl http://localhost:8080/api/account

# Check Binance balance
curl -H "X-MBX-APIKEY: $API_KEY" \
  "https://api.binance.com/api/v3/account"
```

**Rate limiting:**
```bash
# Check rate limit headers
curl -I -X POST https://api.binance.com/api/v3/order/test

# Implement request queuing
export VELOZ_ORDER_RATE_LIMIT=10
```

### Database Connection Issues

#### Symptom
Database queries fail or timeout.

#### Diagnosis
```bash
# Check PostgreSQL status
systemctl status postgresql

# Check connections
psql -c "SELECT count(*) FROM pg_stat_activity;"

# Check logs
tail -f /var/log/postgresql/postgresql-15-main.log
```

#### Solutions

**Connection refused:**
```bash
# Check PostgreSQL is running
systemctl start postgresql

# Check pg_hba.conf
sudo nano /etc/postgresql/15/main/pg_hba.conf
```

**Too many connections:**
```bash
# Increase max connections
sudo nano /etc/postgresql/15/main/postgresql.conf
# max_connections = 200

# Use connection pooling
# Install pgbouncer
sudo apt-get install pgbouncer
```

**Slow queries:**
```bash
# Enable slow query logging
ALTER SYSTEM SET log_min_duration_statement = 1000;
SELECT pg_reload_conf();

# Check query plans
EXPLAIN ANALYZE SELECT * FROM orders WHERE ...;
```

## Recovery Procedures

### Service Recovery

```bash
# Restart service
systemctl restart veloz

# Force restart with clean state
systemctl stop veloz
rm -rf /var/lib/veloz/cache/*
systemctl start veloz
```

### Data Recovery

```bash
# Restore from WAL
./scripts/recover_wal.sh

# Restore from backup
./scripts/recover_database.sh /backup/latest.dump
```

### Emergency Shutdown

```bash
# Graceful shutdown
systemctl stop veloz

# Force shutdown
systemctl kill veloz

# Emergency kill
pkill -9 veloz_engine
```

## Diagnostic Commands

### Network Diagnostics

```bash
# Check DNS resolution
dig api.binance.com

# Check TCP connectivity
nc -zv api.binance.com 443

# Check TLS certificate
echo | openssl s_client -connect api.binance.com:443 2>/dev/null | openssl x509 -noout -dates
```

### Performance Diagnostics

```bash
# CPU profiling
perf record -p $(pgrep veloz_engine) -g -- sleep 30
perf report

# Memory profiling
heaptrack ./veloz_engine
heaptrack_gui heaptrack.veloz_engine.*.gz

# I/O profiling
iotop -p $(pgrep veloz_engine)
```

### System Diagnostics

```bash
# Check system resources
vmstat 1 10
iostat -x 1 10

# Check network stats
netstat -s
ss -s

# Check file system
df -h
lsof -p $(pgrep veloz_engine) | wc -l
```

## Log Analysis

### Common Log Patterns

| Pattern | Meaning | Action |
|---------|---------|--------|
| `WebSocket disconnected` | Connection lost | Check network |
| `Rate limit exceeded` | API throttled | Reduce request rate |
| `Order rejected` | Invalid order | Check parameters |
| `Memory pool exhausted` | Out of memory | Increase pool size |
| `Event loop backlog` | Processing delay | Scale horizontally |

### Log Aggregation Queries

```bash
# Count errors by type
grep -o 'error=[^ ]*' /var/log/veloz/*.log | sort | uniq -c | sort -rn

# Find slow operations
grep 'latency_ms' /var/log/veloz/*.log | awk -F'latency_ms=' '{print $2}' | sort -n | tail -10

# Track WebSocket reconnects
grep 'WebSocket reconnect' /var/log/veloz/*.log | wc -l
```

## Support Resources

### Documentation

- [Installation Guide](../user/installation.md)
- [Configuration Guide](../user/configuration.md)
- [API Reference](../../api/http_api.md)

### Community

- GitHub Issues: https://github.com/your-org/VeloZ/issues
- Discord: https://discord.gg/veloz

### Emergency Contacts

- On-call: ops@example.com
- Escalation: engineering@example.com
