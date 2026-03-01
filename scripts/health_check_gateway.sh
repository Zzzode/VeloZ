#!/usr/bin/env bash
set -euo pipefail

# Health check for Gateway
# Usage: ./scripts/health_check_gateway.sh [url] [timeout] [verbose]

GATEWAY_URL=${1:-"http://127.0.0.1:8081"}
TIMEOUT=${2:-5}
VERBOSE=${3:-1}

# Check if jq is available
if ! command -v jq >/dev/null 2>&1; then
  if [[ ${VERBOSE} -eq 1 ]]; then
    echo "Note: jq not found, using basic parsing"
  fi
  JQ_AVAILABLE=0
else
  JQ_AVAILABLE=1
fi

# Check if curl is available
if ! command -v curl >/dev/null 2>&1; then
  echo "ERROR: curl not found"
  exit 1
fi

# Perform health check
if [[ ${VERBOSE} -eq 1 ]]; then
  echo "Checking gateway health at ${GATEWAY_URL}/health..."
fi

HTTP_CODE=$(curl -s -o /tmp/health_response.json -w "%{http_code}" --max-time "${TIMEOUT}" "${GATEWAY_URL}/health" 2>&1) || {
  echo "ERROR: Health check failed (connection error)"
  echo "URL: ${GATEWAY_URL}/health"
  exit 1
}

if [[ "${HTTP_CODE}" != "200" ]]; then
  echo "ERROR: Health check failed (HTTP ${HTTP_CODE})"
  echo "URL: ${GATEWAY_URL}/health"
  if [[ -f /tmp/health_response.json ]]; then
    cat /tmp/health_response.json
  fi
  exit 1
fi

RESPONSE=$(cat /tmp/health_response.json 2>/dev/null || echo "")

if [[ -z "${RESPONSE}" ]]; then
  echo "ERROR: Empty response from health check"
  exit 1
fi

if [[ ${VERBOSE} -eq 1 ]]; then
  echo "Health check response:"
  if [[ ${JQ_AVAILABLE} -eq 1 ]]; then
    echo "${RESPONSE}" | jq .
  else
    echo "${RESPONSE}"
  fi
fi

if [[ ${JQ_AVAILABLE} -eq 1 ]]; then
  # Parse response
  STATUS=$(echo "${RESPONSE}" | jq -r '.status // "unknown"')
  GATEWAY_TYPE=$(echo "${RESPONSE}" | jq -r '.gateway // "unknown"')
  TIMESTAMP=$(echo "${RESPONSE}" | jq -r '.timestamp // "unknown"')

  # Check for healthy status
  if [[ "${STATUS}" == "healthy" ]]; then
    if [[ ${VERBOSE} -eq 1 ]]; then
      echo ""
      echo "Gateway: ${GATEWAY_TYPE}"
      echo "Status: ${STATUS}"
      echo "Timestamp: ${TIMESTAMP}"

      # Print component checks if available
      CHECKS=$(echo "${RESPONSE}" | jq -r '.checks // empty')
      if [[ -n "${CHECKS}" ]] && [[ "${CHECKS}" != "null" ]]; then
        echo "Component checks:"
        echo "${CHECKS}" | jq -r 'to_entries[] | "  - \(.key): \(.value)"'
      fi
    fi
    echo "Gateway is healthy"
    exit 0
  else
    echo "ERROR: Gateway status is '${STATUS}'"
    exit 1
  fi
else
  # Simple check if response looks OK
  if echo "${RESPONSE}" | grep -q '"status"[[:space:]]*:[[:space:]]*"healthy"'; then
    echo "Gateway is healthy"
    exit 0
  else
    echo "ERROR: Could not verify health status"
    echo "Response: ${RESPONSE}"
    exit 1
  fi
fi
