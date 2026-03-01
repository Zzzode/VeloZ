#!/usr/bin/env bash
set -euo pipefail

# Parse arguments
dry_run=0
preset="dev"
skip_ui=0
skip_cxx=0
force_ui=0
force_cxx=0

for arg in "$@"; do
  case "$arg" in
    --skip-ui) skip_ui=1 ;;
    --skip-cxx) skip_cxx=1 ;;
    --force-ui) force_ui=1 ;;
    --force-cxx) force_cxx=1 ;;
    --dry-run) dry_run=1 ;;
    dev|release|asan|coverage) preset="$arg" ;;
    *) ;;
  esac
done

export VELOZ_PRESET="${preset}"

# Dry run mode
if [[ "${dry_run}" -eq 1 ]]; then
  echo "=== Dry run mode ==="
  echo "Preset: ${preset}"
  echo "Would run:"
  if [[ "${skip_ui}" -eq 1 ]]; then
    echo "  1. Skip UI build"
  else
    echo "  1. Build UI (npm)"
  fi
  if [[ "${skip_cxx}" -eq 1 ]]; then
    echo "  2. Skip C++ build"
  else
    echo "  2. Build C++ engine (cmake)"
  fi
  echo "  3. Execute: python3 apps/gateway/gateway.py"
  exit 0
fi

# Actual execution
# Step 1: Build UI (required for P0 features)
if [[ "${skip_ui}" -eq 1 ]]; then
  echo "=== Step 1/3: Skipping UI build ==="
else
  echo "=== Step 1/3: Building UI ==="
  cd apps/ui
  if ! command -v npm >/dev/null 2>&1; then
    echo "npm not found, skipping UI build"
    skip_ui=1
    cd ../..
  else
  if [[ ! -d "node_modules" ]]; then
    npm ci
  fi
  if [[ "${force_ui}" -eq 1 ]]; then
    npm run build
  else
    if [[ -d "dist" ]]; then
      echo "UI build artifacts present, skipping"
    else
      npm run build
    fi
  fi
  cd ../..
  fi
fi

# Step 2: Build C++ engine with preset
if [[ "${skip_cxx}" -eq 1 ]]; then
  echo "=== Step 2/3: Skipping C++ build ==="
else
  echo "=== Step 2/3: Building C++ engine ==="
  build_dir="build/${preset}"
  if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
    cmake --preset "${preset}"
  fi
  engine_bin="${build_dir}/apps/engine/veloz_engine"
  case "${preset}" in
    dev|release|asan)
      if [[ "${force_cxx}" -eq 1 ]]; then
        cmake --build --preset "${preset}-engine" -j
      else
        if [[ -x "${engine_bin}" ]]; then
          echo "Engine build artifacts present, skipping"
        else
          cmake --build --preset "${preset}-engine" -j
        fi
      fi
      ;;
    *)
      cmake --build --preset "${preset}" -j
      ;;
  esac
fi

# Step 3: Start gateway (includes UI dev server)
echo "=== Step 3/3: Starting gateway ==="
exec python3 apps/gateway/gateway.py
