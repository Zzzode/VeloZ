#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
export VELOZ_PRESET="${preset}"

./scripts/build.sh "${preset}"

exec python3 apps/gateway/gateway.py
