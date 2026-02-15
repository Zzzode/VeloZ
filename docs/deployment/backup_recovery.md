# Backup and Recovery

This document describes backup procedures and disaster recovery for VeloZ.

## Overview

VeloZ requires backup of:
- **Order WAL**: Write-ahead log for order recovery
- **Configuration**: Environment and strategy configuration
- **Database**: PostgreSQL data (if used)
- **Secrets**: API keys and credentials

## Backup Strategy

### Backup Schedule

| Data Type | Frequency | Retention | Method |
|-----------|-----------|-----------|--------|
| Order WAL | Continuous | 7 days | File sync |
| Configuration | On change | 30 days | Git + S3 |
| Database | Daily | 30 days | pg_dump |
| Secrets | On change | Encrypted | Vault |

### Backup Locations

```
/backup/
├── wal/           # Order WAL files
├── config/        # Configuration snapshots
├── database/      # PostgreSQL dumps
└── secrets/       # Encrypted secrets
```

## Order WAL Backup

### WAL File Structure

```
data/wal/
├── orders_0000000000000001.wal
├── orders_0000000000000002.wal
└── orders_current.wal
```

### Continuous WAL Backup

```bash
#!/bin/bash
# backup_wal.sh

WAL_DIR="/var/lib/veloz/wal"
BACKUP_DIR="/backup/wal"
S3_BUCKET="s3://veloz-backups/wal"

# Sync WAL files to backup directory
rsync -av --delete "$WAL_DIR/" "$BACKUP_DIR/"

# Upload to S3
aws s3 sync "$BACKUP_DIR/" "$S3_BUCKET/" --delete

# Log backup completion
echo "$(date): WAL backup completed" >> /var/log/veloz/backup.log
```

### Cron Schedule

```cron
# Backup WAL every 5 minutes
*/5 * * * * /opt/veloz/scripts/backup_wal.sh
```

## Configuration Backup

### Configuration Files

```bash
# List of configuration files to backup
/etc/veloz/config.yaml
/etc/veloz/strategies/*.yaml
/etc/veloz/risk_limits.yaml
.env
```

### Backup Script

```bash
#!/bin/bash
# backup_config.sh

CONFIG_DIR="/etc/veloz"
BACKUP_DIR="/backup/config"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create timestamped backup
tar -czf "$BACKUP_DIR/config_$TIMESTAMP.tar.gz" \
    -C / \
    etc/veloz \
    --exclude='*.secret'

# Upload to S3
aws s3 cp "$BACKUP_DIR/config_$TIMESTAMP.tar.gz" \
    "s3://veloz-backups/config/"

# Cleanup old backups (keep 30 days)
find "$BACKUP_DIR" -name "config_*.tar.gz" -mtime +30 -delete
```

## Database Backup

### PostgreSQL Backup

```bash
#!/bin/bash
# backup_database.sh

DB_NAME="veloz"
DB_USER="veloz"
BACKUP_DIR="/backup/database"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create backup
pg_dump -U "$DB_USER" -Fc "$DB_NAME" > \
    "$BACKUP_DIR/veloz_$TIMESTAMP.dump"

# Upload to S3
aws s3 cp "$BACKUP_DIR/veloz_$TIMESTAMP.dump" \
    "s3://veloz-backups/database/"

# Cleanup old backups
find "$BACKUP_DIR" -name "veloz_*.dump" -mtime +30 -delete
```

### Automated Backup with Docker

```yaml
# docker-compose.yml
services:
  backup:
    image: postgres:15-alpine
    volumes:
      - postgres_data:/var/lib/postgresql/data:ro
      - ./backup:/backup
    environment:
      - PGHOST=postgres
      - PGUSER=veloz
      - PGPASSWORD=${POSTGRES_PASSWORD}
    command: >
      sh -c "while true; do
        pg_dump -Fc veloz > /backup/veloz_$$(date +%Y%m%d_%H%M%S).dump;
        sleep 86400;
      done"
```

## Secrets Backup

### Encrypted Secrets Backup

```bash
#!/bin/bash
# backup_secrets.sh

SECRETS_DIR="/etc/veloz/secrets"
BACKUP_DIR="/backup/secrets"
GPG_KEY="veloz-backup@example.com"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create encrypted backup
tar -czf - -C "$SECRETS_DIR" . | \
    gpg --encrypt --recipient "$GPG_KEY" > \
    "$BACKUP_DIR/secrets_$TIMESTAMP.tar.gz.gpg"

# Upload to S3
aws s3 cp "$BACKUP_DIR/secrets_$TIMESTAMP.tar.gz.gpg" \
    "s3://veloz-backups/secrets/"
```

### HashiCorp Vault Integration

```bash
# Export secrets from Vault
vault kv get -format=json secret/veloz > secrets_backup.json

# Encrypt and store
gpg --encrypt --recipient backup@example.com secrets_backup.json
aws s3 cp secrets_backup.json.gpg s3://veloz-backups/vault/
rm secrets_backup.json
```

## Recovery Procedures

### Order WAL Recovery

```bash
#!/bin/bash
# recover_wal.sh

BACKUP_DIR="/backup/wal"
WAL_DIR="/var/lib/veloz/wal"

# Stop VeloZ
systemctl stop veloz

# Restore WAL files
rsync -av "$BACKUP_DIR/" "$WAL_DIR/"

# Start VeloZ (will replay WAL)
systemctl start veloz

# Verify recovery
curl http://localhost:8080/health
```

### Configuration Recovery

