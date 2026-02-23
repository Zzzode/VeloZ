#!/bin/bash
# restore_postgres.sh - PostgreSQL restore with PITR support
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}/postgres"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
RESTORE_TYPE="${VELOZ_RESTORE_TYPE:-full}"  # full, pitr
TARGET_TIME=""
BACKUP_FILE=""
DATA_DIR="${VELOZ_PG_DATA_DIR:-/var/lib/postgresql/data}"

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Restore PostgreSQL database for VeloZ.

Options:
    -t, --type TYPE         Restore type: full, pitr (default: full)
    -f, --file FILE         Backup file to restore (local path or S3 URI)
    -T, --target-time TIME  Target time for PITR (ISO 8601 format)
    -b, --backup-dir DIR    Local backup directory (default: /backup/postgres)
    -d, --data-dir DIR      PostgreSQL data directory
    -s, --s3-bucket BUCKET  S3 bucket for remote backups
    -h, --help              Show this help message

Restore Types:
    full    - Restore from pg_dump backup
    pitr    - Point-in-time recovery using base backup + WAL

Examples:
    # Restore latest full backup
    $(basename "$0") -t full

    # Restore specific backup file
    $(basename "$0") -f /backup/postgres/pg_full_20260223_120000.tar.gz

    # Point-in-time recovery
    $(basename "$0") -t pitr -T "2026-02-23 12:00:00"

    # Restore from S3
    $(basename "$0") -f s3://veloz-backups/postgres/full/pg_full_20260223_120000.tar.gz
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            RESTORE_TYPE="$2"
            shift 2
            ;;
        -f|--file)
            BACKUP_FILE="$2"
            shift 2
            ;;
        -T|--target-time)
            TARGET_TIME="$2"
            shift 2
            ;;
        -b|--backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        -d|--data-dir)
            DATA_DIR="$2"
            shift 2
            ;;
        -s|--s3-bucket)
            S3_BUCKET="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

find_latest_backup() {
    local type="$1"
    local pattern

    case "$type" in
        full)
            pattern="pg_full_*.tar.gz"
            ;;
        base)
            pattern="pg_base_*"
            ;;
        *)
            log_error "Unknown backup type: $type"
            return 1
            ;;
    esac

    # Check local backups first
    local latest
    if [[ "$type" == "full" ]]; then
        latest=$(find "$BACKUP_DIR" -maxdepth 1 -name "$pattern" -type f 2>/dev/null | sort -r | head -1)
    else
        latest=$(find "${BACKUP_DIR}/base" -maxdepth 1 -name "$pattern" -type d 2>/dev/null | sort -r | head -1)
    fi

    if [[ -n "$latest" ]]; then
        echo "$latest"
        return 0
    fi

    # Check S3 if configured
    if [[ -n "$S3_BUCKET" ]] && command_exists aws; then
        log_info "Searching for backup in S3"
        if [[ "$type" == "full" ]]; then
            latest=$(aws s3 ls "${S3_BUCKET}/postgres/full/" | grep "$pattern" | sort -r | head -1 | awk '{print $4}')
            if [[ -n "$latest" ]]; then
                echo "${S3_BUCKET}/postgres/full/${latest}"
                return 0
            fi
        fi
    fi

    log_error "No backup found"
    return 1
}

