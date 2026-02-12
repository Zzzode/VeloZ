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

# 检查是否有 --check 选项
if [[ "$*" == *"--check"* ]]; then
  echo "Checking code formatting..."
  clang-format --dry-run --Werror "${files[@]}"
  echo "Code formatting is correct"
else
  echo "Formatting code..."
  clang-format -i "${files[@]}"
  echo "Code formatting completed"
fi

