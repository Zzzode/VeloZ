#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"

./scripts/build.sh "${preset}"

bin_dir="build/${preset}"
engine_path="${bin_dir}/apps/engine/veloz_engine"

if [[ ! -x "${engine_path}" ]]; then
  echo "engine not found: ${engine_path}" >&2
  exit 1
fi

timeout 3s "${engine_path}" || true