```bash
#!/bin/bash
# recover_config.sh

BACKUP_FILE="$1"  # e.g., config_20260214_120000.tar.gz

# Download from S3 if needed
if [[ "$BACKUP_FILE" == s3://* ]]; then
    aws s3 cp "$BACKUP_FILE" /tmp/config_backup.tar.gz
    BACKUP_FILE="/tmp/config_backup.tar.gz"
fi

# Stop VeloZ
systemctl stop veloz

# Restore configuration
tar -xzf "$BACKUP_FILE" -C /

# Start VeloZ
systemctl start veloz
```

### Database Recovery

```bash
#!/bin/bash
# recover_database.sh

BACKUP_FILE="$1"  # e.g., veloz_20260214_120000.dump
DB_NAME="veloz"
DB_USER="veloz"

# Download from S3 if needed
if [[ "$BACKUP_FILE" == s3://* ]]; then
    aws s3 cp "$BACKUP_FILE" /tmp/db_backup.dump
    BACKUP_FILE="/tmp/db_backup.dump"
fi

# Stop VeloZ
systemctl stop veloz

# Drop and recreate database
psql -U postgres -c "DROP DATABASE IF EXISTS $DB_NAME;"
psql -U postgres -c "CREATE DATABASE $DB_NAME OWNER $DB_USER;"

# Restore from backup
pg_restore -U "$DB_USER" -d "$DB_NAME" "$BACKUP_FILE"

# Start VeloZ
systemctl start veloz
```

### Secrets Recovery

```bash
#!/bin/bash
# recover_secrets.sh

BACKUP_FILE="$1"  # e.g., secrets_20260214_120000.tar.gz.gpg
SECRETS_DIR="/etc/veloz/secrets"

# Download from S3 if needed
if [[ "$BACKUP_FILE" == s3://* ]]; then
    aws s3 cp "$BACKUP_FILE" /tmp/secrets_backup.tar.gz.gpg
    BACKUP_FILE="/tmp/secrets_backup.tar.gz.gpg"
fi

# Decrypt and restore
gpg --decrypt "$BACKUP_FILE" | tar -xzf - -C "$SECRETS_DIR"

# Set permissions
chmod 600 "$SECRETS_DIR"/*
chown veloz:veloz "$SECRETS_DIR"/*
```

## Disaster Recovery

### Recovery Time Objectives

| Scenario | RTO | RPO |
|----------|-----|-----|
| Single node failure | 5 minutes | 0 (WAL) |
| Database failure | 30 minutes | 24 hours |
| Complete site failure | 4 hours | 5 minutes |

### DR Runbook

#### 1. Assess the Situation

```bash
# Check service status
systemctl status veloz
docker-compose ps

# Check logs
journalctl -u veloz -n 100
docker-compose logs --tail=100
```

#### 2. Determine Recovery Strategy

| Failure Type | Recovery Strategy |
|--------------|-------------------|
| Process crash | Restart service |
| Data corruption | Restore from WAL |
| Database failure | Restore from backup |
| Complete failure | Full restore |

#### 3. Execute Recovery

```bash
# For process crash
systemctl restart veloz

# For data corruption
./scripts/recover_wal.sh

# For database failure
./scripts/recover_database.sh s3://veloz-backups/database/latest.dump

# For complete failure
./scripts/full_restore.sh
```

#### 4. Verify Recovery

```bash
# Check health
curl http://localhost:8080/health

# Check order state
curl http://localhost:8080/api/orders_state

# Check metrics
curl http://localhost:8080/metrics
```

### Full Restore Script

```bash
#!/bin/bash
# full_restore.sh

set -e

echo "Starting full VeloZ restore..."

# 1. Restore secrets
echo "Restoring secrets..."
LATEST_SECRETS=$(aws s3 ls s3://veloz-backups/secrets/ | sort | tail -1 | awk '{print $4}')
./scripts/recover_secrets.sh "s3://veloz-backups/secrets/$LATEST_SECRETS"

# 2. Restore configuration
echo "Restoring configuration..."
LATEST_CONFIG=$(aws s3 ls s3://veloz-backups/config/ | sort | tail -1 | awk '{print $4}')
./scripts/recover_config.sh "s3://veloz-backups/config/$LATEST_CONFIG"

# 3. Restore database
echo "Restoring database..."
LATEST_DB=$(aws s3 ls s3://veloz-backups/database/ | sort | tail -1 | awk '{print $4}')
./scripts/recover_database.sh "s3://veloz-backups/database/$LATEST_DB"

# 4. Restore WAL
echo "Restoring WAL..."
./scripts/recover_wal.sh

# 5. Start services
echo "Starting services..."
docker-compose up -d

# 6. Verify
echo "Verifying recovery..."
sleep 10
curl http://localhost:8080/health

echo "Full restore completed!"
```

## Testing Backups

### Monthly Backup Test

```bash
#!/bin/bash
# test_backup.sh

# Create test environment
docker-compose -f docker-compose.test.yml up -d

# Restore from latest backup
./scripts/full_restore.sh

# Run verification tests
./scripts/verify_restore.sh

# Cleanup
docker-compose -f docker-compose.test.yml down -v

echo "Backup test completed successfully"
```

### Verification Checklist

- [ ] WAL files restored correctly
- [ ] Configuration files present
- [ ] Database accessible
- [ ] Secrets decrypted
- [ ] Service starts successfully
- [ ] Health check passes
- [ ] Order state matches backup
- [ ] API endpoints respond

## Related Documents

- [Production Architecture](production_architecture.md)
- [Monitoring](monitoring.md)
- [Troubleshooting](troubleshooting.md)
