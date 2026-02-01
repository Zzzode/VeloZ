#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found" >&2
  exit 1
fi

files=()
while IFS= read -r -d '' f; do
  files+=("$f")
done < <(find apps libs -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) -print0)

if [[ ${#files[@]} -eq 0 ]]; then
  exit 0
fi

clang-format -i "${files[@]}"

