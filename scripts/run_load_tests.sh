#!/usr/bin/env bash
#
# run_load_tests.sh - Run VeloZ load tests
#
# Usage:
#   ./scripts/run_load_tests.sh [preset] [mode] [options]
#
# Presets:
#   dev      - Development build (default)
#   release  - Release build (recommended for accurate metrics)
#   asan     - AddressSanitizer build (for memory debugging)
#
# Modes:
#   quick     - Quick test (30 seconds)
#   full      - Full test (5 minutes per test)
#   sustained - Sustained test (default: 1 hour)
#   stress    - Stress test (maximum throughput)
#   gateway   - Gateway API load test
#
# Options:
#   --hours N     - Duration for sustained test
#   --gateway-url - Gateway URL for API tests
#
# Examples:
#   ./scripts/run_load_tests.sh dev quick
#   ./scripts/run_load_tests.sh release full
#   ./scripts/run_load_tests.sh dev sustained --hours 24
#   ./scripts/run_load_tests.sh dev gateway --gateway-url http://localhost:8080

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default values
PRESET="${1:-dev}"
MODE="${2:-quick}"
HOURS="1"
GATEWAY_URL="http://127.0.0.1:8080"

# Parse additional arguments
shift 2 2>/dev/null || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --hours)
            HOURS="$2"
            shift 2
            ;;
        --gateway-url)
            GATEWAY_URL="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

BUILD_DIR="$PROJECT_ROOT/build/$PRESET"
LOAD_TEST_BIN="$BUILD_DIR/tests/load/veloz_load_tests"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Build if necessary
build_load_tests() {
    log_info "Building load tests with preset: $PRESET"

    cd "$PROJECT_ROOT"

    # Configure if needed
    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        log_info "Configuring CMake..."
        cmake --preset "$PRESET"
    fi

    # Build load tests
    log_info "Building load test executable..."
    cmake --build "$BUILD_DIR" --target veloz_load_tests -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

    if [[ ! -f "$LOAD_TEST_BIN" ]]; then
        log_error "Load test binary not found: $LOAD_TEST_BIN"
        exit 1
    fi

    log_success "Build completed"
}

# Run C++ load tests
run_cpp_load_tests() {
    local mode="$1"
    local extra_args="${2:-}"

    log_info "Running C++ load tests: $mode"

    cd "$PROJECT_ROOT"

    case "$mode" in
        quick)
            "$LOAD_TEST_BIN" quick
            ;;
        full)
            "$LOAD_TEST_BIN" full
            ;;
        sustained)
            "$LOAD_TEST_BIN" sustained --hours "$HOURS"
            ;;
        stress)
            "$LOAD_TEST_BIN" stress
            ;;
        *)
            log_error "Unknown mode: $mode"
            exit 1
            ;;
    esac
}

# Run Python gateway load tests
run_gateway_load_tests() {
    local mode="$1"

    log_info "Running gateway load tests: $mode"

    cd "$PROJECT_ROOT"

    # Check if Python script exists
    if [[ ! -f "tests/load/gateway_load_test.py" ]]; then
        log_error "Gateway load test script not found"
        exit 1
    fi

    # Check Python dependencies
    if ! python3 -c "import aiohttp" 2>/dev/null; then
        log_warning "Installing required Python packages..."
        pip3 install aiohttp
    fi

    case "$mode" in
        quick)
            python3 tests/load/gateway_load_test.py --mode quick --url "$GATEWAY_URL"
            ;;
        full)
            python3 tests/load/gateway_load_test.py --mode full --url "$GATEWAY_URL"
            ;;
        sustained)
            python3 tests/load/gateway_load_test.py --mode sustained --url "$GATEWAY_URL" --hours "$HOURS"
            ;;
        stress)
            python3 tests/load/gateway_load_test.py --mode stress --url "$GATEWAY_URL"
            ;;
        *)
            log_error "Unknown mode: $mode"
            exit 1
            ;;
    esac
}

# Print usage
print_usage() {
    cat << EOF
VeloZ Load Testing Script

Usage:
  $0 [preset] [mode] [options]

Presets:
  dev      - Development build (default)
  release  - Release build (recommended for accurate metrics)
  asan     - AddressSanitizer build

Modes:
  quick     - Quick test (30 seconds)
  full      - Full test (5 minutes per test)
  sustained - Sustained test for memory leak detection
  stress    - Stress test (maximum throughput)
  gateway   - Gateway API load test

Options:
  --hours N         Duration for sustained test (default: 1)
  --gateway-url URL Gateway URL (default: http://127.0.0.1:8080)

Examples:
  $0 dev quick
  $0 release full
  $0 dev sustained --hours 24
  $0 dev gateway
EOF
}

# Main
main() {
    log_info "VeloZ Load Testing"
    log_info "Preset: $PRESET, Mode: $MODE"

    case "$MODE" in
        help|--help|-h)
            print_usage
            exit 0
            ;;
        gateway)
            run_gateway_load_tests "quick"
            ;;
        gateway-quick|gateway-full|gateway-sustained|gateway-stress)
            local gateway_mode="${MODE#gateway-}"
            run_gateway_load_tests "$gateway_mode"
            ;;
        quick|full|sustained|stress)
            build_load_tests
            run_cpp_load_tests "$MODE"
            ;;
        all)
            # Run both C++ and gateway tests
            build_load_tests
            run_cpp_load_tests "quick"
            run_gateway_load_tests "quick"
            ;;
        *)
            log_error "Unknown mode: $MODE"
            print_usage
            exit 1
            ;;
    esac

    log_success "Load tests completed"
}

main "$@"
