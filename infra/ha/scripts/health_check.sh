#!/bin/bash
# health_check.sh - Health check script for VeloZ HA components
# Can be run as a cron job or monitoring probe

set -euo pipefail

# Configuration
ALERT_WEBHOOK="${VELOZ_ALERT_WEBHOOK:-}"
POSTGRES_PRIMARY="${VELOZ_PG_PRIMARY:-postgres-primary}"
POSTGRES_STANDBY="${VELOZ_PG_STANDBY:-postgres-standby}"
REDIS_MASTER="${VELOZ_REDIS_MASTER:-redis-master}"
ENGINE_HOSTS="${VELOZ_ENGINE_HOSTS:-veloz-engine-1:8080,veloz-engine-2:8080}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Status tracking
ERRORS=()
WARNINGS=()

log_ok() {
    echo -e "${GREEN}[OK]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
    WARNINGS+=("$*")
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
    ERRORS+=("$*")
}

check_postgres_primary() {
    echo "Checking PostgreSQL Primary..."

    if docker exec "$POSTGRES_PRIMARY" pg_isready -q 2>/dev/null; then
        log_ok "PostgreSQL primary is accepting connections"

        # Check if it's actually the primary
        local is_standby
        is_standby=$(docker exec "$POSTGRES_PRIMARY" psql -U veloz -tAc "SELECT pg_is_in_recovery()" 2>/dev/null || echo "error")

        if [[ "$is_standby" == "f" ]]; then
            log_ok "PostgreSQL primary is in primary mode"
        elif [[ "$is_standby" == "t" ]]; then
            log_error "PostgreSQL primary is in recovery mode (split brain?)"
        else
            log_error "Cannot determine PostgreSQL primary state"
        fi
    else
        log_error "PostgreSQL primary is not accepting connections"
    fi
}

