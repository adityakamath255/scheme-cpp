#!/bin/bash

passed=0
failed=0
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
scheme=${1:-$repo_dir/build/cli/scheme}

for test in "$repo_dir"/tests/test_*.scm; do
  name=$(basename "$test" .scm)
  output=$(
    timeout 30s "$scheme" <(cat "$repo_dir/tests/framework.scm" "$test") 2>&1
  )
  if [ $? -eq 0 ]; then
    echo "PASS  $name  ($output)"
    ((passed++))
  else
    echo "FAIL  $name"
    echo "$output"
    ((failed++))
  fi
done

echo ""
echo "$passed passed, $failed failed"
[ $failed -eq 0 ]
