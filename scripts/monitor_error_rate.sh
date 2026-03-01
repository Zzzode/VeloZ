#!/usr/bin/env bash
set -euo pipefail

# Monitor error rate during migration
# Usage: ./scripts/monitor_error_rate.sh [--timeout 60] [--threshold 0.01]

TIMEOUT=60
THRESHOLD=0.01  # 1%
GATEWAY_URL=${1:-"http://127.0.0.1:8081"}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --threshold) THRESHOLD="$2"; shift 2 ;;
    *) GATEWAY_URL="$1"; shift ;;
  esac
done

echo "=== Monitoring Error Rate ==="
echo "Gateway: ${GATEWAY_URL}"
echo "Timeout: ${TIMEOUT}s"
echo "Threshold: $(echo "${THRESHOLD} * 100" | bc)%"
echo ""

# Check if we can access metrics
if ! curl -s -f --max-time 5 "${GATEWAY_URL}/metrics" >/dev/null 2>&1; then
  echo "ERROR: Cannot access metrics endpoint at ${GATEWAY_URL}/metrics"
  exit 1
fi

# Monitor for the specified duration
START_TIME=$(date +%s)
END_TIME=$((START_TIME + TIMEOUT))

echo "Monitoring for ${TIMEOUT} seconds..."
echo ""

SAMPLES=0
ERRORS=0
TOTAL_REQUESTS=0

while [[ $(date +%s) -lt ${END_TIME} ]]; do
  # Get metrics
  METRICS=$(curl -s "${GATEWAY_URL}/metrics" 2>/dev/null || echo "")

  if [[ -n "${METRICS}" ]]; then
    # Extract request counts
    # Look for veloz_gateway_requests_total or similar metrics
    TOTAL=$(echo "${METRICS}" | grep -E "^veloz_gateway_requests_total" | grep -v "^#" | awk '{sum += $2} END {print sum + 0}')
    ERRORS_5XX=$(echo "${METRICS}" | grep -E "^veloz_gateway_requests_total.*status=\"5" | grep -v "^#" | awk '{sum += $2} END {print sum + 0}')

    if [[ -n "${TOTAL}" ]] && [[ "${TOTAL}" -gt 0 ]]; then
      CURRENT_ERROR_RATE=$(echo "scale=6; ${ERRORS_5XX} / ${TOTAL}" | bc 2>/dev/null || echo "0")
      ERROR_PERCENT=$(echo "scale=2; ${CURRENT_ERROR_RATE} * 100" | bc 2>/dev/null || echo "0")

      echo "Sample $((SAMPLES + 1)): Error rate = ${ERROR_PERCENT}% (Total: ${TOTAL}, Errors: ${ERRORS_5XX})"

      TOTAL_REQUESTS=$((TOTAL_REQUESTS + TOTAL))
      SAMPLES=$((SAMPLES + 1))

      # Check if error rate exceeds threshold
      if [[ $(echo "${CURRENT_ERROR_RATE} > ${THRESHOLD}" | bc -l 2>/dev/null || echo "0") -eq 1 ]]; then
        echo ""
        echo "WARNING: Error rate (${ERROR_PERCENT}%) exceeds threshold ($(echo "${THRESHOLD} * 100" | bc)%)"
        echo "Consider rollback if error rate remains high"
      fi
    fi
  fi

  sleep 5
done

echo ""
echo "=== Monitoring Complete ==="
echo "Samples collected: ${SAMPLES}"
echo "Total requests observed: ${TOTAL_REQUESTS}"
echo ""

# Final assessment
if [[ ${SAMPLES} -eq 0 ]]; then
  echo "No samples collected - metrics may not be available"
  exit 0
fi

echo "Error rate is within acceptable range"
exit 0
