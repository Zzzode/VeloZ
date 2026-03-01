#!/bin/bash
# Run VeloZ Gateway performance benchmarks
#
# Usage: ./scripts/run_gateway_benchmarks.sh [build_preset] [options]
#
# Examples:
#   ./scripts/run_gateway_benchmarks.sh release              # Run release benchmarks
#   ./scripts/run_gateway_benchmarks.sh release --analyze     # Run with analysis
#   ./scripts/run_gateway_benchmarks.sh release --report      # Generate report only
#
# Build presets: dev, release, asan

set -e

# Default preset
BUILD_PRESET="${1:-release}"

# Parse options
ANALYZE=""
REPORT_ONLY=""
RUN_COUNT=1

shift || true

while [[ $# -gt 0 ]]; do
  case $1 in
    --analyze)
      ANALYZE="--analyze"
      shift
      ;;
    --report)
      REPORT_ONLY="--report"
      shift
      ;;
    --count)
      RUN_COUNT="$2"
      shift 2
      ;;
    --help)
      echo "Usage: $0 [build_preset] [options]"
      echo ""
      echo "Build presets: dev, release, asan"
      echo ""
      echo "Options:"
      echo "  --analyze    Run bottleneck analysis"
      echo "  --report     Only generate report from existing output"
      echo "  --count N    Run benchmarks N times"
      echo "  --help       Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     VeloZ Gateway Performance Benchmarks                    ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check build directory
BUILD_DIR="$PROJECT_ROOT/build/$BUILD_PRESET"

if [[ ! -d "$BUILD_DIR" ]]; then
  echo -e "${YELLOW}Build directory not found. Configuring with preset: $BUILD_PRESET${NC}"
  cmake --preset "$BUILD_PRESET"
fi

# Check if benchmarks need to be built
BENCHMARK_BIN="$BUILD_DIR/apps/gateway/veloz_gateway_benchmarks"

if [[ ! -f "$BENCHMARK_BIN" ]] || [[ ! -x "$BENCHMARK_BIN" ]]; then
  echo -e "${YELLOW}Benchmarks not built. Building now...${NC}"
  cmake --preset "$BUILD_PRESET"
  cmake --build --preset "${BUILD_PRESET}-all" -j$(nproc) --target veloz_gateway_benchmarks
fi

# Create output directory
OUTPUT_DIR="$BUILD_DIR/benchmark_output"
REPORT_DIR="$OUTPUT_DIR/report"
mkdir -p "$REPORT_DIR"

# Run benchmarks
if [[ -z "$REPORT_ONLY" ]]; then
  echo -e "${BLUE}Running benchmarks (preset: $BUILD_PRESET)...${NC}"
  echo ""

  # Run benchmarks multiple times if requested
  for i in $(seq 1 $RUN_COUNT); do
    if [[ $RUN_COUNT -gt 1 ]]; then
      echo -e "${BLUE}Run $i of $RUN_COUNT...${NC}"
    fi

    OUTPUT_FILE="$OUTPUT_DIR/benchmark_output_${BUILD_PRESET}_${i}.txt"

    # Run the benchmark
    "$BENCHMARK_BIN" 2>&1 | tee "$OUTPUT_FILE"

    if [[ $i -lt $RUN_COUNT ]]; then
      echo ""
      echo -e "${BLUE}Waiting 5 seconds before next run...${NC}"
      sleep 5
    fi
  done

  echo ""
fi

# Generate reports
echo -e "${BLUE}Generating benchmark reports...${NC}"

if [[ $RUN_COUNT -eq 1 ]]; then
  INPUT_FILE="$OUTPUT_DIR/benchmark_output_${BUILD_PRESET}_1.txt"
else
  # Aggregate results from multiple runs
  echo ""
  echo -e "${YELLOW}Aggregating results from $RUN_COUNT runs...${NC}"

  INPUT_FILE="$OUTPUT_DIR/benchmark_output_${BUILD_PRESET}_aggregated.txt"
  cat "$OUTPUT_DIR/benchmark_output_${BUILD_PRESET}_1.txt" > "$INPUT_FILE"
fi

# Check if Python is available
if command -v python3 &> /dev/null; then
  python3 "$PROJECT_ROOT/apps/gateway/benchmarks/benchmark_report.py" \
    --input "$INPUT_FILE" \
    --output "$REPORT_DIR" \
    --format all \
    $ANALYZE

  echo ""
  echo -e "${GREEN}Reports generated in: $REPORT_DIR${NC}"
  echo "  - report.txt: Human-readable summary"
  echo "  - report.json: Machine-readable results"

  # Display summary
  echo ""
  echo -e "${BLUE}Summary:${NC}"
  cat "$REPORT_DIR/report.txt"
else
  echo -e "${YELLOW}Python3 not found. Skipping report generation.${NC}"
  echo -e "${YELLOW}Install Python3 to generate detailed reports.${NC}"
fi

echo ""
echo -e "${GREEN}Benchmark complete!${NC}"
