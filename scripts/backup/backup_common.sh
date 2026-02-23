#!/bin/bash
# backup_common.sh - Common functions for VeloZ backup scripts

# Colors for output (if terminal supports it)
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    BLUE='\033[0;34m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*" >&2
}

log_debug() {
    if [[ "${VELOZ_DEBUG:-0}" == "1" ]]; then
        echo -e "${BLUE}[DEBUG]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
    fi
}

# Check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Validate required commands
require_commands() {
    local missing=()
    for cmd in "$@"; do
        if ! command_exists "$cmd"; then
            missing+=("$cmd")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        log_error "Missing required commands: ${missing[*]}"
        exit 1
    fi
}

# Get PostgreSQL connection parameters
get_pg_params() {
    export PGHOST="${VELOZ_PG_HOST:-localhost}"
    export PGPORT="${VELOZ_PG_PORT:-5432}"
    export PGUSER="${VELOZ_PG_USER:-veloz}"
    export PGDATABASE="${VELOZ_PG_DATABASE:-veloz}"

    # Password from environment or file
    if [[ -n "${VELOZ_PG_PASSWORD:-}" ]]; then
        export PGPASSWORD="$VELOZ_PG_PASSWORD"
    elif [[ -f "${VELOZ_PG_PASSWORD_FILE:-}" ]]; then
        export PGPASSWORD=$(cat "$VELOZ_PG_PASSWORD_FILE")
    fi
}

# Check PostgreSQL connectivity
check_pg_connection() {
    get_pg_params
    if ! pg_isready -q; then
        log_error "Cannot connect to PostgreSQL at ${PGHOST}:${PGPORT}"
        return 1
    fi
    return 0
}

# Generate timestamp for backup files
generate_timestamp() {
    date +%Y%m%d_%H%M%S
}

# Calculate file checksum
calculate_checksum() {
    local file="$1"
    sha256sum "$file" | cut -d' ' -f1
}

# Verify file checksum
verify_checksum() {
    local file="$1"
    local expected_checksum="$2"

    local actual_checksum
    actual_checksum=$(calculate_checksum "$file")

    if [[ "$actual_checksum" == "$expected_checksum" ]]; then
        return 0
    else
        log_error "Checksum mismatch for $file"
        log_error "Expected: $expected_checksum"
        log_error "Actual: $actual_checksum"
        return 1
    fi
}

# Download from S3 with retry
s3_download() {
    local s3_path="$1"
    local local_path="$2"
    local max_retries="${3:-3}"

    local retry=0
    while [[ $retry -lt $max_retries ]]; do
        if aws s3 cp "$s3_path" "$local_path"; then
            return 0
        fi
        retry=$((retry + 1))
        log_warn "S3 download failed, retry $retry/$max_retries"
        sleep $((retry * 2))
    done

    log_error "S3 download failed after $max_retries attempts"
    return 1
}

# Upload to S3 with retry
s3_upload() {
    local local_path="$1"
    local s3_path="$2"
    local max_retries="${3:-3}"

    local retry=0
    while [[ $retry -lt $max_retries ]]; do
        if aws s3 cp "$local_path" "$s3_path"; then
            return 0
        fi
        retry=$((retry + 1))
        log_warn "S3 upload failed, retry $retry/$max_retries"
        sleep $((retry * 2))
    done

    log_error "S3 upload failed after $max_retries attempts"
    return 1
}

# Send notification (supports multiple backends)
send_notification() {
    local level="$1"
    local message="$2"

    # Slack notification
    if [[ -n "${VELOZ_SLACK_WEBHOOK:-}" ]]; then
        local color
        case "$level" in
            success) color="good" ;;
            warning) color="warning" ;;
            error) color="danger" ;;
            *) color="#439FE0" ;;
        esac

        curl -s -X POST "$VELOZ_SLACK_WEBHOOK" \
            -H 'Content-Type: application/json' \
            -d "{\"attachments\":[{\"color\":\"$color\",\"text\":\"$message\"}]}" \
            > /dev/null 2>&1 || true
    fi

    # PagerDuty notification (for errors only)
    if [[ "$level" == "error" && -n "${VELOZ_PAGERDUTY_KEY:-}" ]]; then
        curl -s -X POST "https://events.pagerduty.com/v2/enqueue" \
            -H 'Content-Type: application/json' \
            -d "{
                \"routing_key\": \"$VELOZ_PAGERDUTY_KEY\",
                \"event_action\": \"trigger\",
                \"payload\": {
                    \"summary\": \"VeloZ Backup: $message\",
                    \"severity\": \"critical\",
                    \"source\": \"veloz-backup\"
                }
            }" \
            > /dev/null 2>&1 || true
    fi
}

# Lock file management
acquire_lock() {
    local lock_file="$1"
    local timeout="${2:-60}"

    local start_time
    start_time=$(date +%s)

    while ! mkdir "$lock_file" 2>/dev/null; do
        local current_time
        current_time=$(date +%s)
        local elapsed=$((current_time - start_time))

        if [[ $elapsed -ge $timeout ]]; then
            log_error "Failed to acquire lock: $lock_file (timeout after ${timeout}s)"
            return 1
        fi

        log_debug "Waiting for lock: $lock_file"
        sleep 1
    done

    # Store PID in lock directory
    echo $$ > "${lock_file}/pid"
    log_debug "Acquired lock: $lock_file"
    return 0
}

release_lock() {
    local lock_file="$1"

    if [[ -d "$lock_file" ]]; then
        rm -rf "$lock_file"
        log_debug "Released lock: $lock_file"
    fi
}

# Cleanup handler
cleanup_on_exit() {
    local lock_file="${1:-}"

    if [[ -n "$lock_file" ]]; then
        release_lock "$lock_file"
    fi
}

# Human-readable file size
human_size() {
    local bytes="$1"

    if [[ $bytes -lt 1024 ]]; then
        echo "${bytes}B"
    elif [[ $bytes -lt 1048576 ]]; then
        echo "$((bytes / 1024))KB"
    elif [[ $bytes -lt 1073741824 ]]; then
        echo "$((bytes / 1048576))MB"
    else
        echo "$((bytes / 1073741824))GB"
    fi
}
