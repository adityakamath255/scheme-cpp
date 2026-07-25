#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
wasm_build="$root/build/wasm"
site="$root/build/site"

emcmake cmake -S "$root" -B "$wasm_build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$wasm_build" --parallel

mkdir -p "$site"
npx --yes --package typescript@5.8.3 tsc \
  -p "$root/web/tsconfig.json" --outDir "$site"
cp "$root/web/index.html" "$root/web/styles.css" \
  "$wasm_build/web/scheme.js" "$wasm_build/web/scheme.wasm" "$site/"
