#!/bin/bash
# Initialize PostgreSQL primary for replication

set -e

# Create replication user
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
    -- Create replication user if not exists
    DO \$\$
    BEGIN
        IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${POSTGRES_REPLICATION_USER}') THEN
            CREATE ROLE ${POSTGRES_REPLICATION_USER} WITH REPLICATION LOGIN PASSWORD '${POSTGRES_REPLICATION_PASSWORD}';
        END IF;
    END
    \$\$;

    -- Create replication slot for standby
    SELECT pg_create_physical_replication_slot('veloz_standby_slot', true)
    WHERE NOT EXISTS (SELECT FROM pg_replication_slots WHERE slot_name = 'veloz_standby_slot');

    -- Create WAL archive directory
    -- Note: This is handled by the volume mount
EOSQL

# Create WAL archive directory
mkdir -p /var/lib/postgresql/wal_archive
chown postgres:postgres /var/lib/postgresql/wal_archive

echo "Primary initialization complete"
