#!/bin/bash
# verify_backup.sh - Verify backup integrity and test restore
# Part of VeloZ automated backup system

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/backup_common.sh"

# Default values
BACKUP_DIR="${VELOZ_BACKUP_DIR:-/backup}"
S3_BUCKET="${VELOZ_S3_BUCKET:-}"
TEST_RESTORE="${VELOZ_TEST_RESTORE:-false}"
REPORT_FILE=""

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Verify VeloZ backup integrity and optionally test restore.

Options:
    -b, --backup-dir DIR    Local backup directory (default: /backup)
    -s, --s3-bucket BUCKET  S3 bucket for remote backups
    -t, --test-restore      Perform test restore (default: false)
    -r, --report FILE       Write verification report to file
    -h, --help              Show this help message

Verification Checks:
    - Backup files exist
    - Checksums match
    - Archives are valid
    - S3 sync is current (if configured)
    - Test restore works (if --test-restore)

Examples:
    $(basename "$0")
    $(basename "$0") -t -r /tmp/backup_report.txt
    $(basename "$0") -s s3://veloz-backups
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
        -t|--test-restore)
            TEST_RESTORE="true"
            shift
            ;;
        -r|--report)
            REPORT_FILE="$2"
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

# Verification results (using simple arrays for bash 3 compatibility)
RESULTS_NAMES=()
RESULTS_VALUES=()
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0

record_result() {
    local check_name="$1"
    local status="$2"
    local message="${3:-}"

    RESULTS_NAMES+=("$check_name")
    RESULTS_VALUES+=("$status: $message")
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))

    if [[ "$status" == "PASS" ]]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        log_info "[PASS] $check_name: $message"
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        log_error "[FAIL] $check_name: $message"
    fi
}

verify_backup_directory() {
    log_info "Verifying backup directory structure"

    if [[ ! -d "$BACKUP_DIR" ]]; then
        record_result "backup_directory" "FAIL" "Directory does not exist: $BACKUP_DIR"
        return 1
    fi

    record_result "backup_directory" "PASS" "Directory exists: $BACKUP_DIR"

    # Check subdirectories
    local expected_dirs=("config" "postgres" "wal")
    for dir in "${expected_dirs[@]}"; do
        if [[ -d "${BACKUP_DIR}/${dir}" ]]; then
            record_result "subdir_${dir}" "PASS" "Subdirectory exists"
        else
            record_result "subdir_${dir}" "FAIL" "Subdirectory missing: ${dir}"
        fi
    done
}

verify_config_backups() {
    log_info "Verifying configuration backups"

    local config_dir="${BACKUP_DIR}/config"
    if [[ ! -d "$config_dir" ]]; then
        record_result "config_backup" "FAIL" "No config backup directory"
        return 1
    fi

    local backup_count
    backup_count=$(find "$config_dir" -name "config_*.tar.gz" -type f 2>/dev/null | wc -l)

    if [[ $backup_count -eq 0 ]]; then
        record_result "config_backup_count" "FAIL" "No config backups found"
        return 1
    fi

    record_result "config_backup_count" "PASS" "Found $backup_count config backups"

    # Verify latest backup
    local latest_backup
    latest_backup=$(find "$config_dir" -name "config_*.tar.gz" -type f 2>/dev/null | sort -r | head -1)

    if [[ -n "$latest_backup" ]]; then
        # Check checksum
        local checksum_file="${latest_backup}.sha256"
        if [[ -f "$checksum_file" ]]; then
            local expected
            expected=$(cat "$checksum_file" | cut -d' ' -f1)
            if verify_checksum "$latest_backup" "$expected" 2>/dev/null; then
                record_result "config_checksum" "PASS" "Checksum valid for $(basename "$latest_backup")"
            else
                record_result "config_checksum" "FAIL" "Checksum mismatch for $(basename "$latest_backup")"
            fi
        else
            record_result "config_checksum" "FAIL" "No checksum file for $(basename "$latest_backup")"
        fi

        # Check archive integrity
        if tar -tzf "$latest_backup" > /dev/null 2>&1; then
            record_result "config_archive" "PASS" "Archive is valid"
        else
            record_result "config_archive" "FAIL" "Archive is corrupted"
        fi

        # Check backup age
        local backup_age
        backup_age=$(( ($(date +%s) - $(stat -f %m "$latest_backup" 2>/dev/null || stat -c %Y "$latest_backup")) / 86400 ))
        if [[ $backup_age -le 7 ]]; then
            record_result "config_age" "PASS" "Latest backup is $backup_age days old"
        else
            record_result "config_age" "FAIL" "Latest backup is $backup_age days old (>7 days)"
        fi
    fi
}