download_backup() {
    local source="$1"
    local dest="$2"

    if [[ "$source" == s3://* ]]; then
        log_info "Downloading backup from S3: $source"
        s3_download "$source" "$dest"

        # Download checksum if available
        if aws s3 ls "${source}.sha256" &>/dev/null; then
            s3_download "${source}.sha256" "${dest}.sha256"
        fi
    else
        log_info "Using local backup: $source"
        if [[ "$source" != "$dest" ]]; then
            cp "$source" "$dest"
            [[ -f "${source}.sha256" ]] && cp "${source}.sha256" "${dest}.sha256"
        fi
    fi
}

verify_backup_checksum() {
    local backup_file="$1"
    local checksum_file="${backup_file}.sha256"

    if [[ -f "$checksum_file" ]]; then
        log_info "Verifying backup checksum"
        local expected
        expected=$(cat "$checksum_file" | cut -d' ' -f1)
        if verify_checksum "$backup_file" "$expected"; then
            log_info "Checksum verification passed"
            return 0
        else
            log_error "Checksum verification failed"
            return 1
        fi
    else
        log_warn "No checksum file found, skipping verification"
        return 0
    fi
}

stop_postgresql() {
    log_info "Stopping PostgreSQL"

    # Try systemctl first
    if command_exists systemctl && systemctl is-active --quiet postgresql; then
        sudo systemctl stop postgresql
        return
    fi

    # Try pg_ctl
    if command_exists pg_ctl; then
        pg_ctl stop -D "$DATA_DIR" -m fast 2>/dev/null || true
        return
    fi

    # Try docker
    if command_exists docker; then
        docker stop veloz-postgres 2>/dev/null || true
        return
    fi

    log_warn "Could not stop PostgreSQL automatically"
}

start_postgresql() {
    log_info "Starting PostgreSQL"

    # Try systemctl first
    if command_exists systemctl; then
        sudo systemctl start postgresql
        return
    fi

    # Try pg_ctl
    if command_exists pg_ctl; then
        pg_ctl start -D "$DATA_DIR" -l /var/log/postgresql/postgresql.log
        return
    fi

    # Try docker
    if command_exists docker; then
        docker start veloz-postgres 2>/dev/null || true
        return
    fi

    log_warn "Could not start PostgreSQL automatically"
}

restore_full() {
    log_info "Starting full PostgreSQL restore"

    # Find backup file if not specified
    if [[ -z "$BACKUP_FILE" ]]; then
        BACKUP_FILE=$(find_latest_backup "full")
        log_info "Using latest backup: $BACKUP_FILE"
    fi

    # Download if S3 path
    local local_backup="/tmp/pg_restore_$$.tar.gz"
    download_backup "$BACKUP_FILE" "$local_backup"

    # Verify checksum
    verify_backup_checksum "$local_backup"

    # Extract backup
    log_info "Extracting backup archive"
    local extract_dir="/tmp/pg_restore_$$"
    mkdir -p "$extract_dir"
    tar -xzf "$local_backup" -C "$extract_dir"

    # Find the dump directory
    local dump_dir
    dump_dir=$(find "$extract_dir" -maxdepth 1 -type d -name "pg_full_*" | head -1)

    if [[ -z "$dump_dir" ]]; then
        log_error "Could not find dump directory in archive"
        exit 1
    fi

    # Get connection parameters
    get_pg_params

    # Drop and recreate database
    log_info "Recreating database: $PGDATABASE"
    psql -U postgres -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = '$PGDATABASE' AND pid <> pg_backend_pid();" 2>/dev/null || true
    psql -U postgres -c "DROP DATABASE IF EXISTS $PGDATABASE;"
    psql -U postgres -c "CREATE DATABASE $PGDATABASE OWNER $PGUSER;"

    # Restore from dump
    log_info "Restoring database from dump"
    pg_restore \
        -d "$PGDATABASE" \
        -j 4 \
        --verbose \
        "$dump_dir" \
        2>&1 | while read -r line; do log_debug "$line"; done

    # Cleanup
    rm -rf "$extract_dir" "$local_backup"

    log_info "Full restore completed successfully"
}

restore_pitr() {
    log_info "Starting Point-in-Time Recovery"

    if [[ -z "$TARGET_TIME" ]]; then
        log_error "Target time required for PITR (-T option)"
        exit 1
    fi

    log_info "Target recovery time: $TARGET_TIME"

    # Find latest base backup before target time
    local base_backup
    base_backup=$(find_latest_backup "base")

    if [[ -z "$base_backup" ]]; then
        log_error "No base backup found for PITR"
        exit 1
    fi

    log_info "Using base backup: $base_backup"

    # Stop PostgreSQL
    stop_postgresql

    # Backup current data directory
    if [[ -d "$DATA_DIR" ]]; then
        local backup_current="${DATA_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
        log_info "Backing up current data directory to: $backup_current"
        mv "$DATA_DIR" "$backup_current"
    fi

    # Create new data directory
    mkdir -p "$DATA_DIR"

    # Extract base backup
    log_info "Extracting base backup"
    tar -xzf "${base_backup}/base.tar.gz" -C "$DATA_DIR"

    # Extract pg_wal if present
    if [[ -f "${base_backup}/pg_wal.tar.gz" ]]; then
        mkdir -p "${DATA_DIR}/pg_wal"
        tar -xzf "${base_backup}/pg_wal.tar.gz" -C "${DATA_DIR}/pg_wal"
    fi

    # Copy archived WAL files
    local wal_archive="${BACKUP_DIR}/wal"
    if [[ -d "$wal_archive" ]]; then
        log_info "Copying archived WAL files"
        mkdir -p "${DATA_DIR}/pg_wal"
        for wal_file in "$wal_archive"/*.gz; do
            if [[ -f "$wal_file" ]]; then
                local base_name
                base_name=$(basename "$wal_file" .gz)
                gunzip -c "$wal_file" > "${DATA_DIR}/pg_wal/${base_name}"
            fi
        done
    fi

    # Create recovery configuration
    log_info "Creating recovery configuration"
    cat > "${DATA_DIR}/postgresql.auto.conf" << EOF
# Recovery configuration for PITR
restore_command = 'cp ${wal_archive}/%f.gz - | gunzip > %p'
recovery_target_time = '$TARGET_TIME'
recovery_target_action = 'promote'
EOF

    # Create recovery signal file
    touch "${DATA_DIR}/recovery.signal"

    # Set permissions
    chown -R postgres:postgres "$DATA_DIR" 2>/dev/null || true
    chmod 700 "$DATA_DIR"

    # Start PostgreSQL
    start_postgresql

    # Wait for recovery to complete
    log_info "Waiting for recovery to complete"
    local max_wait=300
    local waited=0
    while [[ $waited -lt $max_wait ]]; do
        if pg_isready -q; then
            # Check if recovery is complete
            local in_recovery
            in_recovery=$(psql -U postgres -tAc "SELECT pg_is_in_recovery();" 2>/dev/null || echo "t")
            if [[ "$in_recovery" == "f" ]]; then
                log_info "Recovery completed successfully"
                break
            fi
        fi
        sleep 5
        waited=$((waited + 5))
        log_debug "Waiting for recovery... ($waited/$max_wait seconds)"
    done

    if [[ $waited -ge $max_wait ]]; then
        log_warn "Recovery may still be in progress"
    fi

    log_info "PITR restore completed"
}

verify_restore() {
    log_info "Verifying restore"

    get_pg_params

    # Check connection
    if ! check_pg_connection; then
        log_error "Cannot connect to PostgreSQL after restore"
        return 1
    fi

    # Check database exists
    local db_exists
    db_exists=$(psql -U postgres -tAc "SELECT 1 FROM pg_database WHERE datname = '$PGDATABASE';" 2>/dev/null || echo "0")
    if [[ "$db_exists" != "1" ]]; then
        log_error "Database $PGDATABASE does not exist"
        return 1
    fi

    # Check tables
    local table_count
    table_count=$(psql -U "$PGUSER" -d "$PGDATABASE" -tAc "SELECT count(*) FROM information_schema.tables WHERE table_schema = 'public';" 2>/dev/null || echo "0")
    log_info "Found $table_count tables in database"

    log_info "Restore verification passed"
    return 0
}

main() {
    require_commands pg_restore psql

    log_info "Starting PostgreSQL restore (type: $RESTORE_TYPE)"

    # Acquire lock
    local lock_file="${BACKUP_DIR}/.restore.lock"
    trap "cleanup_on_exit '$lock_file'" EXIT

    if ! acquire_lock "$lock_file" 60; then
        log_error "Another restore is in progress"
        exit 1
    fi

    # Run restore based on type
    case "$RESTORE_TYPE" in
        full)
            restore_full
            ;;
        pitr)
            restore_pitr
            ;;
        *)
            log_error "Unknown restore type: $RESTORE_TYPE"
            exit 1
            ;;
    esac

    # Verify restore
    verify_restore

    log_info "PostgreSQL restore completed successfully"
}

main "$@"
