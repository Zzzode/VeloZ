# VeloZ High Availability Guide

This document describes the high availability (HA) architecture and procedures for VeloZ.

## Architecture Overview

VeloZ HA consists of the following components:

```
                    ┌─────────────┐
                    │   HAProxy   │
                    │ (Load Bal.) │
                    └──────┬──────┘
                           │
              ┌────────────┴────────────┐
              │                         │
       ┌──────▼──────┐          ┌──────▼──────┐
       │   Engine 1  │          │   Engine 2  │
       │  (Primary)  │          │  (Standby)  │
       └──────┬──────┘          └──────┬──────┘
              │                         │
       ┌──────▼─────────────────────────▼──────┐
       │              PgBouncer                │
       │         (Connection Pool)             │
       └──────┬─────────────────────────┬──────┘
              │                         │
       ┌──────▼──────┐          ┌──────▼──────┐
       │  PostgreSQL │◄────────►│  PostgreSQL │
       │   Primary   │ Streaming│   Standby   │
       └─────────────┘ Repl.    └─────────────┘

       ┌─────────────┐          ┌─────────────┐
       │    Redis    │◄────────►│    Redis    │
       │   Master    │ Repl.    │   Replica   │
       └──────┬──────┘          └──────┬──────┘
              │                         │
       ┌──────▼─────────────────────────▼──────┐
       │           Redis Sentinel              │
       │      (Automatic Failover)             │
       └───────────────────────────────────────┘
```

## Recovery Objectives

| Metric | Target | Description |
|--------|--------|-------------|
| RTO (Recovery Time Objective) | < 30 seconds | Time to recover from failure |
| RPO (Recovery Point Objective) | 0 | Zero data loss (synchronous replication) |
| Availability | 99.99% | Annual uptime target |

## Components

### PostgreSQL Replication

- **Primary**: Handles all write operations
- **Standby**: Synchronous replica for zero data loss
- **Replication**: Streaming replication with synchronous commit
- **Connection Pooling**: PgBouncer for efficient connection management

Configuration:
```
synchronous_commit = on
synchronous_standby_names = 'veloz_standby'
```

### Redis Sentinel

- **Master**: Primary Redis instance
- **Replica**: Synchronous replica
- **Sentinel**: 3-node quorum for automatic failover
- **Failover Time**: ~5 seconds

Configuration:
```
sentinel monitor veloz-master redis-master 6379 2
sentinel down-after-milliseconds veloz-master 5000
sentinel failover-timeout veloz-master 60000
```

### VeloZ Engine

- **Primary Engine**: Handles all trading operations
- **Standby Engine**: Hot standby, ready to take over
- **State Sync**: Via PostgreSQL and Redis
- **Leader Election**: Coordinated via Redis locks

### HAProxy

- **Load Balancing**: Round-robin with health checks
- **Failover**: Automatic backend failover
- **Health Checks**: HTTP health endpoint monitoring
- **Stats**: Real-time monitoring at port 8404

## Deployment

### Prerequisites

- Docker and Docker Compose
- Minimum 3 nodes for production (for Sentinel quorum)
- Network connectivity between all nodes

### Quick Start

```bash
# Navigate to HA directory
cd infra/ha

# Start all services
docker-compose -f docker-compose.ha.yml up -d

# Check status
./scripts/health_check.sh

# View HAProxy stats
open http://localhost:8404/stats
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_PG_USER` | veloz | PostgreSQL username |
| `VELOZ_PG_PASSWORD` | veloz | PostgreSQL password |
| `VELOZ_PG_DATABASE` | veloz | PostgreSQL database |
| `VELOZ_PG_REPL_USER` | replicator | Replication username |
| `VELOZ_PG_REPL_PASSWORD` | replicator | Replication password |
| `VELOZ_REDIS_PASSWORD` | redis | Redis password |

## Failover Procedures

### Automatic Failover

#### Redis (via Sentinel)

Redis Sentinel automatically handles failover:

