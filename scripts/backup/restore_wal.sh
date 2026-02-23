#!/bin/bash
# restore_wal.sh - Restore VeloZ order WAL files
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
WAL_DIR="${VELOZ_WAL_DIR:-/var/lib/veloz/wal}"
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}/wal"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
RESTORE_SOURCE=""

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Restore VeloZ order WAL files from backup.

Options:
    -w, --wal-dir DIR       WAL destination directory (default: /var/lib/veloz/wal)
    -b, --backup-dir DIR    Local backup directory (default: /backup/wal)
    -s, --s3-bucket BUCKET  S3 bucket for remote backup
    -f, --from SOURCE       Restore source: local, s3 (default: auto-detect)
    -h, --help              Show this help message

Examples:
    # Restore from local backup
    $(basename "$0")

    # Restore from S3
    $(basename "$0") -f s3 -s s3://veloz-backups

    # Restore to specific directory
    $(basename "$0") -w /data/veloz/wal
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
        -f|--from)
            RESTORE_SOURCE="$2"
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

stop_veloz() {
    log_info "Stopping VeloZ service"

    # Try systemctl
    if command_exists systemctl && systemctl is-active --quiet veloz; then
        sudo systemctl stop veloz
        return 0
    fi

    # Try docker
    if command_exists docker; then
        docker stop veloz-engine 2>/dev/null || true
        return 0
    fi

    # Try docker-compose
    if command_exists docker-compose; then
        docker-compose stop veloz-engine 2>/dev/null || true
        return 0
    fi

    log_warn "Could not stop VeloZ automatically - please stop manually"
    return 1
}

start_veloz() {
    log_info "Starting VeloZ service"

    # Try systemctl
    if command_exists systemctl; then
        sudo systemctl start veloz
        return 0
    fi

    # Try docker
    if command_exists docker; then
        docker start veloz-engine 2>/dev/null || true
        return 0
    fi

    # Try docker-compose
    if command_exists docker-compose; then
        docker-compose start veloz-engine 2>/dev/null || true
        return 0
    fi

    log_warn "Could not start VeloZ automatically - please start manually"
    return 1
}

restore_from_local() {
    log_info "Restoring WAL files from local backup: $BACKUP_DIR"

    if [[ ! -d "$BACKUP_DIR" ]]; then
        log_error "Backup directory does not exist: $BACKUP_DIR"
        exit 1
    fi

    local wal_count
    wal_count=$(find "$BACKUP_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)

    if [[ $wal_count -eq 0 ]]; then
        log_warn "No WAL files found in backup directory"
        return 0
    fi

    log_info "Found $wal_count WAL files to restore"

    # Create destination directory
    mkdir -p "$WAL_DIR"

    # Restore files
    rsync -av \
        --exclude='*.tmp' \
        --exclude='*.lock' \
        "$BACKUP_DIR/" "$WAL_DIR/"

    log_info "Restored $wal_count WAL files"
}

restore_from_s3() {
    log_info "Restoring WAL files from S3: ${S3_BUCKET}/wal/"

    if [[ -z "$S3_BUCKET" ]]; then
        log_error "S3 bucket not specified"
        exit 1
    fi

    if ! command_exists aws; then
        log_error "AWS CLI required for S3 restore"
        exit 1
    fi

    # Create destination directory
    mkdir -p "$WAL_DIR"

    # Sync from S3
    aws s3 sync \
        "${S3_BUCKET}/wal/" \
        "$WAL_DIR/" \
        --exclude '*.tmp' \
        --exclude '*.lock'

    local wal_count
    wal_count=$(find "$WAL_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)

    log_info "Restored $wal_count WAL files from S3"
}

verify_wal_files() {
    log_info "Verifying WAL files"

    local wal_count
    wal_count=$(find "$WAL_DIR" -name "*.wal" -type f 2>/dev/null | wc -l)

    if [[ $wal_count -eq 0 ]]; then
        log_warn "No WAL files found after restore"
        return 1
    fi

    log_info "WAL directory contains $wal_count files"

    # Check for current WAL file
    if [[ -f "${WAL_DIR}/orders_current.wal" ]]; then
        local current_size
        current_size=$(du -h "${WAL_DIR}/orders_current.wal" | cut -f1)
        log_info "Current WAL file size: $current_size"
    else
        log_warn "No current WAL file found"
    fi

    # List WAL files
    log_info "WAL files:"
    find "$WAL_DIR" -name "*.wal" -type f -exec ls -lh {} \; | while read -r line; do
        log_debug "$line"
    done

    return 0
}

main() {
    log_info "Starting WAL restore"

    # Auto-detect restore source if not specified
    if [[ -z "$RESTORE_SOURCE" ]]; then
        if [[ -d "$BACKUP_DIR" ]] && [[ -n "$(find "$BACKUP_DIR" -name "*.wal" -type f 2>/dev/null | head -1)" ]]; then
            RESTORE_SOURCE="local"
        elif [[ -n "$S3_BUCKET" ]]; then
            RESTORE_SOURCE="s3"
        else
            log_error "No backup source available"
            exit 1
        fi
        log_info "Auto-detected restore source: $RESTORE_SOURCE"
    fi

    # Stop VeloZ before restore
    stop_veloz || true

    # Backup existing WAL directory
    if [[ -d "$WAL_DIR" ]]; then
        local backup_existing="${WAL_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
        log_info "Backing up existing WAL directory to: $backup_existing"
        mv "$WAL_DIR" "$backup_existing"
    fi

    # Restore based on source
    case "$RESTORE_SOURCE" in
        local)
            restore_from_local
            ;;
        s3)
            restore_from_s3
            ;;
        *)
            log_error "Unknown restore source: $RESTORE_SOURCE"
            exit 1
            ;;
    esac

    # Verify restore
    verify_wal_files

    # Set permissions
    chown -R veloz:veloz "$WAL_DIR" 2>/dev/null || true
    chmod 700 "$WAL_DIR"

    # Start VeloZ (will replay WAL)
    log_info "Starting VeloZ to replay WAL"
    start_veloz || true

    # Wait for service to be ready
    log_info "Waiting for VeloZ to be ready"
    local max_wait=60
    local waited=0
    while [[ $waited -lt $max_wait ]]; do
        if curl -sf http://localhost:8080/health > /dev/null 2>&1; then
            log_info "VeloZ is ready"
            break
        fi
        sleep 2
        waited=$((waited + 2))
    done

    if [[ $waited -ge $max_wait ]]; then
        log_warn "VeloZ may not be ready - check manually"
    fi

    log_info "WAL restore completed successfully"
}

main "$@"
