#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
"$root/web/build.sh"
exec python3 -m http.server 8137 -d "$root/build/site"
