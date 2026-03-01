# VeloZ Disaster Recovery Runbook

This document provides step-by-step procedures for disaster recovery scenarios.

## Recovery Time Objectives (RTO) and Recovery Point Objectives (RPO)

| Scenario | RTO | RPO | Priority |
|----------|-----|-----|----------|
| Single node failure | 5 minutes | 0 (WAL replay) | P1 |
| Database corruption | 30 minutes | 5 minutes (WAL) | P1 |
| Complete site failure | 4 hours | 5 minutes | P2 |
| Region failure | 8 hours | 1 hour | P3 |

## Quick Reference

### Emergency Contacts

| Role | Contact | Escalation Time |
|------|---------|-----------------|
| On-call Engineer | PagerDuty | Immediate |
| Database Admin | #db-oncall | 15 minutes |
| Infrastructure Lead | #infra-oncall | 30 minutes |
| Engineering Manager | Direct contact | 1 hour |

### Critical Commands

```bash
# Check service status
systemctl status veloz
docker-compose ps

# View recent logs
journalctl -u veloz -n 100 --no-pager
docker-compose logs --tail=100

# Health check
curl http://localhost:8080/health

# Quick restart
systemctl restart veloz
# or
docker-compose restart veloz-engine
```

## Scenario 1: VeloZ Engine Crash

### Symptoms
- Health check failing
- No response from API endpoints
- Error logs showing crash or panic

### Diagnosis

```bash
# Check process status
systemctl status veloz
ps aux | grep veloz

# Check recent logs
journalctl -u veloz -n 200 --no-pager | tail -100

# Check system resources
free -h
df -h
top -bn1 | head -20
```

### Recovery Steps

1. **Restart the service**
   ```bash
   systemctl restart veloz
   # or
   docker-compose restart veloz-engine
   ```

2. **Verify recovery**
   ```bash
   # Wait for startup
   sleep 10

   # Check health
   curl http://localhost:8080/health

   # Check order state (WAL should replay)
   curl http://localhost:8080/api/orders_state
   ```

3. **If restart fails, check WAL integrity**
   ```bash
   # Check WAL directory
   ls -la /var/lib/veloz/wal/

   # Verify WAL files
   file /var/lib/veloz/wal/*.wal
   ```

4. **If WAL is corrupted, restore from backup**
   ```bash
   /opt/veloz/scripts/backup/restore_wal.sh
   systemctl restart veloz
   ```

### Post-Incident
- Document the crash cause
- Check for memory leaks or resource exhaustion
- Review recent deployments

## Scenario 2: Database Corruption

### Symptoms
- Database connection errors
- Query failures
- Data inconsistency

### Diagnosis

```bash
# Check PostgreSQL status
systemctl status postgresql
pg_isready

# Check PostgreSQL logs
tail -100 /var/log/postgresql/postgresql-*.log

# Check for corruption
psql -U veloz -c "SELECT * FROM pg_stat_database WHERE datname = 'veloz';"
```

### Recovery Steps

#### Option A: Point-in-Time Recovery (Preferred)

1. **Determine target recovery time**
   ```bash
   # Find last known good time from logs
   grep -i "error\|corrupt" /var/log/postgresql/*.log | head -5
   ```

2. **Stop VeloZ**
   ```bash
   systemctl stop veloz
   ```

3. **Perform PITR**
   ```bash
   # Replace with actual target time
   /opt/veloz/scripts/backup/restore_postgres.sh \
     -t pitr \
     -T "2026-02-23 11:55:00"
   ```

4. **Verify recovery**
   ```bash
   psql -U veloz -c "SELECT count(*) FROM orders;"
   ```

5. **Start VeloZ**
   ```bash
   systemctl start veloz
   curl http://localhost:8080/health
   ```

#### Option B: Full Restore (If PITR fails)

1. **Stop all services**
   ```bash
   systemctl stop veloz postgresql
   ```

2. **Restore from latest backup**
   ```bash
   /opt/veloz/scripts/backup/restore_postgres.sh -t full
   ```

3. **Restore WAL**
   ```bash
   /opt/veloz/scripts/backup/restore_wal.sh
   ```

4. **Start services**
   ```bash
   systemctl start postgresql
   sleep 10
   systemctl start veloz
   ```

### Post-Incident
- Analyze corruption cause
- Review backup integrity
- Consider additional monitoring

## Scenario 3: Complete Site Failure

### Symptoms
- All services unreachable
- Infrastructure failure (power, network, hardware)

