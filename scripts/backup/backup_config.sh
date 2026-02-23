#!/bin/bash
# backup_config.sh - Backup configuration files
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
CONFIG_DIR="${VELOZ_CONFIG_DIR:-/etc/veloz}"
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}/config"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
RETENTION_DAYS="${VELOZ_CONFIG_RETENTION_DAYS:-30}"

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Backup VeloZ configuration files.

Options:
    -c, --config-dir DIR    Configuration directory (default: /etc/veloz)
    -b, --backup-dir DIR    Local backup directory (default: /backup/config)
    -s, --s3-bucket BUCKET  S3 bucket for remote backup (optional)
    -r, --retention DAYS    Retention period in days (default: 30)
    -h, --help              Show this help message

Environment variables:
    VELOZ_CONFIG_DIR        Configuration directory
    VELOZ_BACKUP_DIR        Base backup directory
    VELOZ_S3_BUCKET         S3 bucket for remote backups
    VELOZ_CONFIG_RETENTION_DAYS  Retention period

Examples:
    $(basename "$0")
    $(basename "$0") -c /etc/veloz -s s3://veloz-backups
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--config-dir)
            CONFIG_DIR="$2"
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
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

main() {
    log_info "Starting configuration backup"

    # Validate source directory
    if [[ ! -d "$CONFIG_DIR" ]]; then
        log_error "Configuration directory does not exist: $CONFIG_DIR"
        exit 1
    fi

    # Create backup directory
    mkdir -p "$BACKUP_DIR"

    # Generate backup filename
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    BACKUP_FILE="config_${TIMESTAMP}.tar.gz"
    BACKUP_PATH="${BACKUP_DIR}/${BACKUP_FILE}"

    # Create backup archive (excluding secrets)
    log_info "Creating backup archive: $BACKUP_PATH"
    tar -czf "$BACKUP_PATH" \
        -C "$(dirname "$CONFIG_DIR")" \
        "$(basename "$CONFIG_DIR")" \
        --exclude='*.secret' \
        --exclude='*.key' \
        --exclude='.env' \
        2>/dev/null || true

    # Verify backup was created
    if [[ ! -f "$BACKUP_PATH" ]]; then
        log_error "Failed to create backup archive"
        exit 1
    fi

    BACKUP_SIZE=$(du -h "$BACKUP_PATH" | cut -f1)
    log_info "Backup created: $BACKUP_FILE (${BACKUP_SIZE})"

    # Calculate checksum
    CHECKSUM=$(sha256sum "$BACKUP_PATH" | cut -d' ' -f1)
    echo "$CHECKSUM  $BACKUP_FILE" > "${BACKUP_PATH}.sha256"
    log_info "Checksum: $CHECKSUM"

    # Upload to S3 if configured
    if [[ -n "$S3_BUCKET" ]]; then
        log_info "Uploading to S3: ${S3_BUCKET}/config/"
        if command -v aws &> /dev/null; then
            aws s3 cp "$BACKUP_PATH" "${S3_BUCKET}/config/${BACKUP_FILE}"
            aws s3 cp "${BACKUP_PATH}.sha256" "${S3_BUCKET}/config/${BACKUP_FILE}.sha256"
            log_info "S3 upload completed"
        else
            log_warn "AWS CLI not found, skipping S3 upload"
        fi
    fi

    # Cleanup old backups
    log_info "Cleaning up backups older than ${RETENTION_DAYS} days"
    find "$BACKUP_DIR" -name "config_*.tar.gz" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true
    find "$BACKUP_DIR" -name "config_*.tar.gz.sha256" -mtime +"$RETENTION_DAYS" -delete 2>/dev/null || true

    # Log completion
    log_info "Configuration backup completed successfully"

    # Output backup info for automation
    echo "BACKUP_FILE=$BACKUP_FILE"
    echo "BACKUP_PATH=$BACKUP_PATH"
    echo "BACKUP_CHECKSUM=$CHECKSUM"
}

main "$@"