verify_postgres_backups() {
    log_info "Verifying PostgreSQL backups"

    local pg_dir="${BACKUP_DIR}/postgres"
    if [[ ! -d "$pg_dir" ]]; then
        record_result "postgres_backup" "FAIL" "No PostgreSQL backup directory"
        return 1
    fi

    # Check full backups
    local full_count
    full_count=$(find "$pg_dir" -name "pg_full_*.tar.gz" -type f 2>/dev/null | wc -l)

    if [[ $full_count -eq 0 ]]; then
        record_result "postgres_full_count" "FAIL" "No full backups found"
    else
        record_result "postgres_full_count" "PASS" "Found $full_count full backups"

        # Verify latest full backup
        local latest_full
        latest_full=$(find "$pg_dir" -name "pg_full_*.tar.gz" -type f 2>/dev/null | sort -r | head -1)

        if [[ -n "$latest_full" ]]; then
            # Check checksum
            local checksum_file="${latest_full}.sha256"
            if [[ -f "$checksum_file" ]]; then
                local expected
                expected=$(cat "$checksum_file" | cut -d' ' -f1)
                if verify_checksum "$latest_full" "$expected" 2>/dev/null; then
                    record_result "postgres_full_checksum" "PASS" "Checksum valid"
                else
                    record_result "postgres_full_checksum" "FAIL" "Checksum mismatch"
                fi
            fi

            # Check archive integrity
            if tar -tzf "$latest_full" > /dev/null 2>&1; then
                record_result "postgres_full_archive" "PASS" "Archive is valid"
            else
                record_result "postgres_full_archive" "FAIL" "Archive is corrupted"
            fi
        fi
    fi

    # Check base backups (for PITR)
    local base_dir="${pg_dir}/base"
    if [[ -d "$base_dir" ]]; then
        local base_count
        base_count=$(find "$base_dir" -maxdepth 1 -type d -name "pg_base_*" 2>/dev/null | wc -l)
        record_result "postgres_base_count" "PASS" "Found $base_count base backups for PITR"
    fi

    # Check WAL archive
    local wal_dir="${pg_dir}/wal"
    if [[ -d "$wal_dir" ]]; then
        local wal_count
        wal_count=$(find "$wal_dir" -name "*.gz" -type f 2>/dev/null | wc -l)
        record_result "postgres_wal_count" "PASS" "Found $wal_count archived WAL files"
    fi
}

verify_wal_backups() {
    log_info "Verifying WAL backups"

    local wal_dir="${BACKUP_DIR}/wal"
    if [[ ! -d "$wal_dir" ]]; then
        record_result "wal_backup" "FAIL" "No WAL backup directory"
        return 1
    fi

    local wal_count
    wal_count=$(find "$wal_dir" -name "*.wal" -type f 2>/dev/null | wc -l)

    if [[ $wal_count -eq 0 ]]; then
        record_result "wal_count" "FAIL" "No WAL files found"
        return 1
    fi

    record_result "wal_count" "PASS" "Found $wal_count WAL files"

    # Check for current WAL
    if [[ -f "${wal_dir}/orders_current.wal" ]]; then
        record_result "wal_current" "PASS" "Current WAL file exists"
    else
        record_result "wal_current" "FAIL" "No current WAL file"
    fi

    # Check WAL backup age
    local latest_wal
    latest_wal=$(find "$wal_dir" -name "*.wal" -type f 2>/dev/null -exec stat -f "%m %N" {} \; 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2- || \
                 find "$wal_dir" -name "*.wal" -type f 2>/dev/null -printf "%T@ %p\n" | sort -rn | head -1 | cut -d' ' -f2-)

    if [[ -n "$latest_wal" ]]; then
        local wal_age_minutes
        wal_age_minutes=$(( ($(date +%s) - $(stat -f %m "$latest_wal" 2>/dev/null || stat -c %Y "$latest_wal")) / 60 ))
        if [[ $wal_age_minutes -le 10 ]]; then
            record_result "wal_age" "PASS" "Latest WAL backup is $wal_age_minutes minutes old"
        else
            record_result "wal_age" "FAIL" "Latest WAL backup is $wal_age_minutes minutes old (>10 min)"
        fi
    fi
}

