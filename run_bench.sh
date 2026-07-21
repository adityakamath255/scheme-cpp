#!/bin/bash

passed=0
failed=0
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
scheme=${1:-$repo_dir/build/cli/scheme}

for bench in "$repo_dir"/bench/*.scm; do
  name=$(basename "$bench" .scm)

  elapsed=$( { time timeout 30s "$scheme" "$bench" > /dev/null 2>&1 ; } 2>&1 )
  exit_code=$?

  ms=$(echo "$elapsed" | grep '^real' | awk '{print $2}' | \
    sed 's/\([0-9]*\)m\([0-9.]*\)s/\1 \2/' | \
    awk '{printf "%.0f", ($1 * 60 + $2) * 1000}')

  if [ "$exit_code" -ne 0 ]; then
    printf "  %-25s  CRASH\n" "$name"
    ((failed++))
  else
    printf "  %-25s  %s ms\n" "$name" "$ms"
    ((passed++))
  fi
done

echo ""
echo "$passed passed, $failed failed"
