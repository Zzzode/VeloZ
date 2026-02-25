#!/usr/bin/env bash
set -euo pipefail

# Parse arguments
dry_run=0
preset="${1:-dev}"

if [[ "${preset}" == "--dry-run" ]]; then
  dry_run=1
  preset="${2:-dev}"
elif [[ "${2:-}" == "--dry-run" ]]; then
  dry_run=1
fi

export VELOZ_PRESET="${preset}"

# Dry run mode
if [[ "${dry_run}" -eq 1 ]]; then
  echo "=== Dry run mode ==="
  echo "Preset: ${preset}"
  echo "Would run:"
  echo "  1. Build with preset: ${preset}"
  echo "  2. Execute: python3 apps/gateway/gateway.py"
  exit 0
fi

# Actual execution
./scripts/build.sh "${preset}"

exec python3 apps/gateway/gateway.py
