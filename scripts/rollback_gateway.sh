#!/usr/bin/env bash
set -euo pipefail

# Rollback to Python Gateway
# Usage: ./scripts/rollback_gateway.sh [--alert]

echo "=== Rollback to Python Gateway ==="

ALERT=${1:-0}

# Step 1: Route 100% traffic to Python
echo "=== Step 1/4: Routing traffic to Python Gateway ==="
if [[ -f "./scripts/adjust_traffic_split.sh" ]]; then
  ./scripts/adjust_traffic_split.sh 0 2>/dev/null || {
    echo "Warning: Could not adjust traffic split automatically"
    echo "Manual configuration required: Route 100% traffic to port 8080"
  }
else
  echo "Traffic split script not found. Manual configuration required:"
  echo "  - Route 100% traffic to Python Gateway (port 8080)"
fi

# Step 2: Check Python Gateway health
echo "=== Step 2/4: Checking Python Gateway health ==="
PYTHON_GATEWAY_URL="http://127.0.0.1:8080"

if ./scripts/health_check_gateway.sh "${PYTHON_GATEWAY_URL}" 5 0; then
  echo "Python Gateway is healthy"
else
  echo "WARNING: Python Gateway health check failed"
  echo "Attempting to start Python Gateway..."

  # Try to start Python Gateway
  if ! pgrep -f "gateway.py" >/dev/null 2>&1; then
    echo "Starting Python Gateway..."

    # Create log directory if it doesn't exist
    LOG_DIR="/var/log/veloz"
    sudo mkdir -p "${LOG_DIR}" 2>/dev/null || true
    LOG_FILE="${LOG_DIR}/gateway_py.log"

    if [[ ! -w "${LOG_DIR}" ]]; then
      LOG_FILE="./gateway_py.log"
    fi

    nohup python3 apps/gateway/gateway.py > "${LOG_FILE}" 2>&1 &
    PYTHON_PID=$!
    echo "Python Gateway started with PID: ${PYTHON_PID}"

    sleep 5

    if ./scripts/health_check_gateway.sh "${PYTHON_GATEWAY_URL}" 5 0; then
      echo "Python Gateway started successfully"
    else
      echo "ERROR: Failed to start Python Gateway"
      echo "Manual intervention required!"
      exit 1
    fi
  fi
fi

# Step 3: Stop C++ Gateway
echo "=== Step 3/4: Stopping C++ Gateway ==="
if pgrep -f "veloz_gateway_cpp" >/dev/null 2>&1; then
  echo "Stopping C++ Gateway..."

  # Try graceful shutdown first
  CPP_PID=$(pgrep -f "veloz_gateway_cpp")
  kill "${CPP_PID}" 2>/dev/null || true

  # Wait for graceful shutdown
  sleep 5

  # Force kill if still running
  if pgrep -f "veloz_gateway_cpp" >/dev/null 2>&1; then
    echo "Force killing C++ Gateway..."
    pkill -9 -f "veloz_gateway_cpp" || true
  fi

  # Clean up PID file
  rm -f /tmp/veloz_gateway_cpp.pid

  echo "C++ Gateway stopped"
else
  echo "C++ Gateway not running"
fi

# Step 4: Verify traffic
echo "=== Step 4/4: Verifying traffic ==="
sleep 5

echo "Running final health check on Python Gateway..."
if ./scripts/health_check_gateway.sh "${PYTHON_GATEWAY_URL}" 5 0; then
  echo "Python Gateway is healthy and handling traffic"
else
  echo "WARNING: Python Gateway health check failed"
fi

# Alert team if requested
if [[ "${ALERT}" == "--alert" ]]; then
  echo ""
  echo "=== Sending Rollback Alert ==="
  if [[ -f "./scripts/alert_rollback.sh" ]]; then
    ./scripts/alert_rollback.sh "Manual rollback triggered" 2>/dev/null || {
      echo "Warning: Could not send alert automatically"
      echo "Please notify the engineering team manually"
    }
  else
    echo "Alert script not found. Please notify the engineering team:"
    echo "  - Rollback completed at $(date)"
    echo "  - Reason: Manual rollback"
  fi
fi

echo ""
echo "=== Rollback Complete ==="
echo "Traffic routing: 100% Python Gateway"
echo "Python Gateway: ${PYTHON_GATEWAY_URL}"
echo "C++ Gateway: Stopped"
echo ""
echo "Next steps:"
echo "  1. Investigate rollback reason"
echo "  2. Fix identified issues"
echo "  3. Re-deploy C++ Gateway"
echo "  4. Update incident log"
echo ""
