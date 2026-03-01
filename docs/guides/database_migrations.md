# Database Migration Guide

This guide covers database schema management for the VeloZ Gateway using Alembic.

## Overview

VeloZ Gateway uses SQLAlchemy ORM with Alembic for database migrations. This provides:

- Version-controlled schema changes
- Forward and rollback migrations
- Support for SQLite (development) and PostgreSQL (production)
- Automatic migration testing in CI/CD

## Quick Start

### Development Setup

```bash
cd apps/gateway

# Install dependencies
pip install -r requirements.txt

# Initialize database with all migrations
python -m db.migrate init

# Check current status
python -m db.migrate current
```

### Production Setup

```bash
# Set database URL
export VELOZ_DATABASE_URL="postgresql://user:password@host:5432/veloz"

# Apply all migrations
python -m db.migrate upgrade head

# Verify
python -m db.migrate check
```

## Migration Commands

### Apply Migrations

```bash
# Apply all pending migrations
python -m db.migrate upgrade

# Apply up to specific revision
python -m db.migrate upgrade 20260223_000001

# Apply next migration only
python -m db.migrate upgrade +1
```

### Rollback Migrations

```bash
# Rollback one migration
python -m db.migrate downgrade -1

# Rollback to specific revision
python -m db.migrate downgrade 20260223_000001

# Rollback all migrations
python -m db.migrate downgrade base
```

### Check Status

```bash
# Show current revision
python -m db.migrate current

# Show migration history
python -m db.migrate history

# Check for pending migrations
python -m db.migrate check
```

### Create New Migrations

```bash
# Create empty migration
python -m db.migrate create "Add new column"

# Auto-generate migration from model changes
python -m db.migrate create "Add new column" --autogenerate
```

### Generate SQL Scripts

```bash
# Generate SQL for all migrations
python -m db.migrate sql

# Generate SQL for specific range
python -m db.migrate sql --start base --end head
```

## Database Schema

### Tables

| Table | Description |
|-------|-------------|
| `users` | User accounts and authentication |
| `api_keys` | API key management |
| `orders` | Order tracking and history |
| `positions` | Position management |
| `audit_logs` | Audit trail for compliance |
| `trade_history` | Trade execution records |
| `strategy_states` | Strategy state persistence |

### Entity Relationships

```
users
  ├── api_keys (1:N, cascade delete)
  ├── orders (1:N)
  └── positions (1:N)

orders
  └── trade_history (1:N)
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `VELOZ_DATABASE_URL` | Database connection URL | `sqlite:///veloz_gateway.db` |

### Connection URL Examples

```bash
# SQLite (development)
VELOZ_DATABASE_URL="sqlite:///veloz_gateway.db"

# PostgreSQL (production)
VELOZ_DATABASE_URL="postgresql://user:password@localhost:5432/veloz"

# PostgreSQL with SSL
VELOZ_DATABASE_URL="postgresql://user:password@host:5432/veloz?sslmode=require"
```

## Best Practices

### Writing Migrations

1. **Always test migrations locally** before deploying
2. **Use batch mode** for SQLite ALTER TABLE operations (enabled by default)
3. **Include rollback logic** in every migration
4. **Keep migrations small** and focused on single changes
5. **Use descriptive names** for migration files

### Production Deployment

1. **Backup database** before running migrations
2. **Run migrations during maintenance window** for major changes
3. **Test rollback** before deploying to production
4. **Monitor migration duration** for large tables
5. **Use transactions** (enabled by default)

### Migration Naming Convention

```
YYYYMMDD_HHMMSS_description.py
```

Example: `20260223_143052_add_user_preferences.py`

## Troubleshooting

### Common Issues

#### Migration fails with "Target database is not up to date"

```bash
# Stamp database with current revision
python -m db.migrate stamp head
```

#### SQLite ALTER TABLE limitations

SQLite has limited ALTER TABLE support. Alembic uses batch mode to work around this:

```python
# In migration file
with op.batch_alter_table('table_name') as batch_op:
    batch_op.add_column(sa.Column('new_column', sa.String(50)))
```

#### PostgreSQL connection issues

```bash
# Test connection
psql $VELOZ_DATABASE_URL -c "SELECT 1"

# Check SSL mode
VELOZ_DATABASE_URL="postgresql://...?sslmode=disable"
```

### Recovery Procedures

#### Corrupted migration state

```bash
# 1. Check actual database state
python -m db.migrate current

# 2. Stamp to correct revision
python -m db.migrate stamp <correct_revision>

# 3. Continue from there
python -m db.migrate upgrade head
```

#### Failed migration rollback

```bash
# 1. Check current state
python -m db.migrate current

# 2. Manually fix database if needed
psql $VELOZ_DATABASE_URL

# 3. Stamp to last known good state
python -m db.migrate stamp <good_revision>
```

## CI/CD Integration

Migrations are automatically tested in CI:

1. **Unit tests**: `db/test_migrations.py` tests model operations
2. **Migration apply**: Tests `upgrade head` succeeds
3. **Migration rollback**: Tests `downgrade base` and `upgrade head` cycle

### GitHub Actions Workflow

```yaml
migration-check:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-python@v4
      with:
        python-version: "3.10"
    - run: pip install sqlalchemy alembic pytest
    - run: python -m pytest db/test_migrations.py -v
      working-directory: apps/gateway
    - run: python -m db.migrate init
      working-directory: apps/gateway
```

## API Reference

### DatabaseManager

```python
from db import init_database, get_session, close_database

# Initialize
db = init_database(database_url="postgresql://...")

# Use session
with get_session() as session:
    user = session.query(User).filter_by(username="test").first()

# Health check
status = db.health_check()

# Close
close_database()
```

### Models

```python
from db import User, Order, Position, AuditLog

# Create user
user = User(
    id="user-001",
    username="trader",
    password_hash="...",
    role="trader",
)

# Create order
order = Order(
    id="order-001",
    client_order_id="client-001",
    symbol="BTCUSDT",
    side="BUY",
    order_type="LIMIT",
    quantity=0.1,
    price=50000.0,
)
```

## Related Documentation

- [Deployment Guide](../infra/deployment.md)
- [Gateway API Reference](../api/README.md)
- [Security Guide](./security.md)
