#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if ! printf "%s" "${haystack}" | grep -q "${needle}"; then
    printf "Expected output to contain: %s\n" "${needle}"
    printf "Actual output:\n%s\n" "${haystack}"
    return 1
  fi
}

output="$("${root}/scripts/run_gateway.sh" --dry-run --skip-ui)"
assert_contains "${output}" "Preset: dev"

output="$("${root}/scripts/run_gateway.sh" --skip-ui --dry-run)"
assert_contains "${output}" "Preset: dev"

output="$("${root}/scripts/run_gateway.sh" --skip-ui release --dry-run)"
assert_contains "${output}" "Preset: release"