### Prerequisites
- Access to backup storage (S3 or remote backup)
- New infrastructure provisioned

### Recovery Steps

1. **Provision new infrastructure**
   ```bash
   # Using Terraform (if available)
   cd /opt/veloz/infra/terraform
   terraform apply -var="environment=dr"
   ```

2. **Install VeloZ**
   ```bash
   # Clone repository
   git clone https://github.com/veloz/veloz.git /opt/veloz

   # Install dependencies
   /opt/veloz/scripts/install.sh
   ```

3. **Configure S3 access**
   ```bash
   export AWS_ACCESS_KEY_ID="your-key"
   export AWS_SECRET_ACCESS_KEY="your-secret"
   export VELOZ_S3_BUCKET="s3://veloz-backups"
   ```

4. **Run full restore**
   ```bash
   /opt/veloz/scripts/backup/full_restore.sh -y -s "$VELOZ_S3_BUCKET"
   ```

5. **Update DNS/Load Balancer**
   - Point DNS to new infrastructure
   - Update load balancer targets

6. **Verify recovery**
   ```bash
   curl http://localhost:8080/health
   curl http://localhost:8080/api/orders_state
   ```

7. **Notify stakeholders**
   - Update status page
   - Notify trading operations

### Post-Incident
- Conduct post-mortem
- Update DR procedures if needed
- Schedule DR drill

## Scenario 4: Data Loss (Accidental Deletion)

### Symptoms
- Missing orders or positions
- User reports of lost data

### Diagnosis

```bash
# Check current data
psql -U veloz -c "SELECT count(*) FROM orders WHERE created_at > NOW() - INTERVAL '1 hour';"

# Check WAL for recent changes
ls -lt /var/lib/veloz/wal/*.wal | head -5
```

### Recovery Steps

1. **Stop writes immediately**
   ```bash
   # Put VeloZ in read-only mode (if supported)
   curl -X POST http://localhost:8080/api/admin/readonly
   ```

2. **Identify deletion time**
   ```bash
   # Check logs for deletion
   grep -i "delete\|truncate" /var/log/veloz/*.log | tail -20
   ```

3. **Perform PITR to before deletion**
   ```bash
   /opt/veloz/scripts/backup/restore_postgres.sh \
     -t pitr \
     -T "2026-02-23 11:50:00"  # Time before deletion
   ```

4. **Verify data restored**
   ```bash
   psql -U veloz -c "SELECT count(*) FROM orders;"
   ```

5. **Resume normal operations**
   ```bash
   curl -X POST http://localhost:8080/api/admin/readwrite
   ```

## Backup Verification Checklist

Run this checklist monthly:

- [ ] WAL backup running (check every 5 minutes)
- [ ] Config backup exists (within 6 hours)
- [ ] PostgreSQL backup exists (within 24 hours)
- [ ] S3 sync working (if configured)
- [ ] Checksums valid for all backups
- [ ] Test restore successful
- [ ] PITR tested to specific time
- [ ] Recovery time within RTO
- [ ] All documentation current

### Verification Commands

```bash
# Run automated verification
/opt/veloz/scripts/backup/verify_backup.sh -t -r /tmp/backup_report.txt

# Check report
cat /tmp/backup_report.txt
```

## Escalation Procedures

### Level 1 (On-call Engineer)
- Service restart
- Basic diagnostics
- WAL restore

### Level 2 (Database Admin)
- PITR recovery
- Database corruption analysis
- Performance issues

### Level 3 (Infrastructure Lead)
- Full site recovery
- Infrastructure provisioning
- Cross-region failover

### Level 4 (Engineering Manager)
- Business impact assessment
- External communication
- Resource allocation

## Communication Templates

### Initial Incident Notification

```
INCIDENT: VeloZ [Service/Database/Site] Failure
Time: [TIMESTAMP]
Impact: [DESCRIPTION]
Status: Investigating
ETA: [ESTIMATED RECOVERY TIME]
```

### Recovery Notification

```
RESOLVED: VeloZ [Service/Database/Site] Failure
Time: [TIMESTAMP]
Duration: [DURATION]
Impact: [DESCRIPTION]
Root Cause: [BRIEF DESCRIPTION]
Post-mortem: [LINK]
```

## Related Documents

- [Backup and Recovery](backup_recovery.md)
- [Production Architecture](production_architecture.md)
- [Monitoring](monitoring.md)
- [Troubleshooting](troubleshooting.md)
- [Best Practices Guide](../user/best-practices.md) - Operational best practices
- [Glossary](../user/glossary.md) - Technical term definitions
