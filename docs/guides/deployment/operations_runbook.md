# VeloZ Operations Runbook

This document provides operational procedures for day-to-day VeloZ management.

## Quick Reference

### Service Commands

```bash
# Start VeloZ
systemctl start veloz
# or
docker-compose up -d

# Stop VeloZ
systemctl stop veloz
# or
docker-compose down

# Restart VeloZ
systemctl restart veloz
# or
docker-compose restart

# Check status
systemctl status veloz
docker-compose ps

# View logs
journalctl -u veloz -f
docker-compose logs -f
```

### Health Checks

```bash
# Basic health
curl http://localhost:8080/health

# Detailed health
curl http://localhost:8080/health/ready

# Metrics
curl http://localhost:8080/metrics

# Order state
curl http://localhost:8080/api/orders_state
```

## Common Operations

### 1. Deployment

#### Deploy New Version

```bash
# 1. Pull latest code
cd /opt/veloz
git pull origin main

# 2. Build release
cmake --preset release
cmake --build --preset release-all -j$(nproc)

# 3. Run tests
ctest --preset release -j$(nproc)

# 4. Stop current service
systemctl stop veloz

# 5. Backup current binary
cp /opt/veloz/build/release/apps/engine/veloz_engine \
   /opt/veloz/backup/veloz_engine.$(date +%Y%m%d)

# 6. Start new version
systemctl start veloz

# 7. Verify health
sleep 10
curl http://localhost:8080/health
```

#### Rollback Deployment

```bash
# 1. Stop current service
systemctl stop veloz

# 2. Restore previous binary
cp /opt/veloz/backup/veloz_engine.YYYYMMDD \
   /opt/veloz/build/release/apps/engine/veloz_engine

# 3. Start service
systemctl start veloz

# 4. Verify health
curl http://localhost:8080/health
```

### 2. Scaling

#### Horizontal Scaling (Kubernetes)

```bash
# Scale up
kubectl scale deployment veloz-gateway --replicas=5

# Scale down
kubectl scale deployment veloz-gateway --replicas=3

# Check status
kubectl get pods -l app=veloz-gateway
```

#### Horizontal Scaling (Docker Compose)

```bash
# Scale up
docker-compose up -d --scale gateway=5

# Scale down
docker-compose up -d --scale gateway=3
```

### 3. Maintenance Windows

#### Planned Maintenance Procedure

1. **Announce maintenance** (24 hours in advance)
   - Update status page
   - Notify stakeholders

2. **Pre-maintenance checks**
   ```bash
   # Check current state
   curl http://localhost:8080/health
   curl http://localhost:8080/api/orders_state

   # Create backup
   /opt/veloz/scripts/backup/backup_all.sh
   ```

3. **Enter maintenance mode**
   ```bash
   # Stop accepting new orders (if supported)
   curl -X POST http://localhost:8080/api/admin/maintenance

   # Wait for pending orders to complete
   sleep 60

   # Stop service
   systemctl stop veloz
   ```

4. **Perform maintenance**
   - Apply updates
   - Run migrations
   - Update configuration

5. **Exit maintenance mode**
   ```bash
   # Start service
   systemctl start veloz

   # Verify health
   curl http://localhost:8080/health

   # Exit maintenance mode
   curl -X POST http://localhost:8080/api/admin/ready
   ```

6. **Post-maintenance verification**
   ```bash
   # Check order flow
   curl http://localhost:8080/api/orders_state

   # Check metrics
   curl http://localhost:8080/metrics
   ```

### 4. Configuration Changes

#### Update Environment Variables

```bash
# 1. Edit configuration
sudo nano /etc/veloz/veloz.env

# 2. Reload service
systemctl daemon-reload
systemctl restart veloz

# 3. Verify configuration
curl http://localhost:8080/api/config
```

#### Update Risk Limits

```bash
# 1. Edit risk configuration
sudo nano /etc/veloz/risk_limits.yaml

# 2. Reload configuration (hot reload if supported)
curl -X POST http://localhost:8080/api/admin/reload-config

# 3. Verify new limits
curl http://localhost:8080/api/risk/limits
```

### 5. Log Management

#### Log Rotation

```bash
# Manual log rotation
logrotate -f /etc/logrotate.d/veloz

# Check log sizes
du -sh /var/log/veloz/*
```

#### Log Level Change

