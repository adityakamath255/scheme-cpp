#!/bin/bash

passed=0
failed=0

for test in tests/test_*.scm; do
  name=$(basename "$test" .scm)
  output=$(timeout 30s ./cli/build/scheme <(cat tests/framework.scm "$test") 2>&1)
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
