#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

ctest --test-dir "$root/build/native" --output-on-failure
ctest --test-dir "$root/build/wasm" --output-on-failure