1. Sentinel detects master failure (5 seconds)
2. Quorum agrees on failure
3. Sentinel promotes replica to master
4. Clients are redirected to new master

No manual intervention required.

#### VeloZ Engine (via HAProxy)

HAProxy automatically routes traffic:

1. Health check fails on primary engine
2. HAProxy marks backend as DOWN
3. Traffic routed to standby engine
4. Standby becomes active

### Manual Failover

#### PostgreSQL Promotion

```bash
# Check current status
./scripts/failover.sh postgres status

# Promote standby (if primary is down)
./scripts/failover.sh postgres promote
```

#### Redis Failover

```bash
# Check current status
./scripts/failover.sh redis status

# Trigger manual failover
./scripts/failover.sh redis failover
```

## Monitoring

### Health Checks

Run the health check script:

```bash
./scripts/health_check.sh
```

This checks:
- PostgreSQL primary/standby connectivity
- Replication lag
- Redis Sentinel status
- Redis replication
- VeloZ engine health
- HAProxy status

### Metrics

| Metric | Alert Threshold | Description |
|--------|-----------------|-------------|
| `pg_replication_lag_bytes` | > 10MB | Replication lag |
| `redis_connected_slaves` | < 1 | No replicas |
| `haproxy_backend_up` | 0 | All backends down |
| `veloz_engine_health` | unhealthy | Engine not responding |

### HAProxy Stats

Access HAProxy statistics:

```
http://localhost:8404/stats
```

## Troubleshooting

### PostgreSQL Replication Issues

**Symptom**: High replication lag

```bash
# Check replication status
docker exec veloz-postgres-primary psql -U veloz -c "SELECT * FROM pg_stat_replication"

# Check standby status
docker exec veloz-postgres-standby psql -U veloz -c "SELECT pg_last_wal_receive_lsn(), pg_last_wal_replay_lsn()"
```

**Solution**:
1. Check network connectivity
2. Verify WAL settings
3. Check disk space on standby

### Redis Sentinel Issues

**Symptom**: Sentinel not detecting master

```bash
# Check sentinel logs
docker logs veloz-redis-sentinel-1

# Check sentinel info
docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel master veloz-master
```

**Solution**:
1. Verify sentinel configuration
2. Check network connectivity
3. Ensure quorum (3 sentinels)

### Split Brain Prevention

To prevent split brain scenarios:

1. Use synchronous replication for PostgreSQL
2. Require quorum (2 of 3) for Redis Sentinel
3. Use fencing mechanisms if available
4. Monitor for multiple primaries

## Maintenance

### Planned Failover (Switchover)

For planned maintenance:

```bash
# 1. Verify standby is caught up
./scripts/failover.sh postgres status

# 2. Stop writes on primary
docker exec veloz-postgres-primary psql -U veloz -c "SELECT pg_switch_wal()"

# 3. Wait for standby to catch up
sleep 5

# 4. Promote standby
./scripts/failover.sh postgres promote

# 5. Reconfigure old primary as standby
# (Manual process - see documentation)
```

### Adding a New Standby

```bash
# 1. Start new standby container
docker-compose -f docker-compose.ha.yml up -d postgres-standby-2

# 2. Verify replication
./scripts/failover.sh postgres status
```

## Testing

### Failover Test

```bash
# 1. Simulate primary failure
docker stop veloz-postgres-primary

# 2. Observe automatic failover (Redis)
# or manual promotion (PostgreSQL)

# 3. Verify service continuity
curl http://localhost:8080/health

# 4. Restore primary
docker start veloz-postgres-primary
```

### Load Test During Failover

```bash
# 1. Start load test
./scripts/load_test.sh &

# 2. Trigger failover
docker stop veloz-engine-1

# 3. Observe request success rate
# Target: < 1% error rate during failover
```

## Related Documents

- [Backup and Recovery](backup_recovery.md)
- [DR Runbook](dr_runbook.md)
- [Production Architecture](production_architecture.md)
- [Monitoring](monitoring.md)