```bash
# Change log level at runtime (if supported)
curl -X POST http://localhost:8080/api/admin/log-level \
  -H "Content-Type: application/json" \
  -d '{"level": "debug"}'

# Or via environment variable
export VELOZ_LOG_LEVEL=debug
systemctl restart veloz
```

### 6. Database Operations

#### Database Maintenance

```bash
# Vacuum PostgreSQL
psql -U veloz -c "VACUUM ANALYZE;"

# Check table sizes
psql -U veloz -c "SELECT relname, pg_size_pretty(pg_total_relation_size(relid))
                  FROM pg_catalog.pg_statio_user_tables
                  ORDER BY pg_total_relation_size(relid) DESC;"

# Check connection count
psql -U veloz -c "SELECT count(*) FROM pg_stat_activity;"
```

#### Redis Maintenance

```bash
# Check memory usage
redis-cli INFO memory

# Clear cache (use with caution)
redis-cli FLUSHDB

# Check key count
redis-cli DBSIZE
```

### 7. Certificate Management

#### Renew TLS Certificates

```bash
# 1. Generate new certificate (Let's Encrypt)
certbot renew

# 2. Or manually replace certificates
cp /path/to/new/cert.pem /etc/ssl/certs/veloz.crt
cp /path/to/new/key.pem /etc/ssl/private/veloz.key

# 3. Reload nginx
nginx -t && nginx -s reload
```

### 8. Backup Operations

#### Manual Backup

```bash
# Full backup
/opt/veloz/scripts/backup/backup_all.sh

# WAL only
/opt/veloz/scripts/backup/backup_wal.sh

# Database only
/opt/veloz/scripts/backup/backup_postgres.sh
```

#### Verify Backup

```bash
# Check backup status
/opt/veloz/scripts/backup/verify_backup.sh

# List recent backups
ls -la /backup/
aws s3 ls s3://veloz-backups/ --recursive
```

### 9. Monitoring Operations

#### Check Metrics

```bash
# Order metrics
curl -s http://localhost:8080/metrics | grep veloz_orders

# Latency metrics
curl -s http://localhost:8080/metrics | grep veloz_latency

# Risk metrics
curl -s http://localhost:8080/metrics | grep veloz_risk

# Position metrics
curl -s http://localhost:8080/metrics | grep veloz_position
```

#### Silence Alerts

```bash
# Silence alert in Alertmanager
curl -X POST http://alertmanager:9093/api/v1/silences \
  -H "Content-Type: application/json" \
  -d '{
    "matchers": [{"name": "alertname", "value": "VelozHighLatency"}],
    "startsAt": "2026-02-23T12:00:00Z",
    "endsAt": "2026-02-23T14:00:00Z",
    "createdBy": "ops",
    "comment": "Planned maintenance"
  }'
```

## Emergency Procedures

### Emergency Stop

```bash
# Graceful stop
systemctl stop veloz

# Force stop
systemctl kill veloz

# Emergency kill (last resort)
pkill -9 veloz_engine
```

### Emergency Rollback

```bash
# 1. Stop service
systemctl stop veloz

# 2. Restore from backup
/opt/veloz/scripts/backup/restore_wal.sh

# 3. Start service
systemctl start veloz
```

### Incident Response

1. **Assess severity** (P1-P4)
2. **Notify on-call** (if P1/P2)
3. **Start incident channel**
4. **Diagnose issue**
5. **Apply fix or rollback**
6. **Verify recovery**
7. **Document incident**
8. **Schedule post-mortem**

## Operational Checklists

### Daily Checks

- [ ] Review overnight alerts
- [ ] Check service health
- [ ] Review error logs
- [ ] Check backup status
- [ ] Review key metrics (latency, error rate)

### Weekly Checks

- [ ] Review capacity metrics
- [ ] Check disk usage
- [ ] Review security logs
- [ ] Verify backup integrity
- [ ] Review performance trends

### Monthly Checks

- [ ] Test backup restore
- [ ] Review access logs
- [ ] Update documentation
- [ ] Review and rotate credentials
- [ ] Capacity planning review

## Related Documents

- [Troubleshooting Guide](troubleshooting.md)
- [Backup and Recovery](backup_recovery.md)
- [DR Runbook](dr_runbook.md)
- [Monitoring](monitoring.md)
- [On-Call Handbook](oncall_handbook.md)
- [Best Practices Guide](../user/best-practices.md) - Operational best practices
- [Glossary](../user/glossary.md) - Technical term definitions
