#!/bin/bash
# Initialize PostgreSQL standby from primary

set -e

PGDATA=/var/lib/postgresql/data

# Wait for primary to be ready
echo "Waiting for primary to be ready..."
until pg_isready -h postgres-primary -p 5432 -U "$PGUSER"; do
    echo "Primary not ready, waiting..."
    sleep 2
done

# Check if data directory is empty or needs initialization
if [ -z "$(ls -A $PGDATA 2>/dev/null)" ]; then
    echo "Initializing standby from primary..."

    # Create base backup from primary
    pg_basebackup \
        -h postgres-primary \
        -p 5432 \
        -U "$PGUSER" \
        -D "$PGDATA" \
        -Fp \
        -Xs \
        -P \
        -R \
        --slot=veloz_standby_slot \
        --checkpoint=fast

    # Configure standby
    cat >> "$PGDATA/postgresql.auto.conf" <<EOF

# Standby configuration
primary_conninfo = 'host=postgres-primary port=5432 user=$PGUSER password=$PGPASSWORD application_name=veloz_standby'
primary_slot_name = 'veloz_standby_slot'
hot_standby = on
EOF

    # Create standby signal file
    touch "$PGDATA/standby.signal"

    echo "Standby initialization complete"
else
    echo "Data directory exists, starting as standby"

    # Ensure standby signal exists
    if [ ! -f "$PGDATA/standby.signal" ]; then
        touch "$PGDATA/standby.signal"
    fi
fi

# Set permissions
chown -R postgres:postgres "$PGDATA"
chmod 700 "$PGDATA"

# Start PostgreSQL
exec docker-entrypoint.sh postgres \
    -c config_file=/var/lib/postgresql/data/postgresql.conf \
    -c hba_file=/var/lib/postgresql/data/pg_hba.conf
