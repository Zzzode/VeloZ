#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "==> Configuring coverage build..."
cmake --preset coverage

echo "==> Building all targets with coverage..."
cmake --build --preset coverage-all -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "==> Running tests with coverage..."
ctest --preset coverage -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "==> Generating coverage report..."
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/apps/gateway/*' '*/apps/ui/*' '*_test.cpp' '*_tests.cpp' --output-file coverage.info
lcov --list coverage.info

echo "==> Generating HTML report..."
genhtml coverage.info --output-directory coverage_html

echo ""
echo "Coverage report generated!"
echo "Open coverage_html/index.html in your browser to view the report."
echo ""
echo "Summary:"
lcov --summary coverage.info