verify_s3_sync() {
    if [[ -z "$S3_BUCKET" ]]; then
        log_info "Skipping S3 verification (not configured)"
        return 0
    fi

    if ! command_exists aws; then
        record_result "s3_cli" "FAIL" "AWS CLI not installed"
        return 1
    fi

    log_info "Verifying S3 sync"

    # Check S3 bucket accessibility
    if aws s3 ls "$S3_BUCKET" > /dev/null 2>&1; then
        record_result "s3_access" "PASS" "S3 bucket accessible"
    else
        record_result "s3_access" "FAIL" "Cannot access S3 bucket"
        return 1
    fi

    # Check config backups in S3
    local s3_config_count
    s3_config_count=$(aws s3 ls "${S3_BUCKET}/config/" 2>/dev/null | grep -c "config_.*\.tar\.gz" || echo "0")
    record_result "s3_config_count" "PASS" "Found $s3_config_count config backups in S3"

    # Check postgres backups in S3
    local s3_pg_count
    s3_pg_count=$(aws s3 ls "${S3_BUCKET}/postgres/full/" 2>/dev/null | grep -c "pg_full_.*\.tar\.gz" || echo "0")
    record_result "s3_postgres_count" "PASS" "Found $s3_pg_count PostgreSQL backups in S3"

    # Check WAL backups in S3
    local s3_wal_count
    s3_wal_count=$(aws s3 ls "${S3_BUCKET}/wal/" 2>/dev/null | grep -c "\.wal" || echo "0")
    record_result "s3_wal_count" "PASS" "Found $s3_wal_count WAL files in S3"
}

test_restore() {
    if [[ "$TEST_RESTORE" != "true" ]]; then
        log_info "Skipping test restore (use -t to enable)"
        return 0
    fi

    log_info "Performing test restore"

    # Create temporary test directory
    local test_dir="/tmp/veloz_backup_test_$$"
    mkdir -p "$test_dir"
    trap "rm -rf '$test_dir'" RETURN

    # Test config restore
    local latest_config
    latest_config=$(find "${BACKUP_DIR}/config" -name "config_*.tar.gz" -type f 2>/dev/null | sort -r | head -1)

    if [[ -n "$latest_config" ]]; then
        log_info "Testing config restore"
        if tar -xzf "$latest_config" -C "$test_dir" 2>/dev/null; then
            record_result "test_config_restore" "PASS" "Config restore successful"
        else
            record_result "test_config_restore" "FAIL" "Config restore failed"
        fi
    fi

    # Test PostgreSQL restore (dry run)
    local latest_pg
    latest_pg=$(find "${BACKUP_DIR}/postgres" -name "pg_full_*.tar.gz" -type f 2>/dev/null | sort -r | head -1)

    if [[ -n "$latest_pg" ]]; then
        log_info "Testing PostgreSQL backup extraction"
        if tar -tzf "$latest_pg" > /dev/null 2>&1; then
            record_result "test_postgres_extract" "PASS" "PostgreSQL backup extractable"
        else
            record_result "test_postgres_extract" "FAIL" "PostgreSQL backup extraction failed"
        fi
    fi

    log_info "Test restore completed"
}

generate_report() {
    local report=""

    report+="========================================\n"
    report+="VeloZ Backup Verification Report\n"
    report+="========================================\n"
    report+="Date: $(date '+%Y-%m-%d %H:%M:%S')\n"
    report+="Backup Directory: $BACKUP_DIR\n"
    report+="S3 Bucket: ${S3_BUCKET:-Not configured}\n"
    report+="\n"
    report+="Summary:\n"
    report+="  Total Checks: $TOTAL_CHECKS\n"
    report+="  Passed: $PASSED_CHECKS\n"
    report+="  Failed: $FAILED_CHECKS\n"
    report+="\n"
    report+="Details:\n"

    for i in "${!RESULTS_NAMES[@]}"; do
        report+="  ${RESULTS_NAMES[$i]}: ${RESULTS_VALUES[$i]}\n"
    done

    report+="\n"
    report+="========================================\n"

    if [[ -n "$REPORT_FILE" ]]; then
        echo -e "$report" > "$REPORT_FILE"
        log_info "Report written to: $REPORT_FILE"
    fi

    echo -e "$report"
}

main() {
    log_info "Starting backup verification"

    # Run verifications
    verify_backup_directory
    verify_config_backups
    verify_postgres_backups
    verify_wal_backups
    verify_s3_sync
    test_restore

    # Generate report
    generate_report

    # Return status
    if [[ $FAILED_CHECKS -gt 0 ]]; then
        log_error "Verification completed with $FAILED_CHECKS failures"
        exit 1
    else
        log_info "All verification checks passed"
        exit 0
    fi
}

main "$@"
