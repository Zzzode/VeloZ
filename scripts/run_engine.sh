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

# Dry run mode
if [[ "${dry_run}" -eq 1 ]]; then
  echo "=== Dry run mode ==="
  echo "Preset: ${preset}"
  echo "Would run:"
  echo "  1. Build with preset: ${preset}"
  echo "  2. Execute: timeout 3s build/${preset}/apps/engine/veloz_engine"
  exit 0
fi

# Actual execution
./scripts/build.sh "${preset}"

bin_dir="build/${preset}"
engine_path="${bin_dir}/apps/engine/veloz_engine"

if [[ ! -x "${engine_path}" ]]; then
  echo "engine not found: ${engine_path}" >&2
  exit 1
fi

timeout 3s "${engine_path}" || true

