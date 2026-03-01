#!/bin/bash
# failover.sh - Manual failover script for VeloZ HA
# Usage: ./failover.sh [postgres|redis|engine] [promote|status]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') $*"
}

usage() {
    cat << EOF
Usage: $(basename "$0") <component> <action>

Manual failover management for VeloZ HA.

Components:
    postgres    PostgreSQL primary/standby
    redis       Redis master/replica (via Sentinel)
    engine      VeloZ engine instances

Actions:
    promote     Promote standby to primary
    status      Show current HA status
    switchover  Graceful switchover (planned)

Examples:
    $(basename "$0") postgres status
    $(basename "$0") postgres promote
    $(basename "$0") redis status
    $(basename "$0") engine status
EOF
    exit 0
}

postgres_status() {
    log_info "PostgreSQL Replication Status"
    echo ""

    # Check primary
    echo "=== Primary Status ==="
    docker exec veloz-postgres-primary psql -U veloz -c "
        SELECT
            pg_is_in_recovery() as is_standby,
            pg_current_wal_lsn() as current_lsn,
            (SELECT count(*) FROM pg_stat_replication) as connected_standbys
    " 2>/dev/null || echo "Primary not available"

    echo ""
    echo "=== Replication Status ==="
    docker exec veloz-postgres-primary psql -U veloz -c "
        SELECT
            application_name,
            state,
            sync_state,
            sent_lsn,
            write_lsn,
            flush_lsn,
            replay_lsn,
            pg_wal_lsn_diff(sent_lsn, replay_lsn) as lag_bytes
        FROM pg_stat_replication
    " 2>/dev/null || echo "No replication info available"

    echo ""
    echo "=== Standby Status ==="
    docker exec veloz-postgres-standby psql -U veloz -c "
        SELECT
            pg_is_in_recovery() as is_standby,
            pg_last_wal_receive_lsn() as received_lsn,
            pg_last_wal_replay_lsn() as replayed_lsn,
            pg_last_xact_replay_timestamp() as last_replay_time
    " 2>/dev/null || echo "Standby not available"
}

postgres_promote() {
    log_warn "Promoting PostgreSQL standby to primary"
    log_warn "This is a MANUAL FAILOVER - data may be lost if primary was ahead"

    read -p "Are you sure you want to promote the standby? (yes/no): " -r response
    if [[ "$response" != "yes" ]]; then
        log_info "Promotion cancelled"
        exit 0
    fi

    # Check if standby is available
    if ! docker exec veloz-postgres-standby pg_isready -q; then
        log_error "Standby is not available"
        exit 1
    fi

    # Promote standby
    log_info "Promoting standby..."
    docker exec veloz-postgres-standby pg_ctl promote -D /var/lib/postgresql/data

    # Wait for promotion
    sleep 5

    # Verify promotion
    local is_standby
    is_standby=$(docker exec veloz-postgres-standby psql -U veloz -tAc "SELECT pg_is_in_recovery()")

    if [[ "$is_standby" == "f" ]]; then
        log_info "Standby promoted successfully!"
        log_warn "Update your application configuration to point to the new primary"
        log_warn "The old primary should be reconfigured as a standby"
    else
        log_error "Promotion may have failed - check logs"
    fi
}

redis_status() {
    log_info "Redis Sentinel Status"
    echo ""

    echo "=== Sentinel Master Info ==="
    docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel master veloz-master 2>/dev/null || echo "Sentinel not available"

    echo ""
    echo "=== Sentinel Replicas ==="
    docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel replicas veloz-master 2>/dev/null || echo "No replica info"

    echo ""
    echo "=== Master Replication Info ==="
    docker exec veloz-redis-master redis-cli -a redis INFO replication 2>/dev/null | grep -E "role|connected_slaves|slave" || echo "Master not available"
}

redis_failover() {
    log_warn "Triggering Redis Sentinel failover"

    read -p "Are you sure you want to trigger a Redis failover? (yes/no): " -r response
    if [[ "$response" != "yes" ]]; then
        log_info "Failover cancelled"
        exit 0
    fi

    log_info "Triggering failover via Sentinel..."
    docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel failover veloz-master

    # Wait for failover
    sleep 10

    # Check new master
    log_info "Checking new master..."
    redis_status
}

engine_status() {
    log_info "VeloZ Engine Status"
    echo ""

    echo "=== Engine 1 ==="
    curl -s http://localhost:8080/health 2>/dev/null | jq . || echo "Engine 1 not available"

    echo ""
    echo "=== Engine 2 ==="
    # Note: Engine 2 is backup, accessed via different port or internal network
    docker exec veloz-engine-2 curl -s http://localhost:8080/health 2>/dev/null | jq . || echo "Engine 2 not available"

    echo ""
    echo "=== HAProxy Status ==="
    curl -s http://localhost:8404/stats?stats;csv 2>/dev/null | grep veloz || echo "HAProxy stats not available"
}

# Parse arguments
if [[ $# -lt 1 ]]; then
    usage
fi

COMPONENT="${1:-}"
ACTION="${2:-status}"

case "$COMPONENT" in
    postgres)
        case "$ACTION" in
            status)
                postgres_status
                ;;
            promote)
                postgres_promote
                ;;
            *)
                log_error "Unknown action: $ACTION"
                usage
                ;;
        esac
        ;;
    redis)
        case "$ACTION" in
            status)
                redis_status
                ;;
            promote|failover)
                redis_failover
                ;;
            *)
                log_error "Unknown action: $ACTION"
                usage
                ;;
        esac
        ;;
    engine)
        case "$ACTION" in
            status)
                engine_status
                ;;
            *)
                log_error "Unknown action: $ACTION"
                usage
                ;;
        esac
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        log_error "Unknown component: $COMPONENT"
        usage
        ;;
esac
