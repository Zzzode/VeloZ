# Deployment Documentation

This directory contains documentation for deploying VeloZ in production environments.

> **Status**: VeloZ is **PRODUCTION READY** - All 7 phases complete (33/33 tasks)

## Documents

### Architecture & Setup

| Document | Description |
|----------|-------------|
| [Production Architecture](production_architecture.md) | System architecture and deployment topology |
| [Monitoring](monitoring.md) | Observability, metrics, and alerting setup |
| [CI/CD Pipeline](ci_cd.md) | Continuous integration and deployment |

### Operations & Runbooks

| Document | Description |
|----------|-------------|
| [Operations Runbook](operations_runbook.md) | Day-to-day operational procedures |
| [On-Call Handbook](oncall_handbook.md) | Guide for on-call engineers |
| [Incident Response](incident_response.md) | Incident management procedures |
| [Troubleshooting](troubleshooting.md) | Common issues and solutions |

### Disaster Recovery

| Document | Description |
|----------|-------------|
| [Backup & Recovery](backup_recovery.md) | Backup procedures and recovery |
| [DR Runbook](dr_runbook.md) | Disaster recovery scenarios and procedures |

## Quick Links

- [Installation Guide](../user/installation.md) - Initial setup
- [Configuration Guide](../user/configuration.md) - Environment variables
- [Docker Setup](#docker-deployment) - Container deployment

---

## Service Mode Configuration

VeloZ engine supports two operational modes:

### Stdio Mode (Development/Gateway Integration)

The engine communicates via stdin/stdout with the Python gateway:

```bash
./build/release/apps/engine/veloz_engine --stdio
```

### Service Mode (Production)

The engine runs as a standalone HTTP service:

```bash
./build/release/apps/engine/veloz_engine --port 8080
```

Service mode features:
- Direct HTTP API access (no gateway required)
- Built-in strategy management
- Market data WebSocket integration
- Graceful shutdown on SIGINT/SIGTERM

---

## Environment Variables

### Core Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_PRESET` | `dev` | Build preset: `dev`, `release`, `asan` |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_LOG_LEVEL` | `info` | Log level: `debug`, `info`, `warn`, `error` |

### Market Data Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_MARKET_SOURCE` | `sim` | Market source: `sim`, `binance_rest`, `binance_ws` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Default trading symbol |
| `VELOZ_BINANCE_BASE_URL` | `https://api.binance.com` | Binance REST API base URL |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://stream.binance.com:9443/ws` | Binance WebSocket URL |
| `VELOZ_WS_RECONNECT_DELAY` | `5000` | WebSocket reconnect delay (ms) |

### Execution Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode: `sim_engine`, `binance_testnet_spot`, `binance_spot` |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://api.binance.com` | Binance trading API URL |
| `VELOZ_BINANCE_API_KEY` | - | Binance API key (required for live trading) |
| `VELOZ_BINANCE_API_SECRET` | - | Binance API secret (required for live trading) |

### Performance Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WORKER_THREADS` | `4` | Number of worker threads |
| `VELOZ_MEMORY_POOL_SIZE` | `1073741824` | Memory pool size in bytes (1GB) |
| `VELOZ_ORDER_RATE_LIMIT` | `10` | Maximum orders per second |

### WAL Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_WAL_DIR` | `./data/wal` | WAL directory path |
| `VELOZ_WAL_MAX_SIZE` | `67108864` | Max WAL file size (64MB) |
| `VELOZ_WAL_MAX_FILES` | `10` | Max WAL files to retain |
| `VELOZ_WAL_SYNC` | `true` | Sync to disk on each write |
| `VELOZ_WAL_CHECKPOINT_INTERVAL` | `1000` | Entries between checkpoints |

---

## Deployment Options

### 1. Local Development

```bash
./scripts/run_gateway.sh dev
```

### 2. Docker Deployment

```bash
docker-compose up -d
```

### 3. Kubernetes Deployment

See [Production Architecture](production_architecture.md) for Kubernetes manifests.

---

## Production Deployment Checklist

### Pre-Deployment

- [ ] **Build Release Binaries**
  ```bash
  cmake --preset release
  cmake --build --preset release-all -j$(nproc)
  ```

- [ ] **Run All Tests**
  ```bash
  ctest --preset release -j$(nproc)
  ```

- [ ] **Configure Environment Variables**
  - Set `VELOZ_EXECUTION_MODE` for target exchange
  - Configure API credentials securely
  - Set appropriate log level

- [ ] **Set Up TLS/SSL**
  - Configure nginx/HAProxy with TLS certificates
  - Enable HTTPS for all API endpoints

- [ ] **Configure Secrets Management**
  - Store API keys in HashiCorp Vault or Kubernetes Secrets
  - Never commit secrets to version control

### WAL and Persistence

- [ ] **Configure WAL Directory**
  - Ensure WAL directory exists and is writable
  - Use fast storage (SSD recommended)
  - Set appropriate `VELOZ_WAL_MAX_SIZE` for your workload

- [ ] **Set Up WAL Backup**
  - Configure continuous WAL backup (see [Backup & Recovery](backup_recovery.md))
  - Test WAL recovery procedure
  - Set up S3/GCS backup for disaster recovery

- [ ] **Verify Recovery**
  ```bash
  # Stop engine
  systemctl stop veloz

  # Verify WAL files exist
  ls -la /var/lib/veloz/wal/

  # Start engine (will replay WAL)
  systemctl start veloz

  # Verify order state recovered
  curl http://localhost:8080/api/orders_state
  ```

### Monitoring Setup

- [ ] **Configure Prometheus Scraping**
  - Add VeloZ to Prometheus targets
  - Verify metrics at `/metrics` endpoint

- [ ] **Set Up Grafana Dashboards**
  - Import trading dashboard
  - Import system dashboard

- [ ] **Configure Alerting**
  - Set up alerts for high latency
  - Set up alerts for WebSocket disconnection
  - Set up alerts for high error rate

- [ ] **Set Up Log Aggregation**
  - Configure Loki/Promtail for log collection
  - Set up log retention policies

### Security

- [ ] **Network Security**
  - Configure firewall rules
  - Restrict database/cache access to internal network
  - Enable rate limiting

- [ ] **API Security**
  - Generate and configure API keys
  - Set up IP allowlisting if required

### High Availability

- [ ] **Load Balancer Configuration**
  - Configure health checks
  - Set up failover rules

- [ ] **Database Replication**
  - Set up PostgreSQL streaming replication
  - Configure automatic failover

### Post-Deployment

- [ ] **Verify Health Checks**
  ```bash
  curl http://localhost:8080/health
  ```

- [ ] **Test Order Flow**
  - Place test order in simulation mode
  - Verify order state updates

- [ ] **Verify Metrics Collection**
  ```bash
  curl http://localhost:8080/metrics
  ```

- [ ] **Document Runbooks**
  - Create incident response procedures
  - Document common troubleshooting steps

- [ ] **Schedule Backup Verification**
  - Monthly backup restore tests
  - WAL recovery drills

---

## WAL Backup and Recovery

The Order Write-Ahead Log (WAL) provides durability and crash recovery for order state.

### WAL File Structure

```
data/wal/
├── orders_0000000000000001.wal   # Completed WAL segment
├── orders_0000000000000002.wal   # Completed WAL segment
└── orders_current.wal            # Active WAL file
```

### WAL Entry Types

| Type | Description |
|------|-------------|
| `OrderNew` | New order placed |
| `OrderUpdate` | Order status update |
| `OrderFill` | Order fill/execution |
| `OrderCancel` | Order cancellation |
| `Checkpoint` | Full state checkpoint |
| `Rotation` | WAL rotation marker |

### Backup Strategy

1. **Continuous Backup** (every 5 minutes)
   ```bash
   */5 * * * * /opt/veloz/scripts/backup_wal.sh
   ```

2. **Checkpoint Interval**: Every 1000 entries (configurable)

3. **Retention**: 7 days of WAL files

### Recovery Procedure

1. **Stop the Engine**
   ```bash
   systemctl stop veloz
   ```

2. **Restore WAL Files** (if needed)
   ```bash
   rsync -av /backup/wal/ /var/lib/veloz/wal/
   ```

3. **Start the Engine**
   ```bash
   systemctl start veloz
   ```

4. **Verify Recovery**
   ```bash
   curl http://localhost:8080/api/orders_state
   ```

The engine automatically replays WAL entries on startup to reconstruct order state.

### WAL Health Monitoring

Monitor WAL health via metrics:
- `veloz_wal_entries_written` - Total entries written
- `veloz_wal_bytes_written` - Total bytes written
- `veloz_wal_rotations` - Number of file rotations
- `veloz_wal_corrupted_entries` - Corrupted entries detected

---

## Quick Start (Production)

```bash
# 1. Build release
cmake --preset release && cmake --build --preset release-all -j$(nproc)

# 2. Configure environment
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=your_key
export VELOZ_BINANCE_API_SECRET=your_secret
export VELOZ_WAL_DIR=/var/lib/veloz/wal
export VELOZ_LOG_LEVEL=info

# 3. Start service
./build/release/apps/engine/veloz_engine --port 8080

# 4. Verify health
curl http://localhost:8080/health
```

---

## Related Documents

- [Production Architecture](production_architecture.md) - Detailed architecture
- [Monitoring](monitoring.md) - Metrics and alerting
- [Backup & Recovery](backup_recovery.md) - Disaster recovery
- [Troubleshooting](troubleshooting.md) - Common issues
