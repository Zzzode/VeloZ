#!/bin/bash
# Run VeloZ performance benchmarks
#
# Usage: ./scripts/run_benchmarks.sh [preset]
#   preset: dev (default), release, asan

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PRESET="${1:-dev}"

echo "=== VeloZ Performance Benchmarks ==="
echo "Preset: $PRESET"
echo ""

# Build benchmarks
echo "Building benchmarks..."
cmake --preset "$PRESET" > /dev/null 2>&1
cmake --build "$PROJECT_ROOT/build/$PRESET" --target veloz_core_benchmarks -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) > /dev/null 2>&1

# Run benchmarks
echo "Running benchmarks..."
echo ""

BENCHMARK_BIN="$PROJECT_ROOT/build/$PRESET/libs/core/veloz_core_benchmarks"

if [ ! -f "$BENCHMARK_BIN" ]; then
    echo "Error: Benchmark binary not found at $BENCHMARK_BIN"
    exit 1
fi

# Run and save output
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="$PROJECT_ROOT/benchmark_results"
mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="$OUTPUT_DIR/benchmark_${PRESET}_${TIMESTAMP}.txt"

"$BENCHMARK_BIN" 2>&1 | tee "$OUTPUT_FILE"

echo ""
echo "Results saved to: $OUTPUT_FILE"
