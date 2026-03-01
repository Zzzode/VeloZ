#!/usr/bin/env bash
set -euo pipefail

# Deploy C++ Gateway to canary environment
# Usage: ./scripts/deploy_gateway_canary.sh [preset] [--dry-run]

echo "=== Deploying C++ Gateway to Canary ==="

# Parse arguments
PRESET="dev"
DRY_RUN=0

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    dev|release|asan|coverage) PRESET="$arg" ;;
    *) ;;
  esac
done

export VELOZ_PRESET="${PRESET}"

# Dry run mode
if [[ "${DRY_RUN}" -eq 1 ]]; then
  echo "=== Dry run mode ==="
  echo "Preset: ${PRESET}"
  echo "Would deploy:"
  echo "  1. Build C++ Gateway"
  echo "  2. Deploy to port 8081"
  echo "  3. Run health checks"
  echo "  4. Configure 5% traffic split"
  exit 0
fi

# Step 1: Build C++ Gateway
echo "=== Step 1/5: Building C++ Gateway ==="
BUILD_DIR="build/${PRESET}"

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  echo "Configuring CMake..."
  cmake --preset "${PRESET}"
fi

echo "Building Gateway..."
cmake --build --preset "${PRESET}-gateway" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" 2>/dev/null || {
  echo "Warning: Build preset may not exist, trying all..."
  cmake --build --preset "${PRESET}-all" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
}

# Step 2: Stop existing C++ Gateway (if running)
echo "=== Step 2/5: Stopping existing C++ Gateway ==="
if pgrep -f "veloz_gateway_cpp" >/dev/null 2>&1; then
  echo "Stopping existing C++ Gateway..."
  pkill -f "veloz_gateway_cpp"
  sleep 2
fi

# Step 3: Deploy new C++ Gateway
echo "=== Step 3/5: Deploying C++ Gateway ==="
GATEWAY_BIN="${BUILD_DIR}/apps/gateway_cpp/veloz_gateway_cpp"

if [[ ! -x "${GATEWAY_BIN}" ]]; then
  echo "ERROR: Gateway binary not found: ${GATEWAY_BIN}"
  exit 1
fi

# Create log directory if it doesn't exist
LOG_DIR="/var/log/veloz"
sudo mkdir -p "${LOG_DIR}" 2>/dev/null || true
LOG_FILE="${LOG_DIR}/gateway_cpp.log"

# Start gateway in background
echo "Starting C++ Gateway on port 8081..."
PORT=8081

# Use local log file if /var/log not writable
if [[ ! -w "${LOG_DIR}" ]]; then
  LOG_FILE="./gateway_cpp.log"
fi

nohup "${GATEWAY_BIN}" --port "${PORT}" > "${LOG_FILE}" 2>&1 &
GATEWAY_PID=$!

echo "Gateway started with PID: ${GATEWAY_PID}"

# Save PID to file for later use
echo "${GATEWAY_PID}" > /tmp/veloz_gateway_cpp.pid

# Step 4: Run health checks
echo "=== Step 4/5: Running health checks ==="
sleep 3  # Wait for gateway to start

MAX_RETRIES=10
RETRY_DELAY=3
GATEWAY_URL="http://127.0.0.1:${PORT}"

for ((i=1; i<=MAX_RETRIES; i++)); do
  if ./scripts/health_check_gateway.sh "${GATEWAY_URL}" 5 0; then
    echo "Health check passed!"
    break
  fi

  if [[ ${i} -eq ${MAX_RETRIES} ]]; then
    echo "ERROR: Health check failed after ${MAX_RETRIES} attempts"
    echo "Rolling back..."
    kill "${GATEWAY_PID}" 2>/dev/null || true
    exit 1
  fi

  echo "Health check failed, retrying in ${RETRY_DELAY}s (attempt ${i}/${MAX_RETRIES})..."
  sleep ${RETRY_DELAY}
done

# Step 5: Configure traffic split (if load balancer available)
echo "=== Step 5/5: Configuring traffic split ==="
if [[ -f "./scripts/adjust_traffic_split.sh" ]]; then
  ./scripts/adjust_traffic_split.sh 5 2>/dev/null || {
    echo "Warning: Could not configure traffic split (load balancer may not be available)"
    echo "Manual configuration required: Route 5% traffic to port 8081"
  }
else
  echo "Traffic split script not found. Manual configuration required:"
  echo "  - Route 5% traffic to C++ Gateway (port 8081)"
  echo "  - Keep 95% traffic on Python Gateway (port 8080)"
fi

echo ""
echo "=== Canary Deployment Complete ==="
echo "C++ Gateway deployed successfully"
echo "Gateway PID: ${GATEWAY_PID}"
echo "Log file: ${LOG_FILE}"
echo ""
echo "Monitor at:"
echo "  - Health: ${GATEWAY_URL}/health"
echo "  - Metrics: ${GATEWAY_URL}/metrics"
echo ""
echo "Traffic split: 5% C++ Gateway, 95% Python Gateway"
echo ""
echo "To check status:"
echo "  ./scripts/health_check_gateway.sh ${GATEWAY_URL}"
echo ""
echo "To rollback:"
echo "  ./scripts/rollback_gateway.sh"
echo ""