check_postgres_standby() {
    echo "Checking PostgreSQL Standby..."

    if docker exec "$POSTGRES_STANDBY" pg_isready -q 2>/dev/null; then
        log_ok "PostgreSQL standby is accepting connections"

        # Check replication lag
        local lag_bytes
        lag_bytes=$(docker exec "$POSTGRES_PRIMARY" psql -U veloz -tAc "
            SELECT COALESCE(pg_wal_lsn_diff(sent_lsn, replay_lsn), 0)
            FROM pg_stat_replication
            WHERE application_name = 'veloz_standby'
        " 2>/dev/null || echo "error")

        if [[ "$lag_bytes" == "error" ]]; then
            log_warn "Cannot determine replication lag"
        elif [[ "$lag_bytes" -gt 10485760 ]]; then  # 10MB
            log_warn "Replication lag is high: ${lag_bytes} bytes"
        else
            log_ok "Replication lag is acceptable: ${lag_bytes} bytes"
        fi
    else
        log_error "PostgreSQL standby is not accepting connections"
    fi
}

check_redis_sentinel() {
    echo "Checking Redis Sentinel..."

    # Check if sentinel is running
    if docker exec veloz-redis-sentinel-1 redis-cli -p 26379 ping 2>/dev/null | grep -q PONG; then
        log_ok "Redis Sentinel is responding"

        # Check master status
        local master_status
        master_status=$(docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel master veloz-master 2>/dev/null | grep -A1 "flags" | tail -1 || echo "error")

        if [[ "$master_status" == "master" ]]; then
            log_ok "Redis master is healthy"
        elif [[ "$master_status" == *"s_down"* ]] || [[ "$master_status" == *"o_down"* ]]; then
            log_error "Redis master is down: $master_status"
        else
            log_warn "Redis master status: $master_status"
        fi

        # Check number of sentinels
        local num_sentinels
        num_sentinels=$(docker exec veloz-redis-sentinel-1 redis-cli -p 26379 sentinel master veloz-master 2>/dev/null | grep -A1 "num-other-sentinels" | tail -1 || echo "0")

        if [[ "$num_sentinels" -ge 2 ]]; then
            log_ok "Redis Sentinel quorum available: $((num_sentinels + 1)) sentinels"
        else
            log_warn "Low Redis Sentinel count: $((num_sentinels + 1)) sentinels"
        fi
    else
        log_error "Redis Sentinel is not responding"
    fi
}

check_redis_replication() {
    echo "Checking Redis Replication..."

    if docker exec "$REDIS_MASTER" redis-cli -a redis ping 2>/dev/null | grep -q PONG; then
        log_ok "Redis master is responding"

        # Check connected slaves
        local connected_slaves
        connected_slaves=$(docker exec "$REDIS_MASTER" redis-cli -a redis INFO replication 2>/dev/null | grep "connected_slaves" | cut -d: -f2 | tr -d '\r' || echo "0")

        if [[ "$connected_slaves" -ge 1 ]]; then
            log_ok "Redis has $connected_slaves connected replica(s)"
        else
            log_warn "No Redis replicas connected"
        fi
    else
        log_error "Redis master is not responding"
    fi
}

check_veloz_engines() {
    echo "Checking VeloZ Engines..."

    local healthy_count=0
    local total_count=0

    IFS=',' read -ra HOSTS <<< "$ENGINE_HOSTS"
    for host in "${HOSTS[@]}"; do
        total_count=$((total_count + 1))

        if curl -sf "http://${host}/health" > /dev/null 2>&1; then
            log_ok "Engine $host is healthy"
            healthy_count=$((healthy_count + 1))
        else
            log_error "Engine $host is not responding"
        fi
    done

    if [[ $healthy_count -eq 0 ]]; then
        log_error "No healthy VeloZ engines!"
    elif [[ $healthy_count -lt $total_count ]]; then
        log_warn "Only $healthy_count of $total_count engines are healthy"
    fi
}

check_haproxy() {
    echo "Checking HAProxy..."

    if curl -sf "http://localhost:8404/stats" > /dev/null 2>&1; then
        log_ok "HAProxy stats page is accessible"

        # Check backend status
        local backend_status
        backend_status=$(curl -s "http://localhost:8404/stats?stats;csv" 2>/dev/null | grep "veloz_engines,BACKEND" | cut -d, -f18 || echo "error")

        if [[ "$backend_status" == "UP" ]]; then
            log_ok "HAProxy backend is UP"
        else
            log_warn "HAProxy backend status: $backend_status"
        fi
    else
        log_error "HAProxy stats page is not accessible"
    fi
}

send_alert() {
    if [[ -z "$ALERT_WEBHOOK" ]]; then
        return
    fi

    local severity="$1"
    local message="$2"

    curl -s -X POST "$ALERT_WEBHOOK" \
        -H 'Content-Type: application/json' \
        -d "{
            \"severity\": \"$severity\",
            \"source\": \"veloz-ha-health-check\",
            \"message\": \"$message\",
            \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
        }" > /dev/null 2>&1 || true
}

main() {
    echo "=========================================="
    echo "VeloZ HA Health Check"
    echo "Time: $(date)"
    echo "=========================================="
    echo ""

    check_postgres_primary
    echo ""
    check_postgres_standby
    echo ""
    check_redis_sentinel
    echo ""
    check_redis_replication
    echo ""
    check_veloz_engines
    echo ""
    check_haproxy

    echo ""
    echo "=========================================="
    echo "Summary"
    echo "=========================================="

    if [[ ${#ERRORS[@]} -gt 0 ]]; then
        echo -e "${RED}ERRORS: ${#ERRORS[@]}${NC}"
        for error in "${ERRORS[@]}"; do
            echo "  - $error"
        done
        send_alert "critical" "VeloZ HA health check failed: ${#ERRORS[@]} errors"
    fi

    if [[ ${#WARNINGS[@]} -gt 0 ]]; then
        echo -e "${YELLOW}WARNINGS: ${#WARNINGS[@]}${NC}"
        for warning in "${WARNINGS[@]}"; do
            echo "  - $warning"
        done
        if [[ ${#ERRORS[@]} -eq 0 ]]; then
            send_alert "warning" "VeloZ HA health check warnings: ${#WARNINGS[@]}"
        fi
    fi

    if [[ ${#ERRORS[@]} -eq 0 ]] && [[ ${#WARNINGS[@]} -eq 0 ]]; then
        echo -e "${GREEN}All checks passed!${NC}"
    fi

    # Exit with error if there are critical issues
    if [[ ${#ERRORS[@]} -gt 0 ]]; then
        exit 1
    fi
}

main "$@"
