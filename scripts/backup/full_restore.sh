#!/bin/bash
# full_restore.sh - Complete VeloZ disaster recovery restore
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
SKIP_CONFIRMATION="${VELOZ_SKIP_CONFIRMATION:-false}"
RESTORE_COMPONENTS="all"  # all, config, postgres, wal, secrets

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Perform complete VeloZ disaster recovery restore.

Options:
    -b, --backup-dir DIR    Local backup directory (default: /backup)
    -s, --s3-bucket BUCKET  S3 bucket for remote backups
    -c, --components LIST   Components to restore: all, config, postgres, wal, secrets
    -y, --yes               Skip confirmation prompt
    -h, --help              Show this help message

Components:
    all      - Restore everything (default)
    config   - Configuration files only
    postgres - PostgreSQL database only
    wal      - VeloZ order WAL only
    secrets  - Encrypted secrets only

Examples:
    # Full restore from local backup
    $(basename "$0")

    # Full restore from S3
    $(basename "$0") -s s3://veloz-backups

    # Restore specific components
    $(basename "$0") -c config,postgres

    # Non-interactive restore
    $(basename "$0") -y
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        -s|--s3-bucket)
            S3_BUCKET="$2"
            shift 2
            ;;
        -c|--components)
            RESTORE_COMPONENTS="$2"
            shift 2
            ;;
        -y|--yes)
            SKIP_CONFIRMATION="true"
            shift
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

confirm_restore() {
    if [[ "$SKIP_CONFIRMATION" == "true" ]]; then
        return 0
    fi

    echo ""
    echo "=========================================="
    echo "VeloZ Full Disaster Recovery Restore"
    echo "=========================================="
    echo ""
    echo "This will restore VeloZ from backup."
    echo ""
    echo "Backup source: ${S3_BUCKET:-$BACKUP_DIR}"
    echo "Components: $RESTORE_COMPONENTS"
    echo ""
    echo "WARNING: This will overwrite existing data!"
    echo ""
    read -p "Are you sure you want to continue? (yes/no): " -r response

    if [[ "$response" != "yes" ]]; then
        log_info "Restore cancelled"
        exit 0
    fi
}

stop_all_services() {
    log_info "Stopping all VeloZ services"

    # Stop VeloZ engine
    if command_exists systemctl; then
        sudo systemctl stop veloz 2>/dev/null || true
        sudo systemctl stop postgresql 2>/dev/null || true
    fi

    # Stop Docker containers
    if command_exists docker-compose; then
        docker-compose down 2>/dev/null || true
    elif command_exists docker; then
        docker stop veloz-engine veloz-postgres 2>/dev/null || true
    fi

    log_info "Services stopped"
}

start_all_services() {
    log_info "Starting all VeloZ services"

    # Start PostgreSQL first
    if command_exists systemctl; then
        sudo systemctl start postgresql 2>/dev/null || true
        sleep 5
        sudo systemctl start veloz 2>/dev/null || true
    fi

    # Start Docker containers
    if command_exists docker-compose; then
        docker-compose up -d 2>/dev/null || true
    fi

    log_info "Services started"
}

download_from_s3() {
    if [[ -z "$S3_BUCKET" ]]; then
        return 0
    fi

    if ! command_exists aws; then
        log_error "AWS CLI required for S3 restore"
        exit 1
    fi

    log_info "Downloading backups from S3"

    # Create local backup directory
    mkdir -p "$BACKUP_DIR"

    # Sync config backups
    log_info "Downloading config backups"
    aws s3 sync "${S3_BUCKET}/config/" "${BACKUP_DIR}/config/" || true

    # Sync PostgreSQL backups
    log_info "Downloading PostgreSQL backups"
    aws s3 sync "${S3_BUCKET}/postgres/" "${BACKUP_DIR}/postgres/" || true

    # Sync WAL backups
    log_info "Downloading WAL backups"
    aws s3 sync "${S3_BUCKET}/wal/" "${BACKUP_DIR}/wal/" || true

    # Sync secrets (if available)
    log_info "Downloading secrets backups"
    aws s3 sync "${S3_BUCKET}/secrets/" "${BACKUP_DIR}/secrets/" || true

    log_info "S3 download completed"
}

