#!/bin/bash
# backup_postgres.sh - PostgreSQL backup with WAL archiving support
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}/postgres"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
RETENTION_DAYS="${VELOZ_PG_RETENTION_DAYS:-30}"
BACKUP_TYPE="${VELOZ_PG_BACKUP_TYPE:-full}"  # full, incremental, wal
PARALLEL_JOBS="${VELOZ_PG_PARALLEL_JOBS:-4}"

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Backup PostgreSQL database for VeloZ.

Options:
    -t, --type TYPE         Backup type: full, incremental, wal (default: full)
    -b, --backup-dir DIR    Local backup directory (default: /backup/postgres)
    -s, --s3-bucket BUCKET  S3 bucket for remote backup (optional)
    -r, --retention DAYS    Retention period in days (default: 30)
    -j, --jobs NUM          Parallel jobs for pg_dump (default: 4)
    -h, --help              Show this help message

Environment variables:
    VELOZ_PG_HOST           PostgreSQL host (default: localhost)
    VELOZ_PG_PORT           PostgreSQL port (default: 5432)
    VELOZ_PG_USER           PostgreSQL user (default: veloz)
    VELOZ_PG_DATABASE       PostgreSQL database (default: veloz)
    VELOZ_PG_PASSWORD       PostgreSQL password
    VELOZ_PG_PASSWORD_FILE  File containing PostgreSQL password
    VELOZ_BACKUP_DIR        Base backup directory
    VELOZ_S3_BUCKET         S3 bucket for remote backups

Backup Types:
    full        - Complete database dump (pg_dump -Fd)
    incremental - Base backup + WAL for PITR (pg_basebackup)
    wal         - Archive WAL files only

Examples:
    $(basename "$0")
    $(basename "$0") -t full -s s3://veloz-backups
    $(basename "$0") -t incremental
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BACKUP_TYPE="$2"
            shift 2
            ;;
        -b|--backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        -s|--s3-bucket)
            S3_BUCKET="$2"
            shift 2
            ;;
        -r|--retention)
            RETENTION_DAYS="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
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

# Validate backup type
if [[ ! "$BACKUP_TYPE" =~ ^(full|incremental|wal)$ ]]; then
    log_error "Invalid backup type: $BACKUP_TYPE"
    exit 1
fi

backup_full() {
    log_info "Starting full PostgreSQL backup"

    local timestamp
    timestamp=$(generate_timestamp)
    local backup_name="pg_full_${timestamp}"
    local backup_path="${BACKUP_DIR}/${backup_name}"

    # Create backup directory
    mkdir -p "$backup_path"

    # Run pg_dump in directory format (supports parallel)
    log_info "Running pg_dump with ${PARALLEL_JOBS} parallel jobs"
    pg_dump \
        -Fd \
        -j "$PARALLEL_JOBS" \
        -f "$backup_path" \
        --verbose \
        2>&1 | while read -r line; do log_debug "$line"; done

    # Create compressed archive
    log_info "Compressing backup"
    local archive_path="${BACKUP_DIR}/${backup_name}.tar.gz"
    tar -czf "$archive_path" -C "$BACKUP_DIR" "$backup_name"

    # Calculate checksum
    local checksum
    checksum=$(calculate_checksum "$archive_path")
    echo "$checksum  ${backup_name}.tar.gz" > "${archive_path}.sha256"

    # Cleanup uncompressed directory
    rm -rf "$backup_path"

    # Get backup size
    local backup_size
    backup_size=$(du -h "$archive_path" | cut -f1)

    log_info "Full backup completed: ${backup_name}.tar.gz (${backup_size})"
    log_info "Checksum: $checksum"

    # Upload to S3
    if [[ -n "$S3_BUCKET" ]]; then
        upload_to_s3 "$archive_path" "${S3_BUCKET}/postgres/full/"
        upload_to_s3 "${archive_path}.sha256" "${S3_BUCKET}/postgres/full/"
    fi

    echo "BACKUP_FILE=${backup_name}.tar.gz"
    echo "BACKUP_PATH=$archive_path"
    echo "BACKUP_CHECKSUM=$checksum"
}

