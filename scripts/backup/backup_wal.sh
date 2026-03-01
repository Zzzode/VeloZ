#!/bin/bash
# backup_wal.sh - Continuous WAL archiving for VeloZ order recovery
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
WAL_DIR="${VELOZ_WAL_DIR:-/var/lib/veloz/wal}"
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}/wal"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
RETENTION_DAYS="${VELOZ_WAL_RETENTION_DAYS:-7}"
SYNC_MODE="${VELOZ_WAL_SYNC_MODE:-rsync}"  # rsync, cp, s3

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Backup VeloZ order WAL files for disaster recovery.

Options:
    -w, --wal-dir DIR       WAL source directory (default: /var/lib/veloz/wal)
    -b, --backup-dir DIR    Local backup directory (default: /backup/wal)
    -s, --s3-bucket BUCKET  S3 bucket for remote backup (optional)
    -r, --retention DAYS    Retention period in days (default: 7)
    -m, --mode MODE         Sync mode: rsync, cp, s3 (default: rsync)
    -h, --help              Show this help message

Environment variables:
    VELOZ_WAL_DIR           WAL source directory
    VELOZ_BACKUP_DIR        Base backup directory
    VELOZ_S3_BUCKET         S3 bucket for remote backups
    VELOZ_WAL_RETENTION_DAYS  Retention period

Examples:
    $(basename "$0")
    $(basename "$0") -w /data/wal -s s3://veloz-backups
    $(basename "$0") -m s3  # Direct S3 sync
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -w|--wal-dir)
            WAL_DIR="$2"
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
        -m|--mode)
            SYNC_MODE="$2"
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

sync_with_rsync() {
    log_info "Syncing WAL files using rsync"

    rsync -av \
        --delete \
        --exclude='*.tmp' \
        --exclude='*.lock' \
        "$WAL_DIR/" "$BACKUP_DIR/"

    local synced_count
    synced_count=$(find "$BACKUP_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)
    log_info "Synced $synced_count WAL files to local backup"
}

sync_with_cp() {
    log_info "Syncing WAL files using cp"

    # Copy only newer files
    for wal_file in "$WAL_DIR"/*.wal; do
        if [[ -f "$wal_file" ]]; then
            local base_name
            base_name=$(basename "$wal_file")
            local dest_file="${BACKUP_DIR}/${base_name}"

            if [[ ! -f "$dest_file" ]] || [[ "$wal_file" -nt "$dest_file" ]]; then
                cp -p "$wal_file" "$dest_file"
                log_debug "Copied: $base_name"
            fi
        fi
    done
}

sync_to_s3() {
    if [[ -z "$S3_BUCKET" ]]; then
        log_warn "S3 bucket not configured, skipping S3 sync"
        return
    fi

    if ! command_exists aws; then
        log_warn "AWS CLI not found, skipping S3 sync"
        return
    fi

    log_info "Syncing WAL files to S3: ${S3_BUCKET}/wal/"

    # Sync from local backup to S3
    aws s3 sync \
        "$BACKUP_DIR/" \
        "${S3_BUCKET}/wal/" \
        --delete \
        --exclude '*.tmp' \
        --exclude '*.lock'

    log_info "S3 sync completed"
}

sync_direct_s3() {
    if [[ -z "$S3_BUCKET" ]]; then
        log_error "S3 bucket not configured for direct S3 mode"
        exit 1
    fi

    if ! command_exists aws; then
        log_error "AWS CLI required for direct S3 mode"
        exit 1
    fi

    log_info "Syncing WAL files directly to S3: ${S3_BUCKET}/wal/"

    aws s3 sync \
        "$WAL_DIR/" \
        "${S3_BUCKET}/wal/" \
        --delete \
        --exclude '*.tmp' \
        --exclude '*.lock'

    log_info "Direct S3 sync completed"
}

cleanup_old_files() {
    log_info "Cleaning up WAL files older than ${RETENTION_DAYS} days"

    # Local cleanup
    find "$BACKUP_DIR" -name "*.wal" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true

    # S3 cleanup (if configured)
    if [[ -n "$S3_BUCKET" ]] && command_exists aws; then
        log_info "Cleaning up old WAL files in S3"

        # List and delete old files
        local cutoff_date
        cutoff_date=$(date -d "-${RETENTION_DAYS} days" +%Y-%m-%d 2>/dev/null || date -v-${RETENTION_DAYS}d +%Y-%m-%d)

        aws s3 ls "${S3_BUCKET}/wal/" | while read -r line; do
            local file_date
            file_date=$(echo "$line" | awk '{print $1}')
            local file_name
            file_name=$(echo "$line" | awk '{print $4}')

            if [[ "$file_date" < "$cutoff_date" && -n "$file_name" ]]; then
                aws s3 rm "${S3_BUCKET}/wal/${file_name}" || true
                log_debug "Deleted from S3: $file_name"
            fi
        done
    fi
}

verify_backup() {
    log_info "Verifying WAL backup integrity"

    local source_count
    source_count=$(find "$WAL_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)

    local backup_count
    backup_count=$(find "$BACKUP_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)

    log_info "Source WAL files: $source_count"
    log_info "Backup WAL files: $backup_count"

    if [[ $backup_count -lt $source_count ]]; then
        log_warn "Backup may be incomplete: $backup_count < $source_count"
        return 1
    fi

    # Verify current WAL file exists
    if [[ -f "${WAL_DIR}/orders_current.wal" ]]; then
        if [[ ! -f "${BACKUP_DIR}/orders_current.wal" ]]; then
            log_warn "Current WAL file not backed up"
            return 1
        fi
    fi

    log_info "WAL backup verification passed"
    return 0
}

main() {
    log_info "Starting WAL backup"

    # Validate source directory
    if [[ ! -d "$WAL_DIR" ]]; then
        log_warn "WAL directory does not exist: $WAL_DIR"
        log_info "Creating empty backup directory for future use"
        mkdir -p "$BACKUP_DIR"
        exit 0
    fi

    # Create backup directory
    mkdir -p "$BACKUP_DIR"

    # Acquire lock
    local lock_file="${BACKUP_DIR}/.wal_backup.lock"
    trap "cleanup_on_exit '$lock_file'" EXIT

    if ! acquire_lock "$lock_file" 60; then
        log_error "Another WAL backup is in progress"
        exit 1
    fi

    # Run sync based on mode
    case "$SYNC_MODE" in
        rsync)
            sync_with_rsync
            sync_to_s3
            ;;
        cp)
            sync_with_cp
            sync_to_s3
            ;;
        s3)
            sync_direct_s3
            ;;
        *)
            log_error "Unknown sync mode: $SYNC_MODE"
            exit 1
            ;;
    esac

    # Verify backup
    verify_backup || true

    # Cleanup old files
    cleanup_old_files

    # Log completion
    log_info "WAL backup completed successfully"

    # Output stats
    local backup_size
    backup_size=$(du -sh "$BACKUP_DIR" 2>/dev/null | cut -f1 || echo "0")
    echo "BACKUP_DIR=$BACKUP_DIR"
    echo "BACKUP_SIZE=$backup_size"
}

main "$@"