restore_secrets() {
    log_info "Restoring secrets"

    local secrets_dir="${BACKUP_DIR}/secrets"
    local dest_dir="${VELOZ_SECRETS_DIR:-/etc/veloz/secrets}"

    if [[ ! -d "$secrets_dir" ]]; then
        log_warn "No secrets backup found"
        return 0
    fi

    # Find latest encrypted secrets backup
    local latest_secrets
    latest_secrets=$(find "$secrets_dir" -name "secrets_*.tar.gz.gpg" -type f 2>/dev/null | sort -r | head -1)

    if [[ -z "$latest_secrets" ]]; then
        log_warn "No encrypted secrets backup found"
        return 0
    fi

    log_info "Restoring secrets from: $(basename "$latest_secrets")"

    # Create destination directory
    mkdir -p "$dest_dir"

    # Decrypt and extract
    if command_exists gpg; then
        gpg --decrypt "$latest_secrets" 2>/dev/null | tar -xzf - -C "$dest_dir"

        # Set permissions
        chmod 600 "$dest_dir"/* 2>/dev/null || true
        chown veloz:veloz "$dest_dir"/* 2>/dev/null || true

        log_info "Secrets restored successfully"
    else
        log_error "GPG not found - cannot decrypt secrets"
        return 1
    fi
}

restore_config() {
    log_info "Restoring configuration"

    local config_dir="${BACKUP_DIR}/config"
    if [[ ! -d "$config_dir" ]]; then
        log_warn "No config backup found"
        return 0
    fi

    # Find latest config backup
    local latest_config
    latest_config=$(find "$config_dir" -name "config_*.tar.gz" -type f 2>/dev/null | sort -r | head -1)

    if [[ -z "$latest_config" ]]; then
        log_warn "No config backup found"
        return 0
    fi

    log_info "Restoring config from: $(basename "$latest_config")"

    # Verify checksum if available
    local checksum_file="${latest_config}.sha256"
    if [[ -f "$checksum_file" ]]; then
        local expected
        expected=$(cat "$checksum_file" | cut -d' ' -f1)
        if ! verify_checksum "$latest_config" "$expected" 2>/dev/null; then
            log_error "Config backup checksum mismatch"
            return 1
        fi
    fi

    # Extract to root (preserves /etc/veloz structure)
    tar -xzf "$latest_config" -C /

    log_info "Configuration restored successfully"
}

restore_postgres() {
    log_info "Restoring PostgreSQL database"

    # Use the dedicated restore script
    if [[ -x "${SCRIPT_DIR}/restore_postgres.sh" ]]; then
        "${SCRIPT_DIR}/restore_postgres.sh" -t full -b "${BACKUP_DIR}/postgres"
    else
        log_error "PostgreSQL restore script not found"
        return 1
    fi
}

restore_wal() {
    log_info "Restoring VeloZ WAL"

    # Use the dedicated restore script
    if [[ -x "${SCRIPT_DIR}/restore_wal.sh" ]]; then
        "${SCRIPT_DIR}/restore_wal.sh" -b "${BACKUP_DIR}/wal"
    else
        log_error "WAL restore script not found"
        return 1
    fi
}

verify_restore() {
    log_info "Verifying restore"

    local errors=0

    # Wait for services to start
    sleep 10

    # Check VeloZ health
    log_info "Checking VeloZ health"
    local max_wait=60
    local waited=0
    while [[ $waited -lt $max_wait ]]; do
        if curl -sf http://localhost:8080/health > /dev/null 2>&1; then
            log_info "VeloZ health check passed"
            break
        fi
        sleep 5
        waited=$((waited + 5))
    done

    if [[ $waited -ge $max_wait ]]; then
        log_error "VeloZ health check failed"
        errors=$((errors + 1))
    fi

    # Check order state
    log_info "Checking order state"
    if curl -sf http://localhost:8080/api/orders_state > /dev/null 2>&1; then
        log_info "Order state endpoint accessible"
    else
        log_warn "Order state endpoint not accessible"
    fi

    # Check PostgreSQL
    if command_exists psql; then
        get_pg_params
        if check_pg_connection; then
            log_info "PostgreSQL connection successful"
        else
            log_error "PostgreSQL connection failed"
            errors=$((errors + 1))
        fi
    fi

    if [[ $errors -gt 0 ]]; then
        log_error "Restore verification failed with $errors errors"
        return 1
    fi

    log_info "Restore verification passed"
    return 0
}

main() {
    log_info "Starting VeloZ full disaster recovery restore"

    # Confirm restore
    confirm_restore

    # Download from S3 if configured
    download_from_s3

    # Stop all services
    stop_all_services

    # Parse components
    IFS=',' read -ra COMPONENTS <<< "$RESTORE_COMPONENTS"

    # Restore in order: secrets -> config -> postgres -> wal
    for component in "${COMPONENTS[@]}"; do
        case "$component" in
            all)
                restore_secrets
                restore_config
                restore_postgres
                restore_wal
                break
                ;;
            secrets)
                restore_secrets
                ;;
            config)
                restore_config
                ;;
            postgres)
                restore_postgres
                ;;
            wal)
                restore_wal
                ;;
            *)
                log_warn "Unknown component: $component"
                ;;
        esac
    done

    # Start all services
    start_all_services

    # Verify restore
    verify_restore

    log_info "=========================================="
    log_info "VeloZ full restore completed successfully"
    log_info "=========================================="

    # Send notification
    send_notification "success" "VeloZ disaster recovery restore completed successfully"
}

main "$@"