backup_incremental() {
    log_info "Starting incremental PostgreSQL backup (base backup for PITR)"

    local timestamp
    timestamp=$(generate_timestamp)
    local backup_name="pg_base_${timestamp}"
    local backup_path="${BACKUP_DIR}/base/${backup_name}"

    # Create backup directory
    mkdir -p "${BACKUP_DIR}/base"

    # Run pg_basebackup
    log_info "Running pg_basebackup"
    pg_basebackup \
        -D "$backup_path" \
        -Ft \
        -z \
        -P \
        --wal-method=stream \
        --checkpoint=fast \
        2>&1 | while read -r line; do log_info "$line"; done

    # Calculate checksum for base.tar.gz
    if [[ -f "${backup_path}/base.tar.gz" ]]; then
        local checksum
        checksum=$(calculate_checksum "${backup_path}/base.tar.gz")
        echo "$checksum  base.tar.gz" > "${backup_path}/base.tar.gz.sha256"
        log_info "Base backup checksum: $checksum"
    fi

    # Get backup size
    local backup_size
    backup_size=$(du -sh "$backup_path" | cut -f1)

    log_info "Base backup completed: ${backup_name} (${backup_size})"

    # Upload to S3
    if [[ -n "$S3_BUCKET" ]]; then
        log_info "Uploading base backup to S3"
        aws s3 sync "$backup_path" "${S3_BUCKET}/postgres/base/${backup_name}/"
    fi

    echo "BACKUP_NAME=$backup_name"
    echo "BACKUP_PATH=$backup_path"
}

backup_wal() {
    log_info "Archiving WAL files"

    local wal_dir="${VELOZ_PG_WAL_DIR:-/var/lib/postgresql/data/pg_wal}"
    local archive_dir="${BACKUP_DIR}/wal"

    # Create archive directory
    mkdir -p "$archive_dir"

    # Check if WAL directory exists
    if [[ ! -d "$wal_dir" ]]; then
        log_error "WAL directory not found: $wal_dir"
        exit 1
    fi

    # Archive WAL files
    local archived=0
    for wal_file in "$wal_dir"/*.ready 2>/dev/null; do
        if [[ -f "$wal_file" ]]; then
            local base_name
            base_name=$(basename "$wal_file" .ready)
            local source_file="${wal_dir}/${base_name}"

            if [[ -f "$source_file" ]]; then
                # Compress and copy
                gzip -c "$source_file" > "${archive_dir}/${base_name}.gz"

                # Mark as done
                mv "$wal_file" "${wal_file%.ready}.done"

                archived=$((archived + 1))
                log_debug "Archived: $base_name"
            fi
        fi
    done

    log_info "Archived $archived WAL files"

    # Upload to S3
    if [[ -n "$S3_BUCKET" && $archived -gt 0 ]]; then
        log_info "Syncing WAL archive to S3"
        aws s3 sync "$archive_dir" "${S3_BUCKET}/postgres/wal/" --exclude "*.done"
    fi

    # Cleanup old WAL files
    log_info "Cleaning up WAL files older than ${RETENTION_DAYS} days"
    find "$archive_dir" -name "*.gz" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true

    echo "WAL_ARCHIVED=$archived"
}

upload_to_s3() {
    local local_path="$1"
    local s3_path="$2"

    if command_exists aws; then
        log_info "Uploading to S3: $s3_path"
        s3_upload "$local_path" "$s3_path"
    else
        log_warn "AWS CLI not found, skipping S3 upload"
    fi
}

cleanup_old_backups() {
    log_info "Cleaning up backups older than ${RETENTION_DAYS} days"

    # Full backups
    find "${BACKUP_DIR}" -name "pg_full_*.tar.gz" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true
    find "${BACKUP_DIR}" -name "pg_full_*.tar.gz.sha256" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true

    # Base backups (keep at least one)
    local base_count
    base_count=$(find "${BACKUP_DIR}/base" -maxdepth 1 -type d -name "pg_base_*" 2>/dev/null | wc -l)
    if [[ $base_count -gt 1 ]]; then
        find "${BACKUP_DIR}/base" -maxdepth 1 -type d -name "pg_base_*" -mtime +"$RETENTION_DAYS" -exec rm -rf {} \; 2>/dev/null || true
    fi
}

main() {
    require_commands pg_dump pg_basebackup

    # Get PostgreSQL connection parameters
    get_pg_params

    # Check connection
    if ! check_pg_connection; then
        log_error "Cannot connect to PostgreSQL"
        exit 1
    fi

    # Create backup directory
    mkdir -p "$BACKUP_DIR"

    # Acquire lock
    local lock_file="${BACKUP_DIR}/.backup.lock"
    trap "cleanup_on_exit '$lock_file'" EXIT

    if ! acquire_lock "$lock_file" 300; then
        log_error "Another backup is in progress"
        exit 1
    fi

    # Run backup based on type
    case "$BACKUP_TYPE" in
        full)
            backup_full
            ;;
        incremental)
            backup_incremental
            ;;
        wal)
            backup_wal
            ;;
    esac

    # Cleanup old backups
    cleanup_old_backups

    log_info "PostgreSQL backup completed successfully"
}

main "$@"
