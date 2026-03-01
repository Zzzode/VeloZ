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
    echo "  2. Build C++ gateway (cmake)"
  fi
  echo "  3. Execute: build/${preset}/apps/gateway/veloz_gateway"
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

# Step 2: Build C++ gateway with preset
if [[ "${skip_cxx}" -eq 1 ]]; then
  echo "=== Step 2/3: Skipping C++ build ==="
else
  echo "=== Step 2/3: Building C++ gateway ==="
  build_dir="build/${preset}"
  if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
    cmake --preset "${preset}"
  fi
  gateway_bin="${build_dir}/apps/gateway/veloz_gateway"
  case "${preset}" in
    dev|release|asan)
      if [[ "${force_cxx}" -eq 1 ]]; then
        cmake --build --preset "${preset}-all" -j
      else
        if [[ -x "${gateway_bin}" ]]; then
          echo "Gateway build artifacts present, skipping"
        else
          cmake --build --preset "${preset}-all" -j
        fi
      fi
      ;;
    *)
      cmake --build --preset "${preset}" -j
      ;;
  esac
fi

# Step 3: Start gateway
echo "=== Step 3/3: Starting C++ gateway ==="
gateway_bin="build/${preset}/apps/gateway/veloz_gateway"
if [[ ! -x "${gateway_bin}" ]]; then
  echo "Error: C++ gateway not found at ${gateway_bin}"
  echo "Build it with: cmake --build --preset ${preset}-all -j"
  exit 1
fi
exec "${gateway_bin}"